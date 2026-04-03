# How to Add Animated Tileset Tiles

This guide walks through adding animated tiles to a secondary tileset — tiles that cycle through frames at runtime, like water, lava, or swaying plants. This covers the Porytiles workflow for creating the animation frames and the C code that drives them.

---

## Overview

Animated tileset tiles work by replacing tile graphics in VRAM each frame. The build system converts PNG frames into `.4bpp` tile data, which gets compiled into the ROM. At runtime, a callback cycles through these frames on a timer, writing them into the correct VRAM position.

The system involved:

1. **Animation frames** — PNG images in the tileset's `anim/` directory
2. **Porytiles** — compiles the tileset, assigning tile indices to animated regions
3. **C animation driver** — `src/tileset_anims.c` swaps frames at runtime
4. **Callback registration** — connects the driver to the tileset

---

## Step-by-step

Throughout this guide, `Swamp` is the tileset name and `Plants1` is the animation name. Replace with your own names.

### 1. Create the animation frames

Create a directory for your animation under the tileset's `anim/` folder:

```
data/tilesets/secondary/swamp/anim/plants1/
```

Place numbered PNG frames inside:

```
00.png
01.png
02.png
03.png
```

Each frame must:
- Be an indexed (paletted) PNG
- Have the same dimensions as each other
- Be a multiple of 8px in both width and height (each 8x8 block = 1 tile)
- Use the correct tileset palette

For example, a 32x8 image contains 4 tiles (4 wide × 1 tall).

### 2. Set up the animation in Porytiles

In your Porytiles `anim/` source directory, the animation frames define which tiles in the compiled tileset will be animated. Porytiles assigns these tiles specific indices in the output `tiles.png`.

After compiling with Porytiles, note where your animated tiles land in `tiles.png`. You'll need the tile index (position in the tileset, counting left-to-right, top-to-bottom in 8x8 tile units).

### 3. Determine the VRAM tile index

The secondary tileset begins at VRAM tile `NUM_TILES_IN_PRIMARY` (512 in Emerald). Your tile's VRAM index is:

```
vram_tile = NUM_TILES_IN_PRIMARY + position_in_tileset
```

Where `position_in_tileset` is the 0-based index of your tile in `tiles.png`:
- Tile at pixel (0,0) = index 0
- Tile at pixel (8,0) = index 1
- Tile at pixel (0,8) = index 16 (since the tileset is 128px = 16 tiles wide)

**To find the index programmatically**, compare your animation frame against the compiled tileset:

```python
from PIL import Image

tileset = Image.open("data/tilesets/secondary/swamp/tiles.png")
frame0 = Image.open("data/tilesets/secondary/swamp/anim/plants1/00.png")

tw = tileset.width // 8  # tiles per row (16 for 128px wide)
th = tileset.height // 8

for row in range(th):
    for col in range(tw - (frame0.width // 8) + 1):
        px, py = col * 8, row * 8
        region = tileset.crop((px, py, px + frame0.width, py + frame0.height))
        if list(region.getdata()) == list(frame0.getdata()):
            tile_index = row * tw + col
            print(f"Found at tile {tile_index} (VRAM: {512 + tile_index})")
```

### 4. Write the C animation driver in `src/tileset_anims.c`

#### a. INCBIN each frame

At the bottom of the file (or near other secondary tileset anims), include each frame's `.4bpp` data:

```c
const u16 gTilesetAnims_Swamp_Plants1_Frame0[] = INCBIN_U16("data/tilesets/secondary/swamp/anim/plants1/00.4bpp");
const u16 gTilesetAnims_Swamp_Plants1_Frame1[] = INCBIN_U16("data/tilesets/secondary/swamp/anim/plants1/01.4bpp");
const u16 gTilesetAnims_Swamp_Plants1_Frame2[] = INCBIN_U16("data/tilesets/secondary/swamp/anim/plants1/02.4bpp");
const u16 gTilesetAnims_Swamp_Plants1_Frame3[] = INCBIN_U16("data/tilesets/secondary/swamp/anim/plants1/03.4bpp");
```

The build system automatically converts each `.png` to `.4bpp` via the default pattern rule — no `spritesheet_rules.mk` entry is needed for simple tileset animations.

#### b. Create a frame table

```c
const u16 *const gTilesetAnims_Swamp_Plants1[] = {
    gTilesetAnims_Swamp_Plants1_Frame0,
    gTilesetAnims_Swamp_Plants1_Frame1,
    gTilesetAnims_Swamp_Plants1_Frame2,
    gTilesetAnims_Swamp_Plants1_Frame3,
};
```

