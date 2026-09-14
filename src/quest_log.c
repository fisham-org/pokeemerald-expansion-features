#include "global.h"
#include "bg.h"
#include "decompress.h"
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
#include "constants/region_map_sections.h"
#include "constants/rgb.h"
#include "constants/songs.h"

/*
 * Quest log: tabbed list of quests, a detail page with objectives and a journal page.
 *
 * Graphics live in graphics/quest_log/ and are placeholders:
 *   bg_tiles.png           BG1 tiles, palette loaded into BG palette 0
 *   list_*.bin             list screen tilemaps, one per selected tab
 *   detail_*.bin           detail page tilemaps (objectives / journal)
 *   window_gfx.png         checkboxes + status badges, blitted into text windows.
 *                          Its palette is loaded into BG palette 15 and is also the text palette,
 *                          so indices 1-9 must stay compatible with TEXT_COLOR_*.
 *   unread.png             start menu unread mark, blitted into the start menu window
 */

#define LIST_ROWS           5
#define LIST_ROW_HEIGHT     24
#define LIST_TOP_PADDING    4
#define LIST_WINDOW_Y       16
#define BODY_WINDOW_Y       48
#define BODY_LINE_HEIGHT    12
#define MAX_REWARD_ICONS    4

#define PIXEL_HIGHLIGHT     10

#define TAG_SCROLL_ARROWS   0x5B30
#define TAG_ITEM_ICON       0x5B40 // + icon slot

enum QuestLogPage
{
    PAGE_LIST,
    PAGE_OBJECTIVES,
    PAGE_JOURNAL,
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
    GFX_BADGE_LEAD,
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
    u8 listCount;
    u8 cursor;
    u16 scroll;         // first visible list row
    u16 journalScroll;  // first visible journal entry
    u8 journalCount;
    u8 scrollArrowsTaskId;
    u16 *scrollArrowsTarget;
    u8 scrollArrowsMax;
    u8 rowIcons[LIST_ROWS];
    u8 headerIcon;
    u8 rewardIcons[MAX_REWARD_ICONS];
    u8 list[QUEST_MAX];
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
static const u32 sListTilemap_Story[] = INCGFX_U32("graphics/quest_log/list_story.bin", ".smolTM");
static const u32 sListTilemap_Pokemon[] = INCGFX_U32("graphics/quest_log/list_pokemon.bin", ".smolTM");
static const u32 sListTilemap_Side[] = INCGFX_U32("graphics/quest_log/list_side.bin", ".smolTM");
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
    [GFX_BADGE_LEAD]     = { 0,  8, 32, 8 },
    [GFX_BADGE_NEW]      = { 0, 16, 32, 8 },
    [GFX_BADGE_TRACKED]  = { 0, 24, 32, 8 },
    [GFX_BADGE_TURN_IN]  = { 0, 32, 32, 8 },
};

static const u32 *const sListTilemaps[QUEST_CATEGORY_COUNT] =
{
    [QUEST_CATEGORY_STORY]   = sListTilemap_Story,
    [QUEST_CATEGORY_POKEMON] = sListTilemap_Pokemon,
    [QUEST_CATEGORY_SIDE]    = sListTilemap_Side,
};

static const u8 sTextColors[][3] =
{
    [COLOR_DARK]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY},
    [COLOR_LIGHT] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE,     TEXT_COLOR_DARK_GRAY},
    [COLOR_BLUE]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_BLUE,      TEXT_COLOR_LIGHT_BLUE},
    [COLOR_GRAY]  = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_DARK_GRAY},
};

static const u8 *const sCategoryNames[QUEST_CATEGORY_COUNT] =
{
    [QUEST_CATEGORY_STORY]   = COMPOUND_STRING("STORY"),
    [QUEST_CATEGORY_POKEMON] = COMPOUND_STRING("POKéMON"),
    [QUEST_CATEGORY_SIDE]    = COMPOUND_STRING("SIDE"),
};

