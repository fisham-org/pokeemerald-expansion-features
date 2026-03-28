# Porytiles

Porytiles is a tileset compiler/decompiler for pokeemerald-expansion. It converts RGBA PNG tile art into GBA-ready tilesets, handling palette optimization, tile deduplication, and flip detection automatically.

- **Project**: https://github.com/grunt-lucas/porytiles
- **Wiki**: https://github.com/grunt-lucas/porytiles/wiki
- **Installed to**: `~/.local/bin/porytiles`

## Input Format (Porytiles-format tileset)

```
my-tileset/
├── bottom.png          # Bottom layer
├── middle.png          # Middle layer
├── top.png             # Top layer
├── attributes.csv      # Metatile behaviors (optional)
└── anim/               # Animations (optional)
    └── anim_name/
        ├── key.png     # Reference frame (typically duplicate of 00.png)
        ├── 00.png      # Frame 0
        ├── 01.png      # Frame 1
        └── 02.png      # Frame 2
```

### Layer PNG Requirements
- **Width**: 128 pixels (8 metatiles wide)
- **Height**: multiples of 16 pixels
- **Transparency**: magenta `RGB(255, 0, 255)` — not alpha transparency

### attributes.csv
```csv
id,behavior
6,MB_TALL_GRASS
11,MB_NORMAL
```
Unspecified metatiles default to `MB_NORMAL` (0). Behaviors can be labels from `include/constants/metatile_behaviors.h` or numeric values.

## Commands

### Compile Primary Tileset
```bash
porytiles compile-primary -Wall \
  -o <output-path> \
  <source-dir> \
  <metatile_behaviors.h>
```

### Compile Secondary Tileset
Secondary tilesets require the paired primary source as an extra argument:
```bash
porytiles compile-secondary -Wall \
  -o <output-path> \
  <source-dir> \
  <primary-source-dir> \
  <metatile_behaviors.h>
```

Always recompile secondary tilesets after changing their paired primary.

### Decompile Primary Tileset
```bash
porytiles decompile-primary \
  -o <output-dir> \
  <tileset-path> \
  <metatile_behaviors.h>
```

### Decompile Secondary Tileset
```bash
porytiles decompile-secondary \
  -o <output-dir> \
  <secondary-tileset-path> \
  <primary-tileset-path> \
  <metatile_behaviors.h>
```

### Useful Flags
- `-Wall` — Enable all warnings
- `-dual-layer` — For dual-layer fieldmap projects (omit for triple-layer)
- `--disable-attribute-generation` — Skip attributes.csv processing, preserve Porymap edits
- `-preserve-transparency` — Don't normalize transparent color to magenta
- `-default-behavior <label>` — Set fallback behavior for unspecified metatiles

## Workflow

1. Create tileset in Porymap (File > New Tileset)
2. Create/assign a map for the tileset
3. Design layer PNGs in image editor (e.g. Aseprite)
4. Run `porytiles compile-primary` or `compile-secondary`
5. Reload project in Porymap (File > Reload Project)
6. Iterate: edit PNGs > recompile > reload

## Animations

Porytiles compiles animation frames into `.4bpp` files, but the C driver code must be written manually.

### Required C Code

**1. Header declaration** (`include/tileset_anims.h`):
```c
void InitTilesetAnim_MyTileset(void);
```

**2. Tileset callback** (`src/data/tilesets/headers.h`):
```c
const struct Tileset gTileset_MyTileset = {
    // ...
    .callback = InitTilesetAnim_MyTileset,
};
```

