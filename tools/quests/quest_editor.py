#!/usr/bin/env python3
"""
Local quest editor. Serves tools/quests/quest_editor.html and a small JSON API that reads and writes
src/data/quests/*.json and order.json, then validates with quests_to_header.py --check.

Usage (from the repo root or anywhere):
  python3 tools/quests/quest_editor.py [--port 8765]
Then open http://localhost:8765 in a browser. Only listens on 127.0.0.1.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
QUEST_DIR = os.path.join(ROOT, "src", "data", "quests")
ORDER_FILE = os.path.join(QUEST_DIR, "order.json")
HTML_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "quest_editor.html")
GENERATOR = os.path.join(ROOT, "tools", "quests", "quests_to_header.py")

INLINE_WIDTH = 130
# Objects under these keys are always written one key per line
EXPANDED_KEYS = {"stages"}


# *******************************
# JSON formatting that matches the hand-written quest files: small objects on one line

def _inline(value):
    if isinstance(value, dict):
        if not value:
            return "{}"
        return "{ " + ", ".join(f"{json.dumps(k, ensure_ascii=False)}: {_inline(v)}" for k, v in value.items()) + " }"
    if isinstance(value, list):
        return "[" + ", ".join(_inline(v) for v in value) + "]"
    return json.dumps(value, ensure_ascii=False)


def _format(value, indent, prefix_len, parent_key=None):
    pad = "  " * indent
    one_line = _inline(value)
    is_container = isinstance(value, (dict, list)) and value
    if not is_container:
        return one_line
    if isinstance(value, list):
        if not any(isinstance(v, (dict, list)) for v in value):
            return one_line
        items = [pad + "  " + _format(v, indent + 1, len(pad) + 2, parent_key) for v in value]
        return "[\n" + ",\n".join(items) + "\n" + pad + "]"
    if indent > 0 and parent_key not in EXPANDED_KEYS and prefix_len + len(one_line) <= INLINE_WIDTH:
        return one_line
    items = []
    for k, v in value.items():
        key = json.dumps(k, ensure_ascii=False) + ": "
        items.append(pad + "  " + key + _format(v, indent + 1, len(pad) + 2 + len(key), k))
    return "{\n" + ",\n".join(items) + "\n" + pad + "}"


def dump_quest(data):
    return _format(data, 0, 0) + "\n"


def dump_order(order):
    return "[\n" + ",\n".join("  " + _inline(e) for e in order) + "\n]\n"


# *******************************
# Repo data

def read_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def load_state():
    order = read_json(ORDER_FILE)
    quests = {}
    errors = []
    for path in sorted(glob.glob(os.path.join(QUEST_DIR, "*.json"))):
        if os.path.abspath(path) == os.path.abspath(ORDER_FILE):
            continue
        try:
            data = read_json(path)
        except (OSError, json.JSONDecodeError) as e:
            errors.append(f"{os.path.basename(path)}: {e}")
            continue
        qid = data.get("id")
        if qid:
            quests[qid] = {"file": os.path.basename(path), "data": data}
    return {"order": order, "quests": quests, "loadErrors": errors}


def scan_constants(path, prefix):
    """Enum members and #defines starting with prefix."""
    names = []
    try:
        with open(os.path.join(ROOT, path), encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        return names
    pattern = rf"^\s*(?:#define\s+)?({prefix}[A-Z0-9_]+)\b\s*(?:=|,|\s|$)"
    for m in re.finditer(pattern, text, re.M):
        name = m.group(1)
        if name not in names and not name.endswith("_COUNT"):
            names.append(name)
    return names


def load_constants():
    maps = {}
    for path in glob.glob(os.path.join(ROOT, "data", "maps", "*", "map.json")):
        try:
            data = read_json(path)
        except (OSError, json.JSONDecodeError):
            continue
        npcs = [{"localid": o["local_id"], "gfx": o.get("graphics_id", "")}
                for o in data.get("object_events", []) if o.get("local_id")]
        maps[data["id"]] = npcs
    return {
        "maps": dict(sorted(maps.items())),
        "gfx": scan_constants("include/constants/event_objects.h", "OBJ_EVENT_GFX_"),
        "flags": scan_constants("include/constants/flags.h", "FLAG_"),
        "vars": scan_constants("include/constants/vars.h", "VAR_"),
        "items": scan_constants("include/constants/items.h", "ITEM_"),
        "species": scan_constants("include/constants/species.h", "SPECIES_"),
        "types": scan_constants("include/constants/pokemon.h", "TYPE_"),
        "abilities": scan_constants("include/constants/abilities.h", "ABILITY_"),
        "natures": scan_constants("include/constants/pokemon.h", "NATURE_"),
    }


def validate():
    result = subprocess.run([sys.executable, GENERATOR, "--check"], cwd=ROOT, capture_output=True, text=True)
    lines = [l.replace("quests_to_header.py: error: ", "") for l in (result.stdout + result.stderr).splitlines() if l.strip()]
    return {"ok": result.returncode == 0, "messages": lines}


def replace_strings(value, old, new):
    if isinstance(value, dict):
        return {k: replace_strings(v, old, new) for k, v in value.items()}
    if isinstance(value, list):
        return [replace_strings(v, old, new) for v in value]
    return new if value == old else value


def references_to(qid, state):
    """Quest files other than qid's own that mention qid anywhere."""
    refs = []
    for other, entry in state["quests"].items():
        if other != qid and qid in json.dumps(entry["data"]):
            refs.append(other)
    return refs


def safe_file_name(name):
    return re.fullmatch(r"[a-z0-9_]+\.json", name) is not None and name != "order.json"


def save_quest(body):
    """body: { data, file (existing or null), oldId (or null) }"""
    data = body["data"]
    qid = data.get("id", "")
    if not re.fullmatch(r"QUEST_[A-Z0-9_]+", qid):
        return {"error": "The id must look like QUEST_SOMETHING."}
    state = load_state()
    order = state["order"]
    old_id = body.get("oldId")
    file_name = body.get("file") or (qid[len("QUEST_"):].lower() + ".json")
    if not safe_file_name(file_name):
        return {"error": f"Bad file name {file_name}"}

    names = [e if isinstance(e, str) else e.get("removed") for e in order]
    renamed = []
    if old_id is None:
        if qid in names:
            return {"error": f"{qid} is already used in order.json (ids are never reused)."}
        if os.path.exists(os.path.join(QUEST_DIR, file_name)):
            return {"error": f"{file_name} already exists."}
        order.append(qid)
    elif old_id != qid:
        if qid in names:
            return {"error": f"{qid} is already used in order.json."}
        # Keep the save slot: rename in place, then update references in the other quests
        order[names.index(old_id)] = qid
        for other, entry in state["quests"].items():
            if other == old_id:
                continue
            updated = replace_strings(entry["data"], old_id, qid)
            if updated != entry["data"]:
                write_text(os.path.join(QUEST_DIR, entry["file"]), dump_quest(updated))
                renamed.append(other)

    write_text(os.path.join(QUEST_DIR, file_name), dump_quest(data))
    write_text(ORDER_FILE, dump_order(order))
    return {"file": file_name, "updatedReferences": renamed, "validation": validate()}


def delete_quest(body):
    qid = body["id"]
    state = load_state()
    if qid not in state["quests"]:
        return {"error": f"Unknown quest {qid}"}
    order = state["order"]
    order = [{"removed": qid} if e == qid else e for e in order]
    os.remove(os.path.join(QUEST_DIR, state["quests"][qid]["file"]))
    write_text(ORDER_FILE, dump_order(order))
    return {"ok": True, "validation": validate()}


# *******************************
# HTTP

class Handler(BaseHTTPRequestHandler):
    constants = None

    def log_message(self, fmt, *args):
        pass

    def send_json(self, value, status=200):
        body = json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            with open(HTML_FILE, "rb") as f:
                body = f.read()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/api/state":
            self.send_json(load_state())
        elif self.path == "/api/constants":
            if Handler.constants is None:
                Handler.constants = load_constants()
            self.send_json(Handler.constants)
        elif self.path == "/api/validate":
            self.send_json(validate())
        else:
            self.send_json({"error": "not found"}, 404)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        try:
            body = json.loads(self.rfile.read(length) or b"{}")
        except json.JSONDecodeError:
            self.send_json({"error": "bad JSON"}, 400)
            return
        if self.path == "/api/save":
            self.send_json(save_quest(body))
        elif self.path == "/api/delete":
            self.send_json(delete_quest(body))
        elif self.path == "/api/references":
            self.send_json({"references": references_to(body["id"], load_state())})
        else:
            self.send_json({"error": "not found"}, 404)


def main():
    parser = argparse.ArgumentParser(description="Local quest editor for src/data/quests.")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"Quest editor: http://localhost:{args.port}  (Ctrl+C to stop)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
