#include "global.h"
#include "bg.h"
#include "decompress.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_effect.h"
#include "gpu_regs.h"
#include "item.h"
#include "item_icon.h"
#include "line_break.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "overworld.h"
#include "palette.h"
#include "pokemon_icon.h"
#include "quest.h"
#include "quest_log.h"
#include "quest_note.h"
#include "region_map.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "constants/items.h"
#include "constants/region_map_sections.h"
#include "constants/rgb.h"
#include "constants/songs.h"

/*
 * Quest log: Quests / Leads / Profiles pages, each a list with a detail page.
 * Quest details have an objectives page and a journal page.
 *
 * Graphics live in graphics/quest_log/ and are placeholders:
 *   bg_tiles.png           BG1 tiles, palette loaded into BG palette 0
 *   list_*.bin             list screen tilemaps, one per selected page tab
 *   detail_*.bin           detail page tilemaps (objectives / journal; leads and profiles use objectives)
 *   window_gfx.png         checkboxes + status badges, blitted into text windows.
 *                          Its palette is loaded into BG palette 15 and is also the text palette,
 *                          so indices 1-9 must stay compatible with TEXT_COLOR_*.
 *   unread.png             start menu unread mark, blitted into the start menu window
 */

#define LIST_ROWS_MAX       8   // compact layout; see sListLayouts
#define LIST_SKIP           5   // entries moved by Left/Right on the list
#define LIST_WINDOW_Y       16
#define BODY_WINDOW_Y       48
#define BODY_HEIGHT         96
#define BODY_LINE_HEIGHT    12
#define BODY_REWARDS_Y      72
#define BODY_TEXT_WIDTH     232
#define MAX_REWARD_ICONS    4
#define MAX_LIST            (NOTE_MAX > QUEST_MAX ? NOTE_MAX : QUEST_MAX)

#define PIXEL_HIGHLIGHT     10

#define TAG_SCROLL_ARROWS   0x5B30
#define TAG_ITEM_ICON       0x5B40 // + icon slot

enum QuestLogTab
{
    TAB_QUESTS,
    TAB_LEADS,
    TAB_PROFILES,
    TAB_COUNT,
};

enum QuestLogPage
{
    PAGE_LIST,
    PAGE_OBJECTIVES,
    PAGE_JOURNAL,
    PAGE_LEAD,
    PAGE_PROFILE,
};

enum QuestLogWindow
{
    WIN_TABS,
    WIN_FOOTER,
    WIN_LIST,
    WIN_HEADER,
    WIN_BODY,
};

enum QuestLogTextColor
{
    COLOR_DARK,
    COLOR_LIGHT,
    COLOR_BLUE,
    COLOR_GRAY,
};

enum WindowGfxRect
{
    GFX_CHECKBOX_EMPTY,
    GFX_CHECKBOX_DONE,
    GFX_BADGE_MAIN,
    GFX_BADGE_NEW,
    GFX_BADGE_TRACKED,
    GFX_BADGE_TURN_IN,
    GFX_NONE,
};

struct QuestLogState
{
    MainCallback savedCallback;
    u8 loadState;
    u8 page;
    u8 tab;
    bool8 showFinished;
    u16 listCount;
    u16 cursor;
    u16 scroll;         // first visible list row
    u16 bodyScroll;     // detail body scroll, in lines
    u16 bodyMaxScroll;
    u8 scrollArrowsTaskId;
    u16 *scrollArrowsTarget;
    u8 scrollArrowsMax;
    const struct ListLayout *layout;
    u8 rowIcons[LIST_ROWS_MAX];
    u8 headerIcon;
    u8 rewardIcons[MAX_REWARD_ICONS];
    u16 list[MAX_LIST]; // quest, note or subject ids, depending on the tab
};

// Lays out detail body rows so they can scroll. Rows are measured in content pixels and only
// drawn when they fit entirely inside the visible area.
struct BodyWriter
{
    s32 y;          // content y of the next row
    s32 top;        // content y at the top of the visible area
    s32 bottom;     // content y at the bottom of the visible area
    bool32 draw;    // FALSE while measuring
};

// Row metrics for the list pages. Offsets are from the top of the row.
struct ListLayout
{
    u8 rows;
    u8 rowHeight;
    u8 textX;       // left edge of names and lead text
    u8 nameY;       // FONT_NARROW name
    u8 infoY;       // FONT_SMALL_NARROW area or note count
    u8 badgeY;
    bool8 icons;
};

// Icons: 32 px rows so 16x32 overworld sprites and 32x32 Pokémon icons fit inside a row.
// Compact: 16 px rows with no sprites.
static const struct ListLayout sListLayouts[] =
{
    [FALSE] = { .rows = 4, .rowHeight = 32, .textX = 32, .nameY = 8, .infoY = 10, .badgeY = 12, .icons = TRUE },
    [TRUE]  = { .rows = 8, .rowHeight = 16, .textX = 4,  .nameY = 0, .infoY = 2,  .badgeY = 4,  .icons = FALSE },
};

static EWRAM_DATA struct QuestLogState *sQuestLog = NULL;
static EWRAM_DATA u8 *sBg1TilemapBuffer = NULL;

static const struct BgTemplate sQuestLogBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .priority = 1
    },
    // Charblock 2 (not 3): screenblocks 28-31 overlap the upper half of charblock 3, which would cap
    // BG1 at 64 tiles. From charblock 2, BG1 has room for 768 tiles before reaching the tilemaps.
    {
        .bg = 1,
        .charBaseIndex = 2,
        .mapBaseIndex = 30,
        .priority = 2
    },
};

// The list windows and the detail windows are never shown together, so their tile blocks overlap.
static const struct WindowTemplate sQuestLogWindowTemplates[] =
{
    [WIN_TABS]   = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0,  .width = 30, .height = 2,  .paletteNum = 15, .baseBlock = 1 },
    [WIN_FOOTER] = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 18, .width = 30, .height = 2,  .paletteNum = 15, .baseBlock = 61 },
    [WIN_LIST]   = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 2,  .width = 30, .height = 16, .paletteNum = 15, .baseBlock = 121 },
    [WIN_HEADER] = { .bg = 0, .tilemapLeft = 5, .tilemapTop = 0,  .width = 25, .height = 5,  .paletteNum = 15, .baseBlock = 121 },
    [WIN_BODY]   = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 6,  .width = 30, .height = 12, .paletteNum = 15, .baseBlock = 246 },
    DUMMY_WIN_TEMPLATE
};

