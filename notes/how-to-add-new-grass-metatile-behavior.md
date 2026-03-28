# How to Add a New Grass Metatile Behavior

This guide walks through adding a new grass type (tall or long) with its own field effect animation, palette, and metatile behavior. It covers both tall grass and long grass variants, noting the differences between them.

---

## Overview

Adding a new grass type requires changes across several systems:

1. **Metatile behavior** - a new constant so the game recognizes your grass tiles
2. **Metatile behavior functions** - C functions to check for the new behavior
3. **Graphics** - sprite sheets and palette for the rustling animation
4. **Field effect registration** - connecting the sprites into the field effect system
5. **Ground effect flags and functions** - triggering the animation when the player walks/jumps
6. **Sprite callback** - managing the animation lifecycle

The standard implementations (`MB_TALL_GRASS` and `MB_LONG_GRASS`) are the templates. Each new grass type follows the same pattern with its own constants, graphics, and palette.

---

## Tall Grass vs Long Grass: Key Differences

Before starting, understand which base type your new grass should derive from:

| Property | Tall Grass | Long Grass |
|---|---|---|
| Step-on sprite size | 16x16 (2x2 tiles) | 16x16 (2x2 tiles) |
| Step-on animation frames | 5 | 4 |
| Jump sprite size | 16x8 (2x1 tiles) | 16x16 (2x2 tiles) |
| Jump animation frames | 4 | 7 |
| SeekSpriteAnim skip frame | 4 | 6 |
| Slows running | No | Yes |
| Subpriority adjustment | Yes (frame 0 gets +4) | No |
| Jump-on spawns grass under player | Yes (via `Find*FieldEffectSpriteId`) | No |
| Hides lower half of sprites | No | Yes (via OAM table) |

Choose tall grass if your variant should behave like short, dense grass. Choose long grass if it should behave like waist-high grass that partially hides the player.

---

## Step-by-step

Throughout this guide, `Prairie` is used as the example prefix. Replace it with your variant name.

### 1. Define the metatile behavior constant

In `include/constants/metatile_behaviors.h`, add a new enum entry before `NUM_METATILE_BEHAVIORS`:

```c
    MB_PRAIRIE_TALL_GRASS,  // <-- new (or MB_PRAIRIE_LONG_GRASS for long grass)
    NUM_METATILE_BEHAVIORS
};
```

Note the numeric value assigned (the enum auto-increments). You'll set this value in Porymap on your grass metatile's behavior attribute.

### 2. Add metatile behavior check functions

**In `include/metatile_behavior.h`**, declare:

```c
bool8 MetatileBehavior_IsPrairieTallGrass(u8 metatileBehavior);
```

Optionally declare a group check if you have multiple variants of the same base type:

```c
bool8 MetatileBehavior_IsAnyTallGrass(u8 metatileBehavior);
```

**In `src/metatile_behavior.c`**, implement:

```c
bool8 MetatileBehavior_IsPrairieTallGrass(u8 metatileBehavior)
{
    if (metatileBehavior == MB_PRAIRIE_TALL_GRASS)
        return TRUE;
    else
        return FALSE;
}

// Optional group check
bool8 MetatileBehavior_IsAnyTallGrass(u8 metatileBehavior)
{
    if (metatileBehavior == MB_TALL_GRASS || metatileBehavior == MB_PRAIRIE_TALL_GRASS || metatileBehavior == MB_CYCLING_ROAD_PULL_DOWN_GRASS)
        return TRUE;
    else
        return FALSE;
}
```

**Update existing functions** to include your new behavior:

| Function | Purpose | Both types? |
|---|---|---|
| `sTileBitAttributes[]` | Add `[MB_PRAIRIE_TALL_GRASS] = TILE_FLAG_UNUSED \| TILE_FLAG_HAS_ENCOUNTERS` | Yes |
| `MetatileBehavior_IsPokeGrass()` | Wild encounter triggering | Yes |
| `MetatileBehavior_IsCuttableGrass()` | Allows Cut to work on the tile | Yes |