#### c. Write the queue function

```c
static void QueueAnimTiles_Swamp_Plants1(u16 timer)
{
    u16 i = timer % ARRAY_COUNT(gTilesetAnims_Swamp_Plants1);
    AppendTilesetAnimToBuffer(gTilesetAnims_Swamp_Plants1[i], (u16 *)(BG_VRAM + TILE_OFFSET_4BPP(512)), 4 * TILE_SIZE_4BPP);
}
```

The three arguments to `AppendTilesetAnimToBuffer`:
1. **Source** — pointer to the current frame's tile data
2. **Destination** — `(u16 *)(BG_VRAM + TILE_OFFSET_4BPP(vram_tile))` where `vram_tile` is from step 3
3. **Size** — `num_tiles * TILE_SIZE_4BPP` where `num_tiles` is the number of 8x8 tiles per frame (e.g. a 32x8 frame = 4 tiles)

#### d. Add to the tileset's animation driver

If the tileset **already has a driver** (e.g. `TilesetAnim_Swamp`), add your queue call to it:

```c
static void TilesetAnim_Swamp(u16 timer)
{
    if (timer % 16 == 0)
    {
        QueueAnimTiles_Swamp_Puddle1(timer / 16);
        QueueAnimTiles_Swamp_Plants1(timer / 16);  // <-- add here
    }
}
```

If the tileset **has no driver yet**, create all three functions:

```c
static void TilesetAnim_Swamp(u16 timer)
{
    if (timer % 16 == 0)
        QueueAnimTiles_Swamp_Plants1(timer / 16);
}

void InitTilesetAnim_Swamp(void)
{
    sSecondaryTilesetAnimCounter = 0;
    sSecondaryTilesetAnimCounterMax = 256;
    sSecondaryTilesetAnimCallback = TilesetAnim_Swamp;
}
```

The `timer % 16` controls animation speed — lower values = faster animation. The timer is divided before passing to the queue function so each frame holds for multiple game ticks.

### 5. Register the callback (new tilesets only)

Skip this step if the tileset already has a registered callback.

**In `include/tileset_anims.h`**, declare the init function:

```c
void InitTilesetAnim_Swamp(void);
```

**In `src/data/tilesets/headers.h`**, set the callback on the tileset struct:

```c
    .callback = InitTilesetAnim_Swamp,
```

---

## Animation speed reference

The driver's `timer % N` value controls how many game frames each animation frame is held:

| `timer % N` | Effective speed |
|---|---|
| `timer % 8` | Fast (every 8 ticks) |
| `timer % 16` | Medium (every 16 ticks) — most common |
| `timer % 32` | Slow (every 32 ticks) |

Different animations within the same tileset can run at different speeds by using separate timer checks:

```c
static void TilesetAnim_Swamp(u16 timer)
{
    if (timer % 16 == 0)
        QueueAnimTiles_Swamp_Puddle1(timer / 16);
    if (timer % 32 == 0)
        QueueAnimTiles_Swamp_Plants1(timer / 32);  // slower
}
```

---

## Alternative: Step-triggered field effect (tall grass style)

Instead of continuously animating a tile, you can trigger a sprite animation when the player steps on it — like tall grass rustling. The tile itself stays static, and a field effect sprite plays on top when walked on. This approach also supports wild encounters.

This section uses `SwampPlants` as the example name. Replace with your own.

### 1. Create sprite sheets

You need two sprite sheets in `graphics/field_effects/pics/`:

- **Step sprite** — `swamp_plants.png`: horizontal strip of 16x16 frames (e.g. 64x16 for 4 frames)
- **Jump sprite** — `jump_swamp_plants.png`: horizontal strip of 16x8 frames (e.g. 64x8 for 4 frames)

These are indexed PNGs. Pixel index 0 = transparent. Only the plant shapes should use non-zero indices; the background must be index 0 so the player and ground show through.

### 2. Create a palette

Create `graphics/field_effects/palettes/swamp_plants.pal` in JASC-PAL format. The color at each index must match the pixel indices used in your sprite PNGs.

**Important**: The `.pal` file's index-to-color mapping must match the PNG's embedded palette exactly. If the PNG has index 1 = dark green but the `.pal` has index 1 = light green, colors will be wrong at runtime. Check the PNG's palette with:

```python
from PIL import Image
img = Image.open("graphics/field_effects/pics/swamp_plants.png")
pal = img.getpalette()
for i in range(16):
    print(f"[{i}] = {pal[i*3]} {pal[i*3+1]} {pal[i*3+2]}")
```