static const u8 sText_UnknownName[] = _("???");
static const u8 sText_NoQuests[] = _("No quests yet.");
static const u8 sText_NoFinishedQuests[] = _("No finished quests.");
static const u8 sText_NoJournal[] = _("No journal entries yet.");
static const u8 sText_Lead[] = _("Lead");
static const u8 sText_NotStarted[] = _("Not started");
static const u8 sText_Closed[] = _("Closed");
static const u8 sText_Optional[] = _(" (optional)");
static const u8 sText_Rewards[] = _("Rewards:");
static const u8 sText_Journal[] = _("Journal");
static const u8 sText_FooterList[] = _("{A_BUTTON}Open {SELECT_BUTTON}Track {START_BUTTON}Finished {B_BUTTON}Exit");
static const u8 sText_FooterListFinished[] = _("{A_BUTTON}Open {START_BUTTON}Active {B_BUTTON}Exit");
static const u8 sText_FooterObjectives[] = _("{DPAD_LEFTRIGHT}Journal {DPAD_UPDOWN}Quest {SELECT_BUTTON}Track {B_BUTTON}Back");
static const u8 sText_FooterJournal[] = _("{DPAD_LEFTRIGHT}Objectives {DPAD_UPDOWN}Scroll {B_BUTTON}Back");

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