static const u32 sBgTiles[] = INCGFX_U32("graphics/quest_log/bg_tiles.png", ".4bpp.smol");
static const u16 sBgPalette[] = INCGFX_U16("graphics/quest_log/bg_tiles.png", ".gbapal");
static const u32 sListTilemap_Quests[] = INCGFX_U32("graphics/quest_log/list_quests.bin", ".smolTM");
static const u32 sListTilemap_Leads[] = INCGFX_U32("graphics/quest_log/list_leads.bin", ".smolTM");
static const u32 sListTilemap_Profiles[] = INCGFX_U32("graphics/quest_log/list_profiles.bin", ".smolTM");
static const u32 sDetailTilemap_Objectives[] = INCGFX_U32("graphics/quest_log/detail_objectives.bin", ".smolTM");
static const u32 sDetailTilemap_Journal[] = INCGFX_U32("graphics/quest_log/detail_journal.bin", ".smolTM");
static const u8 sWindowGfx[] = INCGFX_U8("graphics/quest_log/window_gfx.png", ".4bpp");
static const u16 sWindowPalette[] = INCGFX_U16("graphics/quest_log/window_gfx.png", ".gbapal");
static const u8 sUnreadGfx[] = INCGFX_U8("graphics/quest_log/unread.png", ".4bpp");

#define WINDOW_GFX_WIDTH  32
#define WINDOW_GFX_HEIGHT 40

static const struct { u8 x, y, width, height; } sWindowGfxRects[] =
{
    [GFX_CHECKBOX_EMPTY] = { 0,  0,  8, 8 },
    [GFX_CHECKBOX_DONE]  = { 8,  0,  8, 8 },
    [GFX_BADGE_MAIN]     = { 0,  8, 32, 8 },
    [GFX_BADGE_NEW]      = { 0, 16, 32, 8 },
    [GFX_BADGE_TRACKED]  = { 0, 24, 32, 8 },
    [GFX_BADGE_TURN_IN]  = { 0, 32, 32, 8 },
};

static const u32 *const sListTilemaps[TAB_COUNT] =
{
    [TAB_QUESTS]   = sListTilemap_Quests,
    [TAB_LEADS]    = sListTilemap_Leads,
    [TAB_PROFILES] = sListTilemap_Profiles,
};

static const bool8 sTabEnabled[TAB_COUNT] =
{
    [TAB_QUESTS]   = TRUE,
    [TAB_LEADS]    = QUEST_LEADS,
    [TAB_PROFILES] = QUEST_PROFILES,
};

static const u8 sTextColors[][3] =
{
    [COLOR_DARK]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY},
    [COLOR_LIGHT] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE,     TEXT_COLOR_DARK_GRAY},
    [COLOR_BLUE]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_BLUE,      TEXT_COLOR_LIGHT_BLUE},
    [COLOR_GRAY]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_DARK_GRAY},
};

static const u8 *const sTabNames[TAB_COUNT] =
{
    [TAB_QUESTS]   = COMPOUND_STRING("QUESTS"),
    [TAB_LEADS]    = COMPOUND_STRING("LEADS"),
    [TAB_PROFILES] = COMPOUND_STRING("PROFILES"),
};

static const u8 sText_NoQuests[] = _("No quests yet.");
static const u8 sText_NoFinishedQuests[] = _("No finished quests.");
static const u8 sText_NoLeads[] = _("No leads to follow.");
static const u8 sText_NoResolvedLeads[] = _("No followed-up leads.");
static const u8 sText_NoProfiles[] = _("No one to note down yet.");
static const u8 sText_NoJournal[] = _("No journal entries yet.");
static const u8 sText_NotStarted[] = _("Not started");
static const u8 sText_Closed[] = _("Closed");
static const u8 sText_Lead[] = _("Lead");
static const u8 sText_FollowedUp[] = _("Followed up");
static const u8 sText_Notes[] = _("{STR_VAR_1} notes");
static const u8 sText_Optional[] = _(" (optional)");
static const u8 sText_Rewards[] = _("Rewards:");
static const u8 sText_Journal[] = _("Journal");
static const u8 sText_FooterQuests[] = _("{A_BUTTON}Open {SELECT_BUTTON}Track {START_BUTTON}Finished {B_BUTTON}Exit");
static const u8 sText_FooterQuestsFinished[] = _("{A_BUTTON}Open {START_BUTTON}Active {B_BUTTON}Exit");
static const u8 sText_FooterLeads[] = _("{A_BUTTON}Open {SELECT_BUTTON}Pin {START_BUTTON}Followed up {B_BUTTON}Exit");
static const u8 sText_FooterLeadsFinished[] = _("{A_BUTTON}Open {START_BUTTON}Open leads {B_BUTTON}Exit");
static const u8 sText_FooterProfiles[] = _("{A_BUTTON}Open {B_BUTTON}Exit");
static const u8 sText_FooterObjectives[] = _("{DPAD_LEFTRIGHT}Journal {L_BUTTON}{R_BUTTON}Quest {SELECT_BUTTON}Track {B_BUTTON}Back");
static const u8 sText_FooterJournal[] = _("{DPAD_LEFTRIGHT}Objectives {DPAD_UPDOWN}Scroll {B_BUTTON}Back");
static const u8 sText_FooterLead[] = _("{L_BUTTON}{R_BUTTON}Lead {SELECT_BUTTON}Pin {B_BUTTON}Back");
static const u8 sText_FooterProfile[] = _("{DPAD_UPDOWN}Scroll {L_BUTTON}{R_BUTTON}Profile {B_BUTTON}Back");

static void QuestLog_SetupCB(void);
static void QuestLog_MainCB(void);
static void QuestLog_VBlankCB(void);
static void Task_QuestLogWaitFadeIn(u8 taskId);
static void Task_QuestLogListInput(u8 taskId);
static void Task_QuestLogDetailInput(u8 taskId);
static void Task_QuestLogWaitFadeAndExit(u8 taskId);
static bool8 QuestLog_InitBgs(void);
static bool8 QuestLog_LoadGraphics(void);
static void QuestLog_InitWindows(void);
static void QuestLog_FreeResources(void);
static void BuildList(void);
static void ShowListPage(void);
static void DrawList(void);
static void ShowDetailPage(u32 page);
static void DrawDetail(void);
static void DestroyRowIcons(void);
static void DestroyDetailIcons(void);
static void RemoveScrollArrows(void);

// *******************************
// Entry points

// The config picks the default; the layout flag, if set up, switches to the other one.
static bool32 IsCompactLayout(void)
{
    bool32 compact = QUEST_LOG_COMPACT;
    if (QUEST_LOG_LAYOUT_FLAG != 0 && FlagGet(QUEST_LOG_LAYOUT_FLAG))
        compact = !compact;
    return compact;
}

void QuestLog_Open(MainCallback returnCallback)
{
    sQuestLog = AllocZeroed(sizeof(*sQuestLog));
    if (sQuestLog == NULL)
    {
        SetMainCallback2(returnCallback);
        return;
    }
    sQuestLog->savedCallback = returnCallback;
    sQuestLog->layout = &sListLayouts[IsCompactLayout()];
    sQuestLog->scrollArrowsTaskId = TASK_NONE;
    sQuestLog->headerIcon = SPRITE_NONE;
    memset(sQuestLog->rowIcons, SPRITE_NONE, sizeof(sQuestLog->rowIcons));
    memset(sQuestLog->rewardIcons, SPRITE_NONE, sizeof(sQuestLog->rewardIcons));
    SetMainCallback2(QuestLog_SetupCB);
}

