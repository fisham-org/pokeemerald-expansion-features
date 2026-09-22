#include "global.h"
#include "bg.h"
#include "main.h"
#include "map_name_popup.h"
#include "menu.h"
#include "overworld.h"
#include "palette.h"
#include "quest.h"
#include "quest_toast.h"
#include "sound.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/songs.h"

#if QUEST_TOASTS

/*
 * Toasts are small non-blocking windows at the top of the field screen that announce quest changes.
 * Toasts queue behind each other and behind the map name popup.
 *
 * Graphics: graphics/quest_log/toast_frame.png is the paper behind the toast, and toast.png holds one
 * 16x16 icon per toast type, stacked vertically. Both share toast.png's palette, which is loaded into
 * BG palette 13, unused in the overworld (palette 14 is the map name popup and std windows, 15 the
 * message box). Indices 1-9 double as text colors.
 */

#define TOAST_QUEUE_SIZE        4
#define TOAST_DURATION          150
#define TOAST_PALETTE_NUM       13
#define TOAST_WIDTH             20
#define TOAST_HEIGHT            4
#define TOAST_ICON_SIZE         16

// Field BG0 uses charblock 2, and tiles from 0x300 up overlap the BG tilemaps in VRAM (screenblocks 28-31).
// 0x200-0x21C holds the window frames; the message box, start menu and choice windows sit below 0x200.
#define TOAST_BASE_BLOCK        0x260
#define FIELD_BG0_TILE_LIMIT    0x300
STATIC_ASSERT(TOAST_BASE_BLOCK + TOAST_WIDTH * TOAST_HEIGHT <= FIELD_BG0_TILE_LIMIT, QuestToastFitsInFieldBg0)

struct QuestToast
{
    u8 type;
    u8 objective;
    u16 id;             // quest
};

enum ToastSoundType
{
    TOAST_SOUND_NONE,
    TOAST_SOUND_SE,
    TOAST_SOUND_FANFARE,
};

struct ToastInfo
{
    const u8 *label;
    u16 song;
    u8 soundType;
};

static EWRAM_DATA struct QuestToast sToastQueue[TOAST_QUEUE_SIZE] = {0};
static EWRAM_DATA u8 sToastQueueCount = 0;

static const u8 sToastFrameGfx[] = INCGFX_U8("graphics/quest_log/toast_frame.png", ".4bpp");
static const u8 sToastIconGfx[] = INCGFX_U8("graphics/quest_log/toast.png", ".4bpp");
static const u16 sToastPalette[] = INCGFX_U16("graphics/quest_log/toast.png", ".gbapal");

// Label and sound per toast type
static const struct ToastInfo sToastInfo[QUEST_TOAST_TYPE_COUNT] =
{
    [QUEST_TOAST_AVAILABLE]     = { COMPOUND_STRING("Quest available"), SE_PIN,          TOAST_SOUND_SE },
    [QUEST_TOAST_STARTED]       = { COMPOUND_STRING("Quest started"),   MUS_LEVEL_UP,    TOAST_SOUND_FANFARE },
    [QUEST_TOAST_UPDATED]       = { COMPOUND_STRING("Quest updated"),   SE_SUCCESS,      TOAST_SOUND_SE },
    [QUEST_TOAST_PROGRESS]      = { NULL,                               0,               TOAST_SOUND_NONE },
    [QUEST_TOAST_COMPLETE]      = { COMPOUND_STRING("Quest complete"),  MUS_OBTAIN_ITEM, TOAST_SOUND_FANFARE },
    [QUEST_TOAST_CLOSED]        = { COMPOUND_STRING("Quest closed"),    SE_PC_OFF,       TOAST_SOUND_SE },
    [QUEST_TOAST_TASK_COMPLETE] = { COMPOUND_STRING("Task complete"),   SE_SUCCESS,      TOAST_SOUND_SE },
};

// Transparent background so the text sits on the paper texture
static const u8 sToastTextColors[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};
static const u8 sToastProgressColors[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_BLUE, TEXT_COLOR_LIGHT_BLUE};

static void Task_QuestToast(u8 taskId);

void QuestToast_Queue(u32 type, u32 id, u32 objective)
{
    if (!Quest_IsValid(id))
        return;

    // Drop the oldest queued toast, but never the one on screen
    if (sToastQueueCount == TOAST_QUEUE_SIZE)
    {
        u32 dropIndex = FuncIsActiveTask(Task_QuestToast) ? 1 : 0;
        for (u32 i = dropIndex; i < TOAST_QUEUE_SIZE - 1; i++)
            sToastQueue[i] = sToastQueue[i + 1];
        sToastQueueCount--;
    }

    sToastQueue[sToastQueueCount].type = type;
    sToastQueue[sToastQueueCount].id = id;
    sToastQueue[sToastQueueCount].objective = objective;
    sToastQueueCount++;
}

u32 QuestToast_GetQueueCount(void)
{
    return sToastQueueCount;
}

// QUEST_TOAST_TYPE_COUNT if there is no toast at that position
u32 QuestToast_GetQueuedType(u32 index)
{
    if (index >= sToastQueueCount)
        return QUEST_TOAST_TYPE_COUNT;
    return sToastQueue[index].type;
}

void QuestToast_ClearQueue(void)
{
    sToastQueueCount = 0;
}

static void PopToast(void)
{
    for (u32 i = 0; i + 1 < sToastQueueCount; i++)
        sToastQueue[i] = sToastQueue[i + 1];
    if (sToastQueueCount != 0)
        sToastQueueCount--;
}

