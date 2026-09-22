#!/usr/bin/env python3
"""
Generates quest and note constants and data tables from JSON.

Quests:
  src/data/quests/order.json     quest ids (position = id = save slot)
  src/data/quests/*.json         one quest per file
Notes:
  src/data/notes/order.json      note ids (position = id = save bit)
  src/data/notes/subjects.json   characters that notes can be about (position = id = save bit)
  src/data/notes/*.json          arrays of notes

Stage ids are the array position inside each quest file.

Outputs:
  include/constants/quests.h  QUEST_*, STAGE_*, OUTCOME_*, NOTE_*, SUBJECT_* and counts
  src/data/quests.h           const quest, note and subject tables and strings
"""

import glob
import json
import os
import re
import sys

DATA_DIR = "src/data/quests"
ORDER_FILE = os.path.join(DATA_DIR, "order.json")
NOTES_DIR = "src/data/notes"
NOTES_ORDER_FILE = os.path.join(NOTES_DIR, "order.json")
SUBJECTS_FILE = os.path.join(NOTES_DIR, "subjects.json")
TYPES_HEADER = "include/constants/quest_types.h"
CONSTANTS_OUT = "include/constants/quests.h"
DATA_OUT = "src/data/quests.h"

CATEGORIES = ("MAIN", "SIDE")
ICON_TYPES = ("OBJECT", "ITEM", "PKMN")
# "dex_count" comes first so its "dex" sub-key is not read as a species condition
CONDITION_KEYS = ("dex_count", "flag", "var", "item", "party", "dex", "quest", "quests_complete", "flags_count")
VAR_OPS = ("ge", "eq", "lt")
PARTY_KEYS = ("species", "type", "ability", "nature")
DEX_STATES = ("seen", "caught")
DEX_KINDS = ("regional", "national")
QUEST_OPS = {"status_ge": "STATUS_GE", "status": "STATUS_EQ", "stage_ge": "STAGE_GE", "outcome": "OUTCOME"}
STATUSES = ("HIDDEN", "AVAILABLE", "ACTIVE", "COMPLETE", "CLOSED")

QUEST_KEYS = {"id", "category", "kind", "parent", "tags", "name", "icon", "giver", "summary", "rewards",
              "stages", "outcomes", "objective", "turn_in"}
STAGE_KEYS = {"id", "title", "journal", "objectives", "turn_in", "paths"}
OBJECTIVE_KEYS = {"text", "target", "condition", "optional", "reveal"}
OUTCOME_KEYS = {"id", "summary", "closes"}
NOTE_KEYS = {"id", "text", "subject", "points_to", "resolved_by"}
SUBJECT_KEYS = {"id", "name", "graphics"}

ID_PATTERN = r"[A-Z_][A-Z0-9_]*"

errors = []


def error(msg):
    errors.append(msg)


def read_define(header_text, name):
    m = re.search(r"#define\s+" + name + r"\s+(\d+)", header_text)
    if not m:
        sys.exit(f"quests_to_header.py: {name} not found in {TYPES_HEADER}")
    return int(m.group(1))


def c_string(text):
    text = text.replace('"', '\\"').replace("\n", "\\n")
    return f'COMPOUND_STRING("{text}")'


def c_string_or_null(text):
    return c_string(text) if text is not None else "NULL"


def check_keys(obj, allowed, where):
    for key in obj:
        if key not in allowed:
            error(f"{where}: unknown key '{key}'")


def load_json(path):
    with open(path, encoding="utf-8") as f:
        try:
            return json.load(f)
        except json.JSONDecodeError as e:
            error(f"{path}: invalid JSON: {e}")
            return None


def read_order(path, prefix):
    """Reads an append-only id list. Returns (slots, names); a removed slot is None in slots."""
    order = load_json(path)
    if not isinstance(order, list):
        error(f"{path}: must be a list")
        return [], []
    slots = []
    names = []
    for i, entry in enumerate(order):
        if isinstance(entry, str):
            slots.append(entry)
            names.append(entry)
        elif isinstance(entry, dict) and "removed" in entry:
            slots.append(None)
            names.append(entry["removed"])
        else:
            error(f"{path}: entry {i} must be a {prefix}* string or {{ \"removed\": \"{prefix}*\" }}")
            slots.append(None)
            names.append(None)
    seen = set()
    for name in names:
        if name in seen:
            error(f"{path}: duplicate entry {name}")
        seen.add(name)
    return slots, names