Index 0 in the `.pal` should be `115 189 238` (standard field effect background). Indices 7-15 can use standard field effect colors (copy from an existing palette like `swamp_tall_grass.pal`).

### 3. Add spritesheet build rules

In `spritesheet_rules.mk`, add rules near other grass entries:

```makefile
$(FLDEFFGFXDIR)/swamp_plants.4bpp: %.4bpp: %.png
	$(GFX) $< $@ -mwidth 2 -mheight 2

$(FLDEFFGFXDIR)/jump_swamp_plants.4bpp: %.4bpp: %.png
	$(GFX) $< $@ -mwidth 2 -mheight 1
```

`-mwidth` and `-mheight` are in 8px tile units: 2x2 = 16x16 for step sprite, 2x1 = 16x8 for jump sprite.

### 4. Define constants

**In `include/constants/metatile_behaviors.h`**, add before `NUM_METATILE_BEHAVIORS`:

```c
    MB_SWAMP_PLANTS,
    NUM_METATILE_BEHAVIORS
```

**In `include/constants/field_effects.h`**, add three groups of constants using the next available values:

```c
// Field effect IDs
#define FLDEFF_SWAMP_PLANTS              83
#define FLDEFF_JUMP_SWAMP_PLANTS         84

// Field effect object template indices
#define FLDEFFOBJ_SWAMP_PLANTS          47
#define FLDEFFOBJ_JUMP_SWAMP_PLANTS     48

// Palette tag
#define FLDEFF_PAL_TAG_SWAMP_PLANTS     0x1016
```

**In `include/event_object_movement.h`**, add ground effect flags using the next available bit positions:

```c
#define GROUND_EFFECT_FLAG_SWAMP_PLANTS_ON_SPAWN    (1 << 23)
#define GROUND_EFFECT_FLAG_SWAMP_PLANTS_ON_MOVE     (1 << 24)
#define GROUND_EFFECT_FLAG_LAND_IN_SWAMP_PLANTS     (1 << 25)
```

### 5. Add metatile behavior functions

**In `include/metatile_behavior.h`**, declare:

```c
bool8 MetatileBehavior_IsSwampPlants(u8 metatileBehavior);
```

**In `src/metatile_behavior.c`**, implement and update existing checks:

```c
bool8 MetatileBehavior_IsSwampPlants(u8 metatileBehavior)
{
    if (metatileBehavior == MB_SWAMP_PLANTS)
        return TRUE;
    else
        return FALSE;
}
```

Also update:
- `sTileBitAttributes[]` — add `[MB_SWAMP_PLANTS] = TILE_FLAG_UNUSED | TILE_FLAG_HAS_ENCOUNTERS`
- `MetatileBehavior_IsPokeGrass()` — add `|| metatileBehavior == MB_SWAMP_PLANTS`
- `MetatileBehavior_IsCuttableGrass()` — add `|| metatileBehavior == MB_SWAMP_PLANTS`

### 6. Register graphics data

**In `src/data/object_events/object_event_graphics.h`**, add:

```c
const u32 gFieldEffectObjectPic_SwampPlants[] = INCBIN_U32("graphics/field_effects/pics/swamp_plants.4bpp");
const u32 gFieldEffectObjectPic_JumpSwampPlants[] = INCBIN_U32("graphics/field_effects/pics/jump_swamp_plants.4bpp");
const u16 gFieldEffectObjectPalette_SwampPlants[] = INCBIN_U16("graphics/field_effects/palettes/swamp_plants.gbapal");
```

### 7. Create sprite templates

**In `src/data/field_effects/field_effect_objects.h`**, add the palette struct, animation table, pic tables, and sprite templates:

