#include "global.h"
#include "bg.h"
#include "dex_minigame.h"
#include "event_data.h"
#include "item.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "new_game.h"
#include "overworld.h"
#include "palette.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "random.h"
#include "rtc.h"
#include "save.h"
#include "scanline_effect.h"
#include "script.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "data/dex_minigame_rewards.h"

// Defined in pokedex.c: every base species up to Pecharunt, sorted by name.
// Custom species added after Pecharunt can only be picked by number.
extern const u16 gPokedexOrder_Alphabetical[];
#define ALPHABETICAL_ORDER_COUNT NATIONAL_DEX_PECHARUNT

#define BOX_TILE_COUNT 10 // Blank + 3x3 outline pieces
#define BOX_BG 1
#define TILEMAP_BUFFER_SIZE (1024 * 2)
#define PICKER_DIGITS 4

struct DexGameScreenState
{
    const struct DexGameScreen *screen;
    u32 boxTiles[BOX_TILE_COUNT * 8];
    u8 tilemap[TILEMAP_BUFFER_SIZE];
};

static EWRAM_DATA const struct DexGameScreen *sPendingScreen = NULL;
static EWRAM_DATA struct DexGameScreenState *sScreenState = NULL;

static const struct BgTemplate sBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .priority = 1
    },
    {
        .bg = BOX_BG,
        .charBaseIndex = 2,
        .mapBaseIndex = 30,
        .priority = 2
    },
};

#define BACKDROP_COLOR RGB(5, 8, 14)
#define OUTLINE_COLOR  RGB(12, 14, 18)
#define CURSOR_COLOR   RGB(31, 26, 4)
#define EMPTY_COLOR    RGB(30, 30, 31)
#define WRONG_COLOR    RGB(30, 17, 17)
#define CORRECT_COLOR  RGB(17, 30, 17)
#define PARTIAL_COLOR  RGB(31, 28, 12)

// Index 1 is the fill, 2 the outer outline and 3 the inner outline (fill unless highlighted).
#define BOX_PALETTE(fill, outline) { BACKDROP_COLOR, fill, outline, (outline) == CURSOR_COLOR ? CURSOR_COLOR : fill }
static const u16 sBoxPalettes[][16] =
{
    [DEX_BOX_EMPTY]                           = BOX_PALETTE(EMPTY_COLOR,   OUTLINE_COLOR),
    [DEX_BOX_WRONG]                           = BOX_PALETTE(WRONG_COLOR,   OUTLINE_COLOR),
    [DEX_BOX_CORRECT]                         = BOX_PALETTE(CORRECT_COLOR, OUTLINE_COLOR),
    [DEX_BOX_PARTIAL]                         = BOX_PALETTE(PARTIAL_COLOR, OUTLINE_COLOR),
    [DEX_BOX_CURSOR_OFFSET + DEX_BOX_EMPTY]   = BOX_PALETTE(EMPTY_COLOR,   CURSOR_COLOR),
    [DEX_BOX_CURSOR_OFFSET + DEX_BOX_WRONG]   = BOX_PALETTE(WRONG_COLOR,   CURSOR_COLOR),
    [DEX_BOX_CURSOR_OFFSET + DEX_BOX_CORRECT] = BOX_PALETTE(CORRECT_COLOR, CURSOR_COLOR),
    [DEX_BOX_CURSOR_OFFSET + DEX_BOX_PARTIAL] = BOX_PALETTE(PARTIAL_COLOR, CURSOR_COLOR),
};
#undef BOX_PALETTE

static const u16 sTextPalette[16] =
{
    [TEXT_COLOR_TRANSPARENT] = RGB_BLACK,
    [TEXT_COLOR_WHITE]       = RGB_WHITE,
    [TEXT_COLOR_DARK_GRAY]   = RGB(3, 4, 7),
    [TEXT_COLOR_LIGHT_GRAY]  = RGB(18, 18, 20),
    [TEXT_COLOR_RED]         = CURSOR_COLOR,
    [TEXT_COLOR_LIGHT_RED]   = RGB(31, 14, 14),
    [TEXT_COLOR_GREEN]       = RGB(14, 29, 14),
    [TEXT_COLOR_LIGHT_GREEN] = RGB(22, 23, 26), // Shadow for DEX_FONT_DARK
};

