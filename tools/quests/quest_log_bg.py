#!/usr/bin/env python3
"""
Builds the quest log background (tileset + tilemaps) from full-screen mockups.

Input: 240x160 indexed PNGs in graphics/quest_log/screens/ that share one 16-colour palette:
  page_list.png          the quest list page
  page_detail.png        the quest detail page

Row containers (same folder, same palette), stamped behind each filled list row:
  page_row.png           240x16
  page_row_selected.png  240x16, the cursor's row                   (falls back to page_row)
Palette index 0 in a row image is transparent: each row is blended with the list page at every row position,
so the background shows through exactly where the row art is index 0.
A missing row image is fully transparent, so no container is drawn.

Output (overwrites the files the game loads):
  graphics/quest_log/bg_tiles.png     unique 8x8 tiles, with the shared palette
  graphics/quest_log/<screen>.bin     32x32 tilemaps (u16 per tile, flip bits, palette 0)
  graphics/quest_log/<row>.bin        row tilemaps, 30 tiles wide, one per row position;
                                      0xFFFF marks a cell that keeps the background tile

Aseprite files (.aseprite in the same folder) can replace the PNGs: a layer or group named after a screen or row
above (e.g. "page_list", "page_row_selected") is used instead of the PNG of that name. A group is flattened from
its visible children, so hidden guide layers inside it are skipped; the named layer's own visibility is ignored.
A named layer or group with nothing visible drawn in it counts as missing (the fallback applies; the PNG is not used).
Only frame 1 is read. A file with such a layer must be 240x160, indexed, with transparent index 0; other files are ignored. A row layer is drawn where the
first list row sits (y=16) and cropped to the row height. A name may appear in only one .aseprite file.

Draw only the background: the game prints text, icons, badges and the cursor on top.
Palette index 0 is the backdrop colour, not transparent.

Usage: python3 tools/quests/quest_log_bg.py [--screens DIR] [--out DIR]
Then run make.
"""

import argparse
import glob
import os
import struct
import sys
import zlib

from PIL import Image

SCREEN_W, SCREEN_H = 240, 160
MAP_W, MAP_H = 32, 32        # tilemap size in tiles; only the top-left 30x20 is visible
MAX_TILES = 768              # BG1 uses charblock 2; more tiles would overwrite the tilemaps
TILESET_COLS = 16            # bg_tiles.png width in tiles
PALETTE_SIZE = 16

# A fallback must come earlier in this list than the screen that uses it
SCREENS = ["page_list", "page_detail"]
FALLBACKS = {}

ROWS = {  # name: (height in pixels, list backgrounds)
    "page_row": (16, ["page_list"]),
    "page_row_selected": (16, ["page_list"]),
}
LIST_Y = 16        # first row's screen y; matches LIST_Y in src/quest_log.c
LIST_HEIGHT = 112  # list panel height; LIST_ROWS rows in src/quest_log.c
ROW_FALLBACKS = {
    "page_row_selected": "page_row",
}
SEE_THROUGH = 0xFFFF


def fail(msg):
    sys.exit(f"quest_log_bg.py: error: {msg}")


def load_screen(path, height=SCREEN_H):
    img = Image.open(path)
    if img.mode != "P":
        fail(f"{path}: must be an indexed PNG (Aseprite: Sprite > Color Mode > Indexed), not {img.mode}")
    if img.size != (SCREEN_W, height):
        fail(f"{path}: must be {SCREEN_W}x{height}, not {img.size[0]}x{img.size[1]}")
    pixels = list(img.getdata())
    bad = max(pixels)
    if bad >= PALETTE_SIZE:
        fail(f"{path}: uses palette index {bad}; only indices 0-{PALETTE_SIZE - 1} are allowed")
    palette = (img.getpalette() or [])[:PALETTE_SIZE * 3]
    palette += [0] * (PALETTE_SIZE * 3 - len(palette))
    return pixels, palette