**Long grass only** - also update these:

| Function | Purpose |
|---|---|
| `MetatileBehavior_IsRunningDisallowed()` | Slows movement in long grass |
| `MetatileBehavior_IsLongGrass_Duplicate()` | Used by Cut field effect |
| `SetObjectEventSpriteOamTableForLongGrass()` in `event_object_movement.c` | Partially hides sprites in grass (change `IsLongGrass` calls to `IsAnyLongGrass`) |

### 3. Create graphics assets

You need files in `graphics/field_effects/`:

#### Option A: New sprite sheets (custom shapes or hand-drawn)

**Sprite sheets** (in `pics/`):

For **tall grass**:
- `prairie_tall_grass.png` - 80x16 indexed PNG (5 frames of 16x16)
- No jump sprite needed (the standard jump tall grass is 16x8, 4 frames)

For **long grass**:
- `prairie_long_grass.png` - 64x16 indexed PNG (4 frames of 16x16)
- `jump_prairie_long_grass.png` - 112x16 indexed PNG (7 frames of 16x16)

**Palette** (in `palettes/`):
- `prairie_long_grass.pal` - JASC-PAL format, 16 colors

The palette has a specific structure. Indices 1-4 are the grass blade colors (light to dark), index 5 is the outline/darkest shade, and indices 0 and 6-15 are shared field effect colors that should stay the same as the standard palette. Only change indices 1-4 to match your tileset.

Example palette structure:
```
JASC-PAL
0100
16
115 189 238    <- index 0: background (keep standard)
224 216 104    <- index 1: lightest grass (match your tileset)
136 152 40     <- index 2: medium grass (match your tileset)
104 120 24     <- index 3: dark grass (match your tileset)
72 80 16       <- index 4: darkest grass (match your tileset)
49 65 0        <- index 5: outline (keep standard)
98 172 238     <- indices 6-15: shared field effect colors (keep standard)
...
```

**Important**: The PNG files must have the same palette embedded as the .pal file. The build system extracts tile data from the PNG and the palette from the .pal file separately - if they don't match, the pixel indices will map to wrong colors.

#### Option B: Reuse existing sprites with a palette swap

If your grass variant is purely a recolor (same animation shapes), you can skip creating new PNGs and reuse the existing sprite data (e.g. `gFieldEffectObjectPic_TallGrass`) in your sprite templates. You still need a `.pal` palette file if your color differs from the standard palette. See step 8 for how to reference existing pic data in the sprite templates.

This approach saves ROM space and is simpler, but means you cannot customize the animation shapes independently.

#### Spritesheet build rules (Option A only)

In `spritesheet_rules.mk`, add build rules near the existing grass entries:

```makefile
$(FLDEFFGFXDIR)/prairie_tall_grass.4bpp: %.4bpp: %.png
	$(GFX) $< $@ -mwidth 2 -mheight 2
```

For long grass, you also need the jump sprite rule:
```makefile
$(FLDEFFGFXDIR)/prairie_long_grass.4bpp: %.4bpp: %.png
	$(GFX) $< $@ -mwidth 2 -mheight 2

$(FLDEFFGFXDIR)/jump_prairie_long_grass.4bpp: %.4bpp: %.png
	$(GFX) $< $@ -mwidth 2 -mheight 2
```

### 4. Define field effect constants

In `include/constants/field_effects.h`, add three groups of constants. Use the next available values after existing entries:

```c
// Field effect IDs
#define FLDEFF_PRAIRIE_TALL_GRASS        83
#define FLDEFF_JUMP_PRAIRIE_TALL_GRASS   84

// Field effect object template indices
#define FLDEFFOBJ_PRAIRIE_TALL_GRASS     47
#define FLDEFFOBJ_JUMP_PRAIRIE_TALL_GRASS 48
```

If creating a new palette (not reusing an existing one), add a palette tag:

```c
#define FLDEFF_PAL_TAG_PRAIRIE_TALL_GRASS 0x1016
```

