#include "global.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "field_player_avatar.h"
#include "field_weather.h"
#include "fieldmap.h"
#include "gpu_regs.h"
#include "graphics.h"
#include "item.h"
#include "item_icon.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "money.h"
#include "move.h"
#include "overworld.h"
#include "palette.h"
#include "scanline_effect.h"
#include "script.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "text_window.h"
#include "window.h"
#include "tm_crafting.h"
#include "battle_main.h" // gTypesInfo
#include "constants/event_objects.h"
#include "constants/items.h"
#include "constants/metatile_behaviors.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// Full-screen TM crafting scene. This mirrors the Poké Mart buy menu (src/shop.c)
// so it has the same look: the live overworld rendered behind framed windows, a
// recipe list on the right, money top-left, and an info panel bottom-left. The
// BG/map-rendering pipeline and the shop's frame graphics (gShopMenu_*) are
// reused verbatim; only the list contents and info panel are crafting-specific.

#define TAG_ICON           5500
#define MENU_PALETTE_ID    (gMapHeader.mapLayout->isFrlg ? 11 : 12) // matches SHOP_MENU_PALETTE_ID
#define MAX_ROWS_SHOWN     8

enum { COLORID_NORMAL, COLORID_HAVE, COLORID_LACK };

enum { WIN_MONEY, WIN_LIST, WIN_INFO, WIN_MESSAGE };

// viewport NPC info indices (as in shop.c)
enum { OBJ_EVENT_ID, X_COORD, Y_COORD, ANIM_NUM, LAYER_TYPE };

struct CraftData
{
    u16 tilemapBuffers[4][0x400];
    u16 scrollOffset;
    u16 selectedRow;
    u8 iconSlot;
    u8 iconSpriteIds[2];
    s16 viewportObjects[OBJECT_EVENTS_COUNT][5];
};

static EWRAM_DATA struct CraftData *sCraftData = NULL;
static EWRAM_DATA struct ListMenuItem *sListItems = NULL;
static EWRAM_DATA u8 (*sListNames)[24] = NULL;
static EWRAM_DATA const struct TMRecipe **sRecipes = NULL;
static EWRAM_DATA u32 sRecipeCount = 0;

#define tListTaskId data[0]
#define tSelected   data[1]

static void CB2_InitMenu(void);
static void CB2_Menu(void);
static void VBlankCB_Menu(void);
static void Task_Menu(u8 taskId);
static void Task_WaitMessage(u8 taskId);
static void Menu_BuildList(void);
static void Menu_InitBgs(void);
static void Menu_InitWindows(void);
static void Menu_DrawGraphics(void);
static void Menu_PrintInfo(s32 id, bool8 onInit, struct ListMenu *list);
static void Menu_DrawInfoText(s32 id);
static void Menu_RestoreAfterMessage(u8 taskId);
static void Menu_AddIcon(enum Item item, u8 slot);
static void Menu_RemoveIcon(u8 slot);
static void Menu_SetIconInvisible(bool8 invisible);
static void Menu_Print(u8 windowId, const u8 *text, u8 x, u8 y, u8 colorId);
static void ConfirmYes(u8 taskId);
static void ConfirmNo(u8 taskId);
static void ExitMenu(u8 taskId);
static void Task_ExitMenu(u8 taskId);
static void PrintMessage(const u8 *str);
// Map rendering (ported from shop.c)
static void Menu_DrawMapGraphics(void);
static void Menu_CopyMenuBgToBg1TilemapBuffer(void);
static void Menu_CollectObjectEventData(void);
static void Menu_DrawObjectEvents(void);
static void Menu_DrawMapBg(void);
static bool8 Menu_CheckForOverlapWithMenuBg(int x, int y);
static void Menu_DrawMapMetatile(s16 x, s16 y, const u16 *src, u8 layerType);
static void Menu_DrawMapMetatileLayer(u16 *dest, s16 offset1, s16 offset2, const u16 *src);
static bool8 Menu_CheckIfObjectEventOverlapsMenuBg(s16 *object);