def npc_ref(obj, where):
    """Returns (map, localid) C expressions for a {map, localid} object."""
    if not isinstance(obj, dict) or "map" not in obj:
        error(f"{where}: needs 'map'")
        return "MAP_UNDEFINED", "LOCALID_NONE"
    check_keys(obj, {"map", "localid"}, where)
    return obj["map"], obj.get("localid", "LOCALID_NONE")


def npc_list(value, where):
    """Accepts one {map, localid} object or a list of them."""
    if value is None:
        return []
    if isinstance(value, dict):
        value = [value]
    if not isinstance(value, list):
        error(f"{where}: must be an object or a list of objects")
        return []
    return [npc_ref(npc, f"{where} {i}") for i, npc in enumerate(value)]


class Generator:
    def __init__(self):
        self.data = []          # C definitions, emitted in order
        self.list_count = 0
        self.slots = []
        self.quests = {}
        self.tags = {}          # tag -> [quest ids in slot order]
        self.counted_tags = {}  # quest id -> tags it counts with quests_complete
        self.stage_ids = {}     # quest id -> stage ids
        self.outcome_ids = {}   # quest id -> outcome ids

    def emit_list(self, values):
        name = f"sQuestList_{self.list_count}"
        self.list_count += 1
        self.data.append(f"static const u16 {name}[] = {{ {', '.join(values)} }};\n")
        return name

    def condition(self, cond, where, owner=None):
        """Returns a C initializer for struct QuestCondition."""
        none = "{ .type = QUEST_COND_NONE }"
        if cond is None:
            return none
        if not isinstance(cond, dict):
            error(f"{where}: condition must be an object")
            return none
        keys = [k for k in CONDITION_KEYS if k in cond]
        if "dex_count" in keys:
            keys = ["dex_count"]
        if len(keys) != 1:
            error(f"{where}: condition needs exactly one of {', '.join(CONDITION_KEYS)}")
            return none
        kind = keys[0]
        progress = cond.get("progress")

        def progress_c(default):
            return "TRUE" if (default if progress is None else progress) else "FALSE"

        if kind == "flag":
            check_keys(cond, {"flag"}, where)
            return f"{{ .type = QUEST_COND_FLAG, .id = {cond['flag']} }}"

        if kind == "var":
            check_keys(cond, {"var", "progress"} | set(VAR_OPS), where)
            ops = [op for op in VAR_OPS if op in cond]
            if len(ops) != 1:
                error(f"{where}: var condition needs exactly one of {', '.join(VAR_OPS)}")
                return none
            op = ops[0]
            return (f"{{ .type = QUEST_COND_VAR, .op = QUEST_VAR_{op.upper()}, .id = {cond['var']}, "
                    f".value = {cond[op]}, .progress = {progress_c(False)} }}")

        if kind == "item":
            check_keys(cond, {"item", "count"}, where)
            return (f"{{ .type = QUEST_COND_ITEM, .id = {cond['item']}, "
                    f".value = {cond.get('count', 1)}, .progress = TRUE }}")

        if kind == "party":
            check_keys(cond, {"party"}, where)
            party = cond["party"]
            if not isinstance(party, dict) or len(party) != 1 or next(iter(party)) not in PARTY_KEYS:
                error(f"{where}: party condition needs exactly one of {', '.join(PARTY_KEYS)}")
                return none
            key, value = next(iter(party.items()))
            return f"{{ .type = QUEST_COND_PARTY, .op = PARTY_COND_{key.upper()}, .id = {value} }}"

        if kind == "dex":
            check_keys(cond, {"dex", "state"}, where)
            state = cond.get("state", "caught")
            if state not in DEX_STATES:
                error(f"{where}: unknown dex state '{state}'")
                state = "caught"
            return f"{{ .type = QUEST_COND_DEX, .op = QUEST_DEX_{state.upper()}, .id = {cond['dex']} }}"

        if kind == "dex_count":
            check_keys(cond, {"dex_count", "dex", "state", "progress"}, where)
            state = cond.get("state", "caught")
            dex = cond.get("dex", "regional")
            if state not in DEX_STATES:
                error(f"{where}: unknown dex state '{state}'")
                state = "caught"
            if dex not in DEX_KINDS:
                error(f"{where}: dex must be one of {', '.join(DEX_KINDS)}")
                dex = "regional"
            return (f"{{ .type = QUEST_COND_DEX_COUNT, .op = QUEST_DEX_{state.upper()}, .id = QUEST_DEX_{dex.upper()}, "
                    f".value = {cond['dex_count']}, .progress = {progress_c(True)} }}")

        if kind == "quest":
            check_keys(cond, {"quest"} | set(QUEST_OPS), where)
            qid = cond["quest"]
            if qid not in self.slots:
                error(f"{where}: unknown quest {qid}")
            ops = [op for op in QUEST_OPS if op in cond]
            if len(ops) != 1:
                error(f"{where}: quest condition needs exactly one of {', '.join(QUEST_OPS)}")
                return none
            op = ops[0]
            value = cond[op]
            if op in ("status_ge", "status"):
                short = value[len("QUEST_STATUS_"):] if isinstance(value, str) and value.startswith("QUEST_STATUS_") else value
                if short not in STATUSES:
                    error(f"{where}: unknown status {value}")
                value = f"QUEST_STATUS_{short}"
            elif op == "stage_ge" and value not in self.stage_ids.get(qid, ()):
                error(f"{where}: {value} is not a stage of {qid}")
            elif op == "outcome" and value not in self.outcome_ids.get(qid, ()):
                error(f"{where}: {value} is not an outcome of {qid}")
            return f"{{ .type = QUEST_COND_QUEST, .op = QUEST_CMP_{QUEST_OPS[op]}, .id = {qid}, .value = {value} }}"

        if kind == "quests_complete":
            check_keys(cond, {"quests_complete", "progress"}, where)
            spec = cond["quests_complete"]
            if not isinstance(spec, dict) or "tag" not in spec:
                error(f"{where}: quests_complete needs {{ \"tag\", \"count\" }}")
                return none
            check_keys(spec, {"tag", "count"}, where)
            members = self.tags.get(spec["tag"], [])
            if not members:
                error(f"{where}: no quest has tag {spec['tag']}")
                return none
            count = spec.get("count", len(members))
            if count > len(members):
                error(f"{where}: count {count} is more than the {len(members)} quests tagged {spec['tag']}")
            if owner is not None:
                self.counted_tags.setdefault(owner, set()).add(spec["tag"])
            return (f"{{ .type = QUEST_COND_QUESTS_COMPLETE, .list = sQuestTag_{spec['tag']}, .listCount = {len(members)}, "
                    f".value = {count}, .progress = {progress_c(True)} }}")

        # flags_count
        check_keys(cond, {"flags_count", "count", "progress"}, where)
        flags = cond["flags_count"]
        if not isinstance(flags, list) or not flags:
            error(f"{where}: flags_count needs a list of flags")
            return none
        count = cond.get("count", len(flags))
        if count > len(flags):
            error(f"{where}: count {count} is more than the {len(flags)} flags listed")
        name = self.emit_list(flags)
        return (f"{{ .type = QUEST_COND_FLAGS_COUNT, .list = {name}, .listCount = {len(flags)}, "
                f".value = {count}, .progress = {progress_c(True)} }}")