**Important - tall grass vs long grass palettes**: Even though both use the same green indices (1-4), the tall grass and long grass sprites were designed for different base palettes that differ at other indices (0, 13). Sharing a single palette between tall and long grass variants can cause visual artifacts. Always create a separate palette per grass type based on its corresponding standard palette (`general_1` for tall grass, `general_1` for long grass).

### 5. Register graphics data

In `src/data/object_events/object_event_graphics.h`, add INCBIN declarations.

**Option A** (new sprite sheets):
```c
const u32 gFieldEffectObjectPic_PrairieTallGrass[] = INCBIN_U32("graphics/field_effects/pics/prairie_tall_grass.4bpp");
const u16 gFieldEffectObjectPalette_PrairieTallGrass[] = INCBIN_U16("graphics/field_effects/palettes/prairie_tall_grass.gbapal");
```

**Option B** (palette swap only - reusing existing sprites): You still need the palette INCBIN even though you skip the sprite INCBIN:
```c
const u16 gFieldEffectObjectPalette_PrairieTallGrass[] = INCBIN_U16("graphics/field_effects/palettes/prairie_tall_grass.gbapal");
```

### 6. Register field effect scripts

In `data/field_effect_scripts.s`, add entries to the `gFieldEffectScriptPointers` table:

```asm
    .4byte gFieldEffectScript_PrairieTallGrass       @ FLDEFF_PRAIRIE_TALL_GRASS
    .4byte gFieldEffectScript_JumpPrairieTallGrass   @ FLDEFF_JUMP_PRAIRIE_TALL_GRASS
```

At the bottom of the file, define the scripts:

```asm
gFieldEffectScript_PrairieTallGrass::
    field_eff_loadfadedpal_callnative gSpritePalette_PrairieTallGrass, FldEff_PrairieTallGrass
    field_eff_end

gFieldEffectScript_JumpPrairieTallGrass::
    field_eff_loadfadedpal_callnative gSpritePalette_PrairieTallGrass, FldEff_JumpPrairieTallGrass
    field_eff_end
```

The first argument to `field_eff_loadfadedpal_callnative` is the `SpritePalette` struct. If you're reusing an existing palette, reference that struct. If you created a new palette, reference your new struct (defined in step 7).

### 7. Create sprite templates

**In `src/data/field_effects/field_effect_objects.h`:**

If using a new palette, add the palette struct near the top alongside other palette declarations:

```c
const struct SpritePalette gSpritePalette_PrairieTallGrass = {gFieldEffectObjectPalette_PrairieTallGrass, FLDEFF_PAL_TAG_PRAIRIE_TALL_GRASS};
```

Add pic tables and sprite templates. The examples below show both tall grass and long grass patterns.

#### Tall grass variant

```c
// Pic table - use your own pic data (Option A) or reuse existing (Option B)
static const struct SpriteFrameImage sPicTable_PrairieTallGrass[] = {
    overworld_frame(gFieldEffectObjectPic_TallGrass, 2, 2, 0),  // Option B: reuse
    overworld_frame(gFieldEffectObjectPic_TallGrass, 2, 2, 1),
    overworld_frame(gFieldEffectObjectPic_TallGrass, 2, 2, 2),
    overworld_frame(gFieldEffectObjectPic_TallGrass, 2, 2, 3),
    overworld_frame(gFieldEffectObjectPic_TallGrass, 2, 2, 4),
};

const struct SpriteTemplate gFieldEffectObjectTemplate_PrairieTallGrass = {
    .tileTag = TAG_NONE,
    .paletteTag = FLDEFF_PAL_TAG_PRAIRIE_TALL_GRASS,
    .oam = &gObjectEventBaseOam_16x16,
    .anims = sAnimTable_TallGrass,   // reuse tall grass animation timing
    .images = sPicTable_PrairieTallGrass,
    .callback = UpdatePrairieTallGrassFieldEffect,
};

// Jump sprite - 16x8, 4 frames (matches standard jump tall grass)
static const struct SpriteFrameImage sPicTable_JumpPrairieTallGrass[] = {
    overworld_frame(gFieldEffectObjectPic_JumpTallGrass, 2, 1, 0),
    overworld_frame(gFieldEffectObjectPic_JumpTallGrass, 2, 1, 1),
    overworld_frame(gFieldEffectObjectPic_JumpTallGrass, 2, 1, 2),
    overworld_frame(gFieldEffectObjectPic_JumpTallGrass, 2, 1, 3),
};

const struct SpriteTemplate gFieldEffectObjectTemplate_JumpPrairieTallGrass = {
    .tileTag = TAG_NONE,
    .paletteTag = FLDEFF_PAL_TAG_PRAIRIE_TALL_GRASS,
    .oam = &gObjectEventBaseOam_16x8,   // NOTE: 16x8 for tall grass jump
    .anims = sAnimTable_JumpTallGrass,
    .images = sPicTable_JumpPrairieTallGrass,
    .callback = UpdateJumpImpactEffect,
};
```