static const u8 sText_SpaceSlash[] = _(" ");
static const u8 sText_Slash[]      = _("/");
static const u8 sText_Cost[]       = _("Cost ¥");
static const u8 sText_TypeSep[]    = _(" / ");
static const u8 sText_CraftQ[]     = _("Craft {STR_VAR_1}?");
static const u8 sText_Crafted[]    = _("Here you go! One {STR_VAR_1}!");
static const u8 sText_NoMats[]     = _("You don't have the materials for {STR_VAR_1}.");
static const u8 sText_NoMoney[]    = _("You can't afford {STR_VAR_1}.");
static const u8 sText_NoSpace[]    = _("There's no room for {STR_VAR_1}.");
static const u8 *const sCategoryNames[] = {
    [DAMAGE_CATEGORY_PHYSICAL] = COMPOUND_STRING("Physical"),
    [DAMAGE_CATEGORY_SPECIAL]  = COMPOUND_STRING("Special"),
    [DAMAGE_CATEGORY_STATUS]   = COMPOUND_STRING("Status"),
};

static const u8 sTextColors[][3] = {
    [COLORID_NORMAL] = {1, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY},
    [COLORID_HAVE]   = {1, TEXT_COLOR_GREEN,     TEXT_COLOR_LIGHT_GRAY},
    [COLORID_LACK]   = {1, TEXT_COLOR_RED,       TEXT_COLOR_LIGHT_GRAY},
};
static const u8 sMsgColors[3] = {1, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY};

static const struct BgTemplate sBgTemplates[] = {
    { .bg = 0, .charBaseIndex = 2, .mapBaseIndex = 31, .priority = 0 },
    { .bg = 1, .charBaseIndex = 0, .mapBaseIndex = 30, .priority = 1 },
    { .bg = 2, .charBaseIndex = 0, .mapBaseIndex = 29, .priority = 2 },
    { .bg = 3, .charBaseIndex = 0, .mapBaseIndex = 28, .priority = 3 },
};

static const struct WindowTemplate sWindowTemplates[] = {
    [WIN_MONEY]   = { .bg = 0, .tilemapLeft =  1, .tilemapTop =  1, .width = 10, .height = 2,  .paletteNum = 15, .baseBlock = 0x001E },
    [WIN_LIST]    = { .bg = 0, .tilemapLeft = 14, .tilemapTop =  2, .width = 15, .height = 16, .paletteNum = 15, .baseBlock = 0x0032 },
    [WIN_INFO]    = { .bg = 0, .tilemapLeft =  0, .tilemapTop = 10, .width = 13, .height = 10, .paletteNum = 15, .baseBlock = 0x0122 },
    [WIN_MESSAGE] = { .bg = 0, .tilemapLeft =  2, .tilemapTop = 15, .width = 27, .height = 4,  .paletteNum = 15, .baseBlock = 0x01B0 },
    DUMMY_WIN_TEMPLATE,
};

static const struct WindowTemplate sYesNoWindowTemplate = {
    .bg = 0, .tilemapLeft = 21, .tilemapTop = 9, .width = 5, .height = 4, .paletteNum = 15, .baseBlock = 0x0250,
};

static const struct YesNoFuncTable sCraftYesNoFuncs = { .yesFunc = ConfirmYes, .noFunc = ConfirmNo };

static const struct ListMenuTemplate sListTemplate = {
    .items = NULL,
    .moveCursorFunc = Menu_PrintInfo,
    .itemPrintFunc = NULL,
    .totalItems = 0,
    .maxShowed = 0,
    .windowId = WIN_LIST,
    .header_X = 0,
    .item_X = 8,
    .cursor_X = 0,
    .upText_Y = 1,
    .cursorPal = 2,
    .fillValue = 0,
    .cursorShadowPal = 3,
    .lettersSpacing = 0,
    .itemVerticalPadding = 0,
    .scrollMultiple = LIST_NO_MULTIPLE_SCROLL,
    .fontId = FONT_NARROW,
    .cursorKind = CURSOR_BLACK_ARROW,
};

// ----- entry from a field script --------------------------------------------

static void Task_OpenAfterFade(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_InitMenu);
    }
}

void TMCrafting_OpenMenu(struct ScriptContext *ctx)
{
    if (!TM_CRAFTING)
        return;
    LockPlayerFieldControls();
    FadeScreen(FADE_TO_BLACK, 0);
    CreateTask(Task_OpenAfterFade, 10);
}