def expand_task(q, where):
    """Fills in the short task form: one stage, one objective, one outcome."""
    if "objective" in q:
        if "stages" in q:
            error(f"{where}: a task has either 'objective' or 'stages', not both")
        stage = {"title": q["name"], "journal": None, "objectives": [q["objective"]]}
        if "turn_in" in q:
            stage["turn_in"] = q["turn_in"]
        q["stages"] = [stage]
    elif "turn_in" in q:
        error(f"{where}: 'turn_in' at the top level is only for the short task form")
    q.setdefault("outcomes", [{"summary": "Done."}])
    q.setdefault("category", "SIDE")
    q.setdefault("summary", "")
    q.setdefault("icon", {"type": "ITEM", "value": "ITEM_NONE"})


def main():
    with open(TYPES_HEADER) as f:
        types_text = f.read()
    quest_max = read_define(types_text, "QUEST_MAX")
    max_objectives = read_define(types_text, "QUEST_MAX_OBJECTIVES")
    max_outcomes = read_define(types_text, "QUEST_MAX_OUTCOMES")
    max_paths = read_define(types_text, "QUEST_MAX_PATHS")
    note_max = read_define(types_text, "NOTE_MAX")
    subject_max = read_define(types_text, "SUBJECT_MAX")

    gen = Generator()
    slots, order_names = read_order(ORDER_FILE, "QUEST_")
    gen.slots = slots
    if len(slots) > quest_max:
        error(f"{ORDER_FILE}: {len(slots)} quests exceeds QUEST_MAX ({quest_max})")

    for path in sorted(glob.glob(os.path.join(DATA_DIR, "*.json"))):
        if os.path.abspath(path) == os.path.abspath(ORDER_FILE):
            continue
        quest = load_json(path)
        if quest is None:
            continue
        quest["_path"] = path
        qid = quest.get("id")
        if qid is None:
            error(f"{path}: missing 'id'")
            continue
        if qid in gen.quests:
            error(f"{path}: duplicate quest id {qid} (also in {gen.quests[qid]['_path']})")
            continue
        if qid not in slots:
            error(f"{path}: {qid} is not listed in {ORDER_FILE}")
        gen.quests[qid] = quest

    for qid in slots:
        if qid is not None and qid not in gen.quests:
            error(f"{ORDER_FILE}: {qid} has no quest file (mark it {{ \"removed\": \"{qid}\" }} if deleted)")

    # First pass: tasks, tags, stage and outcome ids, so conditions can refer to any quest
    for qid in slots:
        if qid is None or qid not in gen.quests:
            continue
        q = gen.quests[qid]
        where = q["_path"]
        if "lead" in q:
            error(f"{where}: 'lead' was removed; leads are notes now (see {NOTES_DIR})")
        kind = q.get("kind", "quest")
        if kind not in ("quest", "task"):
            error(f"{where}: kind must be 'quest' or 'task'")
        if kind == "task":
            if "name" not in q:
                error(f"{where}: missing 'name'")
                q["name"] = ""
            expand_task(q, where)
        tags = q.get("tags", [])
        if not isinstance(tags, list):
            error(f"{where}: 'tags' must be a list")
            tags = []
        for tag in tags:
            if not isinstance(tag, str) or not re.fullmatch(ID_PATTERN, tag):
                error(f"{where}: invalid tag {tag!r}")
                continue
            gen.tags.setdefault(tag, []).append(qid)
        gen.stage_ids[qid] = [s["id"] for s in q.get("stages", []) if isinstance(s, dict) and "id" in s]
        gen.outcome_ids[qid] = [o["id"] for o in q.get("outcomes", []) if isinstance(o, dict) and "id" in o]

    for tag, members in gen.tags.items():
        gen.data.append(f"static const u16 sQuestTag_{tag}[] = {{ {', '.join(members)} }};\n")

    # Shared id namespace check (QUEST_*, STAGE_*, OUTCOME_*, NOTE_*, SUBJECT_*)
    all_ids = {}
    for name in order_names:
        if name:
            all_ids[name] = ORDER_FILE

    def claim(name, where):
        if not isinstance(name, str) or not re.fullmatch(ID_PATTERN, name):
            error(f"{where}: invalid id {name!r}")
            return
        if name in all_ids:
            error(f"{where}: duplicate id {name} (also in {all_ids[name]})")
        else:
            all_ids[name] = where

    constants = []
    quest_inits = []

    for qid in slots:
        if qid is None or qid not in gen.quests:
            continue
        q = gen.quests[qid]
        where = q["_path"]
        is_task = q.get("kind") == "task"
        check_keys(q, QUEST_KEYS | {"_path"}, where)
        required = ["category", "name", "icon", "summary", "stages", "outcomes"]
        if not is_task:
            required.append("giver")
        for req in required:
            if req not in q:
                error(f"{where}: missing '{req}'")
        if any(req not in q for req in required):
            continue

        if q["category"] not in CATEGORIES:
            error(f"{where}: unknown category '{q['category']}' (use {', '.join(CATEGORIES)})")
        icon = q["icon"]
        if icon.get("type") not in ICON_TYPES or "value" not in icon:
            error(f"{where}: icon needs type ({', '.join(ICON_TYPES)}) and value")

        parent = "QUEST_NONE"
        if is_task:
            parent = q.get("parent")
            if parent is None:
                error(f"{where}: a task needs a 'parent'")
                parent = "QUEST_NONE"
            elif parent not in gen.quests:
                error(f"{where}: unknown parent {parent}")
            elif gen.quests[parent].get("kind") == "task":
                error(f"{where}: parent {parent} is a task")
            if not q.get("tags"):
                error(f"{where}: a task needs a tag that its parent counts")
        elif "parent" in q:
            error(f"{where}: only a task has a 'parent'")

        stages = q["stages"]
        if len(stages) == 0:
            error(f"{where}: needs at least one stage")
        if len(stages) > 255:
            error(f"{where}: too many stages")
        outcomes = q["outcomes"]
        if len(outcomes) == 0:
            error(f"{where}: needs at least one outcome")
        if len(outcomes) > max_outcomes:
            error(f"{where}: {len(outcomes)} outcomes exceeds QUEST_MAX_OUTCOMES ({max_outcomes})")

        tag = qid.lower()
        constants.append(f"// {qid}")

        # Stages
        stage_inits = []
        path_stages = [0] * max_paths
        for s_index, stage in enumerate(stages):
            s_where = f"{where}: stage {s_index}"
            if "removed" in stage:
                claim(stage["removed"], s_where)
                stage_inits.append(f"    [{s_index}] = {{ .title = NULL }}, // removed {stage['removed']}")
                continue
            check_keys(stage, STAGE_KEYS, s_where)
            required = ["title", "journal", "objectives"] + ([] if is_task else ["id"])
            for req in required:
                if req not in stage:
                    error(f"{s_where}: missing '{req}'")
            if any(req not in stage for req in required):
                continue
            if "id" in stage:
                claim(stage["id"], s_where)
                constants.append(f"#define {stage['id']} {s_index}")

            paths = stage.get("paths", list(range(max_paths)))
            if not isinstance(paths, list) or not paths:
                error(f"{s_where}: 'paths' must be a non-empty list")
                paths = list(range(max_paths))
            mask = 0
            for p in paths:
                if not isinstance(p, int) or not 0 <= p < max_paths:
                    error(f"{s_where}: path {p!r} must be 0-{max_paths - 1}")
                    continue
                mask |= 1 << p
                path_stages[p] += 1

            objectives = stage["objectives"]
            if len(objectives) > max_objectives:
                error(f"{s_where}: {len(objectives)} objectives exceeds QUEST_MAX_OBJECTIVES ({max_objectives})")

            objective_inits = []
            for o_index, obj in enumerate(objectives):
                o_where = f"{s_where} objective {o_index}"
                check_keys(obj, OBJECTIVE_KEYS, o_where)
                if "text" not in obj:
                    error(f"{o_where}: missing 'text'")
                    continue
                t_map, t_local = npc_ref(obj["target"], f"{o_where} target") if "target" in obj else ("MAP_UNDEFINED", "LOCALID_NONE")
                objective_inits.append(
                    "    {\n"
                    f"        .text = {c_string(obj['text'])},\n"
                    f"        .targetMap = {t_map},\n"
                    f"        .targetLocalId = {t_local},\n"
                    f"        .optional = {'TRUE' if obj.get('optional', False) else 'FALSE'},\n"
                    f"        .condition = {gen.condition(obj.get('condition'), o_where, qid)},\n"
                    f"        .reveal = {gen.condition(obj.get('reveal'), o_where + ' reveal', qid)},\n"
                    "    },")

            obj_array = f"sQuestObjectives_{tag}_{s_index}"
            if objective_inits:
                gen.data.append(f"static const struct QuestObjective {obj_array}[] =\n{{\n" + "\n".join(objective_inits) + "\n};\n")

            turn_ins = npc_list(stage.get("turn_in"), f"{s_where} turn_in")
            turn_in_array = "NULL"
            if turn_ins:
                turn_in_array = f"sQuestTurnIns_{tag}_{s_index}"
                gen.data.append(f"static const struct QuestNpc {turn_in_array}[] =\n{{\n"
                                + "\n".join(f"    {{ {m}, {l} }}," for m, l in turn_ins) + "\n};\n")
            stage_inits.append(
                f"    [{s_index}] =\n"
                "    {\n"
                f"        .title = {c_string(stage['title'])},\n"
                f"        .journal = {c_string_or_null(stage['journal'])},\n"
                f"        .objectives = {obj_array if objective_inits else 'NULL'},\n"
                f"        .turnIns = {turn_in_array},\n"
                f"        .objectiveCount = {len(objective_inits)},\n"
                f"        .turnInCount = {len(turn_ins)},\n"
                f"        .paths = 0x{mask:X},\n"
                "    },")

        if path_stages[0] == 0:
            error(f"{where}: no stage is on path 0 (the default path)")

        gen.data.append(f"static const struct QuestStage sQuestStages_{tag}[] =\n{{\n" + "\n".join(stage_inits) + "\n};\n")

        # Outcomes
        outcome_inits = []
        for o_index, outcome in enumerate(outcomes):
            o_where = f"{where}: outcome {o_index}"
            check_keys(outcome, OUTCOME_KEYS, o_where)
            if "summary" not in outcome or ("id" not in outcome and not is_task):
                error(f"{o_where}: needs 'id' and 'summary'")
                continue
            if "id" in outcome:
                claim(outcome["id"], o_where)
                constants.append(f"#define {outcome['id']} {o_index}")
            closes = outcome.get("closes", [])
            for closed in closes:
                if closed not in slots:
                    error(f"{o_where}: closes unknown quest {closed}")
            closes_array = "NULL"
            if closes:
                closes_array = f"sQuestCloses_{tag}_{o_index}"
                gen.data.append(f"static const u8 {closes_array}[] = {{ {', '.join(closes)} }};\n")
            outcome_inits.append(
                f"    [{o_index}] = {{ .summary = {c_string(outcome['summary'])}, "
                f".closes = {closes_array}, .closesCount = {len(closes)} }},")
        gen.data.append(f"static const struct QuestOutcome sQuestOutcomes_{tag}[] =\n{{\n" + "\n".join(outcome_inits) + "\n};\n")

        # Rewards
        rewards = q.get("rewards", [])
        reward_inits = []
        for r_index, reward in enumerate(rewards):
            r_where = f"{where}: reward {r_index}"
            if "item" in reward:
                check_keys(reward, {"item", "count"}, r_where)
                reward_inits.append(f"    {{ .type = QUEST_REWARD_ITEM, .item = {reward['item']}, .amount = {reward.get('count', 1)} }},")
            elif "money" in reward:
                check_keys(reward, {"money"}, r_where)
                reward_inits.append(f"    {{ .type = QUEST_REWARD_MONEY, .amount = {reward['money']} }},")
            else:
                error(f"{r_where}: needs 'item' or 'money'")
        rewards_array = "NULL"
        if reward_inits:
            rewards_array = f"sQuestRewards_{tag}"
            gen.data.append(f"static const struct QuestReward {rewards_array}[] =\n{{\n" + "\n".join(reward_inits) + "\n};\n")

        # Givers
        givers = npc_list(q.get("giver"), f"{where}: giver")
        givers_array = "NULL"
        if givers:
            givers_array = f"sQuestGivers_{tag}"
            gen.data.append(f"static const struct QuestNpc {givers_array}[] =\n{{\n"
                            + "\n".join(f"    {{ {m}, {l} }}," for m, l in givers) + "\n};\n")

        quest_inits.append(
            f"    [{qid}] =\n"
            "    {\n"
            f"        .name = {c_string(q['name'])},\n"
            f"        .summary = {c_string(q['summary'])},\n"
            f"        .category = QUEST_CATEGORY_{q['category']},\n"
            f"        .iconType = QUEST_ICON_{icon.get('type', 'OBJECT')},\n"
            f"        .icon = {icon.get('value', 0)},\n"
            f"        .parent = {parent},\n"
            f"        .isTask = {'TRUE' if is_task else 'FALSE'},\n"
            f"        .givers = {givers_array},\n"
            f"        .rewards = {rewards_array},\n"
            f"        .stages = sQuestStages_{tag},\n"
            f"        .outcomes = sQuestOutcomes_{tag},\n"
            f"        .giverCount = {len(givers)},\n"
            f"        .rewardCount = {len(reward_inits)},\n"
            f"        .stageCount = {len(stages)},\n"
            f"        .outcomeCount = {len(outcome_inits)},\n"
            "    },")
        constants.append("")

    # A task must be counted by its parent
    for qid in slots:
        q = gen.quests.get(qid) if qid else None
        if q is None or q.get("kind") != "task" or q.get("parent") not in gen.quests:
            continue
        if not set(q.get("tags", [])) & gen.counted_tags.get(q["parent"], set()):
            error(f"{q['_path']}: parent {q['parent']} has no quests_complete objective for any of this task's tags")

    # Notes and subjects
    note_slots, note_names = [], []
    subject_inits = []
    note_inits = []
    if os.path.exists(NOTES_ORDER_FILE):
        note_slots, note_names = read_order(NOTES_ORDER_FILE, "NOTE_")

    # Subjects are listed in order in one file; position = id
    subject_entries = load_json(SUBJECTS_FILE) if os.path.exists(SUBJECTS_FILE) else []
    subject_entries = subject_entries if isinstance(subject_entries, list) else []
    for s_index, subject in enumerate(subject_entries):
        s_where = f"{SUBJECTS_FILE}: entry {s_index}"
        if isinstance(subject, dict) and "removed" in subject:
            claim(subject["removed"], s_where)
            continue
        if not isinstance(subject, dict) or any(k not in subject for k in SUBJECT_KEYS):
            error(f"{s_where}: needs {', '.join(sorted(SUBJECT_KEYS))}")
            continue
        check_keys(subject, SUBJECT_KEYS, s_where)
        claim(subject["id"], s_where)
        subject_inits.append(f"    [{subject['id']}] = {{ .name = {c_string(subject['name'])}, .graphicsId = {subject['graphics']} }},")
    subject_slots = [s["id"] if isinstance(s, dict) and "id" in s else None for s in subject_entries]
    subject_names = [s.get("id", s.get("removed")) if isinstance(s, dict) else None for s in subject_entries]
    if len(subject_slots) > subject_max:
        error(f"{SUBJECTS_FILE}: {len(subject_slots)} subjects exceeds SUBJECT_MAX ({subject_max})")
    if len(note_slots) > note_max:
        error(f"{NOTES_ORDER_FILE}: {len(note_slots)} notes exceeds NOTE_MAX ({note_max})")

    notes = {}
    for path in sorted(glob.glob(os.path.join(NOTES_DIR, "*.json"))):
        if os.path.abspath(path) in (os.path.abspath(NOTES_ORDER_FILE), os.path.abspath(SUBJECTS_FILE)):
            continue
        entries = load_json(path)
        if entries is None:
            continue
        if not isinstance(entries, list):
            error(f"{path}: must be a list of notes")
            continue
        for i, note in enumerate(entries):
            n_where = f"{path}: note {i}"
            if not isinstance(note, dict) or "id" not in note:
                error(f"{n_where}: missing 'id'")
                continue
            if note["id"] in notes:
                error(f"{n_where}: duplicate note id {note['id']}")
                continue
            if note["id"] not in note_slots:
                error(f"{n_where}: {note['id']} is not listed in {NOTES_ORDER_FILE}")
            note["_where"] = n_where
            notes[note["id"]] = note

    for nid in note_slots:
        if nid is not None and nid not in notes:
            error(f"{NOTES_ORDER_FILE}: {nid} has no note (mark it {{ \"removed\": \"{nid}\" }} if deleted)")

    for nid in note_slots:
        if nid is None or nid not in notes:
            continue
        note = notes[nid]
        n_where = note["_where"]
        check_keys(note, NOTE_KEYS | {"_where"}, n_where)
        claim(nid, n_where)
        if "text" not in note:
            error(f"{n_where}: missing 'text'")
            continue
        subject = note.get("subject", "SUBJECT_NONE")
        if subject != "SUBJECT_NONE" and subject not in subject_slots:
            error(f"{n_where}: unknown subject {subject}")
        t_map, t_local = ("MAP_UNDEFINED", "LOCALID_NONE")
        if "points_to" in note:
            t_map, t_local = npc_ref(note["points_to"], f"{n_where} points_to")
            if "resolved_by" not in note:
                error(f"{n_where}: a lead (points_to) needs 'resolved_by'")
        elif "resolved_by" in note:
            error(f"{n_where}: 'resolved_by' is only for leads (notes with points_to)")
        note_inits.append(
            f"    [{nid}] =\n"
            "    {\n"
            f"        .text = {c_string(note['text'])},\n"
            f"        .targetMap = {t_map},\n"
            f"        .targetLocalId = {t_local},\n"
            f"        .subject = {subject},\n"
            f"        .resolvedBy = {gen.condition(note.get('resolved_by'), n_where + ' resolved_by')},\n"
            "    },")

    if errors:
        for e in errors:
            print(f"quests_to_header.py: error: {e}", file=sys.stderr)
        sys.exit(1)

    header = "//\n// DO NOT MODIFY THIS FILE! It is auto-generated by tools/quests/quests_to_header.py\n//\n\n"

    def write_ids(f, names, slots_):
        for slot, name in enumerate(names):
            if slots_[slot] is not None:
                f.write(f"#define {name} {slot}\n")
            else:
                f.write(f"// slot {slot}: removed {name}\n")

    with open(CONSTANTS_OUT, "w", encoding="utf-8") as f:
        f.write("#ifndef GUARD_CONSTANTS_QUESTS_H\n#define GUARD_CONSTANTS_QUESTS_H\n\n")
        f.write(header)
        f.write('#include "constants/quest_types.h"\n\n')
        write_ids(f, order_names, slots)
        f.write(f"\n#define QUEST_COUNT {len(slots)}\n\n")
        write_ids(f, subject_names, subject_slots)
        f.write(f"\n#define SUBJECT_COUNT {len(subject_slots)}\n\n")
        write_ids(f, note_names, note_slots)
        f.write(f"\n#define NOTE_COUNT {len(note_slots)}\n\n")
        f.write("\n".join(constants))
        f.write("\n#endif // GUARD_CONSTANTS_QUESTS_H\n")

    with open(DATA_OUT, "w", encoding="utf-8") as f:
        f.write(header)
        f.write("\n".join(gen.data))
        # At least one (empty, invalid) entry so the table still compiles with no quests
        f.write("\nconst struct Quest gQuests[QUEST_COUNT > 0 ? QUEST_COUNT : 1] =\n{\n")
        f.write("\n".join(quest_inits))
        f.write("\n};\n")
        # At least one entry so the tables exist when there is no content
        f.write("\nconst struct QuestSubject gQuestSubjects[SUBJECT_COUNT > 0 ? SUBJECT_COUNT : 1] =\n{\n")
        f.write("\n".join(subject_inits))
        f.write("\n};\n")
        f.write("\nconst struct QuestNote gQuestNotes[NOTE_COUNT > 0 ? NOTE_COUNT : 1] =\n{\n")
        f.write("\n".join(note_inits))
        f.write("\n};\n")


if __name__ == "__main__":
    main()