static const u8 sFontColors[][3] =
{
    [DEX_FONT_WHITE]     = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE,       TEXT_COLOR_DARK_GRAY},
    [DEX_FONT_HIGHLIGHT] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_RED,         TEXT_COLOR_DARK_GRAY},
    [DEX_FONT_RED]       = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_RED,   TEXT_COLOR_DARK_GRAY},
    [DEX_FONT_GREEN]     = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_GREEN,       TEXT_COLOR_DARK_GRAY},
    [DEX_FONT_GRAY]      = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY,  TEXT_COLOR_DARK_GRAY},
    [DEX_FONT_DARK]      = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY,   TEXT_COLOR_LIGHT_GREEN},
};

static const u16 sPowersOfTen[PICKER_DIGITS] = {1, 10, 100, 1000};

static void Task_OpenScreen(u8 taskId);
static void DexGame_SetupCB(void);
static void DexGame_MainCB(void);
static void DexGame_VBlankCB(void);
static void Task_WaitFadeIn(u8 taskId);
static void Task_WaitFadeAndExit(u8 taskId);
static void FadeAndBail(void);

// ---------------------------------------------------------------------------
// Species pool
// ---------------------------------------------------------------------------

static bool32 IsInAnswerPool(u32 dex, u32 answerPool)
{
    switch (answerPool)
    {
    case DEX_POOL_SEEN:
        return GetSetPokedexFlag(dex, FLAG_GET_SEEN);
    case DEX_POOL_CAUGHT:
        return GetSetPokedexFlag(dex, FLAG_GET_CAUGHT);
    default:
        return TRUE;
    }
}

struct DexPool *DexPool_Create(u32 answerPool)
{
    struct DexPool *pool = AllocZeroed(sizeof(struct DexPool));
    u16 *parent = AllocZeroed(sizeof(u16) * (NATIONAL_DEX_COUNT + 1));
    bool8 *canEvolve = AllocZeroed(sizeof(bool8) * (NATIONAL_DEX_COUNT + 1));
    enum Species species;
    u32 i, dex;

    if (pool == NULL || parent == NULL || canEvolve == NULL)
    {
        TRY_FREE_AND_SET_NULL(pool);
        TRY_FREE_AND_SET_NULL(parent);
        TRY_FREE_AND_SET_NULL(canEvolve);
        return NULL;
    }

    // Base forms come before other forms in the species list, so the first enabled
    // species for each dex number is the one the pool uses.
    for (species = 1; species < NUM_SPECIES; species++)
    {
        if (!IsSpeciesEnabled(species))
            continue;
        dex = gSpeciesInfo[species].natDexNum;
        if (dex == 0 || dex > NATIONAL_DEX_COUNT)
            continue;
        if (pool->species[dex] == SPECIES_NONE)
        {
            pool->species[dex] = species;
            if (IsInAnswerPool(dex, answerPool))
            {
                pool->available[dex] = TRUE;
                pool->count++;
            }
        }

        const struct Evolution *evolutions = GetSpeciesEvolutions(species);
        if (evolutions == NULL)
            continue;

        for (i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
        {
            enum Species target = evolutions[i].targetSpecies;
            u32 targetDex;

            if (evolutions[i].method == EVO_NONE || !IsSpeciesEnabled(target))
                continue;
            targetDex = gSpeciesInfo[target].natDexNum;
            if (targetDex == dex || targetDex == 0 || targetDex > NATIONAL_DEX_COUNT)
                continue;
            // Only the base form decides whether a dex entry evolves (e.g. Corsola doesn't,
            // even though Galarian Corsola does), but any form can be a parent (Perrserker).
            if (species == pool->species[dex])
                canEvolve[dex] = TRUE;
            if (parent[targetDex] == 0)
                parent[targetDex] = dex;
        }
    }

    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        u32 depth = 0;
        u32 ancestor = parent[dex];

        while (ancestor != 0 && depth < 4)
        {
            depth++;
            ancestor = parent[ancestor];
        }

        if (depth == 0)
            pool->stage[dex] = canEvolve[dex] ? DEX_STAGE_BASIC : DEX_STAGE_SINGLE;
        else
            pool->stage[dex] = canEvolve[dex] ? DEX_STAGE_MIDDLE : DEX_STAGE_FINAL;
    }