void Task_QuestLog_OpenFromStartMenu(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        CleanupOverworldWindowsAndTilemaps();
        QuestLog_Open(CB2_ReturnToFieldWithOpenMenu);
        DestroyTask(taskId);
    }
}

void QuestLog_BlitUnreadIndicator(u32 windowId, u32 x, u32 y)
{
    BlitBitmapToWindow(windowId, sUnreadGfx, x, y, 8, 8);
}

// *******************************
// Setup and callbacks

static void QuestLog_SetupCB(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankHBlankCallbacksToNull();
        ClearScheduledBgCopiesToVram();
        gMain.state++;
        break;
    case 1:
        ScanlineEffect_Stop();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        gMain.state++;
        break;
    case 2:
        if (!QuestLog_InitBgs())
        {
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            CreateTask(Task_QuestLogWaitFadeAndExit, 0);
            SetVBlankCallback(QuestLog_VBlankCB);
            SetMainCallback2(QuestLog_MainCB);
            return;
        }
        sQuestLog->loadState = 0;
        gMain.state++;
        break;
    case 3:
        if (QuestLog_LoadGraphics())
            gMain.state++;
        break;
    case 4:
        QuestLog_InitWindows();
        gMain.state++;
        break;
    case 5:
        BuildList();
        ShowListPage();
        CreateTask(Task_QuestLogWaitFadeIn, 0);
        gMain.state++;
        break;
    case 6:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    case 7:
        SetVBlankCallback(QuestLog_VBlankCB);
        SetMainCallback2(QuestLog_MainCB);
        break;
    }
}

static void QuestLog_MainCB(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void QuestLog_VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

#define TILEMAP_BUFFER_SIZE (1024 * 2)
static bool8 QuestLog_InitBgs(void)
{
    ResetAllBgsCoordinates();
    sBg1TilemapBuffer = AllocZeroed(TILEMAP_BUFFER_SIZE);
    if (sBg1TilemapBuffer == NULL)
        return FALSE;

    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sQuestLogBgTemplates, ARRAY_COUNT(sQuestLogBgTemplates));
    SetBgTilemapBuffer(1, sBg1TilemapBuffer);
    ScheduleBgCopyTilemapToVram(1);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    ShowBg(0);
    ShowBg(1);
    return TRUE;
}
#undef TILEMAP_BUFFER_SIZE

static bool8 QuestLog_LoadGraphics(void)
{
    switch (sQuestLog->loadState)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, sBgTiles, 0, 0, 0);
        sQuestLog->loadState++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            LoadPalette(sBgPalette, BG_PLTT_ID(0), PLTT_SIZE_4BPP);
            LoadPalette(sWindowPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
            sQuestLog->loadState = 0;
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void QuestLog_InitWindows(void)
{
    InitWindows(sQuestLogWindowTemplates);
    DeactivateAllTextPrinters();
    for (u32 i = 0; i <= WIN_BODY; i++)
        FillWindowPixelBuffer(i, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    PutWindowTilemap(WIN_FOOTER);
    ScheduleBgCopyTilemapToVram(0);
}

static void QuestLog_FreeResources(void)
{
    RemoveScrollArrows();
    TRY_FREE_AND_SET_NULL(sQuestLog);
    TRY_FREE_AND_SET_NULL(sBg1TilemapBuffer);
    FreeAllWindowBuffers();
    ResetSpriteData();
}

// *******************************
// Helpers

// Writes the area name of a map, or an empty string.
static void CopyMapAreaName(u8 *dest, u16 map)
{
    u16 mapSec = Quest_GetMapSec(map);
    if (mapSec >= MAPSEC_NONE)
        *dest = EOS;
    else
        GetMapName(dest, mapSec, 0);
}

static void CopyQuestAreaName(u8 *dest, u32 questId)
{
    u16 map;
    u8 localId;

    if (Quest_GetTarget(questId, &map, &localId))
        CopyMapAreaName(dest, map);
    else
        *dest = EOS;
}

static void BlitWindowGfx(u32 windowId, u32 gfx, u32 x, u32 y)
{
    BlitBitmapRectToWindow(windowId, sWindowGfx,
                           sWindowGfxRects[gfx].x, sWindowGfxRects[gfx].y,
                           WINDOW_GFX_WIDTH, WINDOW_GFX_HEIGHT,
                           x, y, sWindowGfxRects[gfx].width, sWindowGfxRects[gfx].height);
}

static void Print(u32 windowId, u32 fontId, const u8 *str, u32 x, u32 y, u32 color)
{
    AddTextPrinterParameterized4(windowId, fontId, x, y, 0, 0, sTextColors[color], TEXT_SKIP_DRAW, str);
}

static bool32 IsLineEnd(u8 c)
{
    return c == EOS || c == CHAR_NEWLINE || c == CHAR_PROMPT_SCROLL || c == CHAR_PROMPT_CLEAR;
}

// Copies the first line of str, word-wrapped to width, into dest. Adds "…" if the text goes on.
static void CopyFirstLine(u8 *dest, const u8 *str, u32 fontId, u32 width)
{
    u32 i;

    StringCopy(gStringVar4, str);
    BreakStringAutomatic(gStringVar4, width - GetStringWidth(fontId, COMPOUND_STRING("…"), 0), 0xFF, fontId, HIDE_SCROLL_PROMPT);
    for (i = 0; !IsLineEnd(gStringVar4[i]); i++)
        dest[i] = gStringVar4[i];
    if (gStringVar4[i] != EOS)
        dest[i++] = CHAR_ELLIPSIS;
    dest[i] = EOS;
}

static u32 GetBadge(u32 questId)
{
    if (Quest_IsUnread(questId))
        return GFX_BADGE_NEW;
    if (Quest_IsTurnInReady(questId))
        return GFX_BADGE_TURN_IN;
    if (Quest_GetTracked() == questId)
        return GFX_BADGE_TRACKED;
    if (Quest_GetInfo(questId)->category == QUEST_CATEGORY_MAIN)
        return GFX_BADGE_MAIN;
    return GFX_NONE;
}

// x and y are the centre of the icon. Item icons are 24x24 in the top-left of a 32x32 sprite.
static u8 CreateItemIcon(u16 item, s16 x, s16 y, u32 slot)
{
    u8 spriteId = AddItemIconSprite(TAG_ITEM_ICON + slot, TAG_ITEM_ICON + slot, item);
    if (spriteId >= MAX_SPRITES)
        return SPRITE_NONE;
    gSprites[spriteId].x = x + 4;
    gSprites[spriteId].y = y + 4;
    gSprites[spriteId].oam.priority = 0;
    return spriteId;
}

static u8 CreateIcon(u32 iconType, u32 icon, s16 x, s16 y, u32 slot)
{
    u8 spriteId = SPRITE_NONE;

    switch (iconType)
    {
    case QUEST_ICON_OBJECT:
        spriteId = CreateObjectGraphicsSprite(icon, SpriteCallbackDummy, x, y, 0);
        break;
    case QUEST_ICON_ITEM:
        if (icon == ITEM_NONE)
            return SPRITE_NONE;
        return CreateItemIcon(icon, x, y, slot);
    case QUEST_ICON_PKMN:
        LoadMonIconPalette(icon);
        spriteId = CreateMonIcon(icon, SpriteCB_MonIcon, x, y, 0, 0);
        break;
    }

    if (spriteId >= MAX_SPRITES)
        return SPRITE_NONE;
    gSprites[spriteId].x = x;
    gSprites[spriteId].y = y;
    gSprites[spriteId].oam.priority = 0;
    return spriteId;
}

static u8 CreateQuestIcon(u32 questId, s16 x, s16 y, u32 slot)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    return CreateIcon(quest->iconType, quest->icon, x, y, slot);
}

static u8 CreateSubjectIcon(u32 subjectId, s16 x, s16 y)
{
    if (!QuestSubject_IsValid(subjectId))
        return SPRITE_NONE;
    return CreateIcon(QUEST_ICON_OBJECT, QuestSubject_GetInfo(subjectId)->graphicsId, x, y, 0);
}

static void DestroyIcon(u8 *spriteId)
{
    struct Sprite *sprite;
    u32 paletteNum;
    bool32 usingSheet;
    u16 tileStart;

    if (*spriteId == SPRITE_NONE)
        return;

    sprite = &gSprites[*spriteId];
    paletteNum = sprite->oam.paletteNum;
    usingSheet = sprite->usingSheet;
    tileStart = sprite->sheetTileStart;

    if (sprite->callback == SpriteCB_MonIcon)
        FreeAndDestroyMonIconSprite(sprite);
    else
        DestroySprite(sprite);

    if (usingSheet)
        FieldEffectFreeTilesIfUnused(tileStart);
    FieldEffectFreePaletteIfUnused(paletteNum);
    *spriteId = SPRITE_NONE;
}

static void DestroyRowIcons(void)
{
    for (u32 i = 0; i < LIST_ROWS_MAX; i++)
        DestroyIcon(&sQuestLog->rowIcons[i]);
}

static void DestroyDetailIcons(void)
{
    DestroyIcon(&sQuestLog->headerIcon);
    for (u32 i = 0; i < MAX_REWARD_ICONS; i++)
        DestroyIcon(&sQuestLog->rewardIcons[i]);
}

static void RemoveScrollArrows(void)
{
    if (sQuestLog != NULL && sQuestLog->scrollArrowsTaskId != TASK_NONE)
    {
        RemoveScrollIndicatorArrowPair(sQuestLog->scrollArrowsTaskId);
        sQuestLog->scrollArrowsTaskId = TASK_NONE;
    }
}

static void SetScrollArrows(u16 *scroll, u32 maxScroll, u32 topY, u32 bottomY)
{
    if (sQuestLog->scrollArrowsTaskId != TASK_NONE
     && sQuestLog->scrollArrowsTarget == scroll
     && sQuestLog->scrollArrowsMax == maxScroll)
        return;

    RemoveScrollArrows();
    if (maxScroll > 0)
    {
        sQuestLog->scrollArrowsTaskId = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, 232, topY, bottomY, maxScroll, TAG_SCROLL_ARROWS, TAG_SCROLL_ARROWS, scroll);
        sQuestLog->scrollArrowsTarget = scroll;
        sQuestLog->scrollArrowsMax = maxScroll;
    }
}

