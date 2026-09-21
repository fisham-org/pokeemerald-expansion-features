#include "global.h"
#include "battle_main.h"
#include "dex_minigame.h"
#include "event_data.h"
#include "main.h"
#include "malloc.h"
#include "pokemon.h"
#include "random.h"
#include "sound.h"
#include "sprite.h"
#include "squirdle.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/songs.h"

static void GetTypes(const struct DexPool *pool, u32 dex, u32 *type1, u32 *type2)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[pool->species[dex]];

    *type1 = info->types[0];
    *type2 = (info->types[1] == info->types[0]) ? TYPE_NONE : info->types[1];
}

static u32 CompareValues(u32 guess, u32 target)
{
    if (guess == target)
        return SQUIRDLE_CLUE_CORRECT;
    return (target > guess) ? SQUIRDLE_CLUE_HIGHER : SQUIRDLE_CLUE_LOWER;
}

static u32 CompareType(u32 guessType, u32 targetSameSlot, u32 targetOtherSlot)
{
    if (guessType == targetSameSlot)
        return SQUIRDLE_CLUE_CORRECT;
    if (guessType != TYPE_NONE && guessType == targetOtherSlot)
        return SQUIRDLE_CLUE_OTHER_SLOT;
    return SQUIRDLE_CLUE_WRONG;
}

void Squirdle_Compare(const struct DexPool *pool, u32 guessDex, u32 targetDex, u8 *clues)
{
    const struct SpeciesInfo *guess = &gSpeciesInfo[pool->species[guessDex]];
    const struct SpeciesInfo *target = &gSpeciesInfo[pool->species[targetDex]];
    u32 guessType1, guessType2, targetType1, targetType2;

    GetTypes(pool, guessDex, &guessType1, &guessType2);
    GetTypes(pool, targetDex, &targetType1, &targetType2);

    clues[SQUIRDLE_ATTR_GEN] = CompareValues(DexPool_GetGeneration(guessDex), DexPool_GetGeneration(targetDex));
    clues[SQUIRDLE_ATTR_TYPE_1] = CompareType(guessType1, targetType1, targetType2);
    clues[SQUIRDLE_ATTR_TYPE_2] = CompareType(guessType2, targetType2, targetType1);
    clues[SQUIRDLE_ATTR_HEIGHT] = CompareValues(guess->height, target->height);
    clues[SQUIRDLE_ATTR_WEIGHT] = CompareValues(guess->weight, target->weight);
}

// Solved when every attribute matches, so a different species with identical clues also wins
bool32 Squirdle_IsSolved(const u8 *clues)
{
    u32 i;

    for (i = 0; i < SQUIRDLE_ATTR_COUNT; i++)
    {
        if (clues[i] != SQUIRDLE_CLUE_CORRECT)
            return FALSE;
    }
    return TRUE;
}

// One of the pool's available species. Deterministic for a given day and answer pool.
u32 Squirdle_GetDailyTarget(const struct DexPool *pool, u32 day)
{
    rng_value_t rng = LocalRandomSeed(day * 2246822519u + 0x5071);
    return DexPool_GetNthDex(pool, LocalRandom32(&rng) % pool->count);
}

#if SQUIRDLE_ENABLED

// Layout, in tiles. Each guess row has a 4x4 icon, then a line of text above a line of boxes.
#define LOG_TOP         4
#define ROW_HEIGHT      4
#define VISIBLE_ROWS    3
#define LOG_LEFT        4
#define BOX_HEIGHT      2
#define ICON_X          16

enum SquirdleMode
{
    MODE_BOARD,
    MODE_PICKER,
    MODE_SAVE_MESSAGE,
    MODE_SAVE,
};

enum SquirdleWindows
{
    WIN_TITLE,
    WIN_HEADERS,
    WIN_LOG,
    WIN_INFO,
};

struct SquirdleUi
{
    struct DexPool *pool;
    struct SquirdleSave *game; // The save block's game, or sPracticeGame
    struct DexPicker picker;
    u16 today;
    u8 mode;
    u8 scroll; // First visible row
    u8 rowIconSpriteIds[VISIBLE_ROWS];
    u8 targetIconSpriteId;
    bool8 practice;
    bool8 showStats;
};