    for (i = 0; i < ALPHABETICAL_ORDER_COUNT; i++)
    {
        u32 entry = gPokedexOrder_Alphabetical[i];
        if (entry <= NATIONAL_DEX_COUNT && pool->available[entry])
            pool->alphabetical[pool->alphabeticalCount++] = entry;
    }

    Free(parent);
    Free(canEvolve);
    return pool;
}

u32 DexPool_GetGeneration(u32 dex)
{
    if (dex <= NATIONAL_DEX_MEW)
        return 1;
    if (dex <= NATIONAL_DEX_CELEBI)
        return 2;
    if (dex <= NATIONAL_DEX_DEOXYS)
        return 3;
    if (dex <= NATIONAL_DEX_ARCEUS)
        return 4;
    if (dex <= NATIONAL_DEX_GENESECT)
        return 5;
    if (dex <= NATIONAL_DEX_VOLCANION)
        return 6;
    if (dex <= NATIONAL_DEX_MELMETAL)
        return 7;
    if (dex <= NATIONAL_DEX_ENAMORUS)
        return 8;
    return 9;
}

// Moves delta dex numbers from dex, then on to the next available species in that direction.
// If there's none before the end of the dex, searches back from the end instead, so big steps
// still reach the last available species. The pool must have at least one available species.
u32 DexPool_StepAvailable(const struct DexPool *pool, u32 dex, s32 delta)
{
    s32 result = (s32)dex + delta;
    s32 step = delta > 0 ? 1 : -1;

    if (result < 1)
        result = 1;
    if (result > NATIONAL_DEX_COUNT)
        result = NATIONAL_DEX_COUNT;

    while (!pool->available[result])
    {
        result += step;
        if (result < 1 || result > NATIONAL_DEX_COUNT)
        {
            step = -step;
            result = (step > 0) ? 1 : NATIONAL_DEX_COUNT;
        }
    }
    return result;
}

// The nth available dex number, counting from 0 in dex order
u32 DexPool_GetNthDex(const struct DexPool *pool, u32 n)
{
    u32 dex;

    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        if (pool->available[dex] && n-- == 0)
            return dex;
    }
    return 0;
}

u32 DexPool_GetDexOfSpecies(u32 species)
{
    return gSpeciesInfo[species].natDexNum;
}

// ---------------------------------------------------------------------------
// Daily puzzles
// ---------------------------------------------------------------------------

u32 DexGame_GetDailyStatus(bool32 active, u32 savedDay, bool32 started, bool32 finished, u16 *today)
{
    RtcCalcLocalTime();
    if (RtcGetErrorStatus() & RTC_ERR_FLAG_MASK)
        return DEX_GAME_STATUS_NO_CLOCK;

    *today = RtcGetLocalDayCount();
    if (!active || *today > savedDay)
        return DEX_GAME_STATUS_NEW;
    if (*today < savedDay)
        return DEX_GAME_STATUS_CLOCK_LOCKED;
    if (finished)
        return DEX_GAME_STATUS_FINISHED;
    if (!started)
        return DEX_GAME_STATUS_NOT_STARTED;
    return DEX_GAME_STATUS_IN_PROGRESS;
}

// Whether today's daily puzzle exists, so Open can show it
bool32 DexGame_CanOpenDaily(u32 status)
{
    return status == DEX_GAME_STATUS_NOT_STARTED
        || status == DEX_GAME_STATUS_IN_PROGRESS
        || status == DEX_GAME_STATUS_FINISHED;
}

static const struct { const struct DexRewardTier *tiers; u8 count; } sRewardTables[] =
{
    [DEX_REWARDS_POKEDOKU] = { sPokedokuRewardTiers, ARRAY_COUNT(sPokedokuRewardTiers) },
    [DEX_REWARDS_SQUIRDLE] = { sSquirdleRewardTiers, ARRAY_COUNT(sSquirdleRewardTiers) },
};