static void SwitchWindows(bool32 showList)
{
    if (showList)
    {
        ClearWindowTilemap(WIN_HEADER);
        ClearWindowTilemap(WIN_BODY);
        PutWindowTilemap(WIN_TABS);
        PutWindowTilemap(WIN_LIST);
    }
    else
    {
        ClearWindowTilemap(WIN_TABS);
        ClearWindowTilemap(WIN_LIST);
        PutWindowTilemap(WIN_HEADER);
        PutWindowTilemap(WIN_BODY);
    }
    ScheduleBgCopyTilemapToVram(0);
}

static void DrawFooter(const u8 *text)
{
    FillWindowPixelBuffer(WIN_FOOTER, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    Print(WIN_FOOTER, FONT_SMALL, text, 4, 2, COLOR_LIGHT);
    CopyWindowToVram(WIN_FOOTER, COPYWIN_GFX);
}

// *******************************
// Body writer

static void Writer_Init(struct BodyWriter *writer, u32 scroll, u32 height, bool32 draw)
{
    writer->y = 2;
    writer->top = scroll * BODY_LINE_HEIGHT;
    writer->bottom = writer->top + height;
    writer->draw = draw;
}

// Reserves a row. Returns TRUE and the window y if the row should be drawn.
static bool32 Writer_Row(struct BodyWriter *writer, u32 height, u32 *windowY)
{
    bool32 visible = writer->draw && writer->y >= writer->top && writer->y + (s32)height <= writer->bottom;
    *windowY = writer->y - writer->top;
    writer->y += height;
    return visible;
}

static void Writer_Text(struct BodyWriter *writer, u32 fontId, const u8 *str, u32 x, u32 width, u32 color)
{
    u8 line[80];
    const u8 *src;
    u32 windowY;

    StringCopy(gStringVar4, str);
    BreakStringAutomatic(gStringVar4, width, 0xFF, fontId, HIDE_SCROLL_PROMPT);
    src = gStringVar4;
    while (TRUE)
    {
        u32 n = 0;
        while (!IsLineEnd(*src) && n < sizeof(line) - 1)
            line[n++] = *src++;
        line[n] = EOS;
        if (Writer_Row(writer, BODY_LINE_HEIGHT, &windowY))
            Print(WIN_BODY, fontId, line, x, windowY, color);
        if (*src == EOS)
            break;
        src++;
    }
}

static void Writer_Gap(struct BodyWriter *writer, u32 height)
{
    writer->y += height;
}

// Runs draw once to measure the body, clamps the scroll, then draws the visible rows.
static void DrawScrollableBody(void (*draw)(struct BodyWriter *, u32), u32 id, u32 height)
{
    struct BodyWriter writer;
    s32 overflow;

    Writer_Init(&writer, 0, height, FALSE);
    draw(&writer, id);
    overflow = writer.y - (s32)height;
    sQuestLog->bodyMaxScroll = overflow > 0 ? (overflow + BODY_LINE_HEIGHT - 1) / BODY_LINE_HEIGHT : 0;
    if (sQuestLog->bodyScroll > sQuestLog->bodyMaxScroll)
        sQuestLog->bodyScroll = sQuestLog->bodyMaxScroll;

    Writer_Init(&writer, sQuestLog->bodyScroll, height, TRUE);
    draw(&writer, id);
    SetScrollArrows(&sQuestLog->bodyScroll, sQuestLog->bodyMaxScroll, BODY_WINDOW_Y + 4, BODY_WINDOW_Y + height - 4);
}

// *******************************
// List page

static bool32 QuestMatchesFilter(u32 questId, u32 category)
{
    u32 status = Quest_GetStatus(questId);

    if (!Quest_IsValid(questId) || Quest_IsTask(questId) || Quest_GetInfo(questId)->category != category)
        return FALSE;
    if (sQuestLog->showFinished)
        return status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_CLOSED;
    return status == QUEST_STATUS_AVAILABLE || status == QUEST_STATUS_ACTIVE;
}

static bool32 LeadMatchesFilter(u32 noteId)
{
    if (!QuestNote_IsKnown(noteId) || !QuestNote_IsLead(noteId))
        return FALSE;
    return QuestNote_IsOpenLead(noteId) != sQuestLog->showFinished;
}

static void ClampListCursor(void)
{
    if (sQuestLog->listCount == 0)
    {
        sQuestLog->cursor = 0;
        sQuestLog->scroll = 0;
        return;
    }
    if (sQuestLog->cursor >= sQuestLog->listCount)
        sQuestLog->cursor = sQuestLog->listCount - 1;
    if (sQuestLog->cursor < sQuestLog->scroll)
        sQuestLog->scroll = sQuestLog->cursor;
    u32 rows = sQuestLog->layout->rows;

    if (sQuestLog->cursor >= sQuestLog->scroll + rows)
        sQuestLog->scroll = sQuestLog->cursor - rows + 1;
    if (sQuestLog->listCount <= rows)
        sQuestLog->scroll = 0;
    else if (sQuestLog->scroll > sQuestLog->listCount - rows)
        sQuestLog->scroll = sQuestLog->listCount - rows;
}

static void AddToList(u32 id)
{
    sQuestLog->list[sQuestLog->listCount++] = id;
}

// Quests: tracked first, then main quests, then side quests, each by id. Tasks are shown under their parent.
// Leads: the pinned lead first, then by id. Profiles: by id.
static void BuildList(void)
{
    u32 tracked;

    sQuestLog->listCount = 0;
    switch (sQuestLog->tab)
    {
    case TAB_QUESTS:
        tracked = Quest_GetTracked();
        if (tracked != QUEST_NONE && QuestMatchesFilter(tracked, Quest_GetInfo(tracked)->category))
            AddToList(tracked);
        for (u32 category = 0; category < QUEST_CATEGORY_COUNT; category++)
        {
            for (u32 i = 0; i < QUEST_COUNT; i++)
            {
                if (i != tracked && QuestMatchesFilter(i, category))
                    AddToList(i);
            }
        }
        break;
    case TAB_LEADS:
        tracked = QuestNote_GetTracked();
        if (tracked != NOTE_NONE && LeadMatchesFilter(tracked))
            AddToList(tracked);
        for (u32 i = 0; i < NOTE_COUNT; i++)
        {
            if (i != tracked && LeadMatchesFilter(i))
                AddToList(i);
        }
        break;
    case TAB_PROFILES:
        for (u32 i = 0; i < SUBJECT_COUNT; i++)
        {
            if (QuestSubject_IsValid(i) && QuestSubject_IsKnown(i))
                AddToList(i);
        }
        break;
    }
    ClampListCursor();
}

static void DrawTabs(void)
{
    FillWindowPixelBuffer(WIN_TABS, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    for (u32 i = 0; i < TAB_COUNT; i++)
    {
        u32 x = i * 80 + (80 - GetStringWidth(FONT_NORMAL, sTabNames[i], 0)) / 2;
        if (sTabEnabled[i])
            Print(WIN_TABS, FONT_NORMAL, sTabNames[i], x, 0, i == sQuestLog->tab ? COLOR_LIGHT : COLOR_GRAY);
    }
    CopyWindowToVram(WIN_TABS, COPYWIN_FULL);
}

static s16 GetRowIconY(u32 y)
{
    return LIST_WINDOW_Y + y + sQuestLog->layout->rowHeight / 2;
}

static void DrawQuestRow(u32 questId, u32 row, u32 y)
{
    const struct ListLayout *layout = sQuestLog->layout;
    u32 badge = GetBadge(questId);

    Print(WIN_LIST, FONT_NARROW, Quest_GetInfo(questId)->name, layout->textX, y + layout->nameY, COLOR_DARK);
    CopyQuestAreaName(gStringVar1, questId);
    Print(WIN_LIST, FONT_SMALL_NARROW, gStringVar1, 120, y + layout->infoY, COLOR_BLUE);
    if (badge != GFX_NONE)
        BlitWindowGfx(WIN_LIST, badge, 200, y + layout->badgeY);
    if (layout->icons)
        sQuestLog->rowIcons[row] = CreateQuestIcon(questId, 16, GetRowIconY(y), row);
}

// Icons: area above one line of text. Compact: one line of text only; the area is on the detail page.
static void DrawLeadRow(u32 noteId, u32 row, u32 y)
{
    const struct ListLayout *layout = sQuestLog->layout;
    const struct QuestNote *note = QuestNote_GetInfo(noteId);
    u32 textWidth = 196 - layout->textX;

    if (layout->icons)
    {
        CopyMapAreaName(gStringVar1, note->targetMap);
        Print(WIN_LIST, FONT_SMALL_NARROW, gStringVar1, layout->textX, y + 4, COLOR_BLUE);
        CopyFirstLine(gStringVar2, note->text, FONT_SMALL_NARROW, textWidth);
        Print(WIN_LIST, FONT_SMALL_NARROW, gStringVar2, layout->textX, y + 16, COLOR_DARK);
        sQuestLog->rowIcons[row] = CreateSubjectIcon(note->subject, 16, GetRowIconY(y));
    }
    else
    {
        CopyFirstLine(gStringVar2, note->text, FONT_SMALL_NARROW, textWidth);
        Print(WIN_LIST, FONT_SMALL_NARROW, gStringVar2, layout->textX, y + layout->infoY, COLOR_DARK);
    }
    if (QuestNote_GetTracked() == noteId)
        BlitWindowGfx(WIN_LIST, GFX_BADGE_TRACKED, 200, y + layout->badgeY);
}

static u32 CountSubjectNotes(u32 subjectId)
{
    u32 count = 0;
    for (u32 i = 0; i < NOTE_COUNT; i++)
        count += (QuestNote_GetInfo(i)->subject == subjectId && QuestNote_IsKnown(i));
    return count;
}

static void DrawProfileRow(u32 subjectId, u32 row, u32 y)
{
    const struct ListLayout *layout = sQuestLog->layout;

    Print(WIN_LIST, FONT_NARROW, QuestSubject_GetInfo(subjectId)->name, layout->textX, y + layout->nameY, COLOR_DARK);
    ConvertIntToDecimalStringN(gStringVar1, CountSubjectNotes(subjectId), STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar2, sText_Notes);
    Print(WIN_LIST, FONT_SMALL_NARROW, gStringVar2, 120, y + layout->infoY, COLOR_BLUE);
    if (QuestSubject_IsUnread(subjectId))
        BlitWindowGfx(WIN_LIST, GFX_BADGE_NEW, 200, y + layout->badgeY);
    if (layout->icons)
        sQuestLog->rowIcons[row] = CreateSubjectIcon(subjectId, 16, GetRowIconY(y));
}

static const u8 *GetEmptyListText(void)
{
    switch (sQuestLog->tab)
    {
    case TAB_LEADS:
        return sQuestLog->showFinished ? sText_NoResolvedLeads : sText_NoLeads;
    case TAB_PROFILES:
        return sText_NoProfiles;
    default:
        return sQuestLog->showFinished ? sText_NoFinishedQuests : sText_NoQuests;
    }
}

static void DrawList(void)
{
    DestroyRowIcons();
    FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    if (sQuestLog->listCount == 0)
    {
        const u8 *text = GetEmptyListText();
        Print(WIN_LIST, FONT_NORMAL, text, (240 - GetStringWidth(FONT_NORMAL, text, 0)) / 2, 56, COLOR_DARK);
    }

    for (u32 row = 0; row < sQuestLog->layout->rows && sQuestLog->scroll + row < sQuestLog->listCount; row++)
    {
        u32 index = sQuestLog->scroll + row;
        u32 id = sQuestLog->list[index];
        u32 y = row * sQuestLog->layout->rowHeight;

        if (index == sQuestLog->cursor)
            FillWindowPixelRect(WIN_LIST, PIXEL_FILL(PIXEL_HIGHLIGHT), 0, y, 240, sQuestLog->layout->rowHeight);

        switch (sQuestLog->tab)
        {
        case TAB_QUESTS:
            DrawQuestRow(id, row, y);
            break;
        case TAB_LEADS:
            DrawLeadRow(id, row, y);
            break;
        case TAB_PROFILES:
            DrawProfileRow(id, row, y);
            break;
        }
    }

    CopyWindowToVram(WIN_LIST, COPYWIN_FULL);
    SetScrollArrows(&sQuestLog->scroll,
                    sQuestLog->listCount > sQuestLog->layout->rows ? sQuestLog->listCount - sQuestLog->layout->rows : 0,
                    LIST_WINDOW_Y + 4, LIST_WINDOW_Y + 124);
}

static const u8 *GetListFooter(void)
{
    switch (sQuestLog->tab)
    {
    case TAB_LEADS:
        return sQuestLog->showFinished ? sText_FooterLeadsFinished : sText_FooterLeads;
    case TAB_PROFILES:
        return sText_FooterProfiles;
    default:
        return sQuestLog->showFinished ? sText_FooterQuestsFinished : sText_FooterQuests;
    }
}

static void ShowListPage(void)
{
    sQuestLog->page = PAGE_LIST;
    DestroyDetailIcons();
    CopyToBgTilemapBuffer(1, sListTilemaps[sQuestLog->tab], 0, 0);
    ScheduleBgCopyTilemapToVram(1);
    SwitchWindows(TRUE);
    DrawTabs();
    DrawList();
    DrawFooter(GetListFooter());
}

static void ToggleTracked(u32 questId)
{
    if (Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE)
    {
        PlaySE(SE_FAILURE);
        return;
    }
    if (Quest_GetTracked() == questId)
        Quest_SetTracked(QUEST_NONE);
    else
        Quest_SetTracked(questId);
    PlaySE(SE_SELECT);
}

static void TogglePinned(u32 noteId)
{
    if (!QuestNote_IsOpenLead(noteId))
    {
        PlaySE(SE_FAILURE);
        return;
    }
    if (QuestNote_GetTracked() == noteId)
        QuestNote_SetTracked(NOTE_NONE);
    else
        QuestNote_SetTracked(noteId);
    PlaySE(SE_SELECT);
}

static void ChangeTab(bool32 forward)
{
    do
        sQuestLog->tab = (sQuestLog->tab + (forward ? 1 : TAB_COUNT - 1)) % TAB_COUNT;
    while (!sTabEnabled[sQuestLog->tab]);
    sQuestLog->showFinished = FALSE;
    sQuestLog->cursor = 0;
    sQuestLog->scroll = 0;
}

static void Task_QuestLogWaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_QuestLogListInput;
}

static u32 GetDetailPageForTab(void)
{
    switch (sQuestLog->tab)
    {
    case TAB_LEADS:
        return PAGE_LEAD;
    case TAB_PROFILES:
        return PAGE_PROFILE;
    default:
        return PAGE_OBJECTIVES;
    }
}

static void Task_QuestLogListInput(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_PC_OFF);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_QuestLogWaitFadeAndExit;
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (sQuestLog->listCount != 0)
        {
            PlaySE(SE_SELECT);
            RemoveScrollArrows();
            DestroyRowIcons();
            sQuestLog->bodyScroll = 0;
            ShowDetailPage(GetDetailPageForTab());
            gTasks[taskId].func = Task_QuestLogDetailInput;
        }
    }
    else if (JOY_NEW(START_BUTTON))
    {
        if (sQuestLog->tab != TAB_PROFILES)
        {
            PlaySE(SE_SELECT);
            sQuestLog->showFinished = !sQuestLog->showFinished;
            sQuestLog->cursor = 0;
            sQuestLog->scroll = 0;
            BuildList();
            ShowListPage();
        }
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        if (sQuestLog->listCount != 0 && sQuestLog->tab != TAB_PROFILES)
        {
            u32 id = sQuestLog->list[sQuestLog->cursor];
            if (sQuestLog->tab == TAB_LEADS)
                TogglePinned(id);
            else
                ToggleTracked(id);
            BuildList();
            // Keep the cursor on the same entry after re-sorting
            for (u32 i = 0; i < sQuestLog->listCount; i++)
            {
                if (sQuestLog->list[i] == id)
                    sQuestLog->cursor = i;
            }
            ClampListCursor();
            DrawList();
        }
    }
    else if (JOY_REPEAT(DPAD_LEFT) && sQuestLog->cursor > 0)
    {
        PlaySE(SE_SELECT);
        sQuestLog->cursor = sQuestLog->cursor > LIST_SKIP ? sQuestLog->cursor - LIST_SKIP : 0;
        ClampListCursor();
        DrawList();
    }
    else if (JOY_REPEAT(DPAD_RIGHT) && sQuestLog->cursor + 1 < sQuestLog->listCount)
    {
        PlaySE(SE_SELECT);
        sQuestLog->cursor = min(sQuestLog->cursor + LIST_SKIP, sQuestLog->listCount - 1);
        ClampListCursor();
        DrawList();
    }
    else if (JOY_NEW(L_BUTTON) || JOY_NEW(R_BUTTON))
    {
        PlaySE(SE_SELECT);
        ChangeTab(JOY_NEW(R_BUTTON));
        BuildList();
        ShowListPage();
    }
    else if (JOY_REPEAT(DPAD_UP) && sQuestLog->cursor > 0)
    {
        PlaySE(SE_SELECT);
        sQuestLog->cursor--;
        ClampListCursor();
        DrawList();
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sQuestLog->cursor + 1 < sQuestLog->listCount)
    {
        PlaySE(SE_SELECT);
        sQuestLog->cursor++;
        ClampListCursor();
        DrawList();
    }
}