```c
const struct SpritePalette gSpritePalette_SwampPlants = {gFieldEffectObjectPalette_SwampPlants, FLDEFF_PAL_TAG_SWAMP_PLANTS};

// Custom animation (4 frames — adjust frame count to match your sprite sheet)
static const union AnimCmd sAnim_SwampPlants[] =
{
    ANIMCMD_FRAME(1, 10),
    ANIMCMD_FRAME(2, 10),
    ANIMCMD_FRAME(3, 10),
    ANIMCMD_FRAME(0, 10),
    ANIMCMD_END,
};

static const union AnimCmd *const sAnimTable_SwampPlants[] =
{
    sAnim_SwampPlants,
};

static const struct SpriteFrameImage sPicTable_SwampPlants[] = {
    overworld_frame(gFieldEffectObjectPic_SwampPlants, 2, 2, 0),
    overworld_frame(gFieldEffectObjectPic_SwampPlants, 2, 2, 1),
    overworld_frame(gFieldEffectObjectPic_SwampPlants, 2, 2, 2),
    overworld_frame(gFieldEffectObjectPic_SwampPlants, 2, 2, 3),
};

const struct SpriteTemplate gFieldEffectObjectTemplate_SwampPlants = {
    .tileTag = TAG_NONE,
    .paletteTag = FLDEFF_PAL_TAG_SWAMP_PLANTS,
    .oam = &gObjectEventBaseOam_16x16,
    .anims = sAnimTable_SwampPlants,
    .images = sPicTable_SwampPlants,
    .callback = UpdateSwampPlantsFieldEffect,
};

static const struct SpriteFrameImage sPicTable_JumpSwampPlants[] = {
    overworld_frame(gFieldEffectObjectPic_JumpSwampPlants, 2, 1, 0),
    overworld_frame(gFieldEffectObjectPic_JumpSwampPlants, 2, 1, 1),
    overworld_frame(gFieldEffectObjectPic_JumpSwampPlants, 2, 1, 2),
    overworld_frame(gFieldEffectObjectPic_JumpSwampPlants, 2, 1, 3),
};

const struct SpriteTemplate gFieldEffectObjectTemplate_JumpSwampPlants = {
    .tileTag = TAG_NONE,
    .paletteTag = FLDEFF_PAL_TAG_SWAMP_PLANTS,
    .oam = &gObjectEventBaseOam_16x8,
    .anims = sAnimTable_JumpTallGrass,   // reuse standard jump animation timing
    .images = sPicTable_JumpSwampPlants,
    .callback = UpdateJumpImpactEffect,
};
```

If you have 5 frames (like standard tall grass), use `sAnimTable_TallGrass` instead of creating a custom table.

**In `src/data/field_effects/field_effect_object_template_pointers.h`**, add:

```c
extern const struct SpriteTemplate gFieldEffectObjectTemplate_SwampPlants;
extern const struct SpriteTemplate gFieldEffectObjectTemplate_JumpSwampPlants;

// In the array:
[FLDEFFOBJ_SWAMP_PLANTS]      = &gFieldEffectObjectTemplate_SwampPlants,
[FLDEFFOBJ_JUMP_SWAMP_PLANTS] = &gFieldEffectObjectTemplate_JumpSwampPlants,
```

### 8. Add field effect scripts

**In `data/field_effect_scripts.s`**, add to the `gFieldEffectScriptPointers` table:

```asm
	.4byte gFieldEffectScript_SwampPlants               @ FLDEFF_SWAMP_PLANTS
	.4byte gFieldEffectScript_JumpSwampPlants           @ FLDEFF_JUMP_SWAMP_PLANTS
```

At the bottom of the file:

```asm
gFieldEffectScript_SwampPlants::
	field_eff_loadfadedpal_callnative gSpritePalette_SwampPlants, FldEff_SwampPlants
	field_eff_end

gFieldEffectScript_JumpSwampPlants::
	field_eff_loadfadedpal_callnative gSpritePalette_SwampPlants, FldEff_JumpSwampPlants
	field_eff_end
```

### 9. Implement field effect C functions

**In `include/field_effect_helpers.h`**, declare:

```c
void UpdateSwampPlantsFieldEffect(struct Sprite *sprite);
u8 FindSwampPlantsFieldEffectSpriteId(u8 localId, u8 mapNum, u8 mapGroup, s16 x, s16 y);
u32 FldEff_SwampPlants(void);
u32 FldEff_JumpSwampPlants(void);
```

**In `src/field_effect_helpers.c`**, add the functions. These follow the standard tall grass pattern — clone from `FldEff_SwampTallGrass` / `UpdateSwampTallGrassFieldEffect` / `FldEff_JumpSwampTallGrass` / `FindSwampTallGrassFieldEffectSpriteId` and replace:

- `FLDEFFOBJ_SWAMP_TALL_GRASS` → `FLDEFFOBJ_SWAMP_PLANTS`
- `FLDEFF_SWAMP_TALL_GRASS` → `FLDEFF_SWAMP_PLANTS`
- `FLDEFF_JUMP_SWAMP_TALL_GRASS` → `FLDEFF_JUMP_SWAMP_PLANTS`
- `FLDEFFOBJ_JUMP_SWAMP_TALL_GRASS` → `FLDEFFOBJ_JUMP_SWAMP_PLANTS`
- `MetatileBehavior_IsSwampTallGrass` → `MetatileBehavior_IsSwampPlants`
- `UpdateSwampTallGrassFieldEffect` → `UpdateSwampPlantsFieldEffect`
- `FindSwampTallGrassFieldEffectSpriteId` → `FindSwampPlantsFieldEffectSpriteId`