// Picks an item from the first tier covering the score, weighted, using the seed (the day, so
// resetting before saving gives the same reward). Sets VAR_0x8004 = item, VAR_0x8005 = quantity
// and STR_VAR_1 = the tier's message. Returns FALSE (VAR_0x8004 = ITEM_NONE) if no tier covers the score.
bool32 DexGame_RollReward(enum DexRewardTable table, u32 score, u32 seed)
{
    const struct DexRewardTier *tiers = sRewardTables[table].tiers;
    rng_value_t rng = LocalRandomSeed(seed * 2654435761u + score);
    u32 i, j, totalWeight, roll;

    gSpecialVar_0x8004 = ITEM_NONE;
    gSpecialVar_0x8005 = 0;
    for (i = 0; i < sRewardTables[table].count; i++)
    {
        if (score < tiers[i].minScore || score > tiers[i].maxScore)
            continue;

        totalWeight = 0;
        for (j = 0; j < tiers[i].itemCount; j++)
            totalWeight += tiers[i].items[j].weight;
        if (totalWeight == 0)
            return FALSE;

        roll = LocalRandom(&rng) % totalWeight;
        for (j = 0; roll >= tiers[i].items[j].weight; j++)
            roll -= tiers[i].items[j].weight;

        gSpecialVar_0x8004 = tiers[i].items[j].item;
        gSpecialVar_0x8005 = tiers[i].items[j].quantity;
        StringCopy(gStringVar1, tiers[i].message);
        return TRUE;
    }
    return FALSE;
}

// For the Claim* specials. Rolls the reward and sets VAR_RESULT = score. If the item doesn't
// fit in the bag, VAR_RESULT = DEX_GAME_REWARD_BAG_FULL instead and this returns FALSE, so the
// caller keeps the reward waiting. Returns TRUE if the reward can be marked as claimed.
bool32 DexGame_ClaimReward(enum DexRewardTable table, u32 score, u32 seed)
{
    if (DexGame_RollReward(table, score, seed) && !CheckBagHasSpace(gSpecialVar_0x8004, gSpecialVar_0x8005))
    {
        gSpecialVar_Result = DEX_GAME_REWARD_BAG_FULL;
        return FALSE;
    }
    gSpecialVar_Result = score;
    return TRUE;
}

// Same as the start menu's save. Scripts save once before opening a daily puzzle
// (special SaveGame) so the saved map view and continue warp are current.
void DexGame_SaveProgress(void)
{
    if (gDifferentSaveFile == TRUE)
    {
        TrySavingData(SAVE_OVERWRITE_DIFFERENT_FILE);
        gDifferentSaveFile = FALSE;
    }
    else
    {
        TrySavingData(SAVE_NORMAL);
    }
}

static void Task_ResumeScript(u8 taskId)
{
    ScriptContext_Enable();
    DestroyTask(taskId);
}

// For specials with waitstate that decide not to open: the implicit waitstate
// runs after the special returns, so the script can only be resumed a frame later.
void DexGame_ResumeScriptNextFrame(void)
{
    CreateTask(Task_ResumeScript, 0);
}

// ---------------------------------------------------------------------------
// Screen
// ---------------------------------------------------------------------------

void DexGame_OpenScreen(const struct DexGameScreen *screen)
{
    sPendingScreen = screen;
    CreateTask(Task_OpenScreen, 0);
}

static void Task_OpenScreen(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    CleanupOverworldWindowsAndTilemaps();
    sScreenState = AllocZeroed(sizeof(struct DexGameScreenState));
    if (sScreenState == NULL)
    {
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
    }
    else
    {
        sScreenState->screen = sPendingScreen;
        SetMainCallback2(DexGame_SetupCB);
    }
    DestroyTask(taskId);
}