static void Task_QuestLogWaitFadeAndExit(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sQuestLog->savedCallback);
        QuestLog_FreeResources();
        DestroyTask(taskId);
    }
}

// *******************************
// Quest detail

static void DrawQuestHeader(u32 questId)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    const u8 *subtitle = NULL;
    u16 map;
    u8 localId;

    Print(WIN_HEADER, FONT_NORMAL, quest->name, 0, 1, COLOR_LIGHT);

    switch (Quest_GetStatus(questId))
    {
    case QUEST_STATUS_AVAILABLE:
        subtitle = sText_NotStarted;
        break;
    case QUEST_STATUS_ACTIVE:
        subtitle = Quest_GetCurrentStage(questId)->title;
        break;
    case QUEST_STATUS_COMPLETE:
        subtitle = quest->outcomes[Quest_GetOutcome(questId)].summary;
        break;
    case QUEST_STATUS_CLOSED:
        subtitle = sText_Closed;
        break;
    }
    if (sQuestLog->page == PAGE_JOURNAL)
        subtitle = sText_Journal;
    if (subtitle != NULL)
        Print(WIN_HEADER, FONT_NARROW, subtitle, 0, 15, COLOR_LIGHT);

    if (Quest_GetTarget(questId, &map, &localId))
    {
        CopyMapAreaName(gStringVar1, map);
        Print(WIN_HEADER, FONT_SMALL, gStringVar1, 0, 28, COLOR_GRAY);
    }

    sQuestLog->headerIcon = CreateQuestIcon(questId, 20, 20, LIST_ROWS_MAX);
}