In `FldEff_SwampPlants`, if your sprite has 4 frames (not 5), change `SeekSpriteAnim(sprite, 4)` to `SeekSpriteAnim(sprite, 3)` (last frame index).

### 10. Add ground effect handlers

**In `src/event_object_movement.c`**, add:

**Forward declarations** (near existing swamp declarations):

```c
static void GetGroundEffectFlags_SwampPlantsOnSpawn(struct ObjectEvent *, u32 *);
static void GetGroundEffectFlags_SwampPlantsOnBeginStep(struct ObjectEvent *, u32 *);
```

**Flag-gathering functions:**

```c
static void GetGroundEffectFlags_SwampPlantsOnSpawn(struct ObjectEvent *objEvent, u32 *flags)
{
    if (MetatileBehavior_IsSwampPlants(objEvent->currentMetatileBehavior))
        *flags |= GROUND_EFFECT_FLAG_SWAMP_PLANTS_ON_SPAWN;
}

static void GetGroundEffectFlags_SwampPlantsOnBeginStep(struct ObjectEvent *objEvent, u32 *flags)
{
    if (MetatileBehavior_IsSwampPlants(objEvent->currentMetatileBehavior))
        *flags |= GROUND_EFFECT_FLAG_SWAMP_PLANTS_ON_MOVE;
}
```

**Call them from aggregator functions:**
- `GetAllGroundEffectFlags_OnSpawn()` — add `GetGroundEffectFlags_SwampPlantsOnSpawn(objEvent, flags);`
- `GetAllGroundEffectFlags_OnBeginStep()` — add `GetGroundEffectFlags_SwampPlantsOnBeginStep(objEvent, flags);`

**Jump landing arrays** — add to both `metatileFuncs[]` and `jumpLandingFlags[]` in `GetGroundEffectFlags_JumpLanding()`:

```c
// In metatileFuncs:
MetatileBehavior_IsSwampPlants,

// In jumpLandingFlags (same position):
GROUND_EFFECT_FLAG_LAND_IN_SWAMP_PLANTS,
```

**Ground effect handler functions** — clone from the swamp tall grass handlers, replacing constants:

```c
void GroundEffect_SpawnOnSwampPlants(struct ObjectEvent *objEvent, struct Sprite *sprite)
{
    gFieldEffectArguments[0] = objEvent->currentCoords.x;
    gFieldEffectArguments[1] = objEvent->currentCoords.y;
    gFieldEffectArguments[2] = objEvent->previousElevation;
    gFieldEffectArguments[3] = 2;
    gFieldEffectArguments[4] = objEvent->localId << 8 | objEvent->mapNum;
    gFieldEffectArguments[5] = objEvent->mapGroup;
    gFieldEffectArguments[6] = (u8)gSaveBlock1Ptr->location.mapNum << 8 | (u8)gSaveBlock1Ptr->location.mapGroup;
    gFieldEffectArguments[7] = TRUE;
    FieldEffectStart(FLDEFF_SWAMP_PLANTS);
}

void GroundEffect_StepOnSwampPlants(struct ObjectEvent *objEvent, struct Sprite *sprite)
{
    // Same as spawn but with gFieldEffectArguments[7] = FALSE
    ...
    gFieldEffectArguments[7] = FALSE;
    FieldEffectStart(FLDEFF_SWAMP_PLANTS);
}

void GroundEffect_JumpOnSwampPlants(struct ObjectEvent *objEvent, struct Sprite *sprite)
{
    u8 spriteId;

    gFieldEffectArguments[0] = objEvent->currentCoords.x;
    gFieldEffectArguments[1] = objEvent->currentCoords.y;
    gFieldEffectArguments[2] = objEvent->previousElevation;
    gFieldEffectArguments[3] = 2;
    FieldEffectStart(FLDEFF_JUMP_SWAMP_PLANTS);

    spriteId = FindSwampPlantsFieldEffectSpriteId(
        objEvent->localId,
        objEvent->mapNum,
        objEvent->mapGroup,
        objEvent->currentCoords.x,
        objEvent->currentCoords.y);

    if (spriteId == MAX_SPRITES)
        GroundEffect_SpawnOnSwampPlants(objEvent, sprite);
}
```