// Tile 0 is blank; tiles 1-9 are the corners, edges and middle of a box, row by row.
// Each pixel's value depends on its distance from the box's outer edge: a 1px gap,
// the outline, the inner outline, then fill.
static void BuildBoxTiles(u32 *tiles)
{
    static const u8 sPixelByDistance[] = {0, 2, 3};
    u32 piece, x, y;

    memset(tiles, 0, sizeof(u32) * 8);
    for (piece = 0; piece < 9; piece++)
    {
        u32 edgeX = piece % 3, edgeY = piece / 3;

        for (y = 0; y < 8; y++)
        {
            u32 row = 0;

            for (x = 0; x < 8; x++)
            {
                u32 dist = 8;

                if (edgeX == 0)
                    dist = min(dist, x);
                else if (edgeX == 2)
                    dist = min(dist, 7 - x);
                if (edgeY == 0)
                    dist = min(dist, y);
                else if (edgeY == 2)
                    dist = min(dist, 7 - y);
                row |= (dist < ARRAY_COUNT(sPixelByDistance) ? sPixelByDistance[dist] : 1) << (x * 4);
            }
            tiles[(piece + 1) * 8 + y] = row;
        }
    }
}

static void InitScreenWindows(void)
{
    const struct WindowTemplate *templates = sScreenState->screen->windows;
    u32 i;

    InitWindows(templates);
    DeactivateAllTextPrinters();
    for (i = 0; templates[i].bg != 0xFF; i++)
    {
        FillWindowPixelBuffer(i, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
        PutWindowTilemap(i);
        CopyWindowToVram(i, COPYWIN_FULL);
    }
    ScheduleBgCopyTilemapToVram(0);
}

static void DexGame_SetupCB(void)
{
    const struct DexGameScreen *screen = sScreenState->screen;

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
        if (!screen->init())
        {
            FadeAndBail();
            return;
        }
        ResetAllBgsCoordinates();
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, NELEMS(sBgTemplates));
        SetBgTilemapBuffer(BOX_BG, sScreenState->tilemap);
        ScheduleBgCopyTilemapToVram(BOX_BG);
        ShowBg(0);
        ShowBg(BOX_BG);
        gMain.state++;
        break;
    case 3:
        BuildBoxTiles(sScreenState->boxTiles);
        LoadBgTiles(BOX_BG, sScreenState->boxTiles, sizeof(sScreenState->boxTiles), 0);
        LoadPalette(sBoxPalettes, BG_PLTT_ID(0), sizeof(sBoxPalettes));
        LoadPalette(sTextPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
        LoadMonIconPalettes();
        gMain.state++;
        break;
    case 4:
        InitScreenWindows();
        gMain.state++;
        break;
    case 5:
        screen->draw();
        CreateTask(Task_WaitFadeIn, 0);
        gMain.state++;
        break;
    case 6:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    case 7:
        SetVBlankCallback(DexGame_VBlankCB);
        SetMainCallback2(DexGame_MainCB);
        break;
    }
}

static void DexGame_MainCB(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void DexGame_VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void Task_WaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = sScreenState->screen->inputTask;
}

void DexGame_ExitScreen(u8 taskId)
{
    PlaySE(SE_PC_OFF);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_WaitFadeAndExit;
}

static void Task_WaitFadeAndExit(u8 taskId)
{
    if (gPaletteFade.active)
        return;

    SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
    sScreenState->screen->free();
    FREE_AND_SET_NULL(sScreenState);
    FreeAllWindowBuffers();
    FreeMonIconPalettes();
    ResetSpriteData();
    DestroyTask(taskId);
}

static void FadeAndBail(void)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    CreateTask(Task_WaitFadeAndExit, 0);
    SetVBlankCallback(DexGame_VBlankCB);
    SetMainCallback2(DexGame_MainCB);
}

void DexGame_DrawBox(u32 left, u32 top, u32 width, u32 height, u32 palette)
{
    u32 x, y;

    for (y = 0; y < height; y++)
    {
        u32 edgeY = (y == 0) ? 0 : (y == height - 1) ? 2 : 1;

        for (x = 0; x < width; x++)
        {
            u32 edgeX = (x == 0) ? 0 : (x == width - 1) ? 2 : 1;
            FillBgTilemapBufferRect(BOX_BG, 1 + edgeY * 3 + edgeX, left + x, top + y, 1, 1, palette);
        }
    }
}

void DexGame_ClearBoxes(u32 x, u32 y, u32 width, u32 height)
{
    FillBgTilemapBufferRect(BOX_BG, 0, x, y, width, height, 0);
}