static void DrawRewards(u32 questId, u32 y)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    u32 x = 60;
    u32 icon = 0;

    if (quest->rewardCount == 0)
        return;

    Print(WIN_BODY, FONT_SMALL, sText_Rewards, 4, y + 6, COLOR_DARK);
    for (u32 i = 0; i < quest->rewardCount && x < 232; i++)
    {
        const struct QuestReward *reward = &quest->rewards[i];
        if (reward->type == QUEST_REWARD_MONEY)
        {
            ConvertIntToDecimalStringN(gStringVar1, reward->amount, STR_CONV_MODE_LEFT_ALIGN, 7);
            StringExpandPlaceholders(gStringVar2, gText_PokedollarVar1);
            Print(WIN_BODY, FONT_SMALL, gStringVar2, x, y + 6, COLOR_DARK);
            x += GetStringWidth(FONT_SMALL, gStringVar2, 0) + 8;
        }
        else if (icon < MAX_REWARD_ICONS)
        {
            sQuestLog->rewardIcons[icon] = CreateItemIcon(reward->item, x + 12, BODY_WINDOW_Y + y + 12, LIST_ROWS_MAX + 1 + icon);
            icon++;
            ConvertIntToDecimalStringN(gStringVar1, reward->amount, STR_CONV_MODE_LEFT_ALIGN, 3);
            StringExpandPlaceholders(gStringVar2, COMPOUND_STRING("×{STR_VAR_1}"));
            Print(WIN_BODY, FONT_SMALL, gStringVar2, x + 24, y + 6, COLOR_DARK);
            x += 24 + GetStringWidth(FONT_SMALL, gStringVar2, 0) + 8;
        }
    }
}