#### Long grass variant

```c
static const struct SpriteFrameImage sPicTable_PrairieLongGrass[] = {
    overworld_frame(gFieldEffectObjectPic_PrairieLongGrass, 2, 2, 0),
    overworld_frame(gFieldEffectObjectPic_PrairieLongGrass, 2, 2, 1),
    overworld_frame(gFieldEffectObjectPic_PrairieLongGrass, 2, 2, 2),
    overworld_frame(gFieldEffectObjectPic_PrairieLongGrass, 2, 2, 3),
};

const struct SpriteTemplate gFieldEffectObjectTemplate_PrairieLongGrass = {
    .tileTag = TAG_NONE,
    .paletteTag = FLDEFF_PAL_TAG_PRAIRIE_LONG_GRASS,
    .oam = &gObjectEventBaseOam_16x16,
    .anims = sAnimTable_LongGrass,   // reuse long grass animation timing
    .images = sPicTable_PrairieLongGrass,
    .callback = UpdatePrairieLongGrassFieldEffect,
};

// Jump sprite - 16x16, 7 frames (matches standard jump long grass)
static const struct SpriteFrameImage sPicTable_JumpPrairieLongGrass[] = {
    overworld_frame(gFieldEffectObjectPic_JumpPrairieLongGrass, 2, 2, 0),
    overworld_frame(gFieldEffectObjectPic_JumpPrairieLongGrass, 2, 2, 1),
    overworld_frame(gFieldEffectObjectPic_JumpPrairieLongGrass, 2, 2, 2),
    overworld_frame(gFieldEffectObjectPic_JumpPrairieLongGrass, 2, 2, 3),
    overworld_frame(gFieldEffectObjectPic_JumpPrairieLongGrass, 2, 2, 4),
    overworld_frame(gFieldEffectObjectPic_JumpPrairieLongGrass, 2, 2, 6),
};

const struct SpriteTemplate gFieldEffectObjectTemplate_JumpPrairieLongGrass = {
    .tileTag = TAG_NONE,
    .paletteTag = FLDEFF_PAL_TAG_PRAIRIE_LONG_GRASS,
    .oam = &gObjectEventBaseOam_16x16,  // NOTE: 16x16 for long grass jump
    .anims = sAnimTable_JumpLongGrass,
    .images = sPicTable_JumpPrairieLongGrass,
    .callback = UpdateJumpImpactEffect,
};
```

**In `src/data/field_effects/field_effect_object_template_pointers.h`**, add extern declarations and array entries:

```c
extern const struct SpriteTemplate gFieldEffectObjectTemplate_PrairieTallGrass;
extern const struct SpriteTemplate gFieldEffectObjectTemplate_JumpPrairieTallGrass;

// In the array:
[FLDEFFOBJ_PRAIRIE_TALL_GRASS]      = &gFieldEffectObjectTemplate_PrairieTallGrass,
[FLDEFFOBJ_JUMP_PRAIRIE_TALL_GRASS] = &gFieldEffectObjectTemplate_JumpPrairieTallGrass,
```