**3. Animation driver** (`src/tileset_anims.c`):
```c
// Frame data
const u16 gTilesetAnims_MyTileset_Anim_Frame0[] =
    INCBIN_U16("data/tilesets/primary/my_tileset/anim/anim_name/00.4bpp");
const u16 gTilesetAnims_MyTileset_Anim_Frame1[] =
    INCBIN_U16("data/tilesets/primary/my_tileset/anim/anim_name/01.4bpp");

// Frame table
const u16 *const gTilesetAnims_MyTileset_Anim[] = {
    gTilesetAnims_MyTileset_Anim_Frame0,
    gTilesetAnims_MyTileset_Anim_Frame1,
};

// Buffer copy (TILE_OFFSET_4BPP = starting tile index, size = tile count * TILE_SIZE_4BPP)
static void QueueAnimTiles_MyTileset_Anim(u16 timer) {
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_MyTileset_Anim);
    AppendTilesetAnimToBuffer(
        gTilesetAnims_MyTileset_Anim[i],
        (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(1)),
        4 * TILE_SIZE_4BPP
    );
}

// Driver (timer % 16 = update every 16 frames)
static void TilesetAnim_MyTileset(u16 timer) {
    if (timer % 16 == 0)
        QueueAnimTiles_MyTileset_Anim(timer / 16);
}

// Init
void InitTilesetAnim_MyTileset(void) {
    sPrimaryTilesetAnimCounter = 0;
    sPrimaryTilesetAnimCounterMax = 256;
    sPrimaryTilesetAnimCallback = TilesetAnim_MyTileset;
}
```

### Key Frame Rules
- `key.png` tiles must be unique across all animation key frames in the tileset
- Each subtile in key.png must contain all colors that appear in that subtile across all frames

## How It Works (Algorithm)

1. **Normalization** — Tiles are flipped in all orientations; the lexicographically smallest form is kept (deduplication across flips)
2. **Color Indexing** — All unique BGR15 colors are assigned global indices
3. **ColorSet Matching** — Each tile gets a bitset of which colors it uses
4. **Palette Assignment** — Recursive backtracking assigns color sets to palettes (max 16 colors each), prioritizing overlap
5. **Hardware Palette Generation** — Index 0 reserved for transparency, remaining colors fill sequentially
6. **Tile Assignment** — Final indexed GBATiles are created with palette/flip metadata

## Quick Start: Creating a New Tileset

### 1. Set Up Aseprite File

Create a new `.aseprite` file with exactly **three layers** named `bottom`, `middle`, `top`.

- **Canvas width**: 128px, **height**: multiple of 16px
- Each 16x16 block = one metatile (read left-to-right, top-to-bottom)
- Use transparency for empty areas (the build process converts it to magenta)

### 2. Save to Raw Tilesets Folder

Save as `tilesetase.aseprite` in the correct subfolder:

```
C:\Dev\pkm\graphics\tilesets\
├── primary\
│   └── <tileset_name>\
│       ├── tilesetase.aseprite
│       └── attributes.csv      (optional)
└── secondary\
    └── <tileset_name>\
        ├── tilesetase.aseprite
        └── attributes.csv      (optional)
```

### 3. Create Tileset in Porymap

1. `File > New Tileset` — name it (e.g. `prairie`)
2. Create or assign a map that uses the tileset
3. Save

### 4. Compile

Run the helper script:
```bash
bash /home/jd/decomps/tools/helpful-porytiles/helpful-porytiles.sh
```

- Choose **option 1** for primary or **option 2** for secondary
- Enter the tileset folder name (e.g. `prairie`) when prompted

Or compile manually:
```bash
# Export layers from Aseprite
"/mnt/c/Program Files (x86)/Steam/steamapps/common/Aseprite/aseprite.exe" -b \
  "C:/Dev/pkm/graphics/tilesets/secondary/prairie/tilesetase.aseprite" \
  --save-as "C:/Dev/pkm/graphics/tilesets/secondary/prairie/{layer}.png"

# Normalize transparency to magenta
python3 /home/jd/decomps/tools/helpful-porytiles/normalize.py <layer.png> <layer.png>

# Compile
porytiles compile-secondary -Wall \
  -o data/tilesets/secondary/prairie \
  /mnt/c/Dev/pkm/graphics/tilesets/secondary/prairie \
  /mnt/c/Dev/pkm/graphics/tilesets/primary/prairie \
  include/constants/metatile_behaviors.h
```