**Register in `sGroundEffectFuncs[]` array** (append at end, order must match bit positions):

```c
    GroundEffect_SpawnOnSwampPlants,     // GROUND_EFFECT_FLAG_SWAMP_PLANTS_ON_SPAWN
    GroundEffect_StepOnSwampPlants,      // GROUND_EFFECT_FLAG_SWAMP_PLANTS_ON_MOVE
    GroundEffect_JumpOnSwampPlants,      // GROUND_EFFECT_FLAG_LAND_IN_SWAMP_PLANTS
```

### 11. Set up metatile in Porymap

Assign your new `MB_SWAMP_PLANTS` behavior value to your metatile in Porymap.

---

## Step-triggered field effect: common pitfalls

### Palette index mismatch
The most common issue. The `.pal` file must map the same index-to-color as the PNG's embedded palette. If your sprite appears with wrong colors, compare the PNG's palette with the `.pal` file and ensure they match.

### Sprite appears fully opaque (covers player/ground)
Field effect sprites must use pixel index 0 for transparency. If you generate sprites from tileset frames, the background won't be transparent — tileset tiles fill every pixel. You need dedicated sprite art where only the animated shapes use non-zero indices.

### Wrong SeekSpriteAnim frame number
`SeekSpriteAnim(sprite, N)` skips to frame N (0-based) for the spawn effect. For 4 frames use 3, for 5 frames use 4. Wrong value = visual glitch on spawn.

### Ground effect array ordering
`sGroundEffectFuncs[]` entries must match `GROUND_EFFECT_FLAG_*` bit positions exactly. New entries go at the end with corresponding flag bits (bit 0 = entry 0, etc).

---

## Step-triggered field effect: files modified (summary)

| File | What to add |
|------|------------|
| `graphics/field_effects/pics/` | Step + jump sprite sheet PNGs |
| `graphics/field_effects/palettes/` | JASC-PAL palette file |
| `spritesheet_rules.mk` | Build rules for sprite PNGs |
| `include/constants/metatile_behaviors.h` | `MB_*` enum entry |
| `include/constants/field_effects.h` | `FLDEFF_*`, `FLDEFFOBJ_*`, `FLDEFF_PAL_TAG_*` constants |
| `include/event_object_movement.h` | `GROUND_EFFECT_FLAG_*` defines |
| `include/metatile_behavior.h` | Behavior check function declaration |
| `include/field_effect_helpers.h` | Field effect function declarations |
| `src/metatile_behavior.c` | Behavior function + updates to `IsPokeGrass`, `IsCuttableGrass`, `sTileBitAttributes` |
| `src/field_effect_helpers.c` | Creator, callback, jump, and finder functions |
| `src/event_object_movement.c` | Flag-gathering, ground effect handlers, jump landing arrays, `sGroundEffectFuncs` |
| `data/field_effect_scripts.s` | Script table entries + script definitions |
| `src/data/field_effects/field_effect_objects.h` | Palette struct, anim table, pic tables, sprite templates |
| `src/data/field_effects/field_effect_object_template_pointers.h` | Extern declarations + array entries |
| `src/data/object_events/object_event_graphics.h` | INCBIN declarations |

---

## Common pitfalls

### Wrong tile index
The most common issue. The VRAM tile index must exactly match where Porytiles placed your animated tiles in `tiles.png`. If animations appear on the wrong tiles or look garbled, verify the index using the Python script in step 3.

### Animations appearing swapped
If animation A shows on tile B and vice versa, the tile indices are swapped. Double-check each animation's position in `tiles.png` independently.

### Frame dimensions don't match tile count
The size parameter (`N * TILE_SIZE_4BPP`) must match the number of 8x8 tiles in each frame. A 32x8 frame = 4 tiles, a 16x16 frame = 4 tiles, a 32x16 frame = 8 tiles.

### Palette mismatch
Animation frames must use the same palette as the tileset. If colors look wrong but the tiles are in the right position, check that the PNGs use the correct palette index.

---

## Files modified (summary)

| File | What to add |
|------|------------|
| `data/tilesets/secondary/<name>/anim/<anim>/` | PNG frames (00.png, 01.png, ...) |
| `src/tileset_anims.c` | INCBIN frames, frame table, queue function, driver function |
| `include/tileset_anims.h` | Init function declaration (new tilesets only) |
| `src/data/tilesets/headers.h` | `.callback` on tileset struct (new tilesets only) |