### 8. Implement field effect C functions

**In `include/field_effect_helpers.h`**, declare:

```c
void UpdatePrairieTallGrassFieldEffect(struct Sprite *sprite);
u8 FindPrairieTallGrassFieldEffectSpriteId(u8 localId, u8 mapNum, u8 mapGroup, s16 x, s16 y);  // tall grass only
```

**In `src/field_effect_helpers.c`**, add the creator and callback functions. These are copies of the base type's functions with constants swapped.

#### Tall grass variant

```c
#define sElevation   data[0]
#define sX           data[1]
#define sY           data[2]
#define sMapNum      data[3]
#define sLocalId     data[3]
#define sMapGroup    data[4]
#define sCurrentMap  data[5]
#define sObjectMoved data[7]

u32 FldEff_PrairieTallGrass(void)
{
    u8 spriteId;
    s16 x = gFieldEffectArguments[0];
    s16 y = gFieldEffectArguments[1];
    SetSpritePosToOffsetMapCoords(&x, &y, 8, 8);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_PRAIRIE_TALL_GRASS], x, y, 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sElevation = gFieldEffectArguments[2];
        sprite->sX = gFieldEffectArguments[0];
        sprite->sY = gFieldEffectArguments[1];
        sprite->sMapNum = gFieldEffectArguments[4];
        sprite->sMapGroup = gFieldEffectArguments[5];
        sprite->sCurrentMap = gFieldEffectArguments[6];

        if (gFieldEffectArguments[7])
            SeekSpriteAnim(sprite, 4); // Skip to end of anim (tall grass = 4)
    }
    return 0;
}

void UpdatePrairieTallGrassFieldEffect(struct Sprite *sprite)
{
    u8 metatileBehavior;
    u8 localId;
    u8 objectEventId;
    u8 mapNum = sprite->sCurrentMap >> 8;
    u8 mapGroup = sprite->sCurrentMap;

    if (gCamera.active && (gSaveBlock1Ptr->location.mapNum != mapNum || gSaveBlock1Ptr->location.mapGroup != mapGroup))
    {
        sprite->sX -= gCamera.x;
        sprite->sY -= gCamera.y;
        sprite->sCurrentMap = ((u8)gSaveBlock1Ptr->location.mapNum << 8) | (u8)gSaveBlock1Ptr->location.mapGroup;
    }
    localId = sprite->sLocalId;
    mapNum = sprite->sMapNum;
    mapGroup = sprite->sMapGroup;
    metatileBehavior = MapGridGetMetatileBehaviorAt(sprite->sX, sprite->sY);

    if (TryGetObjectEventIdByLocalIdAndMap(localId, mapNum, mapGroup, &objectEventId)
     || !MetatileBehavior_IsPrairieTallGrass(metatileBehavior)     // <-- your check
     || (sprite->sObjectMoved && sprite->animEnded))
    {
        FieldEffectStop(sprite, FLDEFF_PRAIRIE_TALL_GRASS);         // <-- your constant
    }
    else
    {
        struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
        if ((objectEvent->currentCoords.x != sprite->sX || objectEvent->currentCoords.y != sprite->sY)
        && (objectEvent->previousCoords.x != sprite->sX || objectEvent->previousCoords.y != sprite->sY))
            sprite->sObjectMoved = TRUE;

        // Tall grass subpriority adjustment (not used in long grass)
        metatileBehavior = 0;
        if (sprite->animCmdIndex == 0)
            metatileBehavior = 4;

        UpdateObjectEventSpriteInvisibility(sprite, FALSE);
        UpdateGrassFieldEffectSubpriority(sprite, sprite->sElevation, metatileBehavior);
    }
}

// Tall grass needs this finder for the jump-on effect
u8 FindPrairieTallGrassFieldEffectSpriteId(u8 localId, u8 mapNum, u8 mapGroup, s16 x, s16 y)
{
    u8 i;
    for (i = 0; i < MAX_SPRITES; i++)
    {
        if (gSprites[i].inUse)
        {
            struct Sprite *sprite = &gSprites[i];
            if (sprite->callback == UpdatePrairieTallGrassFieldEffect
                && (x == sprite->sX && y == sprite->sY)
                && localId == (u8)(sprite->sLocalId)
                && mapNum == (sprite->sMapNum & 0xFF)
                && mapGroup == sprite->sMapGroup)
                return i;
        }
    }
    return MAX_SPRITES;
}

u32 FldEff_JumpPrairieTallGrass(void)
{
    u8 spriteId;

    SetSpritePosToOffsetMapCoords((s16 *)&gFieldEffectArguments[0], (s16 *)&gFieldEffectArguments[1], 8, 12);
    spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_JUMP_PRAIRIE_TALL_GRASS], gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    if (spriteId != MAX_SPRITES)
    {
        struct Sprite *sprite = &gSprites[spriteId];
        sprite->coordOffsetEnabled = TRUE;
        sprite->oam.priority = gFieldEffectArguments[3];
        sprite->sJumpElevation = gFieldEffectArguments[2];
        sprite->sJumpFldEff = FLDEFF_JUMP_PRAIRIE_TALL_GRASS;
    }
    return 0;
}

#undef sElevation
#undef sX
#undef sY
#undef sMapNum
#undef sLocalId
#undef sMapGroup
#undef sCurrentMap
#undef sObjectMoved
```