// ----- list + info ----------------------------------------------------------

static void Menu_BuildList(void)
{
    u32 i;

    sRecipes = AllocZeroed(sizeof(*sRecipes) * (gTMRecipeCount + 1));
    sListItems = AllocZeroed(sizeof(*sListItems) * (gTMRecipeCount + 1));
    sListNames = AllocZeroed(sizeof(*sListNames) * (gTMRecipeCount + 1));
    sRecipeCount = 0;

    for (i = 0; i < gTMRecipeCount; i++)
    {
        const struct TMRecipe *recipe = &gTMRecipes[i];
        u8 *str;

        if (!TMCrafting_RecipeIsUnlocked(recipe))
            continue;

        str = StringCopy(sListNames[sRecipeCount], GetItemName(recipe->tm));
        *str++ = CHAR_SPACE;
        StringCopy(str, GetMoveName(GetItemTMHMMoveId(recipe->tm)));
        sListItems[sRecipeCount].name = sListNames[sRecipeCount];
        sListItems[sRecipeCount].id = sRecipeCount;
        sRecipes[sRecipeCount] = recipe;
        sRecipeCount++;
    }

    StringCopy(sListNames[sRecipeCount], gText_Cancel);
    sListItems[sRecipeCount].name = sListNames[sRecipeCount];
    sListItems[sRecipeCount].id = LIST_CANCEL;
}

static void Menu_AddIcon(enum Item item, u8 slot)
{
    u8 spriteId;
    u8 *idPtr = &sCraftData->iconSpriteIds[slot];

    if (*idPtr != SPRITE_NONE)
        return;
    spriteId = AddItemIconSprite(slot + TAG_ICON, slot + TAG_ICON, item);
    if (spriteId != MAX_SPRITES)
    {
        *idPtr = spriteId;
        gSprites[spriteId].oam.priority = 0; // draw in front of the info panel
        gSprites[spriteId].x2 = 84;          // bottom-right of the info panel
        gSprites[spriteId].y2 = 140;
    }
}

static void Menu_RemoveIcon(u8 slot)
{
    u8 *idPtr = &sCraftData->iconSpriteIds[slot];
    if (*idPtr == SPRITE_NONE)
        return;
    FreeSpriteTilesByTag(slot + TAG_ICON);
    FreeSpritePaletteByTag(slot + TAG_ICON);
    DestroySprite(&gSprites[*idPtr]);
    *idPtr = SPRITE_NONE;
}

// Hide/show the TM icon sprite so it doesn't float over the message/confirm box.
static void Menu_SetIconInvisible(bool8 invisible)
{
    u32 slot;
    for (slot = 0; slot < 2; slot++)
    {
        if (sCraftData->iconSpriteIds[slot] != SPRITE_NONE)
            gSprites[sCraftData->iconSpriteIds[slot]].invisible = invisible;
    }
}

static void Menu_Print(u8 windowId, const u8 *text, u8 x, u8 y, u8 colorId)
{
    AddTextPrinterParameterized4(windowId, FONT_SMALL, x, y, 0, 0, sTextColors[colorId], TEXT_SKIP_DRAW, text);
}