void DexGame_CopyBoxesToVram(void)
{
    ScheduleBgCopyTilemapToVram(BOX_BG);
}

void DexGame_PrintText(u32 windowId, u32 font, u32 x, u32 y, u32 color, const u8 *str)
{
    AddTextPrinterParameterized4(windowId, font, x, y, 0, 0, sFontColors[color], TEXT_SKIP_DRAW, str);
}

u8 DexGame_CreateIcon(u32 species, s16 x, s16 y)
{
    u8 spriteId = CreateMonIcon(species, SpriteCB_MonIcon, x, y, 0, 0);
    gSprites[spriteId].oam.priority = 0;
    return spriteId;
}

void DexGame_DestroyIcon(u8 *spriteId)
{
    if (*spriteId != SPRITE_NONE)
    {
        FreeAndDestroyMonIconSprite(&gSprites[*spriteId]);
        *spriteId = SPRITE_NONE;
    }
}

// ---------------------------------------------------------------------------
// Species picker
// ---------------------------------------------------------------------------

static u8 GetFirstLetter(const struct DexPicker *picker, u32 index)
{
    return GetSpeciesName(picker->pool->species[picker->pool->alphabetical[index]])[0];
}

static void SyncAlphabeticalIndex(struct DexPicker *picker)
{
    u32 i;

    for (i = 0; i < picker->pool->alphabeticalCount; i++)
    {
        if (picker->pool->alphabetical[i] == picker->dex)
        {
            picker->alphabeticalIndex = i;
            return;
        }
    }
}

void DexPicker_Init(struct DexPicker *picker, const struct DexPool *pool, u32 windowId, bool32 (*isUsed)(u32 dex))
{
    picker->pool = pool;
    picker->isUsed = isUsed;
    picker->windowId = windowId;
    picker->dex = DexPool_GetNthDex(pool, 0);
    picker->digit = 0;
    picker->byNumber = TRUE;
    SyncAlphabeticalIndex(picker);
    picker->iconSpriteId = SPRITE_NONE;
    picker->message = NULL;
}