#### Long grass variant

The long grass version is similar but with these differences:
- `SeekSpriteAnim(sprite, 6)` instead of 4
- No subpriority adjustment (pass 0 to `UpdateGrassFieldEffectSubpriority`)
- No `Find*FieldEffectSpriteId` function needed
- Jump effect can reuse `FldEff_JumpLongGrass` (just load your palette in the field effect script)

### 9. Add ground effect flags and functions

**In `include/event_object_movement.h`**, add three new flags using the next available bit positions:

```c
#define GROUND_EFFECT_FLAG_PRAIRIE_TALL_GRASS_ON_SPAWN (1 << 23)
#define GROUND_EFFECT_FLAG_PRAIRIE_TALL_GRASS_ON_MOVE  (1 << 24)
#define GROUND_EFFECT_FLAG_LAND_IN_PRAIRIE_TALL_GRASS  (1 << 25)
```

**In `src/event_object_movement.c`**, add:

**Flag-gathering functions** (near existing grass detection functions):

```c
static void GetGroundEffectFlags_PrairieTallGrassOnSpawn(struct ObjectEvent *objEvent, u32 *flags)
{
    if (MetatileBehavior_IsPrairieTallGrass(objEvent->currentMetatileBehavior))
        *flags |= GROUND_EFFECT_FLAG_PRAIRIE_TALL_GRASS_ON_SPAWN;
}

static void GetGroundEffectFlags_PrairieTallGrassOnBeginStep(struct ObjectEvent *objEvent, u32 *flags)
{
    if (MetatileBehavior_IsPrairieTallGrass(objEvent->currentMetatileBehavior))
        *flags |= GROUND_EFFECT_FLAG_PRAIRIE_TALL_GRASS_ON_MOVE;
}
```

**Add forward declarations** above `GetAllGroundEffectFlags_OnSpawn`:

```c
static void GetGroundEffectFlags_PrairieTallGrassOnSpawn(struct ObjectEvent *objEvent, u32 *flags);
static void GetGroundEffectFlags_PrairieTallGrassOnBeginStep(struct ObjectEvent *objEvent, u32 *flags);
```

**Call them from the aggregator functions:**
- `GetAllGroundEffectFlags_OnSpawn()` - add `GetGroundEffectFlags_PrairieTallGrassOnSpawn(objEvent, flags);`
- `GetAllGroundEffectFlags_OnBeginStep()` - add `GetGroundEffectFlags_PrairieTallGrassOnBeginStep(objEvent, flags);`

**Add jump landing support** in `GetGroundEffectFlags_JumpLanding()`:

