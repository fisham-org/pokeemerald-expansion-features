#include "global.h"
#include "bg.h"
#include "event_object_movement.h"
#include "field_effect.h"
#include "gpu_regs.h"
#include "item_icon.h"
#include "line_break.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "palette.h"
#include "pokemon_icon.h"
#include "quest.h"
#include "quest_log.h"
#include "region_map.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/items.h"
#include "constants/region_map_sections.h"
#include "constants/rgb.h"
#include "constants/songs.h"

/*
 * Quest log: a notebook with two pages. The list page shows the quests of one category (L/R switch
 * between Quests and Tasks, START cycles a status filter). A turns to the detail page for the quest
 * under the cursor, where L/R page through the list and Up/Down scroll the details.
 *
 * Graphics live in graphics/quest_log/. Rebuild the background with tools/quests/quest_log_bg.py
 * from graphics/quest_log/screens/.
 *   bg_tiles.png           BG1 tiles, palette loaded into BG palette 0
 *   page_list.bin          list page tilemap
 *   page_detail.bin        detail page tilemap, with the sticky note behind the questgiver sprite
 *   page_row*.bin          row containers stamped behind each filled list row
 *   window_gfx.png         checkboxes and row markers, blitted into text windows.
 *                          Its palette is loaded into BG palette 15 and is also the text palette
 *                          (see TEXT_* below).
 *   unread.png             start menu unread mark, blitted into the start menu window
 */

#define LIST_ROWS           7   // must match LIST_HEIGHT / row height in quest_log_bg.py
#define LIST_ROW_HEIGHT     16
#define LIST_SKIP           5   // entries moved by Left/Right on the list
#define LIST_Y              16  // screen y of the first row; must match LIST_Y in quest_log_bg.py
#define LIST_CURSOR_X       3   // from the list window's left edge
#define LIST_NAME_X         11
#define LIST_AREA_RIGHT     195
#define LIST_MARKER_X       202
#define LIST_ARROWS_X       140
#define ROW_SEE_THROUGH     0xFFFF // row tilemap entry that keeps the background tile

#define MARQUEE_WIDTH       256 // WIN_MARQUEE width in pixels; longer names scroll only this far
#define MARQUEE_DELAY       60  // frames a cut-off name waits before scrolling, and at its end before starting over
#define MARQUEE_STEP_FRAMES 2   // frames per pixel of scrolling

#define TITLE_X             18  // the title is centred between the < > arrows in this span
#define TITLE_WIDTH         180
#define COUNTER_RIGHT       212
#define TRACKED_X           216

#define ICON_X              32  // questgiver sprite centre, on the sticky note
#define ICON_Y              34
#define OBJECT_ICON_Y_SHIFT 2   // overworld sprites fill the lower part of their 32 px frame

#define LINE_HEIGHT         14
#define THIN_LINE_HEIGHT    10  // FONT_SMALL_NARROWER, used where space is tight: list locations and the summary
#define DIVIDER_HEIGHT      6
#define BODY_WINDOW_Y       56
#define BODY_WIDTH          212
#define BODY_HEIGHT         88
#define GIVER_LINE_HEIGHT   12
#define GLYPH_Y             4   // 8x8 checkboxes and markers line up with capitals at this offset from the text y
#define CHECKBOX_X          2
#define OBJECTIVE_X         13
#define SUB_INDENT          11
#define BODY_ARROWS_X       226

#define TAG_LIST_ARROWS     0x5B30
#define TAG_BODY_ARROWS     0x5B31
#define TAG_ITEM_ICON       0x5B40

// Text palette indices, from window_gfx.png
#define TEXT_PAPER          1
#define TEXT_INK            2
#define TEXT_SHADE          3
#define TEXT_RED            4
#define TEXT_FADED          5
#define TEXT_BLUE           8
#define TEXT_LIGHT_BLUE     9

enum QuestLogFilter
{
    FILTER_ALL,
    FILTER_LEADS,
    FILTER_ACTIVE,
    FILTER_DONE,
    FILTER_COUNT,
};

// The list windows and the detail windows are never shown together, so their tile blocks overlap
enum QuestLogWindow
{
    WIN_FOOTER,
    WIN_TABS,
    WIN_LIST,
    WIN_TITLE,
    WIN_GIVER,
    WIN_BODY,
    WIN_MARQUEE,    // off screen: the selected row's name, drawn in full and cut off, for scrolling
    WIN_COUNT,
};

enum QuestLogTextColor
{
    COLOR_INK,
    COLOR_INK_SELECTED, // on the selected row's highlight
    COLOR_BLUE,
    COLOR_FADED,
    COLOR_RED,
};

enum WindowGfxRect
{
    GFX_CHECKBOX_EMPTY,
    GFX_CHECKBOX_DONE,
    GFX_MARKER_NEW,
    GFX_MARKER_TRACKED,
    GFX_MARKER_TURN_IN,
    GFX_NONE,
};

struct ScrollArrows
{
    u8 taskId;
    u8 max;
};

enum MarqueePhase
{
    MARQUEE_OFF,
    MARQUEE_WAIT,   // the cut-off name is shown
    MARQUEE_SCROLL,
    MARQUEE_END,    // the end of the name is shown
};

// The selected row's name, when it is cut off
struct Marquee
{
    u8 phase;
    u8 timer;
    u16 x;          // name area in WIN_LIST
    u16 y;
    u16 width;
    u16 offset;     // pixels scrolled
    u16 maxOffset;
};