### 5. Reload Porymap

`File > Reload Project` to see the compiled tileset.

### Common Pitfalls

- **Don't reuse vanilla primary tiles in secondary tilesets**: Porytiles reshuffles palettes when it compiles. If your secondary references tiles from a vanilla primary (e.g. `general`), the palette assignments won't match what the game actually loads, causing corrupted/garbled colors. Use entirely custom tiles in your secondary instead. If you must share tiles with a vanilla primary, you need to recompile the primary too and use that recompiled version in your map — but this breaks connecting maps, door animations, and tileset animations. Porytiles 2 (in development) aims to resolve this limitation.
- **Color precision loss**: GBA uses 15-bit color (5 bits per channel). Colors that look different in Aseprite may collapse to the same GBA color. Avoid very similar colors.
- **Recompile secondaries after changing a primary**: Use option 8 in the helper script for bulk recompilation.
- **Missing attributes.csv**: Not an error — metatiles default to `MB_NORMAL`. Add the file when you're ready to assign behaviors like `MB_TALL_GRASS`.

## Helper Tools

Located at `/home/jd/decomps/tools/helpful-porytiles/`.

### normalize.py

Converts alpha transparency to magenta `#FF00FF` (required by porytiles). Useful when exporting from Aseprite which uses alpha transparency.

```bash
python3 normalize.py <input.png> <output.png>
```

Requires `Pillow` (`pip3 install Pillow`). Can overwrite in-place (same input/output path).

### helpful-porytiles.sh

Interactive menu-driven wrapper that automates the full porytiles workflow:

1. Auto-exports Aseprite layers (bottom, middle, top) from `.aseprite` files
2. Runs `normalize.py` on each layer to convert transparency
3. Runs the appropriate `porytiles compile-*` or `decompile-*` command
4. Supports bulk recompilation of all secondary tilesets
5. Caches paired primary tileset name in `primarysrc.txt` for secondary tilesets

**Run with:**
```bash
bash /home/jd/decomps/tools/helpful-porytiles/helpful-porytiles.sh
```

**Menu options:**
| # | Action |
|---|--------|
| 1 | Compile primary |
| 2 | Compile secondary |
| 3 | Decompile primary |
| 4 | Decompile secondary |
| 5 | Repeat last command |
| 6 | Toggle double confirmation |
| 7 | Edit paths |
| 8 | Bulk recompile all secondary tilesets |
| 9 | Toggle `--disable-attribute-generation` |
| 0 | Exit |

#### Configured Paths

These are set at the top of `helpful-porytiles.sh`:

| Variable | Path |
|----------|------|
| `dir_raw_tilesets` | `/mnt/c/Dev/pkm/graphics/tilesets/` |
| `dir_aseprite_folder` | `/mnt/c/Program Files (x86)/Steam/steamapps/common/` |
| `dir_aseprite_raw_tilesets` | `C:/Dev/pkm/graphics/tilesets/` (Windows path for Aseprite CLI) |
| `dir_compiled_primary` | `/home/jd/decomps/pokeemerald-expansion-features/data/tilesets/primary/` |
| `dir_compiled_secondary` | `/home/jd/decomps/pokeemerald-expansion-features/data/tilesets/secondary/` |
| `metatile_behaviors` | `/home/jd/decomps/pokeemerald-expansion-features/include/constants/metatile_behaviors.h` |
| `normalize_py` | `/home/jd/decomps/tools/helpful-porytiles/normalize.py` |

#### Secondary Tileset Pairing

When compiling a secondary tileset, the script saves the paired primary folder name to `primarysrc.txt` inside the secondary's raw folder. On subsequent compiles you can enter `-1` for the primary prompt to reuse the cached value. This also powers option 8 (bulk recompile).