def load_aseprite(path, names):
    """Returns {name: (pixels, palette)} for each layer or group in names, flattened to 240x160,
    or {name: None} when it has nothing visible drawn."""
    with open(path, "rb") as f:
        data = f.read()
    _, magic, _, width, height, depth = struct.unpack_from("<IHHHHH", data, 0)
    transparent = data[28]
    if magic != 0xA5E0:
        fail(f"{path}: not an Aseprite file")

    # Frame 1 only: layers (bottom to top), cels and palette
    layers = []  # (name, type, child level, visible)
    cels = {}    # layer index: (x, y, w, h, pixels)
    palette = [0] * (PALETTE_SIZE * 3)
    _, _, old_chunks, _, _, new_chunks = struct.unpack_from("<IHHH2sI", data, 128)
    pos = 128 + 16
    for _ in range(new_chunks or old_chunks):
        size, kind = struct.unpack_from("<IH", data, pos)
        body = pos + 6
        if kind == 0x2004:
            flags, layer_type, level = struct.unpack_from("<HHH", data, body)
            name_len = struct.unpack_from("<H", data, body + 16)[0]
            name = data[body + 18:body + 18 + name_len].decode("utf-8")
            layers.append((name, layer_type, level, bool(flags & 1)))
        elif kind == 0x2005:
            layer, x, y, _, cel_type = struct.unpack_from("<HhhBH", data, body)
            if cel_type in (0, 2):
                w, h = struct.unpack_from("<HH", data, body + 16)
                raw = data[body + 20:pos + size]
                cels[layer] = (x, y, w, h, raw if cel_type == 0 else zlib.decompress(raw))
        elif kind == 0x2019:
            first, last = struct.unpack_from("<II", data, body + 4)
            entry = body + 20
            for i in range(first, last + 1):
                entry_flags, r, g, b = struct.unpack_from("<HBBB", data, entry)
                entry += 6
                if entry_flags & 1:
                    entry += 2 + struct.unpack_from("<H", data, entry)[0]
                if i < PALETTE_SIZE:
                    palette[i * 3:i * 3 + 3] = [r, g, b]
        pos += size

    # Files without a screen or row layer (e.g. old working files) are ignored, so only check the ones used
    if not any(layer[0] in names for layer in layers):
        return {}
    if depth != 8:
        fail(f"{path}: must be indexed (Aseprite: Sprite > Color Mode > Indexed)")
    if (width, height) != (SCREEN_W, SCREEN_H):
        fail(f"{path}: must be {SCREEN_W}x{SCREEN_H}, not {width}x{height}")
    if transparent != 0:
        fail(f"{path}: transparent colour must be index 0 (Sprite > Properties), not {transparent}")

    found = {}
    for i, (name, layer_type, level, _) in enumerate(layers):
        if name not in names:
            continue
        if name in found:
            fail(f"{path}: more than one layer is named '{name}'")
        # A group's members are the layers after it with a deeper child level, skipping hidden ones
        members = [i]
        if layer_type == 1:
            members = []
            hidden_level = None
            for j in range(i + 1, len(layers)):
                _, child_type, child_level, child_visible = layers[j]
                if child_level <= level:
                    break
                if hidden_level is not None and child_level > hidden_level:
                    continue
                hidden_level = None if child_visible else child_level
                if child_visible and child_type != 1:
                    members.append(j)
        if not any(j in cels for j in members):
            found[name] = None  # nothing drawn: treat as missing so the fallback applies, never the PNG
            continue
        pixels = [0] * (SCREEN_W * SCREEN_H)
        for j in members:
            if layers[j][1] == 2:
                fail(f"{path}: '{layers[j][0]}' is a tilemap layer; use a normal layer")
            if j not in cels:
                continue
            x0, y0, w, h, cel = cels[j]
            for y in range(max(0, -y0), min(h, SCREEN_H - y0)):
                for x in range(max(0, -x0), min(w, SCREEN_W - x0)):
                    colour = cel[y * w + x]
                    if colour != transparent:
                        pixels[(y0 + y) * SCREEN_W + x0 + x] = colour
        bad = max(pixels)
        if bad >= PALETTE_SIZE:
            fail(f"{path}: '{name}' uses palette index {bad}; only indices 0-{PALETTE_SIZE - 1} are allowed")
        found[name] = (pixels, palette)
    return found