static void Writer_Objective(struct BodyWriter *writer, const struct QuestObjective *objective, bool32 done)
{
    u16 current, target;
    u32 windowY;
    u8 *end;

    if (!Writer_Row(writer, BODY_LINE_HEIGHT, &windowY))
        return;

    BlitWindowGfx(WIN_BODY, done ? GFX_CHECKBOX_DONE : GFX_CHECKBOX_EMPTY, 6, windowY + 3);
    end = StringCopy(gStringVar3, objective->text);
    if (objective->optional)
        StringCopy(end, sText_Optional);
    Print(WIN_BODY, FONT_SMALL, gStringVar3, 18, windowY, done ? COLOR_GRAY : COLOR_DARK);

    if (!done && Quest_GetConditionProgress(&objective->condition, &current, &target))
    {
        ConvertIntToDecimalStringN(gStringVar1, min(current, target), STR_CONV_MODE_LEFT_ALIGN, 5);
        ConvertIntToDecimalStringN(gStringVar2, target, STR_CONV_MODE_LEFT_ALIGN, 5);
        StringExpandPlaceholders(gStringVar3, COMPOUND_STRING("{STR_VAR_1}/{STR_VAR_2}"));
        Print(WIN_BODY, FONT_SMALL, gStringVar3, 228 - GetStringWidth(FONT_SMALL, gStringVar3, 0), windowY, COLOR_BLUE);
    }
}

// The quests a counted objective counts, indented under it. Undiscovered ones are left out.
static void Writer_CountedQuests(struct BodyWriter *writer, const struct QuestCondition *condition)
{
    u32 windowY;

    for (u32 i = 0; i < condition->listCount; i++)
    {
        u32 questId = condition->list[i];
        bool32 done = Quest_GetStatus(questId) == QUEST_STATUS_COMPLETE;

        if (Quest_GetStatus(questId) == QUEST_STATUS_HIDDEN)
            continue;
        if (Writer_Row(writer, BODY_LINE_HEIGHT, &windowY))
        {
            BlitWindowGfx(WIN_BODY, done ? GFX_CHECKBOX_DONE : GFX_CHECKBOX_EMPTY, 18, windowY + 3);
            Print(WIN_BODY, FONT_SMALL, Quest_GetInfo(questId)->name, 30, windowY, done ? COLOR_GRAY : COLOR_DARK);
        }
    }
}

static void Writer_QuestObjectives(struct BodyWriter *writer, u32 questId)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    const struct QuestStage *stage;

    Writer_Text(writer, FONT_SMALL, quest->summary, 4, BODY_TEXT_WIDTH, COLOR_DARK);
    if (Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE)
        return;

    Writer_Gap(writer, 2);
    stage = Quest_GetCurrentStage(questId);
    for (u32 i = 0; i < stage->objectiveCount; i++)
    {
        const struct QuestObjective *objective = &stage->objectives[i];

        if (!Quest_IsObjectiveVisible(questId, i))
            continue;
        Writer_Objective(writer, objective, Quest_IsObjectiveDone(questId, i));
        if (objective->condition.type == QUEST_COND_QUESTS_COMPLETE)
            Writer_CountedQuests(writer, &objective->condition);
    }
}