// Draws just the textual contents of the info panel (no icon handling), so it
// can be re-run to restore the panel after a message box covered it.
static void Menu_DrawInfoText(s32 id)
{
    const struct TMRecipe *recipe;
    enum Move move;
    u8 *str;
    u32 i;
    u8 y;

    FillWindowPixelBuffer(WIN_INFO, PIXEL_FILL(1));

    if (id == LIST_CANCEL || (u32)id >= sRecipeCount)
    {
        CopyWindowToVram(WIN_INFO, COPYWIN_GFX);
        return;
    }

    recipe = sRecipes[id];
    move = GetItemTMHMMoveId(recipe->tm);

    // Type / Category.
    str = StringCopy(gStringVar4, gTypesInfo[GetMoveType(move)].name);
    str = StringAppend(str, sText_TypeSep);
    StringCopy(str, sCategoryNames[GetMoveCategory(move)]);
    Menu_Print(WIN_INFO, gStringVar4, 6, 2, COLORID_NORMAL);

    // Materials with have/need counts, colored.
    y = 16;
    for (i = 0; i < recipe->materialCount; i++)
    {
        const struct TMRecipeMaterial *mat = &recipe->materials[i];
        u32 have = CountTotalItemQuantityInBag(mat->item);
        u8 color = (have >= mat->quantity) ? COLORID_HAVE : COLORID_LACK;

        str = StringCopy(gStringVar4, GetItemName(mat->item));
        str = StringAppend(str, sText_SpaceSlash);
        str = ConvertIntToDecimalStringN(str, have, STR_CONV_MODE_LEFT_ALIGN, 3);
        str = StringAppend(str, sText_Slash);
        ConvertIntToDecimalStringN(str, mat->quantity, STR_CONV_MODE_LEFT_ALIGN, 3);
        Menu_Print(WIN_INFO, gStringVar4, 6, y, color);
        y += 14;
    }

    // Cost.
    str = StringCopy(gStringVar4, sText_Cost);
    ConvertIntToDecimalStringN(str, recipe->cost, STR_CONV_MODE_LEFT_ALIGN, 6);
    Menu_Print(WIN_INFO, gStringVar4, 6, y, COLORID_NORMAL);

    CopyWindowToVram(WIN_INFO, COPYWIN_GFX);
}

static void Menu_PrintInfo(s32 id, bool8 onInit, struct ListMenu *list)
{
    if (!onInit)
        PlaySE(SE_SELECT);

    // Swap the displayed item icon (two-slot scheme, as the shop does).
    if (id != LIST_CANCEL && (u32)id < sRecipeCount)
        Menu_AddIcon(sRecipes[id]->tm, sCraftData->iconSlot);
    Menu_RemoveIcon(sCraftData->iconSlot ^ 1);
    sCraftData->iconSlot ^= 1;

    Menu_DrawInfoText(id);
}

// Clears the message/confirm box and redraws the panel + list it covered, so
// dismissing a message doesn't leave black holes in the UI.
static void Menu_RestoreAfterMessage(u8 taskId)
{
    ClearStdWindowAndFrameToTransparent(WIN_MESSAGE, FALSE);
    ClearWindowTilemap(WIN_MESSAGE);
    DrawStdFrameWithCustomTileAndPalette(WIN_INFO, FALSE, 1, 13);
    Menu_DrawInfoText(gTasks[taskId].tSelected);
    Menu_SetIconInvisible(FALSE);
    PutWindowTilemap(WIN_LIST);
    RedrawListMenu(gTasks[taskId].tListTaskId);
    ScheduleBgCopyTilemapToVram(0);
    gTasks[taskId].func = Task_Menu;
}

// ----- scene setup ----------------------------------------------------------