void QuestLog_Open(MainCallback returnCallback)
{
    sQuestLog = AllocZeroed(sizeof(*sQuestLog));
    if (sQuestLog == NULL)
    {
        SetMainCallback2(returnCallback);
        return;
    }
    sQuestLog->savedCallback = returnCallback;
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

static const u8 *GetQuestDisplayName(u32 questId)
{
    if (Quest_GetStatus(questId) == QUEST_STATUS_LEAD)
        return sText_UnknownName;
    return Quest_GetInfo(questId)->name;
}

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

// Word-wraps str into gStringVar4 and prints it. Returns the number of lines printed.
static u32 PrintWrapped(u32 windowId, u32 fontId, const u8 *str, u32 x, u32 y, u32 width, u32 color)
{
    StringCopy(gStringVar4, str);
    BreakStringAutomatic(gStringVar4, width, 0xFF, fontId, HIDE_SCROLL_PROMPT);
    Print(windowId, fontId, gStringVar4, x, y, color);
    return CountLineBreaks(gStringVar4) + 1;
}

static u32 GetWrappedLineCount(u32 fontId, const u8 *str, u32 width)
{
    StringCopy(gStringVar4, str);
    BreakStringAutomatic(gStringVar4, width, 0xFF, fontId, HIDE_SCROLL_PROMPT);
    return CountLineBreaks(gStringVar4) + 1;
}

static u32 GetBadge(u32 questId)
{
    if (Quest_IsUnread(questId))
        return GFX_BADGE_NEW;
    if (Quest_IsTurnInReady(questId))
        return GFX_BADGE_TURN_IN;
    if (Quest_GetTracked() == questId)
        return GFX_BADGE_TRACKED;
    if (Quest_GetStatus(questId) == QUEST_STATUS_LEAD)
        return GFX_BADGE_LEAD;
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

static u8 CreateQuestIcon(u32 questId, s16 x, s16 y, u32 slot)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    u8 spriteId = SPRITE_NONE;

    switch (quest->iconType)
    {
    case QUEST_ICON_OBJECT:
        spriteId = CreateObjectGraphicsSprite(quest->icon, SpriteCallbackDummy, x, y, 0);
        break;
    case QUEST_ICON_ITEM:
        return CreateItemIcon(quest->icon, x, y, slot);
    case QUEST_ICON_PKMN:
        LoadMonIconPalette(quest->icon);
        spriteId = CreateMonIcon(quest->icon, SpriteCB_MonIcon, x, y, 0, 0);
        break;
    }

    if (spriteId >= MAX_SPRITES)
        return SPRITE_NONE;
    gSprites[spriteId].x = x;
    gSprites[spriteId].y = y;
    gSprites[spriteId].oam.priority = 0;
    return spriteId;
}

static void DestroyIcon(u8 *spriteId)
{
    struct Sprite *sprite;
    u32 paletteNum;
    u16 tileStart = 0;

    if (*spriteId == SPRITE_NONE)
        return;

    sprite = &gSprites[*spriteId];
    paletteNum = sprite->oam.paletteNum;
    if (sprite->usingSheet)
        tileStart = sprite->sheetTileStart;

    if (sprite->callback == SpriteCB_MonIcon)
        FreeAndDestroyMonIconSprite(sprite);
    else
        DestroySprite(sprite);

    if (tileStart)
        FieldEffectFreeTilesIfUnused(tileStart);
    FieldEffectFreePaletteIfUnused(paletteNum);
    *spriteId = SPRITE_NONE;
}

static void DestroyRowIcons(void)
{
    for (u32 i = 0; i < LIST_ROWS; i++)
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
// List page

static bool32 MatchesFilter(u32 questId)
{
    u32 status = Quest_GetStatus(questId);

    if (!Quest_IsValid(questId) || Quest_GetInfo(questId)->category != sQuestLog->tab)
        return FALSE;
    if (sQuestLog->showFinished)
        return status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_CLOSED;
    return status == QUEST_STATUS_LEAD || status == QUEST_STATUS_AVAILABLE || status == QUEST_STATUS_ACTIVE;
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
    if (sQuestLog->cursor >= sQuestLog->scroll + LIST_ROWS)
        sQuestLog->scroll = sQuestLog->cursor - LIST_ROWS + 1;
    if (sQuestLog->listCount <= LIST_ROWS)
        sQuestLog->scroll = 0;
    else if (sQuestLog->scroll > sQuestLog->listCount - LIST_ROWS)
        sQuestLog->scroll = sQuestLog->listCount - LIST_ROWS;
}

// The tracked quest is sorted first, the rest by id.
static void BuildList(void)
{
    u32 tracked = Quest_GetTracked();

    sQuestLog->listCount = 0;
    if (tracked != QUEST_NONE && MatchesFilter(tracked))
        sQuestLog->list[sQuestLog->listCount++] = tracked;
    for (u32 i = 0; i < QUEST_COUNT; i++)
    {
        if (i != tracked && MatchesFilter(i))
            sQuestLog->list[sQuestLog->listCount++] = i;
    }
    ClampListCursor();
}

static void DrawTabs(void)
{
    FillWindowPixelBuffer(WIN_TABS, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    for (u32 i = 0; i < QUEST_CATEGORY_COUNT; i++)
    {
        u32 x = i * 80 + (80 - GetStringWidth(FONT_NORMAL, sCategoryNames[i], 0)) / 2;
        Print(WIN_TABS, FONT_NORMAL, sCategoryNames[i], x, 0, i == sQuestLog->tab ? COLOR_LIGHT : COLOR_GRAY);
    }
    CopyWindowToVram(WIN_TABS, COPYWIN_FULL);
}

static void DrawList(void)
{
    DestroyRowIcons();
    FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    if (sQuestLog->listCount == 0)
    {
        const u8 *text = sQuestLog->showFinished ? sText_NoFinishedQuests : sText_NoQuests;
        Print(WIN_LIST, FONT_NORMAL, text, (240 - GetStringWidth(FONT_NORMAL, text, 0)) / 2, 56, COLOR_DARK);
    }

    for (u32 row = 0; row < LIST_ROWS && sQuestLog->scroll + row < sQuestLog->listCount; row++)
    {
        u32 index = sQuestLog->scroll + row;
        u32 questId = sQuestLog->list[index];
        u32 y = LIST_TOP_PADDING + row * LIST_ROW_HEIGHT;
        u32 badge = GetBadge(questId);

        if (index == sQuestLog->cursor)
            FillWindowPixelRect(WIN_LIST, PIXEL_FILL(PIXEL_HIGHLIGHT), 0, y, 240, LIST_ROW_HEIGHT);

        Print(WIN_LIST, FONT_NARROW, GetQuestDisplayName(questId), 32, y + 4, COLOR_DARK);
        CopyQuestAreaName(gStringVar1, questId);
        Print(WIN_LIST, FONT_SMALL_NARROW, gStringVar1, 120, y + 6, COLOR_BLUE);
        if (badge != GFX_NONE)
            BlitWindowGfx(WIN_LIST, badge, 200, y + 8);

        sQuestLog->rowIcons[row] = CreateQuestIcon(questId, 16, LIST_WINDOW_Y + y + LIST_ROW_HEIGHT / 2, row);
    }

    CopyWindowToVram(WIN_LIST, COPYWIN_FULL);
    SetScrollArrows(&sQuestLog->scroll,
                    sQuestLog->listCount > LIST_ROWS ? sQuestLog->listCount - LIST_ROWS : 0,
                    LIST_WINDOW_Y + 4, LIST_WINDOW_Y + 124);
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
    DrawFooter(sQuestLog->showFinished ? sText_FooterListFinished : sText_FooterList);
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

static void Task_QuestLogWaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_QuestLogListInput;
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
            sQuestLog->journalScroll = 0;
            ShowDetailPage(PAGE_OBJECTIVES);
            gTasks[taskId].func = Task_QuestLogDetailInput;
        }
    }
    else if (JOY_NEW(START_BUTTON))
    {
        PlaySE(SE_SELECT);
        sQuestLog->showFinished = !sQuestLog->showFinished;
        sQuestLog->cursor = 0;
        sQuestLog->scroll = 0;
        BuildList();
        ShowListPage();
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        if (sQuestLog->listCount != 0)
        {
            u32 questId = sQuestLog->list[sQuestLog->cursor];
            ToggleTracked(questId);
            BuildList();
            // Keep the cursor on the same quest after re-sorting
            for (u32 i = 0; i < sQuestLog->listCount; i++)
            {
                if (sQuestLog->list[i] == questId)
                    sQuestLog->cursor = i;
            }
            ClampListCursor();
            DrawList();
        }
    }
    else if (JOY_NEW(L_BUTTON) || JOY_NEW(R_BUTTON))
    {
        PlaySE(SE_SELECT);
        if (JOY_NEW(L_BUTTON))
            sQuestLog->tab = (sQuestLog->tab + QUEST_CATEGORY_COUNT - 1) % QUEST_CATEGORY_COUNT;
        else
            sQuestLog->tab = (sQuestLog->tab + 1) % QUEST_CATEGORY_COUNT;
        sQuestLog->cursor = 0;
        sQuestLog->scroll = 0;
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
// Detail page

static u32 GetPassedStageCount(u32 questId)
{
    u32 status = Quest_GetStatus(questId);
    u32 stage = Quest_GetStage(questId);

    if (status == QUEST_STATUS_COMPLETE)
        return min(stage + 1, Quest_GetInfo(questId)->stageCount);
    if (status == QUEST_STATUS_ACTIVE || status == QUEST_STATUS_CLOSED)
        return stage;
    return 0;
}

static void DrawHeader(u32 questId)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    const u8 *subtitle = NULL;
    u16 map;
    u8 localId;

    FillWindowPixelBuffer(WIN_HEADER, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    Print(WIN_HEADER, FONT_NORMAL, GetQuestDisplayName(questId), 0, 1, COLOR_LIGHT);

    switch (Quest_GetStatus(questId))
    {
    case QUEST_STATUS_LEAD:
        subtitle = sText_Lead;
        break;
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

    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
    sQuestLog->headerIcon = CreateQuestIcon(questId, 20, 20, LIST_ROWS);
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
            sQuestLog->rewardIcons[icon] = CreateItemIcon(reward->item, x + 12, BODY_WINDOW_Y + y + 12, LIST_ROWS + 1 + icon);
            icon++;
            ConvertIntToDecimalStringN(gStringVar1, reward->amount, STR_CONV_MODE_LEFT_ALIGN, 3);
            StringExpandPlaceholders(gStringVar2, COMPOUND_STRING("×{STR_VAR_1}"));
            Print(WIN_BODY, FONT_SMALL, gStringVar2, x + 24, y + 6, COLOR_DARK);
            x += 24 + GetStringWidth(FONT_SMALL, gStringVar2, 0) + 8;
        }
    }
}

static void DrawObjectivesBody(u32 questId)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    u32 status = Quest_GetStatus(questId);
    u32 y = 2;

    if (status == QUEST_STATUS_LEAD)
    {
        if (quest->leadHint != NULL)
            PrintWrapped(WIN_BODY, FONT_NORMAL, quest->leadHint, 4, y, 232, COLOR_DARK);
        return; // leads hide rewards
    }

    if (status == QUEST_STATUS_ACTIVE)
    {
        const struct QuestStage *stage = Quest_GetCurrentStage(questId);
        u32 visible = 0;

        for (u32 i = 0; i < stage->objectiveCount; i++)
            visible += Quest_IsObjectiveVisible(questId, i);

        // Summary goes above the objectives while there is room for it
        if (y + (GetWrappedLineCount(FONT_SMALL, quest->summary, 232) + visible) * BODY_LINE_HEIGHT <= 68)
            y += PrintWrapped(WIN_BODY, FONT_SMALL, quest->summary, 4, y, 232, COLOR_DARK) * BODY_LINE_HEIGHT + 2;

        for (u32 i = 0; i < stage->objectiveCount; i++)
        {
            const struct QuestObjective *objective = &stage->objectives[i];
            bool32 done = Quest_IsObjectiveDone(questId, i);
            u16 current, target;
            u8 *end;

            if (!Quest_IsObjectiveVisible(questId, i))
                continue;

            BlitWindowGfx(WIN_BODY, done ? GFX_CHECKBOX_DONE : GFX_CHECKBOX_EMPTY, 6, y + 3);
            end = StringCopy(gStringVar3, objective->text);
            if (objective->optional)
                end = StringCopy(end, sText_Optional);
            Print(WIN_BODY, FONT_SMALL, gStringVar3, 18, y, done ? COLOR_GRAY : COLOR_DARK);

            if (!done && Quest_GetConditionProgress(&objective->condition, &current, &target))
            {
                ConvertIntToDecimalStringN(gStringVar1, min(current, target), STR_CONV_MODE_LEFT_ALIGN, 5);
                ConvertIntToDecimalStringN(gStringVar2, target, STR_CONV_MODE_LEFT_ALIGN, 5);
                StringExpandPlaceholders(gStringVar3, COMPOUND_STRING("{STR_VAR_1}/{STR_VAR_2}"));
                Print(WIN_BODY, FONT_SMALL, gStringVar3, 228 - GetStringWidth(FONT_SMALL, gStringVar3, 0), y, COLOR_BLUE);
            }
            y += BODY_LINE_HEIGHT;
        }
    }
    else
    {
        PrintWrapped(WIN_BODY, FONT_SMALL, quest->summary, 4, y, 232, COLOR_DARK);
    }

    DrawRewards(questId, 72);
}

static void DrawJournalBody(u32 questId)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    u32 y = 2;
    u32 shown = 0;

    sQuestLog->journalCount = 0;
    for (u32 i = 0; i < GetPassedStageCount(questId); i++)
    {
        if (quest->stages[i].title != NULL)
            sQuestLog->journalCount++;
    }

    if (sQuestLog->journalCount == 0)
    {
        Print(WIN_BODY, FONT_NORMAL, sText_NoJournal, 4, y, COLOR_DARK);
        return;
    }

    if (sQuestLog->journalScroll >= sQuestLog->journalCount)
        sQuestLog->journalScroll = sQuestLog->journalCount - 1;

    for (u32 i = 0; i < GetPassedStageCount(questId) && y < 96; i++)
    {
        const struct QuestStage *stage = &quest->stages[i];

        if (stage->title == NULL)
            continue;
        if (shown++ < sQuestLog->journalScroll)
            continue;

        Print(WIN_BODY, FONT_SMALL, stage->title, 4, y, COLOR_BLUE);
        y += BODY_LINE_HEIGHT;
        y += PrintWrapped(WIN_BODY, FONT_SMALL, stage->journal, 4, y, 232, COLOR_DARK) * BODY_LINE_HEIGHT + 4;
    }
}