static void Writer_QuestJournal(struct BodyWriter *writer, u32 questId)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    bool32 any = FALSE;
    u32 windowY;

    for (u32 i = 0; i < quest->stageCount; i++)
    {
        const struct QuestStage *stage = &quest->stages[i];

        if (stage->title == NULL || stage->journal == NULL || !Quest_IsStagePassed(questId, i))
            continue;
        any = TRUE;
        if (Writer_Row(writer, BODY_LINE_HEIGHT, &windowY))
            Print(WIN_BODY, FONT_SMALL, stage->title, 4, windowY, COLOR_BLUE);
        Writer_Text(writer, FONT_SMALL, stage->journal, 4, BODY_TEXT_WIDTH, COLOR_DARK);
        Writer_Gap(writer, 4);
    }

    if (!any && Writer_Row(writer, BODY_LINE_HEIGHT, &windowY))
        Print(WIN_BODY, FONT_NORMAL, sText_NoJournal, 4, windowY, COLOR_DARK);
}

static void DrawQuestDetail(u32 questId)
{
    Quest_ClearUnread(questId);
    DrawQuestHeader(questId);
    if (sQuestLog->page == PAGE_JOURNAL)
    {
        DrawScrollableBody(Writer_QuestJournal, questId, BODY_HEIGHT);
    }
    else
    {
        bool32 hasRewards = Quest_GetInfo(questId)->rewardCount != 0;
        DrawScrollableBody(Writer_QuestObjectives, questId, hasRewards ? BODY_REWARDS_Y : BODY_HEIGHT);
        DrawRewards(questId, BODY_REWARDS_Y);
    }
}

// *******************************
// Lead and profile detail

static void Writer_Lead(struct BodyWriter *writer, u32 noteId)
{
    Writer_Text(writer, FONT_NORMAL, QuestNote_GetInfo(noteId)->text, 4, BODY_TEXT_WIDTH, COLOR_DARK);
}

static void DrawLeadDetail(u32 noteId)
{
    const struct QuestNote *note = QuestNote_GetInfo(noteId);
    const u8 *name = QuestSubject_IsValid(note->subject) ? QuestSubject_GetInfo(note->subject)->name : sText_Lead;

    Print(WIN_HEADER, FONT_NORMAL, name, 0, 1, COLOR_LIGHT);
    Print(WIN_HEADER, FONT_NARROW, QuestNote_IsOpenLead(noteId) ? sText_Lead : sText_FollowedUp, 0, 15, COLOR_LIGHT);
    CopyMapAreaName(gStringVar1, note->targetMap);
    Print(WIN_HEADER, FONT_SMALL, gStringVar1, 0, 28, COLOR_GRAY);
    sQuestLog->headerIcon = CreateSubjectIcon(note->subject, 20, 20);
    DrawScrollableBody(Writer_Lead, noteId, BODY_HEIGHT);
}

static void Writer_Profile(struct BodyWriter *writer, u32 subjectId)
{
    for (u32 i = 0; i < NOTE_COUNT; i++)
    {
        if (QuestNote_GetInfo(i)->subject != subjectId || !QuestNote_IsKnown(i))
            continue;
        Writer_Text(writer, FONT_SMALL, QuestNote_GetInfo(i)->text, 4, BODY_TEXT_WIDTH, COLOR_DARK);
        Writer_Gap(writer, 4);
    }
}

static void DrawProfileDetail(u32 subjectId)
{
    QuestSubject_ClearUnread(subjectId);
    Print(WIN_HEADER, FONT_NORMAL, QuestSubject_GetInfo(subjectId)->name, 0, 1, COLOR_LIGHT);
    ConvertIntToDecimalStringN(gStringVar1, CountSubjectNotes(subjectId), STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar2, sText_Notes);
    Print(WIN_HEADER, FONT_NARROW, gStringVar2, 0, 15, COLOR_LIGHT);
    sQuestLog->headerIcon = CreateSubjectIcon(subjectId, 20, 20);
    DrawScrollableBody(Writer_Profile, subjectId, BODY_HEIGHT);
}

static void DrawDetail(void)
{
    u32 id = sQuestLog->list[sQuestLog->cursor];

    DestroyDetailIcons();
    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    FillWindowPixelBuffer(WIN_BODY, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    switch (sQuestLog->page)
    {
    case PAGE_LEAD:
        DrawLeadDetail(id);
        break;
    case PAGE_PROFILE:
        DrawProfileDetail(id);
        break;
    default:
        DrawQuestDetail(id);
        break;
    }
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
    CopyWindowToVram(WIN_BODY, COPYWIN_FULL);
}

static const u8 *GetDetailFooter(u32 page)
{
    switch (page)
    {
    case PAGE_JOURNAL:
        return sText_FooterJournal;
    case PAGE_LEAD:
        return sText_FooterLead;
    case PAGE_PROFILE:
        return sText_FooterProfile;
    default:
        return sText_FooterObjectives;
    }
}

static void ShowDetailPage(u32 page)
{
    sQuestLog->page = page;
    CopyToBgTilemapBuffer(1, page == PAGE_JOURNAL ? sDetailTilemap_Journal : sDetailTilemap_Objectives, 0, 0);
    ScheduleBgCopyTilemapToVram(1);
    SwitchWindows(FALSE);
    DrawDetail();
    DrawFooter(GetDetailFooter(page));
}

static void Task_QuestLogDetailInput(u8 taskId)
{
    bool32 isQuest = sQuestLog->page == PAGE_OBJECTIVES || sQuestLog->page == PAGE_JOURNAL;

    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        RemoveScrollArrows();
        BuildList();
        ShowListPage();
        gTasks[taskId].func = Task_QuestLogListInput;
    }
    else if (isQuest && (JOY_NEW(DPAD_LEFT) || JOY_NEW(DPAD_RIGHT)))
    {
        PlaySE(SE_SELECT);
        sQuestLog->bodyScroll = 0;
        ShowDetailPage(sQuestLog->page == PAGE_JOURNAL ? PAGE_OBJECTIVES : PAGE_JOURNAL);
    }
    else if (JOY_NEW(SELECT_BUTTON) && sQuestLog->page != PAGE_PROFILE)
    {
        u32 id = sQuestLog->list[sQuestLog->cursor];
        if (sQuestLog->page == PAGE_LEAD)
            TogglePinned(id);
        else
            ToggleTracked(id);
        DrawDetail();
    }
    else if (JOY_NEW(L_BUTTON) && sQuestLog->cursor > 0)
    {
        PlaySE(SE_SELECT);
        sQuestLog->cursor--;
        sQuestLog->bodyScroll = 0;
        DrawDetail();
    }
    else if (JOY_NEW(R_BUTTON) && sQuestLog->cursor + 1 < sQuestLog->listCount)
    {
        PlaySE(SE_SELECT);
        sQuestLog->cursor++;
        sQuestLog->bodyScroll = 0;
        DrawDetail();
    }
    else if (JOY_REPEAT(DPAD_UP) && sQuestLog->bodyScroll > 0)
    {
        PlaySE(SE_SELECT);
        sQuestLog->bodyScroll--;
        DrawDetail();
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sQuestLog->bodyScroll < sQuestLog->bodyMaxScroll)
    {
        PlaySE(SE_SELECT);
        sQuestLog->bodyScroll++;
        DrawDetail();
    }
}