struct QuestLogState
{
    MainCallback savedCallback;
    u8 loadState;
    u8 tab;             // QUEST_CATEGORY_*
    u8 filter;
    u8 icon;
    bool8 deferCopies;  // set while a page switch draws; see SwitchPage
    u16 listCount;
    u16 cursor;
    u16 scroll;         // first visible list row
    u16 bodyScroll;     // detail body scroll, in lines
    u16 bodyMaxScroll;
    struct ScrollArrows listArrows;
    struct ScrollArrows bodyArrows;
    struct Marquee marquee;
    u8 list[QUEST_MAX];
    u8 text[1000];      // word wrap buffer, sized like gStringVar4
};

// Lays out the detail body so it can scroll. Rows are only drawn when they fit entirely
// inside the visible area.
struct BodyWriter
{
    s32 y;          // content y of the next row
    s32 top;        // content y at the top of the visible area
    bool32 draw;    // FALSE while measuring
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

static const struct WindowTemplate sQuestLogWindowTemplates[] =
{
    [WIN_FOOTER] = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 18, .width = 27, .height = 2,  .paletteNum = 15, .baseBlock = 1 },
    [WIN_TABS]   = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 0,  .width = 27, .height = 2,  .paletteNum = 15, .baseBlock = 55 },
    [WIN_LIST]   = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 2,  .width = 27, .height = 14, .paletteNum = 15, .baseBlock = 109 },
    [WIN_TITLE]  = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0,  .width = 30, .height = 2,  .paletteNum = 15, .baseBlock = 55 },
    [WIN_GIVER]  = { .bg = 0, .tilemapLeft = 6, .tilemapTop = 2,  .width = 23, .height = 5,  .paletteNum = 15, .baseBlock = 115 },
    [WIN_BODY]   = { .bg = 0, .tilemapLeft = 2, .tilemapTop = 7,  .width = 27, .height = 11, .paletteNum = 15, .baseBlock = 230 },
    // Never put on the tilemap or copied to VRAM; rows 0-1 hold the full name, rows 2-3 the cut-off name
    [WIN_MARQUEE] = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = MARQUEE_WIDTH / 8, .height = 4, .paletteNum = 15, .baseBlock = 527 },
    DUMMY_WIN_TEMPLATE
};

static const u32 sBgTiles[] = INCGFX_U32("graphics/quest_log/bg_tiles.png", ".4bpp.smol");
static const u16 sBgPalette[] = INCGFX_U16("graphics/quest_log/bg_tiles.png", ".gbapal");
static const u32 sListTilemap[] = INCGFX_U32("graphics/quest_log/page_list.bin", ".smolTM");
static const u32 sDetailTilemap[] = INCGFX_U32("graphics/quest_log/page_detail.bin", ".smolTM");
// Row containers stamped over the list page behind each filled row. Uncompressed, 30 tiles wide,
// one block per row position because each is pre-blended with that spot of the background.
static const u16 sRowTilemap[] = INCBIN_U16("graphics/quest_log/page_row.bin");
static const u16 sRowTilemap_Selected[] = INCBIN_U16("graphics/quest_log/page_row_selected.bin");
static const u8 sWindowGfx[] = INCGFX_U8("graphics/quest_log/window_gfx.png", ".4bpp");
static const u16 sWindowPalette[] = INCGFX_U16("graphics/quest_log/window_gfx.png", ".gbapal");
static const u8 sUnreadGfx[] = INCGFX_U8("graphics/quest_log/unread.png", ".4bpp");

#define WINDOW_GFX_WIDTH  32
#define WINDOW_GFX_HEIGHT 56

static const struct { u8 x, y, width, height; } sWindowGfxRects[] =
{
    [GFX_CHECKBOX_EMPTY] = { 0,  0,  8, 8 },
    [GFX_CHECKBOX_DONE]  = { 8,  0,  8, 8 },
    [GFX_MARKER_NEW]     = { 16, 0,  8, 8 },
    [GFX_MARKER_TRACKED] = { 24, 0,  8, 8 },
    [GFX_MARKER_TURN_IN] = { 16, 40, 8, 8 },
};

static const u8 sTextColors[][3] =
{
    [COLOR_INK]          = {TEXT_COLOR_TRANSPARENT, TEXT_INK,   TEXT_SHADE},
    [COLOR_INK_SELECTED] = {TEXT_COLOR_TRANSPARENT, TEXT_INK,   TEXT_PAPER},
    [COLOR_BLUE]         = {TEXT_COLOR_TRANSPARENT, TEXT_BLUE,  TEXT_LIGHT_BLUE},
    [COLOR_FADED]        = {TEXT_COLOR_TRANSPARENT, TEXT_FADED, TEXT_COLOR_TRANSPARENT},
    [COLOR_RED]          = {TEXT_COLOR_TRANSPARENT, TEXT_RED,   TEXT_COLOR_TRANSPARENT},
};

static const u8 *const sTabNames[QUEST_CATEGORY_COUNT] =
{
    [QUEST_CATEGORY_MAIN] = COMPOUND_STRING("QUESTS"),
    [QUEST_CATEGORY_SIDE] = COMPOUND_STRING("TASKS"),
};

static const u8 *const sCategoryNames[QUEST_CATEGORY_COUNT] =
{
    [QUEST_CATEGORY_MAIN] = COMPOUND_STRING("Quest"),
    [QUEST_CATEGORY_SIDE] = COMPOUND_STRING("Task"),
};

static const u8 *const sStatusNames[QUEST_STATUS_COUNT] =
{
    [QUEST_STATUS_HIDDEN]    = COMPOUND_STRING(""),
    [QUEST_STATUS_AVAILABLE] = COMPOUND_STRING("Not started"),
    [QUEST_STATUS_ACTIVE]    = COMPOUND_STRING("Active"),
    [QUEST_STATUS_COMPLETE]  = COMPOUND_STRING("Complete"),
    [QUEST_STATUS_CLOSED]    = COMPOUND_STRING("Closed"),
};