static void Menu_InitBgs(void)
{
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
    SetBgTilemapBuffer(1, sCraftData->tilemapBuffers[1]);
    SetBgTilemapBuffer(2, sCraftData->tilemapBuffers[3]);
    SetBgTilemapBuffer(3, sCraftData->tilemapBuffers[2]);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    SetGpuReg(REG_OFFSET_BG2HOFS, 0);
    SetGpuReg(REG_OFFSET_BG2VOFS, 0);
    SetGpuReg(REG_OFFSET_BG3HOFS, 0);
    SetGpuReg(REG_OFFSET_BG3VOFS, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    ShowBg(3);
}

static void Menu_InitWindows(void)
{
    InitWindows(sWindowTemplates);
    DeactivateAllTextPrinters();
    LoadUserWindowBorderGfx(WIN_MONEY, 1, BG_PLTT_ID(13));
    LoadMessageBoxGfx(WIN_MONEY, 0xA, BG_PLTT_ID(14));
    Menu_LoadStdPalAt(BG_PLTT_ID(15));
    PutWindowTilemap(WIN_MONEY);
    PutWindowTilemap(WIN_LIST);
    PutWindowTilemap(WIN_INFO);
}

static void Menu_DrawGraphics(void)
{
    Menu_DrawMapGraphics();
    Menu_CopyMenuBgToBg1TilemapBuffer();
    // Give the info/message boxes a solid backing so their frame's transparent
    // edge pixels don't reveal the live map (the shop graphic does this for the
    // money box and list). Reuse the list panel's solid background tile.
    {
        u16 backing = sCraftData->tilemapBuffers[1][3 * 32 + 20]; // a list-panel bg tile (BG1)
        u32 bx, by;
        for (by = 9; by < 20; by++)
            for (bx = 0; bx < 14; bx++)
                sCraftData->tilemapBuffers[1][by * 32 + bx] = backing;
    }
    // Own (taller) frame for the info panel, drawn over the shop graphic's box.
    FillWindowPixelBuffer(WIN_INFO, PIXEL_FILL(1));
    DrawStdFrameWithCustomTileAndPalette(WIN_INFO, FALSE, 1, 13);
    AddMoneyLabelObject(19, 11);
    PrintMoneyAmountInMoneyBoxWithBorder(WIN_MONEY, 1, 13, GetMoney(&gSaveBlock1Ptr->money));
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    ScheduleBgCopyTilemapToVram(3);
}

static void CB2_InitMenu(void)
{
    u8 taskId;

    switch (gMain.state)
    {
    case 0:
        SetVBlankHBlankCallbacksToNull();
        CpuFastFill(0, (void *)OAM, OAM_SIZE);
        ScanlineEffect_Stop();
        ResetTempTileDataBuffers();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        ClearScheduledBgCopiesToVram();
        sCraftData = AllocZeroed(sizeof(struct CraftData));
        sCraftData->iconSpriteIds[0] = SPRITE_NONE;
        sCraftData->iconSpriteIds[1] = SPRITE_NONE;
        Menu_BuildList();
        Menu_InitBgs();
        FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 0x20, 0x20);
        FillBgTilemapBufferRect_Palette0(1, 0, 0, 0, 0x20, 0x20);
        FillBgTilemapBufferRect_Palette0(2, 0, 0, 0, 0x20, 0x20);
        FillBgTilemapBufferRect_Palette0(3, 0, 0, 0, 0x20, 0x20);
        Menu_InitWindows();
        DecompressAndCopyTileDataToVram(1, gShopMenu_Gfx, 0x3A0, 0x3E3, 0);
        DecompressDataWithHeaderWram(gShopMenu_Tilemap, sCraftData->tilemapBuffers[0]);
        LoadPalette(gShopMenu_Pal, BG_PLTT_ID(MENU_PALETTE_ID), PLTT_SIZE_4BPP);
        // Erase the shop graphic in the bottom-left (the baked description box AND
        // the list panel's left border in this row range) so neither the tan box
        // nor the orange list shows behind/through our own WIN_INFO frame.
        {
            u32 bx, by;
            for (by = 9; by < 20; by++)
                for (bx = 0; bx < 14; bx++)
                    sCraftData->tilemapBuffers[0][by * 32 + bx] = 0;
        }
        gMain.state++;
        break;
    case 1:
        if (!FreeTempTileDataBuffersIfPossible())
            gMain.state++;
        break;
    default:
        Menu_DrawGraphics();
        gMultiuseListMenuTemplate = sListTemplate;
        gMultiuseListMenuTemplate.items = sListItems;
        gMultiuseListMenuTemplate.totalItems = sRecipeCount + 1;
        gMultiuseListMenuTemplate.maxShowed = (sRecipeCount + 1 < MAX_ROWS_SHOWN) ? sRecipeCount + 1 : MAX_ROWS_SHOWN;

        taskId = CreateTask(Task_Menu, 8);
        gTasks[taskId].tListTaskId = ListMenuInit(&gMultiuseListMenuTemplate, 0, 0);
        BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_Menu);
        SetMainCallback2(CB2_Menu);
        break;
    }
}