```c
// In metatileFuncs array (add after the existing grass entries):
MetatileBehavior_IsPrairieTallGrass,

// In jumpLandingFlags array (same position):
GROUND_EFFECT_FLAG_LAND_IN_PRAIRIE_TALL_GRASS,
```

**Ground effect handler functions.** The spawn and step functions are the same for both types. The jump function differs:

```c
void GroundEffect_SpawnOnPrairieTallGrass(struct ObjectEvent *objEvent, struct Sprite *sprite)
{
    gFieldEffectArguments[0] = objEvent->currentCoords.x;
    gFieldEffectArguments[1] = objEvent->currentCoords.y;
    gFieldEffectArguments[2] = objEvent->previousElevation;
    gFieldEffectArguments[3] = 2;
    gFieldEffectArguments[4] = objEvent->localId << 8 | objEvent->mapNum;
    gFieldEffectArguments[5] = objEvent->mapGroup;
    gFieldEffectArguments[6] = (u8)gSaveBlock1Ptr->location.mapNum << 8 | (u8)gSaveBlock1Ptr->location.mapGroup;
    gFieldEffectArguments[7] = 1;   // skip to end of anim (spawn = already in grass)
    FieldEffectStart(FLDEFF_PRAIRIE_TALL_GRASS);
}

void GroundEffect_StepOnPrairieTallGrass(struct ObjectEvent *objEvent, struct Sprite *sprite)
{
    gFieldEffectArguments[0] = objEvent->currentCoords.x;
    gFieldEffectArguments[1] = objEvent->currentCoords.y;
    gFieldEffectArguments[2] = objEvent->previousElevation;
    gFieldEffectArguments[3] = 2;
    gFieldEffectArguments[4] = (objEvent->localId << 8) | objEvent->mapNum;
    gFieldEffectArguments[5] = objEvent->mapGroup;
    gFieldEffectArguments[6] = (u8)gSaveBlock1Ptr->location.mapNum << 8 | (u8)gSaveBlock1Ptr->location.mapGroup;
    gFieldEffectArguments[7] = 0;   // play full animation
    FieldEffectStart(FLDEFF_PRAIRIE_TALL_GRASS);
}
```

**Tall grass jump handler** (spawns grass effect under player after landing):

```c
void GroundEffect_JumpOnPrairieTallGrass(struct ObjectEvent *objEvent, struct Sprite *sprite)
{
    u8 spriteId;

    gFieldEffectArguments[0] = objEvent->currentCoords.x;
    gFieldEffectArguments[1] = objEvent->currentCoords.y;
    gFieldEffectArguments[2] = objEvent->previousElevation;
    gFieldEffectArguments[3] = 2;
    FieldEffectStart(FLDEFF_JUMP_PRAIRIE_TALL_GRASS);

    // Tall grass also spawns a standing-in-grass effect after landing
    spriteId = FindPrairieTallGrassFieldEffectSpriteId(
        objEvent->localId,
        objEvent->mapNum,
        objEvent->mapGroup,
        objEvent->currentCoords.x,
        objEvent->currentCoords.y);

    if (spriteId == MAX_SPRITES)
        GroundEffect_SpawnOnPrairieTallGrass(objEvent, sprite);
}
```

**Long grass jump handler** (simpler, no spawn-after-land):

```c
void GroundEffect_JumpOnPrairieLongGrass(struct ObjectEvent *objEvent, struct Sprite *sprite)
{
    gFieldEffectArguments[0] = objEvent->currentCoords.x;
    gFieldEffectArguments[1] = objEvent->currentCoords.y;
    gFieldEffectArguments[2] = objEvent->previousElevation;
    gFieldEffectArguments[3] = 2;
    FieldEffectStart(FLDEFF_JUMP_PRAIRIE_LONG_GRASS);
}
```

**Register in `sGroundEffectFuncs[]` array** (append at end, order must match bit positions):