static const u8 *const sFilterNames[FILTER_COUNT] =
{
    [FILTER_ALL]    = COMPOUND_STRING("All"),
    [FILTER_LEADS]  = COMPOUND_STRING("Leads"),
    [FILTER_ACTIVE] = COMPOUND_STRING("Active"),
    [FILTER_DONE]   = COMPOUND_STRING("Done"),
};

static const u8 sText_Empty[] = _("Nothing here yet.");
static const u8 sText_Objectives[] = _("Objectives");
static const u8 sText_Optional[] = _(" (optional)");
static const u8 sText_CategoryStatus[] = _("{STR_VAR_1} - {STR_VAR_2}");
static const u8 sText_Counter[] = _("{STR_VAR_1}/{STR_VAR_2}");
static const u8 sText_FooterList[] = _("{A_BUTTON} Open  {SELECT_BUTTON} Track  {B_BUTTON} Exit");
static const u8 sText_FooterDetail[] = _("{L_BUTTON}{R_BUTTON} Quest  {SELECT_BUTTON} Track  {B_BUTTON} Back");

static void QuestLog_SetupCB(void);
static void QuestLog_MainCB(void);
static void QuestLog_VBlankCB(void);
static void Task_QuestLogWaitFadeIn(u8 taskId);
static void Task_QuestLogListInput(u8 taskId);
static void Task_QuestLogDetailInput(u8 taskId);
static void Task_QuestLogCopyPage(u8 taskId);
static void Task_QuestLogWaitFadeAndExit(u8 taskId);
static bool8 QuestLog_InitBgs(void);
static bool8 QuestLog_LoadGraphics(void);
static void QuestLog_InitWindows(void);
static void QuestLog_FreeResources(void);
static void BuildList(void);
static void ShowListPage(void);
static void ShowDetailPage(void);
static void DestroyIcon(void);
static void RemoveScrollArrows(struct ScrollArrows *arrows);

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
    sQuestLog->listArrows.taskId = TASK_NONE;
    sQuestLog->bodyArrows.taskId = TASK_NONE;
    sQuestLog->icon = SPRITE_NONE;
    SetMainCallback2(QuestLog_SetupCB);
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
    for (u32 i = 0; i < WIN_COUNT; i++)
        FillWindowPixelBuffer(i, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    PutWindowTilemap(WIN_FOOTER);
    ScheduleBgCopyTilemapToVram(0);
}

static void QuestLog_FreeResources(void)
{
    if (sQuestLog != NULL)
    {
        RemoveScrollArrows(&sQuestLog->listArrows);
        RemoveScrollArrows(&sQuestLog->bodyArrows);
    }
    TRY_FREE_AND_SET_NULL(sQuestLog);
    TRY_FREE_AND_SET_NULL(sBg1TilemapBuffer);
    FreeAllWindowBuffers();
    ResetSpriteData();
}

// *******************************
// Helpers

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

static void PrintRightAligned(u32 windowId, u32 fontId, const u8 *str, u32 right, u32 y, u32 color)
{
    Print(windowId, fontId, str, right - GetStringWidth(fontId, str, 0), y, color);
}

static bool32 IsLineEnd(u8 c)
{
    return c == EOS || c == CHAR_NEWLINE || c == CHAR_PROMPT_SCROLL || c == CHAR_PROMPT_CLEAR;
}

// Word-wraps str to width into the text buffer. Returns the buffer.
static u8 *WrapText(const u8 *str, u32 fontId, u32 width)
{
    StringCopy(sQuestLog->text, str);
    BreakStringAutomatic(sQuestLog->text, width, 0xFF, fontId, HIDE_SCROLL_PROMPT);
    return sQuestLog->text;
}

// Copies one line from *src into dest and moves *src to the start of the next line. Returns FALSE after the last line.
static bool32 NextLine(const u8 **src, u8 *dest, u32 size)
{
    u32 n = 0;

    while (!IsLineEnd(**src) && n < size - 1)
        dest[n++] = *(*src)++;
    dest[n] = EOS;
    if (**src == EOS)
        return FALSE;
    (*src)++;
    return TRUE;
}

// Copies the first line of str, word-wrapped to width, into dest. Adds "…" if the text goes on.
static void CopyFirstLine(u8 *dest, const u8 *str, u32 fontId, u32 width)
{
    const u8 *src = WrapText(str, fontId, width - GetStringWidth(fontId, COMPOUND_STRING("…"), 0));
    u32 i;

    for (i = 0; !IsLineEnd(src[i]); i++)
        dest[i] = src[i];
    if (src[i] != EOS)
        dest[i++] = CHAR_ELLIPSIS;
    dest[i] = EOS;
}

static bool32 IsFinished(u32 questId)
{
    u32 status = Quest_GetStatus(questId);
    return status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_CLOSED;
}

static u32 GetMarker(u32 questId)
{
    if (IsFinished(questId))
        return GFX_CHECKBOX_DONE;
    if (Quest_IsUnread(questId))
        return GFX_MARKER_NEW;
    if (Quest_IsTurnInReady(questId))
        return GFX_MARKER_TURN_IN;
    if (Quest_GetTracked() == questId)
        return GFX_MARKER_TRACKED;
    return GFX_NONE;
}