static void VBlankCB_Menu(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_Menu(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

// ----- main loop ------------------------------------------------------------

static void Task_Menu(u8 taskId)
{
    s32 input;

    if (gPaletteFade.active)
        return;

    input = ListMenu_ProcessInput(gTasks[taskId].tListTaskId);
    ListMenuGetScrollAndRow(gTasks[taskId].tListTaskId, &sCraftData->scrollOffset, &sCraftData->selectedRow);

    if (input == LIST_NOTHING_CHOSEN)
        return;

    if (input == LIST_CANCEL || input < 0 || (u32)input >= sRecipeCount)
    {
        PlaySE(SE_SELECT);
        ExitMenu(taskId);
        return;
    }

    PlaySE(SE_SELECT);
    gTasks[taskId].tSelected = input;
    StringCopy(gStringVar1, GetItemName(sRecipes[input]->tm));

    switch (TMCrafting_CheckCraft(sRecipes[input]))
    {
    case TM_CRAFT_SUCCESS:
        StringExpandPlaceholders(gStringVar4, sText_CraftQ);
        PrintMessage(gStringVar4);
        CreateYesNoMenuWithCallbacks(taskId, &sYesNoWindowTemplate, 1, 0, 0, 1, 13, &sCraftYesNoFuncs);
        break;
    case TM_CRAFT_NO_MATERIALS:
        StringExpandPlaceholders(gStringVar4, sText_NoMats);
        PrintMessage(gStringVar4);
        gTasks[taskId].func = Task_WaitMessage;
        break;
    case TM_CRAFT_NO_MONEY:
        StringExpandPlaceholders(gStringVar4, sText_NoMoney);
        PrintMessage(gStringVar4);
        gTasks[taskId].func = Task_WaitMessage;
        break;
    case TM_CRAFT_NO_SPACE:
        StringExpandPlaceholders(gStringVar4, sText_NoSpace);
        PrintMessage(gStringVar4);
        gTasks[taskId].func = Task_WaitMessage;
        break;
    }
}

static void ConfirmYes(u8 taskId)
{
    const struct TMRecipe *recipe = sRecipes[gTasks[taskId].tSelected];

    TMCrafting_Craft(recipe);
    PlaySE(SE_SHOP);
    PrintMoneyAmountInMoneyBoxWithBorder(WIN_MONEY, 1, 13, GetMoney(&gSaveBlock1Ptr->money));
    // Restore the recipe list the Yes/No box covered (the "No" path does this via
    // Menu_RestoreAfterMessage; the "Yes" path must do it before showing a message).
    PutWindowTilemap(WIN_LIST);
    RedrawListMenu(gTasks[taskId].tListTaskId);
    // PrintMoneyAmount... formats the amount through gStringVar1, clobbering the
    // TM name; re-buffer it for the "Here you go!" message.
    StringCopy(gStringVar1, GetItemName(recipe->tm));
    StringExpandPlaceholders(gStringVar4, sText_Crafted);
    PrintMessage(gStringVar4);
    ScheduleBgCopyTilemapToVram(0);
    gTasks[taskId].func = Task_WaitMessage;
}

static void ConfirmNo(u8 taskId)
{
    Menu_RestoreAfterMessage(taskId);
}

static void Task_WaitMessage(u8 taskId)
{
    if (JOY_NEW(A_BUTTON | B_BUTTON))
    {
        PlaySE(SE_SELECT);
        Menu_RestoreAfterMessage(taskId);
    }
}

static void PrintMessage(const u8 *str)
{
    Menu_SetIconInvisible(TRUE);
    FillWindowPixelBuffer(WIN_MESSAGE, PIXEL_FILL(1));
    DrawStdFrameWithCustomTileAndPalette(WIN_MESSAGE, FALSE, 1, 13);
    AddTextPrinterParameterized4(WIN_MESSAGE, FONT_NORMAL, 4, 6, 0, 0, sMsgColors, TEXT_SKIP_DRAW, str);
    CopyWindowToVram(WIN_MESSAGE, COPYWIN_FULL);
}

// ----- exit ------------------------------------------------------------------

static void ExitMenu(u8 taskId)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_ExitMenu;
}

static void Task_ExitMenu(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    DestroyListMenuTask(gTasks[taskId].tListTaskId, NULL, NULL);
    Menu_RemoveIcon(0);
    Menu_RemoveIcon(1);
    RemoveMoneyLabelObject();
    DestroyTask(taskId);

    Free(sRecipes);
    Free(sListItems);
    Free(sListNames);
    Free(sCraftData);
    FreeAllWindowBuffers();

    SetMainCallback2(CB2_ReturnToFieldContinueScript);
}

// ----- live overworld behind the menu (ported from src/shop.c) ---------------

static void Menu_DrawMapGraphics(void)
{
    Menu_CollectObjectEventData();
    Menu_DrawObjectEvents();
    Menu_DrawMapBg();
}

static void Menu_DrawMapBg(void)
{
    s16 i, j;
    s16 x, y;
    const struct MapLayout *mapLayout;
    u16 metatile;
    u16 numMetatilesInPrimary;
    u8 metatileLayerType;

    mapLayout = gMapHeader.mapLayout;
    numMetatilesInPrimary = GetNumMetatilesInPrimary(mapLayout);
    GetXYCoordsOneStepInFrontOfPlayer(&x, &y);
    x -= 4;
    y -= 4;

    for (j = 0; j < 10; j++)
    {
        for (i = 0; i < 15; i++)
        {
            metatile = MapGridGetMetatileIdAt(x + i, y + j);
            if (Menu_CheckForOverlapWithMenuBg(i, j) == TRUE)
                metatileLayerType = MapGridGetMetatileLayerTypeAt(x + i, y + j);
            else
                metatileLayerType = METATILE_LAYER_TYPE_COVERED;

            if (metatile < numMetatilesInPrimary)
                Menu_DrawMapMetatile(i, j, mapLayout->primaryTileset->metatiles + metatile * NUM_TILES_PER_METATILE, metatileLayerType);
            else
                Menu_DrawMapMetatile(i, j, mapLayout->secondaryTileset->metatiles + ((metatile - numMetatilesInPrimary) * NUM_TILES_PER_METATILE), metatileLayerType);
        }
    }
}

static void Menu_DrawMapMetatile(s16 x, s16 y, const u16 *src, u8 metatileLayerType)
{
    u16 offset1 = x * 2;
    u16 offset2 = y * 64;

    switch (metatileLayerType)
    {
    case METATILE_LAYER_TYPE_NORMAL:
        Menu_DrawMapMetatileLayer(sCraftData->tilemapBuffers[3], offset1, offset2, src);
        Menu_DrawMapMetatileLayer(sCraftData->tilemapBuffers[1], offset1, offset2, src + 4);
        break;
    case METATILE_LAYER_TYPE_COVERED:
        Menu_DrawMapMetatileLayer(sCraftData->tilemapBuffers[2], offset1, offset2, src);
        Menu_DrawMapMetatileLayer(sCraftData->tilemapBuffers[3], offset1, offset2, src + 4);
        break;
    case METATILE_LAYER_TYPE_SPLIT:
        Menu_DrawMapMetatileLayer(sCraftData->tilemapBuffers[2], offset1, offset2, src);
        Menu_DrawMapMetatileLayer(sCraftData->tilemapBuffers[1], offset1, offset2, src + 4);
        break;
    }
}

static void Menu_DrawMapMetatileLayer(u16 *dest, s16 offset1, s16 offset2, const u16 *src)
{
    dest[offset1 + offset2] = src[0];
    dest[offset1 + offset2 + 1] = src[1];
    dest[offset1 + offset2 + 32] = src[2];
    dest[offset1 + offset2 + 33] = src[3];
}

static void Menu_CollectObjectEventData(void)
{
    s16 facingX, facingY;
    u8 y, x;
    u8 numObjects = 0;

    GetXYCoordsOneStepInFrontOfPlayer(&facingX, &facingY);

    for (y = 0; y < OBJECT_EVENTS_COUNT; y++)
        sCraftData->viewportObjects[y][OBJ_EVENT_ID] = OBJECT_EVENTS_COUNT;

    for (y = 0; y < 5; y++)
    {
        for (x = 0; x < 7; x++)
        {
            u8 objEventId = GetObjectEventIdByXY(facingX - 4 + x, facingY - 2 + y);

            if (objEventId != OBJECT_EVENTS_COUNT && !(gObjectEvents[objEventId].active && gObjectEvents[objEventId].graphicsId & OBJ_EVENT_MON && gObjectEvents[objEventId].localId != OBJ_EVENT_ID_FOLLOWER))
            {
                sCraftData->viewportObjects[numObjects][OBJ_EVENT_ID] = objEventId;
                sCraftData->viewportObjects[numObjects][X_COORD] = x;
                sCraftData->viewportObjects[numObjects][Y_COORD] = y;
                sCraftData->viewportObjects[numObjects][LAYER_TYPE] = MapGridGetMetatileLayerTypeAt(facingX - 4 + x, facingY - 2 + y);

                switch (gObjectEvents[objEventId].facingDirection)
                {
                case DIR_SOUTH: sCraftData->viewportObjects[numObjects][ANIM_NUM] = ANIM_STD_FACE_SOUTH; break;
                case DIR_NORTH: sCraftData->viewportObjects[numObjects][ANIM_NUM] = ANIM_STD_FACE_NORTH; break;
                case DIR_WEST:  sCraftData->viewportObjects[numObjects][ANIM_NUM] = ANIM_STD_FACE_WEST;  break;
                case DIR_EAST:
                default:        sCraftData->viewportObjects[numObjects][ANIM_NUM] = ANIM_STD_FACE_EAST;  break;
                }
                numObjects++;
            }
        }
    }
}

static void Menu_DrawObjectEvents(void)
{
    u8 i, spriteId;
    const struct ObjectEventGraphicsInfo *graphicsInfo;
    u8 weatherTemp = gWeatherPtr->palProcessingState;

    if (weatherTemp == WEATHER_PAL_STATE_SCREEN_FADING_OUT)
        gWeatherPtr->palProcessingState = WEATHER_PAL_STATE_IDLE;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (sCraftData->viewportObjects[i][OBJ_EVENT_ID] == OBJECT_EVENTS_COUNT)
            continue;

        graphicsInfo = GetObjectEventGraphicsInfo(gObjectEvents[sCraftData->viewportObjects[i][OBJ_EVENT_ID]].graphicsId);

        spriteId = CreateObjectGraphicsSprite(
            gObjectEvents[sCraftData->viewportObjects[i][OBJ_EVENT_ID]].graphicsId,
            SpriteCallbackDummy,
            (u16)sCraftData->viewportObjects[i][X_COORD] * 16 + 8,
            (u16)sCraftData->viewportObjects[i][Y_COORD] * 16 + 48 - graphicsInfo->height / 2,
            2);

        if (Menu_CheckIfObjectEventOverlapsMenuBg(sCraftData->viewportObjects[i]) == TRUE)
        {
            gSprites[spriteId].subspriteTableNum = 4;
            gSprites[spriteId].subspriteMode = SUBSPRITES_ON;
        }

        StartSpriteAnim(&gSprites[spriteId], sCraftData->viewportObjects[i][ANIM_NUM]);
    }

    gWeatherPtr->palProcessingState = weatherTemp;
    CpuFastCopy(gPlttBufferFaded + 16 * 16, gPlttBufferUnfaded + 16 * 16, PLTT_BUFFER_SIZE);
}

static bool8 Menu_CheckIfObjectEventOverlapsMenuBg(s16 *object)
{
    if (!Menu_CheckForOverlapWithMenuBg(object[X_COORD], object[Y_COORD] + 2) && object[LAYER_TYPE] != METATILE_LAYER_TYPE_COVERED)
        return TRUE;
    return FALSE;
}

static void Menu_CopyMenuBgToBg1TilemapBuffer(void)
{
    s16 i;
    u16 *dest = sCraftData->tilemapBuffers[1];
    const u16 *src = sCraftData->tilemapBuffers[0];

    for (i = 0; i < 1024; i++)
    {
        if (src[i] != 0)
            dest[i] = src[i] + ((MENU_PALETTE_ID << 12) | 0x3E3);
    }
}

static bool8 Menu_CheckForOverlapWithMenuBg(int x, int y)
{
    const u16 *metatile = sCraftData->tilemapBuffers[0];
    int offset1 = x * 2;
    int offset2 = y * 64;

    if (metatile[offset2 + offset1] == 0 &&
        metatile[offset2 + offset1 + 32] == 0 &&
        metatile[offset2 + offset1 + 1] == 0 &&
        metatile[offset2 + offset1 + 33] == 0)
        return TRUE;

    return FALSE;
}