static EWRAM_DATA struct SquirdleUi *sUi = NULL;
static EWRAM_DATA u8 sRequestedMode = 0;
static EWRAM_DATA struct SquirdleSave sPracticeGame = {0};
static EWRAM_DATA bool8 sPracticeGameReady = FALSE;

static const struct WindowTemplate sWindowTemplates[] =
{
    [WIN_TITLE]   = { .bg = 0, .tilemapLeft = 0,        .tilemapTop = 0,       .width = 30, .height = 2,  .paletteNum = 15, .baseBlock = 1 },
    [WIN_HEADERS] = { .bg = 0, .tilemapLeft = 0,        .tilemapTop = 2,       .width = 30, .height = 2,  .paletteNum = 15, .baseBlock = 61 },
    [WIN_LOG]     = { .bg = 0, .tilemapLeft = LOG_LEFT, .tilemapTop = LOG_TOP, .width = 26, .height = 12, .paletteNum = 15, .baseBlock = 121 },
    [WIN_INFO]    = { .bg = 0, .tilemapLeft = 0,        .tilemapTop = 16,      .width = 30, .height = 4,  .paletteNum = 15, .baseBlock = 433 },
    DUMMY_WIN_TEMPLATE
};

// Box column and width in tiles, for each attribute
static const struct { u8 x; u8 width; const u8 *label; } sColumns[SQUIRDLE_ATTR_COUNT] =
{
    [SQUIRDLE_ATTR_GEN]    = { 4,  4, COMPOUND_STRING("Gen") },
    [SQUIRDLE_ATTR_TYPE_1] = { 8,  6, COMPOUND_STRING("Type 1") },
    [SQUIRDLE_ATTR_TYPE_2] = { 14, 6, COMPOUND_STRING("Type 2") },
    [SQUIRDLE_ATTR_HEIGHT] = { 20, 5, COMPOUND_STRING("Hgt (m)") },
    [SQUIRDLE_ATTR_WEIGHT] = { 25, 5, COMPOUND_STRING("Wgt (kg)") },
};

static const u8 sClueBoxPalettes[] =
{
    [SQUIRDLE_CLUE_CORRECT]    = DEX_BOX_CORRECT,
    [SQUIRDLE_CLUE_WRONG]      = DEX_BOX_WRONG,
    [SQUIRDLE_CLUE_OTHER_SLOT] = DEX_BOX_PARTIAL,
    [SQUIRDLE_CLUE_HIGHER]     = DEX_BOX_EMPTY,
    [SQUIRDLE_CLUE_LOWER]      = DEX_BOX_EMPTY,
};

static bool32 Squirdle_Init(void);
static void Squirdle_Draw(void);
static void Task_SquirdleMainInput(u8 taskId);
static void Squirdle_Free(void);
static void DrawTitle(void);
static void DrawLog(void);
static void DrawInfo(void);

static const struct DexGameScreen sSquirdleScreen =
{
    .windows = sWindowTemplates,
    .init = Squirdle_Init,
    .draw = Squirdle_Draw,
    .inputTask = Task_SquirdleMainInput,
    .free = Squirdle_Free,
};

#define sSave (&gSaveBlock3Ptr->squirdle)

static u32 GetDailyStatus(u16 *today)
{
    return DexGame_GetDailyStatus(sSave->active, sSave->day, sSave->numGuesses > 0, sSave->finished, today);
}

void GetSquirdleStatus(void)
{
    u16 today;
    gSpecialVar_Result = GetDailyStatus(&today);
}

static void StartGame(struct SquirdleSave *game, const struct DexPool *pool, u32 targetDex, u32 day, u32 answerPool)
{
    game->day = day;
    game->target = pool->species[targetDex];
    memset(game->guesses, 0, sizeof(game->guesses));
    game->numGuesses = 0;
    game->answerPool = answerPool;
    game->active = TRUE;
    game->finished = FALSE;
    game->won = FALSE;
}

static u32 GetRandomTarget(const struct DexPool *pool)
{
    return DexPool_GetNthDex(pool, Random32() % pool->count);
}