// x and y are the centre of the icon. Item icons are 24x24 in the top-left of a 32x32 sprite.
static u8 CreateIcon(u32 iconType, u32 icon, s16 x, s16 y)
{
    u8 spriteId = SPRITE_NONE;

    switch (iconType)
    {
    case QUEST_ICON_OBJECT:
        y -= OBJECT_ICON_Y_SHIFT;
        spriteId = CreateObjectGraphicsSprite(icon, SpriteCallbackDummy, x, y, 0);
        break;
    case QUEST_ICON_ITEM:
        if (icon == ITEM_NONE)
            return SPRITE_NONE;
        spriteId = AddItemIconSprite(TAG_ITEM_ICON, TAG_ITEM_ICON, icon);
        x += 4;
        y += 4;
        break;
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

static void DestroyIcon(void)
{
    struct Sprite *sprite;
    u32 paletteNum;
    bool32 usingSheet;
    u16 tileStart;

    if (sQuestLog->icon == SPRITE_NONE)
        return;

    sprite = &gSprites[sQuestLog->icon];
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
    sQuestLog->icon = SPRITE_NONE;
}

static void RemoveScrollArrows(struct ScrollArrows *arrows)
{
    if (arrows->taskId != TASK_NONE)
    {
        RemoveScrollIndicatorArrowPair(arrows->taskId);
        arrows->taskId = TASK_NONE;
    }
}

// Keeps a pair of arrows on scroll, recreating it only when the scroll range changes
static void SetScrollArrows(struct ScrollArrows *arrows, u16 *scroll, u32 maxScroll, u32 x, u32 topY, u32 bottomY, u32 tag)
{
    if (arrows->taskId != TASK_NONE && arrows->max == maxScroll)
        return;

    RemoveScrollArrows(arrows);
    if (maxScroll > 0)
    {
        arrows->taskId = AddScrollIndicatorArrowPairParameterized(SCROLL_ARROW_UP, x, topY, bottomY, maxScroll, tag, tag, scroll);
        arrows->max = maxScroll;
    }
}

static void CopyWindowGfx(u32 windowId)
{
    if (!sQuestLog->deferCopies)
        CopyWindowToVram(windowId, COPYWIN_GFX);
}

static void DrawFooter(const u8 *text)
{
    FillWindowPixelBuffer(WIN_FOOTER, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    Print(WIN_FOOTER, FONT_NORMAL, text, 2, 2, COLOR_INK);
    CopyWindowGfx(WIN_FOOTER);
}

static void SwitchWindows(bool32 showList)
{
    if (showList)
    {
        ClearWindowTilemap(WIN_TITLE);
        ClearWindowTilemap(WIN_GIVER);
        ClearWindowTilemap(WIN_BODY);
        PutWindowTilemap(WIN_TABS);
        PutWindowTilemap(WIN_LIST);
    }
    else
    {
        ClearWindowTilemap(WIN_TABS);
        ClearWindowTilemap(WIN_LIST);
        PutWindowTilemap(WIN_TITLE);
        PutWindowTilemap(WIN_GIVER);
        PutWindowTilemap(WIN_BODY);
    }
    ScheduleBgCopyTilemapToVram(0);
}

// *******************************
// List page

static bool32 QuestMatchesFilter(u32 questId)
{
    u32 status;

    if (!Quest_IsValid(questId) || Quest_IsTask(questId) || Quest_GetInfo(questId)->category != sQuestLog->tab)
        return FALSE;

    status = Quest_GetStatus(questId);
    switch (sQuestLog->filter)
    {
    case FILTER_LEADS:
        return status == QUEST_STATUS_AVAILABLE;
    case FILTER_ACTIVE:
        return status == QUEST_STATUS_ACTIVE;
    case FILTER_DONE:
        return IsFinished(questId);
    default:
        return status != QUEST_STATUS_HIDDEN;
    }
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

static void AddToList(u32 questId)
{
    sQuestLog->list[sQuestLog->listCount++] = questId;
}

// The tracked quest first, then unfinished quests, then finished ones, each by id.
// Tasks (kind "task") are shown under their parent quest's objectives, not in the list.
static void BuildList(void)
{
    u32 tracked = Quest_GetTracked();

    sQuestLog->listCount = 0;
    if (tracked != QUEST_NONE && QuestMatchesFilter(tracked))
        AddToList(tracked);
    for (u32 finished = FALSE; finished <= TRUE; finished++)
    {
        for (u32 i = 0; i < QUEST_COUNT; i++)
        {
            if (i != tracked && QuestMatchesFilter(i) && IsFinished(i) == finished)
                AddToList(i);
        }
    }
    ClampListCursor();
}

// L, the tab names and R on the left; the filter on the right
static void DrawTabs(void)
{
    static const u8 sText_L[] = _("{L_BUTTON}");
    static const u8 sText_R[] = _("{R_BUTTON}");
    u32 x;

    FillWindowPixelBuffer(WIN_TABS, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    Print(WIN_TABS, FONT_NORMAL, sText_L, 2, 0, COLOR_INK);
    x = 2 + GetStringWidth(FONT_NORMAL, sText_L, 0) + 4;
    for (u32 i = 0; i < QUEST_CATEGORY_COUNT; i++)
    {
        Print(WIN_TABS, FONT_NORMAL, sTabNames[i], x, 0, i == sQuestLog->tab ? COLOR_BLUE : COLOR_FADED);
        x += GetStringWidth(FONT_NORMAL, sTabNames[i], 0) + 6;
    }
    Print(WIN_TABS, FONT_NORMAL, sText_R, x - 2, 0, COLOR_INK);

    StringCopy(gStringVar1, sFilterNames[sQuestLog->filter]);
    StringExpandPlaceholders(gStringVar4, COMPOUND_STRING("{START_BUTTON} {STR_VAR_1}"));
    PrintRightAligned(WIN_TABS, FONT_NORMAL, gStringVar4, 209, 0, COLOR_INK);
    CopyWindowGfx(WIN_TABS);
}

// Redraws the list page, then stamps a row container behind each filled row
// (the highlighted variant on the cursor's row). See-through cells keep the background.
static void DrawRowContainers(void)
{
    u16 *tilemap = (u16 *)sBg1TilemapBuffer;
    u32 rowTiles = LIST_ROW_HEIGHT / 8;

    CopyToBgTilemapBuffer(1, sListTilemap, 0, 0);
    for (u32 row = 0; row < LIST_ROWS && sQuestLog->scroll + row < sQuestLog->listCount; row++)
    {
        const u16 *src = (sQuestLog->scroll + row == sQuestLog->cursor ? sRowTilemap_Selected : sRowTilemap)
                       + row * rowTiles * 30;
        for (u32 ty = 0; ty < rowTiles; ty++)
        {
            for (u32 tx = 0; tx < 30; tx++)
            {
                u16 entry = src[ty * 30 + tx];
                if (entry != ROW_SEE_THROUGH)
                    tilemap[(LIST_Y / 8 + row * rowTiles + ty) * 32 + tx] = entry;
            }
        }
    }
    ScheduleBgCopyTilemapToVram(1);
}

// A cut-off name on the selected row waits, scrolls left until its end shows, waits again, then starts over.
// Both versions of the name are drawn into WIN_MARQUEE once; each step blits one of them into the list.
static void StartMarquee(const u8 *name, const u8 *cutName, u32 x, u32 y, u32 width, u32 color)
{
    struct Marquee *marquee = &sQuestLog->marquee;
    u32 fullWidth = min(GetStringWidth(FONT_NORMAL, name, 0), MARQUEE_WIDTH);

    if (fullWidth <= width)
        return;
    FillWindowPixelBuffer(WIN_MARQUEE, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    Print(WIN_MARQUEE, FONT_NORMAL, name, 0, 0, color);
    Print(WIN_MARQUEE, FONT_NORMAL, cutName, 0, LIST_ROW_HEIGHT, color);
    marquee->phase = MARQUEE_WAIT;
    marquee->timer = 0;
    marquee->x = x;
    marquee->y = y;
    marquee->width = width;
    marquee->offset = 0;
    marquee->maxOffset = fullWidth - width;
}

static void DrawMarquee(bool32 cut)
{
    struct Marquee *marquee = &sQuestLog->marquee;

    FillWindowPixelRect(WIN_LIST, PIXEL_FILL(TEXT_COLOR_TRANSPARENT), marquee->x, marquee->y, marquee->width, LIST_ROW_HEIGHT);
    BlitBitmapRectToWindow(WIN_LIST, gWindows[WIN_MARQUEE].tileData,
                           cut ? 0 : marquee->offset, cut ? LIST_ROW_HEIGHT : 0, MARQUEE_WIDTH, LIST_ROW_HEIGHT * 2,
                           marquee->x, marquee->y, marquee->width, LIST_ROW_HEIGHT);
    CopyWindowRectToVram(WIN_LIST, COPYWIN_GFX, marquee->x / 8, marquee->y / 8,
                         (marquee->x + marquee->width + 7) / 8 - marquee->x / 8, LIST_ROW_HEIGHT / 8);
}

static void UpdateMarquee(void)
{
    struct Marquee *marquee = &sQuestLog->marquee;

    switch (marquee->phase)
    {
    case MARQUEE_WAIT:
        if (++marquee->timer >= MARQUEE_DELAY)
        {
            marquee->timer = 0;
            marquee->phase = MARQUEE_SCROLL;
        }
        break;
    case MARQUEE_SCROLL:
        if (++marquee->timer >= MARQUEE_STEP_FRAMES)
        {
            marquee->timer = 0;
            DrawMarquee(FALSE);
            if (++marquee->offset > marquee->maxOffset)
            {
                marquee->offset = 0;
                marquee->phase = MARQUEE_END;
            }
        }
        break;
    case MARQUEE_END:
        if (++marquee->timer >= MARQUEE_DELAY)
        {
            marquee->timer = 0;
            marquee->phase = MARQUEE_WAIT;
            DrawMarquee(TRUE);
        }
        break;
    }
}

static void DrawList(void)
{
    DrawRowContainers();
    FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    sQuestLog->marquee.phase = MARQUEE_OFF;

    if (sQuestLog->listCount == 0)
        Print(WIN_LIST, FONT_NORMAL, sText_Empty, (BODY_WIDTH - GetStringWidth(FONT_NORMAL, sText_Empty, 0)) / 2, 48, COLOR_FADED);

    for (u32 row = 0; row < LIST_ROWS && sQuestLog->scroll + row < sQuestLog->listCount; row++)
    {
        u32 questId = sQuestLog->list[sQuestLog->scroll + row];
        bool32 selected = sQuestLog->scroll + row == sQuestLog->cursor;
        bool32 finished = IsFinished(questId);
        u32 y = row * LIST_ROW_HEIGHT;
        u32 marker = GetMarker(questId);
        u32 nameColor = finished ? COLOR_FADED : (selected ? COLOR_INK_SELECTED : COLOR_INK);
        u32 areaWidth, nameWidth;

        if (selected)
            Print(WIN_LIST, FONT_NORMAL, COMPOUND_STRING(">"), LIST_CURSOR_X, y, COLOR_RED);

        CopyQuestAreaName(gStringVar2, questId);
        areaWidth = GetStringWidth(FONT_SMALL_NARROWER, gStringVar2, 0);
        Print(WIN_LIST, FONT_SMALL_NARROWER, gStringVar2, LIST_AREA_RIGHT - areaWidth, y,
              finished ? COLOR_FADED : (selected ? COLOR_INK_SELECTED : COLOR_BLUE));

        nameWidth = LIST_AREA_RIGHT - areaWidth - 6 - LIST_NAME_X;
        CopyFirstLine(gStringVar1, Quest_GetInfo(questId)->name, FONT_NORMAL, nameWidth);
        Print(WIN_LIST, FONT_NORMAL, gStringVar1, LIST_NAME_X, y, nameColor);
        if (selected)
            StartMarquee(Quest_GetInfo(questId)->name, gStringVar1, LIST_NAME_X, y, nameWidth, nameColor);

        if (marker != GFX_NONE)
            BlitWindowGfx(WIN_LIST, marker, LIST_MARKER_X, y + GLYPH_Y);
    }

    CopyWindowGfx(WIN_LIST);
    SetScrollArrows(&sQuestLog->listArrows, &sQuestLog->scroll,
                    sQuestLog->listCount > LIST_ROWS ? sQuestLog->listCount - LIST_ROWS : 0,
                    LIST_ARROWS_X, LIST_Y - 8, LIST_Y + LIST_ROWS * LIST_ROW_HEIGHT + 8, TAG_LIST_ARROWS);
}

static void ShowListPage(void)
{
    RemoveScrollArrows(&sQuestLog->bodyArrows);
    SwitchWindows(TRUE);
    DrawTabs();
    DrawList();
    DrawFooter(sText_FooterList);
}

// *******************************
// Detail page

// Quest name centred between < and >, the position in the list, and the tracked marker
static void DrawTitle(u32 questId)
{
    u32 width, x;

    FillWindowPixelBuffer(WIN_TITLE, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    CopyFirstLine(gStringVar1, Quest_GetInfo(questId)->name, FONT_NORMAL, TITLE_WIDTH - 20);
    width = GetStringWidth(FONT_NORMAL, gStringVar1, 0);
    x = TITLE_X + (TITLE_WIDTH - width) / 2;
    Print(WIN_TITLE, FONT_NORMAL, gStringVar1, x, 0, COLOR_INK);
    if (sQuestLog->cursor > 0)
        Print(WIN_TITLE, FONT_NORMAL, COMPOUND_STRING("<"), x - 9, 0, COLOR_RED);
    if (sQuestLog->cursor + 1 < sQuestLog->listCount)
        Print(WIN_TITLE, FONT_NORMAL, COMPOUND_STRING(">"), x + width + 4, 0, COLOR_RED);

    ConvertIntToDecimalStringN(gStringVar1, sQuestLog->cursor + 1, STR_CONV_MODE_LEFT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar2, sQuestLog->listCount, STR_CONV_MODE_LEFT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, sText_Counter);
    PrintRightAligned(WIN_TITLE, FONT_NORMAL, gStringVar4, COUNTER_RIGHT, 0, COLOR_FADED);
    if (Quest_GetTracked() == questId)
        BlitWindowGfx(WIN_TITLE, GFX_MARKER_TRACKED, TRACKED_X, GLYPH_Y);
    CopyWindowGfx(WIN_TITLE);
}

// Beside the sticky note: giver name, location, category and status. Quests without a giver name show none.
static void DrawGiver(u32 questId)
{
    const struct Quest *quest = Quest_GetInfo(questId);
    u32 width = sQuestLogWindowTemplates[WIN_GIVER].width * 8 - 8;

    FillWindowPixelBuffer(WIN_GIVER, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    if (quest->giverName != NULL)
    {
        CopyFirstLine(gStringVar1, quest->giverName, FONT_NORMAL, width);
        Print(WIN_GIVER, FONT_NORMAL, gStringVar1, 5, 0, COLOR_BLUE);
    }
    CopyQuestAreaName(gStringVar2, questId);
    Print(WIN_GIVER, FONT_NORMAL, gStringVar2, 5, GIVER_LINE_HEIGHT, COLOR_INK);

    StringCopy(gStringVar1, sCategoryNames[quest->category]);
    StringCopy(gStringVar2, sStatusNames[Quest_GetStatus(questId)]);
    StringExpandPlaceholders(gStringVar4, sText_CategoryStatus);
    Print(WIN_GIVER, FONT_NORMAL, gStringVar4, 5, GIVER_LINE_HEIGHT * 2, COLOR_FADED);
    CopyWindowGfx(WIN_GIVER);

    sQuestLog->icon = CreateIcon(quest->iconType, quest->icon, ICON_X, ICON_Y);
}

static void Writer_Init(struct BodyWriter *writer, bool32 draw)
{
    writer->y = 0;
    writer->top = sQuestLog->bodyScroll * LINE_HEIGHT;
    writer->draw = draw;
}

// Reserves height pixels. Returns TRUE and the window y if they should be drawn.
static bool32 Writer_Row(struct BodyWriter *writer, u32 height, u32 *windowY)
{
    bool32 visible = writer->draw && writer->y >= writer->top && writer->y + (s32)height <= writer->top + BODY_HEIGHT;
    *windowY = writer->y - writer->top;
    writer->y += height;
    return visible;
}

static void Writer_Divider(struct BodyWriter *writer)
{
    u32 windowY;

    if (Writer_Row(writer, DIVIDER_HEIGHT, &windowY))
        FillWindowPixelRect(WIN_BODY, PIXEL_FILL(TEXT_SHADE), 1, windowY + DIVIDER_HEIGHT / 2 - 1, BODY_WIDTH, 1);
}

// Word-wrapped text with an optional checkbox before the first line and a right-aligned progress count
static void Writer_Text(struct BodyWriter *writer, u32 fontId, const u8 *str, u32 x, u32 checkbox, const u8 *progress, u32 color)
{
    u32 lineHeight = fontId == FONT_SMALL_NARROWER ? THIN_LINE_HEIGHT : LINE_HEIGHT;
    u32 progressWidth = progress != NULL ? GetStringWidth(FONT_NORMAL, progress, 0) + 4 : 0;
    const u8 *src = WrapText(str, fontId, BODY_WIDTH - x - progressWidth);
    u8 line[80];
    u32 windowY;
    bool32 more;

    do
    {
        more = NextLine(&src, line, sizeof(line));
        if (Writer_Row(writer, lineHeight, &windowY))
        {
            if (checkbox != GFX_NONE)
                BlitWindowGfx(WIN_BODY, checkbox, x - (OBJECTIVE_X - CHECKBOX_X), windowY + GLYPH_Y);
            if (progress != NULL)
                PrintRightAligned(WIN_BODY, FONT_NORMAL, progress, BODY_WIDTH, windowY, COLOR_BLUE);
            Print(WIN_BODY, fontId, line, x, windowY, color);
        }
        checkbox = GFX_NONE;
        progress = NULL;
    } while (more);
}

static void Writer_Objective(struct BodyWriter *writer, const struct QuestObjective *objective, bool32 done)
{
    u16 current, target;
    const u8 *progress = NULL;
    u8 *end = StringCopy(gStringVar3, objective->text);

    if (objective->optional)
        StringCopy(end, sText_Optional);
    if (!done && Quest_GetConditionProgress(&objective->condition, &current, &target))
    {
        ConvertIntToDecimalStringN(gStringVar1, min(current, target), STR_CONV_MODE_LEFT_ALIGN, 5);
        ConvertIntToDecimalStringN(gStringVar2, target, STR_CONV_MODE_LEFT_ALIGN, 5);
        StringExpandPlaceholders(gStringVar4, sText_Counter);
        progress = gStringVar4;
    }
    Writer_Text(writer, FONT_NORMAL, gStringVar3, OBJECTIVE_X, done ? GFX_CHECKBOX_DONE : GFX_CHECKBOX_EMPTY, progress, done ? COLOR_FADED : COLOR_INK);
}

// The quests a counted objective counts, indented under it. Undiscovered ones are left out.
static void Writer_CountedQuests(struct BodyWriter *writer, const struct QuestCondition *condition)
{
    for (u32 i = 0; i < condition->listCount; i++)
    {
        u32 questId = condition->list[i];
        bool32 done = Quest_GetStatus(questId) == QUEST_STATUS_COMPLETE;

        if (Quest_GetStatus(questId) == QUEST_STATUS_HIDDEN)
            continue;
        Writer_Text(writer, FONT_NORMAL, Quest_GetInfo(questId)->name, OBJECTIVE_X + SUB_INDENT,
                    done ? GFX_CHECKBOX_DONE : GFX_CHECKBOX_EMPTY, NULL, done ? COLOR_FADED : COLOR_INK);
    }
}

static void Writer_Objectives(struct BodyWriter *writer, u32 questId)
{
    const struct QuestStage *stage;

    switch (Quest_GetStatus(questId))
    {
    case QUEST_STATUS_ACTIVE:
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
        break;
    case QUEST_STATUS_COMPLETE:
        Writer_Text(writer, FONT_NORMAL, Quest_GetInfo(questId)->outcomes[Quest_GetOutcome(questId)].summary, 2, GFX_NONE, NULL, COLOR_INK);
        break;
    }
}

// The stage title while active, otherwise the status
static const u8 *GetObjectivesHeading(u32 questId)
{
    if (Quest_GetStatus(questId) == QUEST_STATUS_ACTIVE)
    {
        const u8 *title = Quest_GetCurrentStage(questId)->title;
        return title != NULL ? title : sText_Objectives;
    }
    return sStatusNames[Quest_GetStatus(questId)];
}

// Summary, a divider, the heading, then the objectives. They scroll together.
static void Writer_Body(struct BodyWriter *writer, u32 questId)
{
    Writer_Text(writer, FONT_SMALL_NARROWER, Quest_GetInfo(questId)->summary, 2, GFX_NONE, NULL, COLOR_INK);
    Writer_Divider(writer);
    Writer_Text(writer, FONT_NORMAL, GetObjectivesHeading(questId), 2, GFX_NONE, NULL, COLOR_BLUE);
    Writer_Objectives(writer, questId);
}

static void DrawBody(u32 questId)
{
    struct BodyWriter writer;
    s32 overflow;

    FillWindowPixelBuffer(WIN_BODY, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    Writer_Init(&writer, FALSE);
    Writer_Body(&writer, questId);
    overflow = writer.y - BODY_HEIGHT;
    sQuestLog->bodyMaxScroll = overflow > 0 ? (overflow + LINE_HEIGHT - 1) / LINE_HEIGHT : 0;
    if (sQuestLog->bodyScroll > sQuestLog->bodyMaxScroll)
        sQuestLog->bodyScroll = sQuestLog->bodyMaxScroll;

    Writer_Init(&writer, TRUE);
    Writer_Body(&writer, questId);
    CopyWindowGfx(WIN_BODY);
    SetScrollArrows(&sQuestLog->bodyArrows, &sQuestLog->bodyScroll, sQuestLog->bodyMaxScroll,
                    BODY_ARROWS_X, BODY_WINDOW_Y, BODY_WINDOW_Y + BODY_HEIGHT, TAG_BODY_ARROWS);
}

static void DrawDetail(void)
{
    u32 questId = sQuestLog->list[sQuestLog->cursor];

    DestroyIcon();
    Quest_ClearUnread(questId);
    DrawTitle(questId);
    DrawGiver(questId);
    DrawBody(questId);
}

static void ShowDetailPage(void)
{
    RemoveScrollArrows(&sQuestLog->listArrows);
    CopyToBgTilemapBuffer(1, sDetailTilemap, 0, 0);
    ScheduleBgCopyTilemapToVram(1);
    SwitchWindows(FALSE);
    sQuestLog->bodyScroll = 0;
    DrawDetail();
    DrawFooter(sText_FooterDetail);
}

// *******************************
// Input

// Returns TRUE if the list order may have changed
static bool32 ToggleTracked(void)
{
    u32 questId;

    if (sQuestLog->listCount == 0)
        return FALSE;
    questId = sQuestLog->list[sQuestLog->cursor];
    if (Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE)
    {
        PlaySE(SE_FAILURE);
        return FALSE;
    }
    if (Quest_GetTracked() == questId)
        Quest_SetTracked(QUEST_NONE);
    else
        Quest_SetTracked(questId);
    PlaySE(SE_SELECT);

    // The tracked quest sorts first; keep the cursor on the same quest
    BuildList();
    for (u32 i = 0; i < sQuestLog->listCount; i++)
    {
        if (sQuestLog->list[i] == questId)
            sQuestLog->cursor = i;
    }
    ClampListCursor();
    return TRUE;
}

static void MoveListCursor(u32 cursor)
{
    PlaySE(SE_SELECT);
    sQuestLog->cursor = cursor;
    ClampListCursor();
    DrawList();
}

static void ResetList(void)
{
    PlaySE(SE_SELECT);
    sQuestLog->cursor = 0;
    sQuestLog->scroll = 0;
    BuildList();
    DrawTabs();
    DrawList();
}

#define tShowDetail data[0]

// Drawing a page can run past a VBlank, and the list and detail windows share VRAM tiles, so copying
// each window as it is drawn would briefly show the new text on the old page's tilemap. Instead the
// page is drawn with its VRAM copies held back, then copied in one go at the start of the next frame.
static void SwitchPage(u8 taskId, bool32 showDetail)
{
    sQuestLog->deferCopies = TRUE;
    if (showDetail)
        ShowDetailPage();
    else
        ShowListPage();
    sQuestLog->deferCopies = FALSE;
    ClearScheduledBgCopiesToVram();
    if (showDetail && sQuestLog->icon != SPRITE_NONE)
        gSprites[sQuestLog->icon].invisible = TRUE;
    gTasks[taskId].tShowDetail = showDetail;
    gTasks[taskId].func = Task_QuestLogCopyPage;
}

static void Task_QuestLogCopyPage(u8 taskId)
{
    if (gTasks[taskId].tShowDetail)
    {
        CopyWindowToVram(WIN_TITLE, COPYWIN_GFX);
        CopyWindowToVram(WIN_GIVER, COPYWIN_GFX);
        CopyWindowToVram(WIN_BODY, COPYWIN_GFX);
        if (sQuestLog->icon != SPRITE_NONE)
            gSprites[sQuestLog->icon].invisible = FALSE;
        gTasks[taskId].func = Task_QuestLogDetailInput;
    }
    else
    {
        CopyWindowToVram(WIN_TABS, COPYWIN_GFX);
        CopyWindowToVram(WIN_LIST, COPYWIN_GFX);
        DestroyIcon();
        gTasks[taskId].func = Task_QuestLogListInput;
    }
    CopyWindowToVram(WIN_FOOTER, COPYWIN_GFX);
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
}

#undef tShowDetail

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
            SwitchPage(taskId, TRUE);
        }
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        if (ToggleTracked())
            DrawList();
    }
    else if (JOY_NEW(START_BUTTON))
    {
        sQuestLog->filter = (sQuestLog->filter + 1) % FILTER_COUNT;
        ResetList();
    }
    else if (JOY_NEW(L_BUTTON) || JOY_NEW(R_BUTTON))
    {
        sQuestLog->tab = (sQuestLog->tab + (JOY_NEW(R_BUTTON) ? 1 : QUEST_CATEGORY_COUNT - 1)) % QUEST_CATEGORY_COUNT;
        ResetList();
    }
    else if (JOY_REPEAT(DPAD_UP) && sQuestLog->cursor > 0)
    {
        MoveListCursor(sQuestLog->cursor - 1);
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sQuestLog->cursor + 1 < sQuestLog->listCount)
    {
        MoveListCursor(sQuestLog->cursor + 1);
    }
    else if (JOY_REPEAT(DPAD_LEFT) && sQuestLog->cursor > 0)
    {
        MoveListCursor(sQuestLog->cursor > LIST_SKIP ? sQuestLog->cursor - LIST_SKIP : 0);
    }
    else if (JOY_REPEAT(DPAD_RIGHT) && sQuestLog->cursor + 1 < sQuestLog->listCount)
    {
        MoveListCursor(min(sQuestLog->cursor + LIST_SKIP, sQuestLog->listCount - 1));
    }
    else
    {
        UpdateMarquee();
    }
}

static void Task_QuestLogDetailInput(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        SwitchPage(taskId, FALSE);
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        if (ToggleTracked())
            DrawDetail();
    }
    else if (JOY_NEW(L_BUTTON) && sQuestLog->cursor > 0)
    {
        PlaySE(SE_SELECT);
        sQuestLog->cursor--;
        sQuestLog->bodyScroll = 0;
        ClampListCursor();
        DrawDetail();
    }
    else if (JOY_NEW(R_BUTTON) && sQuestLog->cursor + 1 < sQuestLog->listCount)
    {
        PlaySE(SE_SELECT);
        sQuestLog->cursor++;
        sQuestLog->bodyScroll = 0;
        ClampListCursor();
        DrawDetail();
    }
    else if (JOY_REPEAT(DPAD_UP) && sQuestLog->bodyScroll > 0)
    {
        PlaySE(SE_SELECT);
        sQuestLog->bodyScroll--;
        DrawBody(sQuestLog->list[sQuestLog->cursor]);
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sQuestLog->bodyScroll < sQuestLog->bodyMaxScroll)
    {
        PlaySE(SE_SELECT);
        sQuestLog->bodyScroll++;
        DrawBody(sQuestLog->list[sQuestLog->cursor]);
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