static void DrawDetail(void)
{
    u32 questId = sQuestLog->list[sQuestLog->cursor];

    DestroyDetailIcons();
    Quest_ClearUnread(questId);
    DrawHeader(questId);

    FillWindowPixelBuffer(WIN_BODY, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    if (sQuestLog->page == PAGE_JOURNAL)
    {
        DrawJournalBody(questId);
        SetScrollArrows(&sQuestLog->journalScroll, sQuestLog->journalCount ? sQuestLog->journalCount - 1 : 0, BODY_WINDOW_Y + 4, BODY_WINDOW_Y + 92);
    }
    else
    {
        RemoveScrollArrows();
        DrawObjectivesBody(questId);
    }
    CopyWindowToVram(WIN_BODY, COPYWIN_FULL);
}

static void ShowDetailPage(u32 page)
{
    sQuestLog->page = page;
    CopyToBgTilemapBuffer(1, page == PAGE_JOURNAL ? sDetailTilemap_Journal : sDetailTilemap_Objectives, 0, 0);
    ScheduleBgCopyTilemapToVram(1);
    SwitchWindows(FALSE);
    DrawDetail();
    DrawFooter(page == PAGE_JOURNAL ? sText_FooterJournal : sText_FooterObjectives);
}

static void Task_QuestLogDetailInput(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        RemoveScrollArrows();
        BuildList();
        ShowListPage();
        gTasks[taskId].func = Task_QuestLogListInput;
    }
    else if (JOY_NEW(DPAD_LEFT) || JOY_NEW(DPAD_RIGHT))
    {
        PlaySE(SE_SELECT);
        sQuestLog->journalScroll = 0;
        ShowDetailPage(sQuestLog->page == PAGE_JOURNAL ? PAGE_OBJECTIVES : PAGE_JOURNAL);
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        ToggleTracked(sQuestLog->list[sQuestLog->cursor]);
        DrawDetail();
    }
    else if (sQuestLog->page == PAGE_JOURNAL)
    {
        if (JOY_REPEAT(DPAD_UP) && sQuestLog->journalScroll > 0)
        {
            PlaySE(SE_SELECT);
            sQuestLog->journalScroll--;
            DrawDetail();
        }
        else if (JOY_REPEAT(DPAD_DOWN) && sQuestLog->journalScroll + 1 < sQuestLog->journalCount)
        {
            PlaySE(SE_SELECT);
            sQuestLog->journalScroll++;
            DrawDetail();
        }
    }
    else
    {
        if (JOY_REPEAT(DPAD_UP) && sQuestLog->cursor > 0)
        {
            PlaySE(SE_SELECT);
            sQuestLog->cursor--;
            DrawDetail();
        }
        else if (JOY_REPEAT(DPAD_DOWN) && sQuestLog->cursor + 1 < sQuestLog->listCount)
        {
            PlaySE(SE_SELECT);
            sQuestLog->cursor++;
            DrawDetail();
        }
    }
}