// VAR_0x8004 is a DEX_GAME_MODE_* and VAR_0x8005 a DEX_POOL_* (which species can be guessed; the
// target is always one of them). Picks today's daily target (if there isn't one yet) or a practice
// target, before the screen fades. VAR_RESULT = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES if the answer
// pool is empty, or DEX_GAME_PREPARE_REWARD_WAITING if a new daily puzzle would replace one whose
// reward is unclaimed.
void PrepareSquirdle(void)
{
    u32 mode = gSpecialVar_0x8004, answerPool = gSpecialVar_0x8005;
    struct DexPool *pool;
    u16 today = 0;

    gSpecialVar_Result = DEX_GAME_PREPARE_OK;
    if (mode == DEX_GAME_MODE_DAILY && GetDailyStatus(&today) != DEX_GAME_STATUS_NEW)
        return;
    if (mode == DEX_GAME_MODE_DAILY && sSave->rewardPending)
    {
        gSpecialVar_Result = DEX_GAME_PREPARE_REWARD_WAITING;
        return;
    }

    pool = DexPool_Create(answerPool);
    if (pool == NULL || pool->count == 0)
    {
        gSpecialVar_Result = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES;
    }
    else if (mode == DEX_GAME_MODE_DAILY)
    {
        StartGame(sSave, pool, Squirdle_GetDailyTarget(pool, today), today, answerPool);
    }
    else
    {
        StartGame(&sPracticeGame, pool, GetRandomTarget(pool), 0, answerPool);
        sPracticeGameReady = TRUE;
    }
    TRY_FREE_AND_SET_NULL(pool);
}

// VAR_0x8004 is a DEX_GAME_MODE_*. Opens the game made by PrepareSquirdle. For daily mode, scripts
// should save the game (special SaveGame) first, since progress is saved after every guess.
// VAR_RESULT = FALSE if it didn't open (e.g. the day changed since PrepareSquirdle); the screen
// is still faded out then, so the script has to fade it back in.
void OpenSquirdle(void)
{
    u16 today;

    sRequestedMode = gSpecialVar_0x8004;
    if (sRequestedMode == DEX_GAME_MODE_PRACTICE ? sPracticeGameReady : DexGame_CanOpenDaily(GetDailyStatus(&today)))
    {
        gSpecialVar_Result = TRUE;
        DexGame_OpenScreen(&sSquirdleScreen);
    }
    else
    {
        gSpecialVar_Result = FALSE;
        DexGame_ResumeScriptNextFrame();
    }
}

// Once per won daily puzzle: VAR_RESULT = the guesses it took (1-8), and VAR_0x8004/VAR_0x8005/
// STR_VAR_1 = the item, quantity and message rolled from its reward tier (VAR_0x8004 = ITEM_NONE if
// no tier covers the score). If the item doesn't fit in the bag, VAR_RESULT = DEX_GAME_REWARD_BAG_FULL
// and the reward stays waiting. With no reward waiting, VAR_RESULT = DEX_GAME_NO_REWARD.
void ClaimSquirdleReward(void)
{
    if (sSave->rewardPending)
    {
        if (DexGame_ClaimReward(DEX_REWARDS_SQUIRDLE, sSave->numGuesses, sSave->day))
            sSave->rewardPending = FALSE;
    }
    else
    {
        gSpecialVar_Result = DEX_GAME_NO_REWARD;
    }
}

static u32 GetGuessDex(u32 index)
{
    return DexPool_GetDexOfSpecies(sUi->game->guesses[index]);
}

static u32 GetTargetDex(void)
{
    return DexPool_GetDexOfSpecies(sUi->game->target);
}

static bool32 IsAlreadyGuessed(u32 dex)
{
    u32 i;

    for (i = 0; i < sUi->game->numGuesses; i++)
    {
        if (GetGuessDex(i) == dex)
            return TRUE;
    }
    return FALSE;
}

// The log shows every guess, plus an empty row for the next one while playing
static u32 GetRowCount(void)
{
    return sUi->game->numGuesses + (sUi->game->finished ? 0 : 1);
}