```c
    GroundEffect_SpawnOnPrairieTallGrass,  // GROUND_EFFECT_FLAG_PRAIRIE_TALL_GRASS_ON_SPAWN
    GroundEffect_StepOnPrairieTallGrass,   // GROUND_EFFECT_FLAG_PRAIRIE_TALL_GRASS_ON_MOVE
    GroundEffect_JumpOnPrairieTallGrass,   // GROUND_EFFECT_FLAG_LAND_IN_PRAIRIE_TALL_GRASS
```

### 10. Set up metatile in Porymap

In Porymap, assign your new `MB_PRAIRIE_TALL_GRASS` behavior value to your grass metatile. The numeric value comes from the enum position in `metatile_behaviors.h`.

---

## Common Pitfalls

### The `sLocalId` macro (critical)

When copying the sprite data `#define` block from an existing field effect, pay close attention to how `sLocalId` and `sMapNum` are defined. The grass effects pack both values into `data[3]`:

```c
#define sMapNum      data[3]      // Lower 8 bits (read via u8 cast)
#define sLocalId     data[3]      // Upper 8 bits (written as localId << 8 | mapNum)
```

The creator writes `sprite->sMapNum = (objEvent->localId << 8) | objEvent->mapNum`, packing both values. The callback reads them back. If `sLocalId` is defined incorrectly, `TryGetObjectEventIdByLocalIdAndMap()` receives the wrong localId, fails to find the object, and **immediately destroys the sprite on its first frame** - making it look like the animation never plays at all.

### Missing `.pal` source file

The build system converts `.pal` (JASC format) to `.gbapal` (binary). If you only place a pre-built `.gbapal` file, it works until `make clean` deletes it. Always provide the `.pal` source file in `graphics/field_effects/palettes/`.

### Missing spritesheet build rules

Without entries in `spritesheet_rules.mk`, the `.4bpp` files won't be regenerated from the `.png` sources after `make clean`. The sprite dimensions (`-mwidth 2 -mheight 2` for 16x16, `-mwidth 2 -mheight 1` for 16x8) must be correct.

### Palette/PNG mismatch

The `.png` sprite files are indexed PNGs. Their embedded palette must match the `.pal` file, because the build system extracts pixel data (indices) from the PNG and the runtime palette from the `.pal` file separately. If you update one, update the other.

### Ground effect array ordering

The `sGroundEffectFuncs[]` array entries must match the bit positions of the `GROUND_EFFECT_FLAG_*` constants exactly. Entry 0 corresponds to bit 0, entry 1 to bit 1, etc. New entries go at the end of the array with corresponding flag bits.

### SeekSpriteAnim frame number

Tall grass uses `SeekSpriteAnim(sprite, 4)` (5 frames, skip to last). Long grass uses `SeekSpriteAnim(sprite, 6)` (7 frames in the long grass animation sequence). Using the wrong value causes visual glitches on spawn.

---

## Files modified (summary)

| File | What to add |
|------|------------|
| `include/constants/metatile_behaviors.h` | Enum entry |
| `include/constants/field_effects.h` | FLDEFF, FLDEFFOBJ, and PAL_TAG constants |
| `include/event_object_movement.h` | Ground effect flag defines |
| `include/field_effect_helpers.h` | Callback and finder function declarations |
| `include/metatile_behavior.h` | Function declarations |
| `src/metatile_behavior.c` | Behavior check functions + updates to existing checks |
| `src/event_object_movement.c` | Ground effect flags, handlers, array entries, aggregator calls |
| `src/field_effect_helpers.c` | Creator + callback + finder functions |
| `data/field_effect_scripts.s` | Script table entries + script definitions |
| `src/data/field_effects/field_effect_objects.h` | Palette struct, pic tables, sprite templates |
| `src/data/field_effects/field_effect_object_template_pointers.h` | Extern declarations + array entries |
| `src/data/object_events/object_event_graphics.h` | INCBIN declarations (Option A only) |
| `spritesheet_rules.mk` | Build rules for .png to .4bpp (Option A only) |
| `graphics/field_effects/pics/` | Sprite sheet PNGs (Option A only) |
| `graphics/field_effects/palettes/` | JASC-PAL palette file (if new palette) |
