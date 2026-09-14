#include "global.h"
#include "event_object_movement.h"
#include "field_effect.h"
#include "field_weather.h"
#include "main.h"
#include "overworld.h"
#include "quest.h"
#include "quest_guidance.h"
#include "region_map.h"
#include "sprite.h"
#include "constants/event_objects.h"
#include "constants/region_map_sections.h"

/*
 * Guidance: NPC markers above quest givers, turn-ins and objective targets, and the tracked quest's
 * pin on the Town Map. Both are driven by quest definitions.
 *
 * Graphics (placeholders):
 *   graphics/quest_log/markers.png  9 frames of 16x16: (story, pokemon, side) x (turn-in, available, target)
 *   graphics/quest_log/pin.png      16x16 Town Map pin
 */

// Marker priority: lower value wins. See enum QuestMarkerType.
u32 QuestMarkers_GetType(u32 localId, u32 mapNum, u32 mapGroup, u8 *category)
{
    u16 map = (mapGroup << 8) | mapNum;
    u32 best = QUEST_MARKER_NONE;

    if (localId == LOCALID_NONE)
        return QUEST_MARKER_NONE;

    for (u32 questId = 0; questId < QUEST_COUNT; questId++)
    {
        const struct Quest *quest;
        const struct QuestStage *stage;
        u32 status = Quest_GetStatus(questId);

        if (!Quest_IsValid(questId))
            continue;
        quest = Quest_GetInfo(questId);

        if (status == QUEST_STATUS_LEAD || status == QUEST_STATUS_AVAILABLE)
        {
            if (quest->giverMap == map && quest->giverLocalId == localId
             && (best == QUEST_MARKER_NONE || best > QUEST_MARKER_AVAILABLE))
            {
                best = QUEST_MARKER_AVAILABLE;
                *category = quest->category;
            }
        }
        else if (status == QUEST_STATUS_ACTIVE)
        {
            stage = Quest_GetCurrentStage(questId);
            if (stage->turnInMap == map && stage->turnInLocalId == localId && Quest_IsTurnInReady(questId))
            {
                *category = quest->category;
                return QUEST_MARKER_TURN_IN;
            }
            if (best != QUEST_MARKER_NONE)
                continue;
            for (u32 i = 0; i < stage->objectiveCount; i++)
            {
                const struct QuestObjective *objective = &stage->objectives[i];
                if (objective->targetMap == map && objective->targetLocalId == localId
                 && !Quest_IsObjectiveDone(questId, i) && Quest_IsObjectiveVisible(questId, i))
                {
                    best = QUEST_MARKER_TARGET;
                    *category = quest->category;
                    break;
                }
            }
        }
    }
    return best;
}

#if QUEST_NPC_MARKERS

#define TAG_QUEST_MARKER 0x5B20

static const u8 sMarkerGfx[] = INCGFX_U8("graphics/quest_log/markers.png", ".4bpp");
static const u16 sMarkerPalette[] = INCGFX_U16("graphics/quest_log/markers.png", ".gbapal");

static const struct SpritePalette sMarkerSpritePalette = { sMarkerPalette, TAG_QUEST_MARKER };

static const struct OamData sOam_QuestMarker =
{
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 1,
};

static const struct SpriteFrameImage sPicTable_QuestMarker[] =
{
    overworld_frame(sMarkerGfx, 2, 2, 0),
    overworld_frame(sMarkerGfx, 2, 2, 1),
    overworld_frame(sMarkerGfx, 2, 2, 2),
    overworld_frame(sMarkerGfx, 2, 2, 3),
    overworld_frame(sMarkerGfx, 2, 2, 4),
    overworld_frame(sMarkerGfx, 2, 2, 5),
    overworld_frame(sMarkerGfx, 2, 2, 6),
    overworld_frame(sMarkerGfx, 2, 2, 7),
    overworld_frame(sMarkerGfx, 2, 2, 8),
};