static void ScrollToEnd(void)
{
    u32 rows = GetRowCount();
    sUi->scroll = (rows > VISIBLE_ROWS) ? rows - VISIBLE_ROWS : 0;
}

static bool32 Squirdle_Init(void)
{
    u32 i;

    sUi = AllocZeroed(sizeof(struct SquirdleUi));
    if (sUi == NULL)
        return FALSE;

    sUi->practice = (sRequestedMode == DEX_GAME_MODE_PRACTICE);
    sUi->game = sUi->practice ? &sPracticeGame : sSave;
    sUi->pool = DexPool_Create(sUi->game->answerPool);
    if (sUi->pool == NULL)
        return FALSE;
    if (!sUi->practice)
        GetDailyStatus(&sUi->today);

    for (i = 0; i < VISIBLE_ROWS; i++)
        sUi->rowIconSpriteIds[i] = SPRITE_NONE;
    sUi->targetIconSpriteId = SPRITE_NONE;
    sUi->showStats = sUi->game->finished && !sUi->practice;
    ScrollToEnd();
    DexPicker_Init(&sUi->picker, sUi->pool, WIN_INFO, IsAlreadyGuessed);
    return TRUE;
}

static void Squirdle_Draw(void)
{
    u32 i;

    FillWindowPixelBuffer(WIN_HEADERS, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    DexGame_PrintText(WIN_HEADERS, FONT_SMALL_NARROW, 4, 3, DEX_FONT_WHITE, COMPOUND_STRING("Guess"));
    for (i = 0; i < SQUIRDLE_ATTR_COUNT; i++)
        DexGame_PrintText(WIN_HEADERS, FONT_SMALL_NARROW, sColumns[i].x * 8 + 3, 3, DEX_FONT_WHITE, sColumns[i].label);
    CopyWindowToVram(WIN_HEADERS, COPYWIN_GFX);

    DrawTitle();
    DrawLog();
    DrawInfo();
}

static void Squirdle_Free(void)
{
    sPracticeGameReady = FALSE;
    if (sUi != NULL)
    {
        TRY_FREE_AND_SET_NULL(sUi->pool);
        FREE_AND_SET_NULL(sUi);
    }
}

static void RecordFinishedGame(void)
{
    struct SquirdleSave *game = sUi->game;

    game->finished = TRUE;
    PlaySE(game->won ? SE_SUCCESS : SE_FAILURE);
    if (sUi->practice)
        return;

    sUi->showStats = TRUE;
    if (game->won)
    {
        game->streak = (game->wins > 0 && game->lastWinDay + 1 == game->day) ? game->streak + 1 : 1;
        if (game->streak > game->bestStreak)
            game->bestStreak = game->streak;
        game->wins++;
        if (game->winsByGuesses[game->numGuesses - 1] < UCHAR_MAX)
            game->winsByGuesses[game->numGuesses - 1]++;
        game->lastWinDay = game->day;
        game->rewardPending = TRUE;
    }
    else
    {
        game->streak = 0;
    }
}

static void SubmitGuess(u32 dex)
{
    struct SquirdleSave *game = sUi->game;
    u8 clues[SQUIRDLE_ATTR_COUNT];

    game->guesses[game->numGuesses++] = sUi->pool->species[dex];
    if (!sUi->practice && game->numGuesses == 1)
        game->played++;

    Squirdle_Compare(sUi->pool, dex, GetTargetDex(), clues);
    game->won = Squirdle_IsSolved(clues);
    if (game->won || game->numGuesses == SQUIRDLE_MAX_GUESSES)
        RecordFinishedGame();
    else
        PlaySE(SE_SELECT);

    ScrollToEnd();
    DrawTitle();
    DrawLog();
    sUi->mode = sUi->practice ? MODE_BOARD : MODE_SAVE_MESSAGE;
    DrawInfo();
}

static void StartNewPracticeGame(void)
{
    PlaySE(SE_SELECT);
    StartGame(sUi->game, sUi->pool, GetRandomTarget(sUi->pool), 0, sUi->game->answerPool);
    ScrollToEnd();
    DrawTitle();
    DrawLog();
    DrawInfo();
}

static void HandleBoardInput(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        DexGame_ExitScreen(taskId);
    }
    else if (JOY_NEW(A_BUTTON) && !sUi->game->finished)
    {
        PlaySE(SE_SELECT);
        sUi->mode = MODE_PICKER;
        DexPicker_Show(&sUi->picker);
    }
    else if (JOY_NEW(START_BUTTON) && sUi->practice && sUi->game->finished)
    {
        StartNewPracticeGame();
    }
    else if (JOY_NEW(SELECT_BUTTON) && sUi->game->finished && !sUi->practice)
    {
        sUi->showStats = !sUi->showStats;
        DrawInfo();
    }
    else if (JOY_REPEAT(DPAD_UP) && sUi->scroll > 0)
    {
        PlaySE(SE_SELECT);
        sUi->scroll--;
        DrawLog();
    }
    else if (JOY_REPEAT(DPAD_DOWN) && sUi->scroll + VISIBLE_ROWS < GetRowCount())
    {
        PlaySE(SE_SELECT);
        sUi->scroll++;
        DrawLog();
    }
}

