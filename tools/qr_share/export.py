#!/usr/bin/env python3
"""Exports the data the QR Share website needs from a built ROM.

Run from the repo root after `make`:
    python3 tools/qr_share/export.py

Writes tools/qr_share/web/data/v<QR_SHARE_DATA_VERSION>/{layout,names,backgrounds}.json,
overworld sprites to tools/qr_share/web/img/overworld/ and front sprites to tools/qr_share/web/img/front/. An existing
backgrounds.json keeps its coordinates; only new backgrounds are added.

IDs and names are read from pokeemerald.elf/.gba so they always match the ROM.
Struct offsets come from compiling a small C file with the game's headers.
"""

import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / "tools" / "qr_share" / "web"
ELF = ROOT / "pokeemerald.elf"
ROM = ROOT / "pokeemerald.gba"
ROM_BASE = 0x08000000
EOS = 0xFF

# Default tile positions for a 240x160 (15x10 tile) background.
DEFAULT_PLAYER = [7, 5]
DEFAULT_PARTY = [[6, 6], [8, 6], [5, 6], [9, 6], [4, 6], [10, 6]]

OFFSETS_C = """
#include "global.h"
#include "item.h"
#include "move.h"
#include "pokemon.h"
#include "qr_share.h"
#include "config/overworld.h"
#define V(x) (u32)(x),
const u32 kQrShareExport[] = {
    V(QR_SHARE_DATA_VERSION)
    V(NUM_SPECIES) V(ITEMS_COUNT) V(MOVES_COUNT) V(ABILITIES_COUNT) V(NATIONAL_DEX_COUNT)
    V(sizeof(struct SpeciesInfo)) V(offsetof(struct SpeciesInfo, speciesName))
#if OW_POKEMON_OBJECT_EVENTS
    V(offsetof(struct SpeciesInfo, overworldData))
    V(offsetof(struct SpeciesInfo, overworldPalette)) V(offsetof(struct SpeciesInfo, overworldShinyPalette))
#else
    V(-1) V(-1) V(-1)
#endif
    V(sizeof(struct MoveInfo)) V(offsetof(struct MoveInfo, name))
    V(sizeof(struct ItemInfo)) V(offsetof(struct ItemInfo, name))
    V(sizeof(struct AbilityInfo)) V(offsetof(struct AbilityInfo, name))
    V(sizeof(struct NatureInfo)) V(offsetof(struct NatureInfo, name))
    V(offsetof(struct ObjectEventGraphicsInfo, width)) V(offsetof(struct ObjectEventGraphicsInfo, height))
    V(offsetof(struct ObjectEventGraphicsInfo, images))
    V(offsetof(struct SpeciesInfo, frontPic))
    V(offsetof(struct SpeciesInfo, palette)) V(offsetof(struct SpeciesInfo, shinyPalette))
    V(QR_SHARE_SHOW_MEGA_FORMS) V(offsetof(struct SpeciesInfo, formChangeTable))
    V(sizeof(struct FormChange)) V(offsetof(struct FormChange, targetSpecies))
    V(sizeof(((struct FormChange *)0)->targetSpecies)) V(offsetof(struct FormChange, param1))
    V(FORM_CHANGE_TERMINATOR) V(FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM)
};
"""
OFFSET_KEYS = [
    "dataVersion", "numSpecies", "itemsCount", "movesCount", "abilitiesCount", "nationalDexCount",
    "speciesSize", "speciesName", "speciesOverworld", "speciesPalette", "speciesShinyPalette",
    "moveSize", "moveName", "itemSize", "itemName", "abilitySize", "abilityName", "natureSize", "natureName",
    "gfxWidth", "gfxHeight", "gfxImages",
    "speciesFrontPic", "speciesFrontPalette", "speciesFrontShinyPalette",
    "showMegaForms", "speciesFormChanges", "formChangeSize", "formChangeTarget", "formChangeTargetSize",
    "formChangeParam1", "formChangeTerminator", "formChangeMegaItem",
]


def fail(message):
    sys.exit(f"export.py: {message}")