static void DrawPicker(struct DexPicker *picker)
{
    u32 species = picker->pool->species[picker->dex];
    u8 *end;

    FillWindowPixelBuffer(picker->windowId, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    end = StringCopy(gStringVar4, COMPOUND_STRING("No."));
    end = ConvertIntToDecimalStringN(end, picker->dex, STR_CONV_MODE_LEADING_ZEROS, PICKER_DIGITS);
    end = StringCopy(end, COMPOUND_STRING(" "));
    StringCopy(end, GetSpeciesName(species));
    DexGame_PrintText(picker->windowId, FONT_NORMAL, 4, 1, picker->isUsed(picker->dex) ? DEX_FONT_GRAY : DEX_FONT_WHITE, gStringVar4);

    if (picker->message != NULL)
    {
        DexGame_PrintText(picker->windowId, FONT_NORMAL, 4, 16, DEX_FONT_RED, picker->message);
    }
    else if (picker->byNumber)
    {
        end = StringCopy(gStringVar4, COMPOUND_STRING("{DPAD_UPDOWN}+"));
        end = ConvertIntToDecimalStringN(end, sPowersOfTen[picker->digit], STR_CONV_MODE_LEFT_ALIGN, PICKER_DIGITS);
        StringCopy(end, COMPOUND_STRING(" {DPAD_LEFTRIGHT}Digit {SELECT_BUTTON}A-Z {A_BUTTON}OK {B_BUTTON}Back"));
        DexGame_PrintText(picker->windowId, FONT_SMALL, 4, 18, DEX_FONT_WHITE, gStringVar4);
    }
    else
    {
        DexGame_PrintText(picker->windowId, FONT_SMALL, 4, 18, DEX_FONT_WHITE,
                          COMPOUND_STRING("{DPAD_LEFTRIGHT}A-Z {DPAD_UPDOWN}Scroll {SELECT_BUTTON}By No. {A_BUTTON}OK {B_BUTTON}Back"));
    }

    CopyWindowToVram(picker->windowId, COPYWIN_GFX);
    DexGame_DestroyIcon(&picker->iconSpriteId);
    picker->iconSpriteId = DexGame_CreateIcon(species, DEX_PICKER_ICON_X, DEX_PICKER_ICON_Y);
}

void DexPicker_Show(struct DexPicker *picker)
{
    picker->message = NULL;
    DrawPicker(picker);
}

void DexPicker_Hide(struct DexPicker *picker)
{
    DexGame_DestroyIcon(&picker->iconSpriteId);
}

// Steps through dex numbers like the debug menu's species input, skipping unavailable species.
static bool32 StepDexNumber(struct DexPicker *picker, s32 delta)
{
    u32 dex = DexPool_StepAvailable(picker->pool, picker->dex, delta);

    if (dex == picker->dex)
        return FALSE;
    picker->dex = dex;
    return TRUE;
}

// Left goes to the start of the current letter, or of the previous letter if already there.
// Right goes to the start of the next letter.
static bool32 JumpLetter(struct DexPicker *picker, bool32 forward)
{
    u32 index = picker->alphabeticalIndex;
    u8 letter = GetFirstLetter(picker, index);

    if (forward)
    {
        while (index < picker->pool->alphabeticalCount - 1u && GetFirstLetter(picker, index) == letter)
            index++;
    }
    else
    {
        while (index > 0 && GetFirstLetter(picker, index - 1) == letter)
            index--;
        if (index == picker->alphabeticalIndex && index > 0)
        {
            letter = GetFirstLetter(picker, --index);
            while (index > 0 && GetFirstLetter(picker, index - 1) == letter)
                index--;
        }
    }

    if (index == picker->alphabeticalIndex)
        return FALSE;
    picker->alphabeticalIndex = index;
    picker->dex = picker->pool->alphabetical[index];
    return TRUE;
}

static bool32 StepAlphabetical(struct DexPicker *picker, s32 delta)
{
    s32 index = picker->alphabeticalIndex + delta;

    if (index < 0 || index >= picker->pool->alphabeticalCount)
        return FALSE;
    picker->alphabeticalIndex = index;
    picker->dex = picker->pool->alphabetical[index];
    return TRUE;
}

enum DexPickerResult DexPicker_HandleInput(struct DexPicker *picker)
{
    bool32 changed = FALSE;

    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        DexPicker_Hide(picker);
        return DEX_PICKER_CANCEL;
    }

    if (JOY_NEW(A_BUTTON))
    {
        if (picker->isUsed(picker->dex))
        {
            PlaySE(SE_BOO);
            picker->message = COMPOUND_STRING("Already guessed!");
            DrawPicker(picker);
            return DEX_PICKER_NONE;
        }
        DexPicker_Hide(picker);
        return DEX_PICKER_SUBMIT;
    }

    if (JOY_NEW(SELECT_BUTTON))
    {
        picker->byNumber = !picker->byNumber;
        if (!picker->byNumber)
            SyncAlphabeticalIndex(picker);
        changed = TRUE;
    }
    else if (picker->byNumber)
    {
        if (JOY_REPEAT(DPAD_UP))
            changed = StepDexNumber(picker, sPowersOfTen[picker->digit]);
        else if (JOY_REPEAT(DPAD_DOWN))
            changed = StepDexNumber(picker, -sPowersOfTen[picker->digit]);
        else if (JOY_NEW(DPAD_LEFT) && picker->digit > 0)
            changed = (picker->digit--, TRUE);
        else if (JOY_NEW(DPAD_RIGHT) && picker->digit < PICKER_DIGITS - 1)
            changed = (picker->digit++, TRUE);
    }
    else
    {
        if (JOY_REPEAT(DPAD_UP))
            changed = StepAlphabetical(picker, -1);
        else if (JOY_REPEAT(DPAD_DOWN))
            changed = StepAlphabetical(picker, 1);
        else if (JOY_REPEAT(DPAD_LEFT))
            changed = JumpLetter(picker, FALSE);
        else if (JOY_REPEAT(DPAD_RIGHT))
            changed = JumpLetter(picker, TRUE);
    }

    if (changed)
    {
        PlaySE(SE_SELECT);
        picker->message = NULL;
        DrawPicker(picker);
    }
    return DEX_PICKER_NONE;
}