static void Task_SquirdleMainInput(u8 taskId)
{
    switch (sUi->mode)
    {
    case MODE_BOARD:
        HandleBoardInput(taskId);
        break;
    case MODE_PICKER:
        switch (DexPicker_HandleInput(&sUi->picker))
        {
        case DEX_PICKER_SUBMIT:
            SubmitGuess(sUi->picker.dex);
            break;
        case DEX_PICKER_CANCEL:
            sUi->mode = MODE_BOARD;
            DrawInfo();
            break;
        default:
            break;
        }
        break;
    case MODE_SAVE_MESSAGE:
        // Give the "Saving..." text a frame to reach VRAM before the save blocks
        sUi->mode = MODE_SAVE;
        break;
    case MODE_SAVE:
        DexGame_SaveProgress();
        sUi->mode = MODE_BOARD;
        DrawInfo();
        break;
    }
}

static void DrawTitle(void)
{
    struct SquirdleSave *game = sUi->game;
    u8 *end;

    FillWindowPixelBuffer(WIN_TITLE, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    DexGame_PrintText(WIN_TITLE, FONT_NORMAL, 4, 1, DEX_FONT_WHITE,
                      sUi->practice ? COMPOUND_STRING("SQUIRDLE  Practice") : COMPOUND_STRING("SQUIRDLE  Daily"));

    end = StringCopy(gStringVar4, COMPOUND_STRING("Guess "));
    end = ConvertIntToDecimalStringN(end, game->finished ? game->numGuesses : game->numGuesses + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
    end = StringCopy(end, COMPOUND_STRING("/"));
    ConvertIntToDecimalStringN(end, SQUIRDLE_MAX_GUESSES, STR_CONV_MODE_LEFT_ALIGN, 1);
    DexGame_PrintText(WIN_TITLE, FONT_NORMAL, 180, 1, DEX_FONT_WHITE, gStringVar4);
    CopyWindowToVram(WIN_TITLE, COPYWIN_GFX);
}

// Heights and weights are stored in tenths of a meter/kilogram
static u8 *FormatTenths(u8 *dest, u32 value)
{
    dest = ConvertIntToDecimalStringN(dest, value / 10, STR_CONV_MODE_LEFT_ALIGN, 4);
    dest = StringCopy(dest, COMPOUND_STRING("."));
    return ConvertIntToDecimalStringN(dest, value % 10, STR_CONV_MODE_LEFT_ALIGN, 1);
}

static void PrintClue(u32 row, u32 attribute, u32 dex, u32 clue)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[sUi->pool->species[dex]];
    u32 type1, type2;
    u8 *end = gStringVar4;

    GetTypes(sUi->pool, dex, &type1, &type2);
    switch (attribute)
    {
    case SQUIRDLE_ATTR_GEN:
        end = ConvertIntToDecimalStringN(end, DexPool_GetGeneration(dex), STR_CONV_MODE_LEFT_ALIGN, 1);
        break;
    case SQUIRDLE_ATTR_TYPE_1:
        end = StringCopy(end, gTypesInfo[type1].name);
        break;
    case SQUIRDLE_ATTR_TYPE_2:
        end = StringCopy(end, gTypesInfo[type2].name);
        break;
    case SQUIRDLE_ATTR_HEIGHT:
        end = FormatTenths(end, info->height);
        break;
    case SQUIRDLE_ATTR_WEIGHT:
        end = FormatTenths(end, info->weight);
        break;
    }
    if (clue == SQUIRDLE_CLUE_HIGHER)
        StringCopy(end, COMPOUND_STRING("{UP_ARROW}"));
    else if (clue == SQUIRDLE_CLUE_LOWER)
        StringCopy(end, COMPOUND_STRING("{DOWN_ARROW}"));

    DexGame_PrintText(WIN_LOG, FONT_SMALL_NARROW, (sColumns[attribute].x - LOG_LEFT) * 8 + 4,
                      row * ROW_HEIGHT * 8 + 18, DEX_FONT_DARK, gStringVar4);
}

static void DrawRow(u32 row, u32 index)
{
    u32 boxY = LOG_TOP + row * ROW_HEIGHT + (ROW_HEIGHT - BOX_HEIGHT);
    u32 textY = row * ROW_HEIGHT * 8 + 1;
    u8 clues[SQUIRDLE_ATTR_COUNT];
    u32 i, dex;
    u8 *end;

    end = ConvertIntToDecimalStringN(gStringVar4, index + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
    end = StringCopy(end, COMPOUND_STRING(". "));

    if (index >= sUi->game->numGuesses)
    {
        StringCopy(end, COMPOUND_STRING("{A_BUTTON} Guess"));
        DexGame_PrintText(WIN_LOG, FONT_NORMAL, 0, textY, DEX_FONT_GRAY, gStringVar4);
        for (i = 0; i < SQUIRDLE_ATTR_COUNT; i++)
            DexGame_DrawBox(sColumns[i].x, boxY, sColumns[i].width, BOX_HEIGHT, DEX_BOX_EMPTY);
        return;
    }

    dex = GetGuessDex(index);
    StringCopy(end, GetSpeciesName(sUi->pool->species[dex]));
    DexGame_PrintText(WIN_LOG, FONT_NORMAL, 0, textY, DEX_FONT_WHITE, gStringVar4);
    sUi->rowIconSpriteIds[row] = DexGame_CreateIcon(sUi->pool->species[dex], ICON_X, (LOG_TOP + row * ROW_HEIGHT) * 8 + 16);

    Squirdle_Compare(sUi->pool, dex, GetTargetDex(), clues);
    for (i = 0; i < SQUIRDLE_ATTR_COUNT; i++)
    {
        DexGame_DrawBox(sColumns[i].x, boxY, sColumns[i].width, BOX_HEIGHT, sClueBoxPalettes[clues[i]]);
        PrintClue(row, i, dex, clues[i]);
    }
}

static void DrawLog(void)
{
    u32 row;

    FillWindowPixelBuffer(WIN_LOG, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    DexGame_ClearBoxes(0, LOG_TOP, 30, VISIBLE_ROWS * ROW_HEIGHT);
    for (row = 0; row < VISIBLE_ROWS; row++)
    {
        DexGame_DestroyIcon(&sUi->rowIconSpriteIds[row]);
        if (sUi->scroll + row < GetRowCount())
            DrawRow(row, sUi->scroll + row);
    }
    CopyWindowToVram(WIN_LOG, COPYWIN_GFX);
    DexGame_CopyBoxesToVram();
}

static void PrintStats(void)
{
    struct SquirdleSave *game = sUi->game;
    u32 i;
    u8 *end;

    end = StringCopy(gStringVar4, COMPOUND_STRING("Played "));
    end = ConvertIntToDecimalStringN(end, game->played, STR_CONV_MODE_LEFT_ALIGN, 5);
    end = StringCopy(end, COMPOUND_STRING("  Won "));
    end = ConvertIntToDecimalStringN(end, game->played ? (game->wins * 100) / game->played : 0, STR_CONV_MODE_LEFT_ALIGN, 3);
    end = StringCopy(end, COMPOUND_STRING("%  Streak "));
    end = ConvertIntToDecimalStringN(end, (game->lastWinDay + 1 >= sUi->today) ? game->streak : 0, STR_CONV_MODE_LEFT_ALIGN, 5);
    end = StringCopy(end, COMPOUND_STRING("  Best "));
    ConvertIntToDecimalStringN(end, game->bestStreak, STR_CONV_MODE_LEFT_ALIGN, 5);
    DexGame_PrintText(WIN_INFO, FONT_SMALL, 4, 2, DEX_FONT_WHITE, gStringVar4);

    end = StringCopy(gStringVar4, COMPOUND_STRING("Wins by guess "));
    for (i = 0; i < SQUIRDLE_MAX_GUESSES; i++)
    {
        end = StringCopy(end, COMPOUND_STRING(" "));
        end = ConvertIntToDecimalStringN(end, i + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
        end = StringCopy(end, COMPOUND_STRING(":"));
        end = ConvertIntToDecimalStringN(end, game->winsByGuesses[i], STR_CONV_MODE_LEFT_ALIGN, 3);
    }
    DexGame_PrintText(WIN_INFO, FONT_SMALL_NARROWER, 4, 18, DEX_FONT_WHITE, gStringVar4);
}

static void PrintResult(void)
{
    struct SquirdleSave *game = sUi->game;
    u8 *end;

    if (game->won)
    {
        end = StringCopy(gStringVar4, COMPOUND_STRING("Solved in "));
        end = ConvertIntToDecimalStringN(end, game->numGuesses, STR_CONV_MODE_LEFT_ALIGN, 1);
        StringCopy(end, game->numGuesses == 1 ? COMPOUND_STRING(" guess!") : COMPOUND_STRING(" guesses!"));
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_GREEN, gStringVar4);
    }
    else
    {
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_RED, COMPOUND_STRING("Out of guesses!"));
    }

    end = StringCopy(gStringVar4, COMPOUND_STRING("It was "));
    end = StringCopy(end, GetSpeciesName(game->target));
    StringCopy(end, sUi->practice ? COMPOUND_STRING(".  {START_BUTTON} New") : COMPOUND_STRING(".  {SELECT_BUTTON} Stats"));
    DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 16, DEX_FONT_WHITE, gStringVar4);

    if (sUi->targetIconSpriteId == SPRITE_NONE)
        sUi->targetIconSpriteId = DexGame_CreateIcon(game->target, DEX_PICKER_ICON_X, DEX_PICKER_ICON_Y);
}

static void DrawInfo(void)
{
    FillWindowPixelBuffer(WIN_INFO, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    DexGame_DestroyIcon(&sUi->targetIconSpriteId);

    if (sUi->mode == MODE_SAVE_MESSAGE || sUi->mode == MODE_SAVE)
    {
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_WHITE, COMPOUND_STRING("Saving…"));
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 16, DEX_FONT_WHITE, COMPOUND_STRING("Don't turn off the power."));
    }
    else if (sUi->game->finished && sUi->showStats)
    {
        PrintStats();
    }
    else if (sUi->game->finished)
    {
        PrintResult();
    }
    else
    {
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_WHITE,
                          COMPOUND_STRING("{A_BUTTON} Guess  {DPAD_UPDOWN} Scroll  {B_BUTTON} Exit"));
        DexGame_PrintText(WIN_INFO, FONT_SMALL_NARROW, 4, 18, DEX_FONT_GRAY,
                          COMPOUND_STRING("Yellow: other type slot  {UP_ARROW}{DOWN_ARROW}: target higher/lower"));
    }

    CopyWindowToVram(WIN_INFO, COPYWIN_GFX);
}

#else

void GetSquirdleStatus(void)
{
    gSpecialVar_Result = DEX_GAME_STATUS_DISABLED;
}

void PrepareSquirdle(void)
{
    gSpecialVar_Result = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES;
}

void OpenSquirdle(void)
{
    DexGame_ResumeScriptNextFrame();
}

void ClaimSquirdleReward(void)
{
    gSpecialVar_Result = DEX_GAME_NO_REWARD;
}

#endif // SQUIRDLE_ENABLED