static const union AnimCmd sAnim_Marker0[] = { ANIMCMD_FRAME(0, 1), ANIMCMD_END };
static const union AnimCmd sAnim_Marker1[] = { ANIMCMD_FRAME(1, 1), ANIMCMD_END };
static const union AnimCmd sAnim_Marker2[] = { ANIMCMD_FRAME(2, 1), ANIMCMD_END };
static const union AnimCmd sAnim_Marker3[] = { ANIMCMD_FRAME(3, 1), ANIMCMD_END };
static const union AnimCmd sAnim_Marker4[] = { ANIMCMD_FRAME(4, 1), ANIMCMD_END };
static const union AnimCmd sAnim_Marker5[] = { ANIMCMD_FRAME(5, 1), ANIMCMD_END };
static const union AnimCmd sAnim_Marker6[] = { ANIMCMD_FRAME(6, 1), ANIMCMD_END };
static const union AnimCmd sAnim_Marker7[] = { ANIMCMD_FRAME(7, 1), ANIMCMD_END };
static const union AnimCmd sAnim_Marker8[] = { ANIMCMD_FRAME(8, 1), ANIMCMD_END };

// Anim index = category * 3 + (marker type - 1)
static const union AnimCmd *const sAnims_QuestMarker[] =
{
    sAnim_Marker0, sAnim_Marker1, sAnim_Marker2,
    sAnim_Marker3, sAnim_Marker4, sAnim_Marker5,
    sAnim_Marker6, sAnim_Marker7, sAnim_Marker8,
};

static void SpriteCB_QuestMarker(struct Sprite *sprite);

static const struct SpriteTemplate sSpriteTemplate_QuestMarker =
{
    .tileTag = TAG_NONE,
    .paletteTag = TAG_QUEST_MARKER,
    .oam = &sOam_QuestMarker,
    .anims = sAnims_QuestMarker,
    .images = sPicTable_QuestMarker,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_QuestMarker,
};

#define sLocalId   data[0]
#define sMapNum    data[1]
#define sMapGroup  data[2]
#define sBobTimer  data[3]

static void DestroyMarker(struct Sprite *sprite)
{
    u32 paletteNum = sprite->oam.paletteNum;
    DestroySprite(sprite);
    FieldEffectFreePaletteIfUnused(paletteNum);
}

static void SpriteCB_QuestMarker(struct Sprite *sprite)
{
    static const s8 sBobOffsets[] = {0, 0, 0, 0, 0, 0, -1, -1, -1, -2, -2, -2, -2, -2, -2, -1, -1, -1};
    struct Sprite *objectSprite;
    u8 objectEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(sprite->sLocalId, sprite->sMapNum, sprite->sMapGroup, &objectEventId))
    {
        DestroyMarker(sprite);
        return;
    }

    objectSprite = &gSprites[gObjectEvents[objectEventId].spriteId];
    sprite->x = objectSprite->x + objectSprite->x2;
    sprite->y = objectSprite->y + objectSprite->y2 + objectSprite->centerToCornerVecY - 6;
    sprite->invisible = objectSprite->invisible || gObjectEvents[objectEventId].invisible;
    sprite->sBobTimer = (sprite->sBobTimer + 1) % (ARRAY_COUNT(sBobOffsets) * 2);
    sprite->y2 = sBobOffsets[sprite->sBobTimer / 2];
}

static void DestroyMarkerFor(u32 localId, u32 mapNum, u32 mapGroup)
{
    for (u32 i = 0; i < MAX_SPRITES; i++)
    {
        struct Sprite *sprite = &gSprites[i];
        if (sprite->inUse && sprite->callback == SpriteCB_QuestMarker
         && sprite->sLocalId == localId && sprite->sMapNum == mapNum && sprite->sMapGroup == mapGroup)
            DestroyMarker(sprite);
    }
}