// Called every frame from CB2_Overworld.
void QuestToast_Update(void)
{
    if (sToastQueueCount == 0
     || FuncIsActiveTask(Task_QuestToast)
     || IsMapNamePopupActive()
     || gPaletteFade.active)
        return;
    CreateTask(Task_QuestToast, 90);
}

static void PlayToastSound(u32 type)
{
    switch (sToastInfo[type].soundType)
    {
    case TOAST_SOUND_SE:
        PlaySE(sToastInfo[type].song);
        break;
    case TOAST_SOUND_FANFARE:
        PlayFanfare(sToastInfo[type].song);
        break;
    }
}

static void DrawToast(u32 windowId, const struct QuestToast *toast)
{
    const struct Quest *quest = Quest_GetInfo(toast->id);
    const u32 textX = TOAST_ICON_SIZE + 8;
    u32 width = TOAST_WIDTH * 8;
    u32 height = TOAST_HEIGHT * 8;

    FillWindowPixelBuffer(windowId, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    BlitBitmapRectToWindow(windowId, sToastFrameGfx, 0, 0, width, height, 0, 0, width, height);

    BlitBitmapRectToWindow(windowId, sToastIconGfx, 0, toast->type * TOAST_ICON_SIZE,
                           TOAST_ICON_SIZE, TOAST_ICON_SIZE * QUEST_TOAST_TYPE_COUNT,
                           4, (height - TOAST_ICON_SIZE) / 2, TOAST_ICON_SIZE, TOAST_ICON_SIZE);

    // The stage may have moved on since a progress toast was queued
    if (toast->type == QUEST_TOAST_PROGRESS && toast->objective < Quest_GetCurrentStage(toast->id)->objectiveCount)
    {
        const struct QuestObjective *objective = &Quest_GetCurrentStage(toast->id)->objectives[toast->objective];
        u16 current, target;

        AddTextPrinterParameterized4(windowId, FONT_SMALL, textX, 3, 0, 0, sToastTextColors, TEXT_SKIP_DRAW, quest->name);
        AddTextPrinterParameterized4(windowId, FONT_SMALL, textX, 17, 0, 0, sToastTextColors, TEXT_SKIP_DRAW, objective->text);

        if (Quest_IsObjectiveDone(toast->id, toast->objective))
            StringCopy(gStringVar3, COMPOUND_STRING("Done!"));
        else if (Quest_GetConditionProgress(&objective->condition, &current, &target))
        {
            ConvertIntToDecimalStringN(gStringVar1, min(current, target), STR_CONV_MODE_LEFT_ALIGN, 5);
            ConvertIntToDecimalStringN(gStringVar2, target, STR_CONV_MODE_LEFT_ALIGN, 5);
            StringExpandPlaceholders(gStringVar3, COMPOUND_STRING("{STR_VAR_1}/{STR_VAR_2}"));
        }
        else
            gStringVar3[0] = EOS;
        AddTextPrinterParameterized4(windowId, FONT_SMALL, width - 4 - GetStringWidth(FONT_SMALL, gStringVar3, 0), 3, 0, 0,
                                     sToastProgressColors, TEXT_SKIP_DRAW, gStringVar3);
    }
    else
    {
        AddTextPrinterParameterized4(windowId, FONT_SMALL, textX, 3, 0, 0, sToastTextColors, TEXT_SKIP_DRAW,
                                     sToastInfo[toast->type].label ? sToastInfo[toast->type].label : sToastInfo[QUEST_TOAST_UPDATED].label);
        AddTextPrinterParameterized4(windowId, FONT_NARROW, textX, 15, 0, 0, sToastTextColors, TEXT_SKIP_DRAW, quest->name);
    }
}

#define tState    data[0]
#define tTimer    data[1]
#define tWindowId data[2]

static void Task_QuestToast(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    struct WindowTemplate template;

    switch (task->tState)
    {
    case 0:
        if (sToastQueueCount == 0)
        {
            DestroyTask(taskId);
            return;
        }
        template = CreateWindowTemplate(0, 1, 1, TOAST_WIDTH, TOAST_HEIGHT, TOAST_PALETTE_NUM, TOAST_BASE_BLOCK);
        task->tWindowId = AddWindow(&template);
        if (task->tWindowId == WINDOW_NONE)
        {
            DestroyTask(taskId);
            return;
        }
        LoadPalette(sToastPalette, BG_PLTT_ID(TOAST_PALETTE_NUM), PLTT_SIZE_4BPP);
        DrawToast(task->tWindowId, &sToastQueue[0]);
        PutWindowTilemap(task->tWindowId);
        CopyWindowToVram(task->tWindowId, COPYWIN_FULL);
        PlayToastSound(sToastQueue[0].type);
        task->tTimer = 0;
        task->tState++;
        break;
    case 1:
        // A map name popup takes over the top of the screen; hide and show this toast again after it
        if (IsMapNamePopupActive())
        {
            ClearWindowTilemap(task->tWindowId);
            CopyWindowToVram(task->tWindowId, COPYWIN_MAP);
            RemoveWindow(task->tWindowId);
            DestroyTask(taskId);
            return;
        }
        if (++task->tTimer >= TOAST_DURATION)
            task->tState++;
        break;
    case 2:
        ClearWindowTilemap(task->tWindowId);
        CopyWindowToVram(task->tWindowId, COPYWIN_MAP);
        RemoveWindow(task->tWindowId);
        PopToast();
        DestroyTask(taskId);
        break;
    }
}

#undef tState
#undef tTimer
#undef tWindowId

#endif // QUEST_TOASTS