def compile_offsets():
    with tempfile.TemporaryDirectory() as tmp:
        source = Path(tmp) / "qr_share_offsets.c"
        source.write_text(OFFSETS_C)
        result = subprocess.run(
            ["arm-none-eabi-gcc", "-iquote", "include", "-DMODERN=1", "-DTESTING=0", "-DEMERALD",
             "-std=gnu17", "-mthumb", "-mabi=apcs-gnu", "-march=armv4t", "-S", "-o", "-", str(source)],
            cwd=ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        fail("compiling struct offsets failed:\n" + result.stderr)
    words = re.findall(r"^\s*\.word\s+(-?\d+)", result.stdout.split("kQrShareExport:")[1], re.M)
    return dict(zip(OFFSET_KEYS, (int(w) & 0xFFFFFFFF for w in words)))


def read_symbols():
    result = subprocess.run(["arm-none-eabi-nm", "-S", str(ELF)], capture_output=True, text=True, check=True)
    by_name, by_address = {}, {}
    for line in result.stdout.splitlines():
        parts = line.split()
        if len(parts) == 4:
            address, size, _, name = int(parts[0], 16), int(parts[1], 16), parts[2], parts[3]
        elif len(parts) == 3:
            address, size, name = int(parts[0], 16), 0, parts[2]
        else:
            continue
        by_name[name] = (address, size)
        by_address.setdefault(address, name)
    return by_name, by_address


def load_charset():
    charset = [""] * 256
    for line in (ROOT / "charmap.txt").read_text(encoding="utf-8").splitlines():
        match = re.match(r"^'(\\?.)'\s*=\s*([0-9A-Fa-f]{2})\s*$", line)
        if match:
            char = match.group(1).replace("\\", "")
            byte = int(match.group(2), 16)
            if not charset[byte]:
                charset[byte] = char
    charset[0x53], charset[0x54] = "PK", "MN"
    return charset


class Rom:
    def __init__(self, path, charset):
        self.data = path.read_bytes()
        self.charset = charset

    def u16(self, address):
        offset = address - ROM_BASE
        return int.from_bytes(self.data[offset:offset + 2], "little")

    def u32(self, address):
        offset = address - ROM_BASE
        return int.from_bytes(self.data[offset:offset + 4], "little")

    def string(self, address, max_length=64):
        offset = address - ROM_BASE
        out = []
        for byte in self.data[offset:offset + max_length]:
            if byte == EOS:
                break
            out.append(self.charset[byte])
        return "".join(out)

    def table(self, symbols, name, stride):
        address, size = symbols[name]
        return [address + i * stride for i in range(size // stride)]


def source_paths():
    """Maps graphics symbols (e.g. gObjectEventPic_Bulbasaur) to their source PNG/PAL files."""
    paths = {}
    pattern = re.compile(r"\b(\w+)\[\]\s*=\s*INC\w*\(\s*\"([^\"]+)\"")
    for header in list((ROOT / "src" / "data").rglob("*.h")) + list((ROOT / "src").glob("*.c")):
        for symbol, path in pattern.findall(header.read_text(encoding="utf-8", errors="ignore")):
            path = re.sub(r"\.4bpp(\.lz|\.smol|\.fastSmol)?$", ".png", path)
            path = re.sub(r"\.gbapal(\.lz)?$", ".pal", path)
            paths[symbol] = ROOT / path
    return paths


def read_jasc(path):
    lines = path.read_text().split()
    count = int(lines[2])
    return [tuple(int(v) for v in lines[3 + i * 3:6 + i * 3]) for i in range(count)]


def export_sprite(pic_path, pal_path, width, height, out_name, folder="overworld"):
    image = Image.open(pic_path)
    if image.mode != "P":
        fail(f"{pic_path} is not an indexed PNG")
    frame = image.crop((0, 0, width, height))
    if pal_path is not None:
        colors = read_jasc(pal_path)
        frame.putpalette([c for color in colors for c in color] + [0] * (768 - 3 * len(colors)))
    rgba = frame.convert("RGBA")
    pixels = rgba.load()
    indices = frame.load()
    for y in range(height):
        for x in range(width):
            if indices[x, y] == 0:
                pixels[x, y] = (0, 0, 0, 0)
    out_dir = WEB / "img" / folder
    out_dir.mkdir(parents=True, exist_ok=True)
    rgba.save(out_dir / out_name)
    return f"img/{folder}/{out_name}"


def sprite_name(path):
    rel = path.relative_to(ROOT / "graphics").with_suffix("")
    return "_".join(rel.parts) + ".png"


def export_graphics_info(rom, offsets, symbols_by_address, paths, info_address, pal_address, cache):
    """Exports the first frame of an ObjectEventGraphicsInfo. Returns the image URL or None."""
    images = rom.u32(info_address + offsets["gfxImages"])
    if images == 0:
        return None
    pic = symbols_by_address.get(rom.u32(images))
    pal = symbols_by_address.get(pal_address) if pal_address else None
    if pic not in paths or (pal is not None and pal not in paths):
        return None
    pic_path = paths[pic]
    pal_path = paths[pal] if pal else None
    key = (pic_path, pal_path)
    if key not in cache:
        width = rom.u16(info_address + offsets["gfxWidth"])
        height = rom.u16(info_address + offsets["gfxHeight"])
        cache[key] = export_sprite(pic_path, pal_path, width, height, sprite_name(pal_path or pic_path))
    return cache[key]


def export_front_pic(symbols_by_address, paths, pic_address, pal_address, cache):
    """Exports the first 64x64 frame of a species' front pic. Returns the image URL or None."""
    pic = symbols_by_address.get(pic_address)
    pal = symbols_by_address.get(pal_address)
    if pic not in paths or pal not in paths:
        return None
    key = (paths[pic], paths[pal])
    if key not in cache:
        name = sprite_name(paths[pic])[:-len(".png")] + "_" + paths[pal].stem + ".png"
        cache[key] = export_sprite(paths[pic], paths[pal], 64, 64, name, "front")
    return cache[key]


def mega_forms(rom, offsets, table):
    """Returns [[item, mega species], ...] for a species' Mega Stone form changes."""
    forms = []
    while table:
        method = rom.u16(table)
        if method == offsets["formChangeTerminator"]:
            break
        if method == offsets["formChangeMegaItem"]:
            read = rom.u16 if offsets["formChangeTargetSize"] == 2 else rom.u32
            forms.append([rom.u16(table + offsets["formChangeParam1"]), read(table + offsets["formChangeTarget"])])
        table += offsets["formChangeSize"]
    return forms


def bits_for(count):
    bits = 1
    while (1 << bits) < count:
        bits += 1
    return bits


def parse_tables():
    """Reads flag/var labels and background names from the non-TESTING block of src/data/qr_share.h."""
    text = (ROOT / "src" / "data" / "qr_share.h").read_text(encoding="utf-8")
    text = re.sub(r"//.*", "", text)
    text = re.sub(r"#if TESTING.*?#else", "", text, flags=re.S)

    def block(name):
        match = re.search(name + r"\[\]\s*=\s*\{(.*?)\};", text, re.S)
        if not match:
            fail(f"{name} not found in src/data/qr_share.h")
        return match.group(1)

    flags = re.findall(r"\b(FLAG_\w+)", block("sQrShareFlags"))
    variables = [{"name": n, "bits": int(b)} for n, b in re.findall(r"\{\s*(VAR_\w+)\s*,\s*(\d+)\s*\}", block("sQrShareVars"))]
    backgrounds = re.findall(r"COMPOUND_STRING\(\"(.*?)\"\)", block("sQrShareBackgrounds"))
    return flags, variables, backgrounds


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")


def main():
    for path in (ELF, ROM):
        if not path.exists():
            fail(f"{path.name} not found; run make first")

    offsets = compile_offsets()
    symbols, symbols_by_address = read_symbols()
    charset = load_charset()
    rom = Rom(ROM, charset)
    paths = source_paths()
    version = offsets["dataVersion"]
    out_dir = WEB / "data" / f"v{version}"

    flags, variables, background_names = parse_tables()
    write_json(out_dir / "layout.json", {
        "dataVersion": version,
        "bits": {
            "species": bits_for(offsets["numSpecies"]),
            "item": bits_for(offsets["itemsCount"]),
            "move": bits_for(offsets["movesCount"]),
            "ability": bits_for(offsets["abilitiesCount"]),
            "dexCount": bits_for(offsets["nationalDexCount"] + 1),
        },
        "flags": flags,
        "vars": variables,
    })

    has_overworld = offsets["speciesOverworld"] != 0xFFFFFFFF
    cache = {}
    species = []
    for address in rom.table(symbols, "gSpeciesInfo", offsets["speciesSize"]):
        entry = {"name": rom.string(address + offsets["speciesName"])}
        pic = rom.u32(address + offsets["speciesFrontPic"])
        entry["front"] = export_front_pic(symbols_by_address, paths, pic, rom.u32(address + offsets["speciesFrontPalette"]), cache)
        entry["shinyFront"] = export_front_pic(symbols_by_address, paths, pic, rom.u32(address + offsets["speciesFrontShinyPalette"]), cache)
        if offsets["showMegaForms"]:
            mega = mega_forms(rom, offsets, rom.u32(address + offsets["speciesFormChanges"]))
            if mega:
                entry["mega"] = mega
        if has_overworld:
            info = address + offsets["speciesOverworld"]
            normal = rom.u32(address + offsets["speciesPalette"])
            shiny = rom.u32(address + offsets["speciesShinyPalette"])
            entry["sprite"] = export_graphics_info(rom, offsets, symbols_by_address, paths, info, normal, cache)
            entry["shinySprite"] = export_graphics_info(rom, offsets, symbols_by_address, paths, info, shiny, cache)
        species.append(entry)
    # Species without overworld graphics or a front pic use SPECIES_NONE's (the "?" sprite).
    for entry in species:
        for key in ("sprite", "shinySprite"):
            if has_overworld and entry[key] is None:
                entry[key] = species[0]["sprite"]
        for key in ("front", "shinyFront"):
            if entry[key] is None:
                entry[key] = species[0]["front"]

    def names(table, size_key, name_key, inline=False):
        out = []
        for address in rom.table(symbols, table, offsets[size_key]):
            field = address + offsets[name_key]
            pointer = field if inline else rom.u32(field)
            out.append(rom.string(pointer) if pointer else "")
        return out

    player = {}
    for gender, symbol in (("male", "gObjectEventGraphicsInfo_BrendanNormal"), ("female", "gObjectEventGraphicsInfo_MayNormal")):
        player[gender] = export_graphics_info(rom, offsets, symbols_by_address, paths, symbols[symbol][0], None, cache)

    write_json(out_dir / "names.json", {
        "charset": charset,
        "species": species,
        "moves": names("gMovesInfo", "moveSize", "moveName"),
        "items": names("gItemsInfo", "itemSize", "itemName"),
        "abilities": names("gAbilitiesInfo", "abilitySize", "abilityName", inline=True),
        "natures": names("gNaturesInfo", "natureSize", "natureName"),
        "player": player,
    })

    backgrounds_path = out_dir / "backgrounds.json"
    existing = {b["id"]: b for b in json.loads(backgrounds_path.read_text())} if backgrounds_path.exists() else {}
    backgrounds = []
    for i, name in enumerate(background_names):
        entry = existing.get(i, {"id": i, "image": f"img/backgrounds/{i}.png",
                                 "player": DEFAULT_PLAYER, "party": DEFAULT_PARTY})
        entry["name"] = name
        backgrounds.append(entry)
    write_json(backgrounds_path, backgrounds)

    placeholder = WEB / "img" / "backgrounds" / "0.png"
    if not placeholder.exists():
        placeholder.parent.mkdir(parents=True, exist_ok=True)
        Image.new("RGB", (240, 160), "white").save(placeholder)

    print(f"Exported data version {version}: {len(species)} species, {len(cache)} sprites, "
          f"{len(flags)} flags, {len(variables)} vars, {len(backgrounds)} backgrounds -> {out_dir.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