def cut_tiles(pixels, height=SCREEN_H):
    """Returns the image's 8x8 tiles in reading order, each a tuple of 64 indices."""
    tiles = []
    for ty in range(height // 8):
        for tx in range(SCREEN_W // 8):
            tiles.append(tuple(pixels[(ty * 8 + y) * SCREEN_W + tx * 8 + x] for y in range(8) for x in range(8)))
    return tiles


def hflip(tile):
    return tuple(tile[y * 8 + (7 - x)] for y in range(8) for x in range(8))


def vflip(tile):
    return tuple(tile[(7 - y) * 8 + x] for y in range(8) for x in range(8))


def main():
    parser = argparse.ArgumentParser(description="Build the quest log background from full-screen mockups.")
    parser.add_argument("--screens", default="graphics/quest_log/screens", help="folder with the 240x160 mockups")
    parser.add_argument("--out", default="graphics/quest_log", help="folder the game loads from")
    args = parser.parse_args()

    # Layers from .aseprite files take the place of PNGs with the same name
    layer_art = {}
    layer_paths = {}
    art_names = set(SCREENS) | set(ROWS)
    for path in sorted(glob.glob(os.path.join(args.screens, "*.aseprite"))):
        for name, art in load_aseprite(path, art_names).items():
            if name in layer_paths:
                fail(f"{path}: layer '{name}' is also in {layer_paths[name]}")
            layer_paths[name] = path
            layer_art[name] = None if art is None else art + (path,)

    def load(name, height=SCREEN_H):
        """Returns (pixels, palette, source path) for a screen or row, or None if neither file has it.
        A name in an .aseprite file never falls back to the PNG, even when its layer is empty."""
        if name in layer_art:
            if layer_art[name] is None:
                return None
            pixels, pal, path = layer_art[name]
            if height != SCREEN_H:
                pixels = pixels[LIST_Y * SCREEN_W:(LIST_Y + height) * SCREEN_W]
            return pixels, pal, f"{os.path.basename(path)}:{name}"
        path = os.path.join(args.screens, f"{name}.png")
        if not os.path.exists(path):
            return None
        return load_screen(path, height) + (f"{name}.png",)

    loaded = {}
    art_sources = {}
    palette = None
    palette_source = None
    for name in SCREENS:
        art = load(name)
        if art is None:
            continue
        pixels, pal, path = art
        art_sources[name] = path
        if palette is None:
            palette, palette_source = pal, path
        elif pal != palette:
            fail(f"{path}: palette differs from {palette_source}; all screens must share one 16-colour palette")
        loaded[name] = pixels

    rows = {}
    for name, (height, _) in ROWS.items():
        art = load(name, height)
        if art is None:
            continue
        pixels, pal, path = art
        art_sources[name] = path
        if palette is None:
            palette, palette_source = pal, path
        elif pal != palette:
            fail(f"{path}: palette differs from {palette_source}; all screens must share one 16-colour palette")
        rows[name] = pixels
    row_sources = {}
    for name, (height, _) in ROWS.items():
        if name in rows:
            row_sources[name] = name
        elif name in ROW_FALLBACKS and row_sources.get(ROW_FALLBACKS[name]) is not None:
            rows[name] = rows[ROW_FALLBACKS[name]]
            row_sources[name] = row_sources[ROW_FALLBACKS[name]]
        else:
            rows[name] = [0] * (SCREEN_W * height)
            row_sources[name] = None

    if not loaded:
        fail(f"no screens found in {args.screens} (expected {', '.join(s + '.png' for s in SCREENS)})")

    # Fill in missing screens. A fallback always comes earlier in SCREENS, so it is already resolved.
    sources = {}
    for name in SCREENS:
        if name in loaded:
            sources[name] = name
        elif name in FALLBACKS:
            loaded[name] = loaded[FALLBACKS[name]]
            sources[name] = sources[FALLBACKS[name]]
        else:
            loaded[name] = [0] * (SCREEN_W * SCREEN_H)
            sources[name] = "blank"

    # Deduplicate tiles across all screens, matching flipped copies.
    # Tile 0 is blank (index 0) so the unused part of each 32x32 tilemap shows the backdrop.
    blank = tuple([0] * 64)
    tiles = [blank]
    lookup = {blank: (0, False, False)}
    def entry_for(tile):
        if tile not in lookup:
            index = len(tiles)
            tiles.append(tile)
            h, v = hflip(tile), vflip(tile)
            hv = vflip(h)
            # A tile drawn flipped matches the stored tile flipped back
            for variant, flips in ((tile, (False, False)), (h, (True, False)), (v, (False, True)), (hv, (True, True))):
                lookup.setdefault(variant, (index, flips[0], flips[1]))
        index, fh, fv = lookup[tile]
        return index | (fh << 10) | (fv << 11)

    tilemaps = {name: [entry_for(tile) for tile in cut_tiles(loaded[name])] for name in SCREENS}
    # Blend each row with the list background at every row position of every tab.
    # A cell with no row pixels keeps the background tile (SEE_THROUGH).
    row_tilemaps = {}
    for name, (height, backgrounds) in ROWS.items():
        art = rows[name]
        entries = []
        for background in backgrounds:
            under = loaded[background]
            for slot in range(LIST_HEIGHT // height):
                y0 = LIST_Y + slot * height
                blended = [art[y * SCREEN_W + x] or under[(y0 + y) * SCREEN_W + x]
                           for y in range(height) for x in range(SCREEN_W)]
                for art_tile, tile in zip(cut_tiles(art, height), cut_tiles(blended, height)):
                    entries.append(SEE_THROUGH if art_tile == blank else entry_for(tile))
        row_tilemaps[name] = entries

    if len(tiles) > MAX_TILES:
        fail(f"{len(tiles)} unique tiles; the limit is {MAX_TILES}. Reuse more 8x8 tiles (flipped copies count as the same tile)")

    # Tileset PNG, padded to whole rows of blank tiles
    tileset_rows = (len(tiles) + TILESET_COLS - 1) // TILESET_COLS
    tileset = Image.new("P", (TILESET_COLS * 8, tileset_rows * 8), 0)
    tileset.putpalette(palette)
    for i, tile in enumerate(tiles):
        ox, oy = (i % TILESET_COLS) * 8, (i // TILESET_COLS) * 8
        for p, colour in enumerate(tile):
            tileset.putpixel((ox + p % 8, oy + p // 8), colour)
    tileset.save(os.path.join(args.out, "bg_tiles.png"))

    # 32x32 tilemaps: the visible 30x20 area, then tile 0 everywhere else
    for name in SCREENS:
        data = bytearray()
        entries = tilemaps[name]
        for y in range(MAP_H):
            for x in range(MAP_W):
                value = entries[y * (SCREEN_W // 8) + x] if x < SCREEN_W // 8 and y < SCREEN_H // 8 else 0
                data += struct.pack("<H", value)
        with open(os.path.join(args.out, f"{name}.bin"), "wb") as f:
            f.write(data)

    for name, entries in row_tilemaps.items():
        with open(os.path.join(args.out, f"{name}.bin"), "wb") as f:
            f.write(b"".join(struct.pack("<H", e) for e in entries))

    used = sorted(set(c for name in SCREENS for c in loaded[name]) | set(c for name in ROWS for c in rows[name]))
    print(f"quest_log_bg.py: {len(tiles)}/{MAX_TILES} tiles, {len(used)}/{PALETTE_SIZE} colours used (palette from {palette_source})")
    for name in SCREENS:
        print(f"  {name}.bin  <- {'blank' if sources[name] == 'blank' else art_sources[sources[name]]}")
    for name in ROWS:
        src = row_sources[name]
        print(f"  {name}.bin  <- {'none (no container)' if src is None else art_sources[src]}")
    print("Run make to rebuild.")


if __name__ == "__main__":
    main()