void QuestMarkers_OnObjectSpawn(u32 objectEventId)
{
    struct ObjectEvent *objectEvent = &gObjectEvents[objectEventId];
    u32 type;
    u8 category = 0;
    u8 spriteId;

    if (!objectEvent->active || objectEvent->isPlayer)
        return;

    DestroyMarkerFor(objectEvent->localId, objectEvent->mapNum, objectEvent->mapGroup);
    type = QuestMarkers_GetType(objectEvent->localId, objectEvent->mapNum, objectEvent->mapGroup, &category);
    if (type == QUEST_MARKER_NONE)
        return;

    if (IndexOfSpritePaletteTag(TAG_QUEST_MARKER) == 0xFF)
        UpdateSpritePaletteWithWeather(LoadSpritePalette(&sMarkerSpritePalette), FALSE);

    spriteId = CreateSprite(&sSpriteTemplate_QuestMarker, 0, 0, 0);
    if (spriteId == MAX_SPRITES)
        return;

    gSprites[spriteId].coordOffsetEnabled = TRUE;
    gSprites[spriteId].sLocalId = objectEvent->localId;
    gSprites[spriteId].sMapNum = objectEvent->mapNum;
    gSprites[spriteId].sMapGroup = objectEvent->mapGroup;
    StartSpriteAnim(&gSprites[spriteId], category * 3 + type - 1);
    SpriteCB_QuestMarker(&gSprites[spriteId]);
}

// Rebuilds all markers after a quest state change. Outside the field, markers are
// created by the object spawn hook when the map loads.
void QuestMarkers_Refresh(void)
{
    if (gMain.callback2 != CB2_Overworld)
        return;

    for (u32 i = 0; i < MAX_SPRITES; i++)
    {
        if (gSprites[i].inUse && gSprites[i].callback == SpriteCB_QuestMarker)
            DestroyMarker(&gSprites[i]);
    }
    for (u32 i = 0; i < OBJECT_EVENTS_COUNT; i++)
        QuestMarkers_OnObjectSpawn(i);
}

#undef sLocalId
#undef sMapNum
#undef sMapGroup
#undef sBobTimer

#endif // QUEST_NPC_MARKERS

#if QUEST_TOWN_MAP_PIN

#define TAG_QUEST_PIN 0x5B21

// Same offsets as MAPCURSOR_X_MIN / MAPCURSOR_Y_MIN in region_map.c
#define TOWN_MAP_X_OFFSET 1
#define TOWN_MAP_Y_OFFSET 2

static const u8 sPinGfx[] = INCGFX_U8("graphics/quest_log/pin.png", ".4bpp");
static const u16 sPinPalette[] = INCGFX_U16("graphics/quest_log/pin.png", ".gbapal");

static const struct SpriteSheet sSpriteSheet_QuestPin = { sPinGfx, sizeof(sPinGfx), TAG_QUEST_PIN };
static const struct SpritePalette sSpritePalette_QuestPin = { sPinPalette, TAG_QUEST_PIN };

static const struct OamData sOam_QuestPin =
{
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 1,
};

static const struct SpriteTemplate sSpriteTemplate_QuestPin =
{
    .tileTag = TAG_QUEST_PIN,
    .paletteTag = TAG_QUEST_PIN,
    .oam = &sOam_QuestPin,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// Draws the tracked quest's target on the non-zoomed Town Map (field_region_map.c).
void QuestPin_CreateTownMapSprite(void)
{
    const struct RegionMapLocation *location;
    u16 map, mapSec;
    u8 localId;
    s16 x, y;

    if (!Quest_GetTrackedTarget(&map, &localId))
        return;
    mapSec = Quest_GetMapSec(map);
    if (mapSec >= MAPSEC_NONE)
        return;

    location = &gRegionMapEntries[mapSec];
    x = (location->x + (location->width - 1) / 2 + TOWN_MAP_X_OFFSET) * 8 + 4;
    y = (location->y + (location->height - 1) / 2 + TOWN_MAP_Y_OFFSET) * 8 - 4;

    LoadSpriteSheet(&sSpriteSheet_QuestPin);
    LoadSpritePalette(&sSpritePalette_QuestPin);
    CreateSprite(&sSpriteTemplate_QuestPin, x, y, 0);
}

#endif // QUEST_TOWN_MAP_PIN
