#include "global.h"
#include "dex_minigame.h"
#include "event_data.h"
#include "main.h"
#include "malloc.h"
#include "pokedoku.h"
#include "pokemon.h"
#include "random.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/songs.h"

#if POKEDOKU_ENABLED

// Layout, in tiles. Row headers sit left of the grid, column headers above it.
#define CELL_WIDTH      6
#define CELL_HEIGHT     4
#define GRID_LEFT       11
#define GRID_TOP        4

#define ALL_CELLS_MASK  ((1 << POKEDOKU_NUM_CELLS) - 1)

enum PokedokuMode
{
    MODE_BOARD,
    MODE_PICKER,
    MODE_SAVE_MESSAGE,
    MODE_SAVE,
};

enum PokedokuWindows
{
    WIN_TITLE,
    WIN_COL_HEADER_0,
    WIN_COL_HEADER_1,
    WIN_COL_HEADER_2,
    WIN_ROW_HEADER_0,
    WIN_ROW_HEADER_1,
    WIN_ROW_HEADER_2,
    WIN_INFO,
};

struct PokedokuUi
{
    struct DexPool *pool;
    struct PokedokuSave *board; // The save block's board, or sPracticeBoard
    struct DexPicker picker;
    u32 cellTotals[POKEDOKU_NUM_CELLS]; // Popularity of every valid answer, for popularity percents
    u16 today;
    u8 mode;
    u8 cursorRow; // 0 is the column header row
    u8 cursorCol; // 0 is the row header column
    u8 iconSpriteIds[POKEDOKU_NUM_CELLS];
    bool8 practice;
    bool8 showStats;
};

static EWRAM_DATA struct PokedokuUi *sUi = NULL;
static EWRAM_DATA u8 sRequestedMode = 0;
static EWRAM_DATA struct PokedokuSave sPracticeBoard = {0};
static EWRAM_DATA bool8 sPracticeBoardReady = FALSE;

#define HEADER_WINDOW(col) \
    { .bg = 0, .tilemapLeft = GRID_LEFT + (col) * CELL_WIDTH, .tilemapTop = 0, .width = CELL_WIDTH, .height = GRID_TOP, .paletteNum = 15, .baseBlock = 45 + (col) * 24 }
#define ROW_WINDOW(row) \
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = GRID_TOP + (row) * CELL_HEIGHT, .width = GRID_LEFT, .height = CELL_HEIGHT, .paletteNum = 15, .baseBlock = 117 + (row) * 44 }

static const struct WindowTemplate sWindowTemplates[] =
{
    [WIN_TITLE]        = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = GRID_LEFT, .height = GRID_TOP, .paletteNum = 15, .baseBlock = 1 },
    [WIN_COL_HEADER_0] = HEADER_WINDOW(0),
    [WIN_COL_HEADER_1] = HEADER_WINDOW(1),
    [WIN_COL_HEADER_2] = HEADER_WINDOW(2),
    [WIN_ROW_HEADER_0] = ROW_WINDOW(0),
    [WIN_ROW_HEADER_1] = ROW_WINDOW(1),
    [WIN_ROW_HEADER_2] = ROW_WINDOW(2),
    [WIN_INFO]         = { .bg = 0, .tilemapLeft = 0, .tilemapTop = 16, .width = 30, .height = 4, .paletteNum = 15, .baseBlock = 249 },
    DUMMY_WIN_TEMPLATE
};

#undef HEADER_WINDOW
#undef ROW_WINDOW

static bool32 Pokedoku_Init(void);
static void Pokedoku_Draw(void);
static void Task_PokedokuMainInput(u8 taskId);
static void Pokedoku_Free(void);
static void DrawBoard(void);
static void DrawCell(u32 cell);
static void DrawTitle(void);
static void DrawHeaders(void);
static void DrawInfo(void);
static void DrawCellIcon(u32 cell);

static const struct DexGameScreen sPokedokuScreen =
{
    .windows = sWindowTemplates,
    .init = Pokedoku_Init,
    .draw = Pokedoku_Draw,
    .inputTask = Task_PokedokuMainInput,
    .free = Pokedoku_Free,
};

#define sSave (&gSaveBlock3Ptr->pokedoku)

static u32 GetDailyStatus(u16 *today)
{
    u32 cell;
    bool32 started = FALSE;

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
        started |= (sSave->guesses[cell] != SPECIES_NONE);
    return DexGame_GetDailyStatus(sSave->active, sSave->day, started, sSave->finished, today);
}

void GetPokedokuStatus(void)
{
    u16 today;
    gSpecialVar_Result = GetDailyStatus(&today);
}

static void StartBoard(struct PokedokuSave *board, const u8 *criteria, u32 day, u32 answerPool)
{
    memcpy(board->criteria, criteria, sizeof(board->criteria));
    board->day = day;
    memset(board->guesses, 0, sizeof(board->guesses));
    board->correctMask = 0;
    board->answerPool = answerPool;
    board->active = TRUE;
    board->finished = FALSE;
}

// VAR_0x8004 is a DEX_GAME_MODE_* and VAR_0x8005 a DEX_POOL_* (which species count as answers).
// Makes today's daily board (if it doesn't exist yet) or a practice board, before the screen fades.
// VAR_RESULT = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES if the answer pool can't make a solvable board,
// or DEX_GAME_PREPARE_REWARD_WAITING if a new daily board would replace one whose reward is unclaimed.
void PreparePokedoku(void)
{
    u32 mode = gSpecialVar_0x8004, answerPool = gSpecialVar_0x8005;
    u8 criteria[POKEDOKU_GRID_SIZE * 2];
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
    if (pool == NULL || !Pokedoku_GenerateBoard(pool, mode == DEX_GAME_MODE_DAILY ? today : Random32(), criteria))
    {
        gSpecialVar_Result = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES;
    }
    else if (mode == DEX_GAME_MODE_DAILY)
    {
        StartBoard(sSave, criteria, today, answerPool);
    }
    else
    {
        StartBoard(&sPracticeBoard, criteria, 0, answerPool);
        sPracticeBoardReady = TRUE;
    }
    TRY_FREE_AND_SET_NULL(pool);
}

// VAR_0x8004 is a DEX_GAME_MODE_*. Opens the board made by PreparePokedoku. For daily mode, scripts
// should save the game (special SaveGame) first, since progress is saved after every guess.
// VAR_RESULT = FALSE if it didn't open (e.g. the day changed since PreparePokedoku); the screen
// is still faded out then, so the script has to fade it back in.
void OpenPokedoku(void)
{
    u16 today;

    sRequestedMode = gSpecialVar_0x8004;
    if (sRequestedMode == DEX_GAME_MODE_PRACTICE ? sPracticeBoardReady : DexGame_CanOpenDaily(GetDailyStatus(&today)))
    {
        gSpecialVar_Result = TRUE;
        DexGame_OpenScreen(&sPokedokuScreen);
    }
    else
    {
        gSpecialVar_Result = FALSE;
        DexGame_ResumeScriptNextFrame();
    }
}

// Once per finished daily board: VAR_RESULT = its score (squares correct, 0-9), and
// VAR_0x8004/VAR_0x8005/STR_VAR_1 = the item, quantity and message rolled from its reward tier
// (VAR_0x8004 = ITEM_NONE if no tier covers the score). If the item doesn't fit in the bag,
// VAR_RESULT = DEX_GAME_REWARD_BAG_FULL and the reward stays waiting. With no reward waiting,
// VAR_RESULT = DEX_GAME_NO_REWARD.
void ClaimPokedokuReward(void)
{
    if (sSave->rewardPending)
    {
        if (DexGame_ClaimReward(DEX_REWARDS_POKEDOKU, __builtin_popcount(sSave->correctMask), sSave->day))
            sSave->rewardPending = FALSE;
    }
    else
    {
        gSpecialVar_Result = DEX_GAME_NO_REWARD;
    }
}

static u32 GetAttemptedMask(void)
{
    u32 cell, mask = 0;

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
    {
        if (sUi->board->guesses[cell] != SPECIES_NONE)
            mask |= 1 << cell;
    }
    return mask;
}

static bool32 IsCursorOnCell(void)
{
    return sUi->cursorRow != 0 && sUi->cursorCol != 0;
}

static u32 GetCursorCell(void)
{
    return (sUi->cursorRow - 1) * POKEDOKU_GRID_SIZE + (sUi->cursorCol - 1);
}

static u32 GetCellX(u32 cell)
{
    return GRID_LEFT + (cell % POKEDOKU_GRID_SIZE) * CELL_WIDTH;
}

static u32 GetCellY(u32 cell)
{
    return GRID_TOP + (cell / POKEDOKU_GRID_SIZE) * CELL_HEIGHT;
}

static void LoadCellTotals(void)
{
    u32 cell;

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
        sUi->cellTotals[cell] = Pokedoku_GetCellPopularityTotal(sUi->pool, sUi->board->criteria, cell);
}

static u32 GetRarityScore(void)
{
    return Pokedoku_GetRarityScore(sUi->pool, sUi->board, sUi->cellTotals);
}

static void RecordFinishedBoard(void)
{
    struct PokedokuSave *board = sUi->board;
    bool32 continuesStreak;

    board->finished = TRUE;
    sUi->showStats = TRUE;
    if (sUi->practice)
        return;

    continuesStreak = board->played > 0 && board->lastFinishedDay + 1 == board->day;
    board->played++;
    board->streak = continuesStreak ? board->streak + 1 : 1;
    if (board->streak > board->bestStreak)
        board->bestStreak = board->streak;

    if (board->correctMask == ALL_CELLS_MASK)
    {
        board->perfect++;
        board->perfectStreak = (continuesStreak && board->perfectStreak > 0) ? board->perfectStreak + 1 : 1;
    }
    else
    {
        board->perfectStreak = 0;
    }
    board->lastFinishedDay = board->day;
    board->rewardPending = TRUE;
    if (board->bestRarity == 0 || GetRarityScore() < board->bestRarity)
        board->bestRarity = GetRarityScore();
}

static bool32 IsAlreadyGuessed(u32 dex)
{
    u32 cell;

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
    {
        if (sUi->board->guesses[cell] != SPECIES_NONE && DexPool_GetDexOfSpecies(sUi->board->guesses[cell]) == dex)
            return TRUE;
    }
    return FALSE;
}

// For the results screen: the most and least popular answers in the player's answer pool
static void FindPopularityExtremes(u32 cell, u32 *mostDex, u32 *leastDex)
{
    const u16 *popularity = sUi->pool->popularity;
    u32 dex;

    *mostDex = *leastDex = 0;
    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        if (!sUi->pool->available[dex] || !Pokedoku_IsValidGuess(sUi->pool, sUi->board->criteria, cell, dex))
            continue;
        if (*mostDex == 0 || popularity[dex] > popularity[*mostDex])
            *mostDex = dex;
        if (*leastDex == 0 || popularity[dex] < popularity[*leastDex])
            *leastDex = dex;
    }
}

static bool32 Pokedoku_Init(void)
{
    u32 i;

    sUi = AllocZeroed(sizeof(struct PokedokuUi));
    if (sUi == NULL)
        return FALSE;

    sUi->practice = (sRequestedMode == DEX_GAME_MODE_PRACTICE);
    sUi->board = sUi->practice ? &sPracticeBoard : sSave;
    sUi->pool = DexPool_Create(sUi->board->answerPool);
    if (sUi->pool == NULL)
        return FALSE;
    DexPool_LoadPopularity(sUi->pool);
    LoadCellTotals();
    if (!sUi->practice)
        GetDailyStatus(&sUi->today);

    sUi->cursorRow = 1;
    sUi->cursorCol = 1;
    sUi->showStats = sUi->board->finished;
    for (i = 0; i < POKEDOKU_NUM_CELLS; i++)
        sUi->iconSpriteIds[i] = SPRITE_NONE;
    DexPicker_Init(&sUi->picker, sUi->pool, WIN_INFO, IsAlreadyGuessed);
    return TRUE;
}

static void Pokedoku_Draw(void)
{
    u32 cell;

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
        DrawCellIcon(cell);
    DrawBoard();
}

static void Pokedoku_Free(void)
{
    sPracticeBoardReady = FALSE;
    if (sUi != NULL)
    {
        TRY_FREE_AND_SET_NULL(sUi->pool);
        FREE_AND_SET_NULL(sUi);
    }
}

static void MoveCursor(s32 rowDelta, s32 colDelta)
{
    s32 row = sUi->cursorRow + rowDelta;
    s32 col = sUi->cursorCol + colDelta;
    u32 oldRow = sUi->cursorRow, oldCol = sUi->cursorCol;

    // The top-left corner holds the title, not a header
    if (row < 0 || row > POKEDOKU_GRID_SIZE || col < 0 || col > POKEDOKU_GRID_SIZE || (row == 0 && col == 0))
        return;

    PlaySE(SE_SELECT);
    sUi->cursorRow = row;
    sUi->cursorCol = col;
    sUi->showStats = FALSE;
    if (oldRow != 0 && oldCol != 0)
        DrawCell((oldRow - 1) * POKEDOKU_GRID_SIZE + (oldCol - 1));
    if (IsCursorOnCell())
        DrawCell(GetCursorCell());
    DrawHeaders();
    DrawInfo();
    DexGame_CopyBoxesToVram();
}

static void StartNewPracticeBoard(void)
{
    u8 criteria[POKEDOKU_GRID_SIZE * 2];
    u32 cell;

    // The answer pool made one board already, so this only fails if it's very small and unlucky
    if (!Pokedoku_GenerateBoard(sUi->pool, Random32(), criteria))
    {
        PlaySE(SE_BOO);
        return;
    }

    PlaySE(SE_SELECT);
    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
        DexGame_DestroyIcon(&sUi->iconSpriteIds[cell]);
    StartBoard(sUi->board, criteria, 0, sUi->board->answerPool);
    LoadCellTotals();
    sUi->showStats = FALSE;
    DrawBoard();
}

static void HandleBoardInput(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        DexGame_ExitScreen(taskId);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        if (IsCursorOnCell() && !sUi->board->finished && sUi->board->guesses[GetCursorCell()] == SPECIES_NONE)
        {
            PlaySE(SE_SELECT);
            sUi->mode = MODE_PICKER;
            DexPicker_Show(&sUi->picker);
        }
    }
    else if (JOY_NEW(START_BUTTON) && sUi->practice && sUi->board->finished)
    {
        StartNewPracticeBoard();
    }
    else if (JOY_NEW(SELECT_BUTTON) && sUi->board->finished)
    {
        sUi->showStats = !sUi->showStats;
        DrawInfo();
    }
    else if (JOY_REPEAT(DPAD_UP))
    {
        MoveCursor(-1, 0);
    }
    else if (JOY_REPEAT(DPAD_DOWN))
    {
        MoveCursor(1, 0);
    }
    else if (JOY_REPEAT(DPAD_LEFT))
    {
        MoveCursor(0, -1);
    }
    else if (JOY_REPEAT(DPAD_RIGHT))
    {
        MoveCursor(0, 1);
    }
}

static void SubmitGuess(u32 dex)
{
    u32 cell = GetCursorCell();

    sUi->board->guesses[cell] = sUi->pool->species[dex];
    if (Pokedoku_IsValidGuess(sUi->pool, sUi->board->criteria, cell, dex))
    {
        sUi->board->correctMask |= 1 << cell;
        PlaySE(SE_SUCCESS);
    }
    else
    {
        PlaySE(SE_FAILURE);
    }

    if (GetAttemptedMask() == ALL_CELLS_MASK)
        RecordFinishedBoard();

    DrawCellIcon(cell);
    DrawCell(cell);
    DrawTitle();
    DexGame_CopyBoxesToVram();
    sUi->mode = sUi->practice ? MODE_BOARD : MODE_SAVE_MESSAGE;
    DrawInfo();
}

static void Task_PokedokuMainInput(u8 taskId)
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

static void DrawCell(u32 cell)
{
    u32 palette;

    if (sUi->board->correctMask & (1 << cell))
        palette = DEX_BOX_CORRECT;
    else if (sUi->board->guesses[cell] != SPECIES_NONE)
        palette = DEX_BOX_WRONG;
    else
        palette = DEX_BOX_EMPTY;

    if (IsCursorOnCell() && GetCursorCell() == cell)
        palette += DEX_BOX_CURSOR_OFFSET;

    DexGame_DrawBox(GetCellX(cell), GetCellY(cell), CELL_WIDTH, CELL_HEIGHT, palette);
}

static void DrawTitle(void)
{
    u32 attempted = __builtin_popcount(GetAttemptedMask());
    u8 *end;

    FillWindowPixelBuffer(WIN_TITLE, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    DexGame_PrintText(WIN_TITLE, FONT_NORMAL, 4, 1, DEX_FONT_WHITE, COMPOUND_STRING("POKéDOKU"));
    if (sUi->board->finished)
    {
        end = ConvertIntToDecimalStringN(gStringVar4, __builtin_popcount(sUi->board->correctMask), STR_CONV_MODE_LEFT_ALIGN, 1);
        end = StringCopy(end, COMPOUND_STRING("/9  Rarity "));
        ConvertIntToDecimalStringN(end, GetRarityScore(), STR_CONV_MODE_LEFT_ALIGN, 3);
        DexGame_PrintText(WIN_TITLE, FONT_SMALL, 4, 17, DEX_FONT_WHITE, gStringVar4);
        CopyWindowToVram(WIN_TITLE, COPYWIN_GFX);
        return;
    }
    else
    {
        end = StringCopy(gStringVar4, sUi->practice ? COMPOUND_STRING("Practice ") : COMPOUND_STRING("Guesses: "));
        end = ConvertIntToDecimalStringN(end, POKEDOKU_NUM_CELLS - attempted, STR_CONV_MODE_LEFT_ALIGN, 1);
    }
    end = StringCopy(end, COMPOUND_STRING("/"));
    ConvertIntToDecimalStringN(end, POKEDOKU_NUM_CELLS, STR_CONV_MODE_LEFT_ALIGN, 1);
    DexGame_PrintText(WIN_TITLE, FONT_SMALL, 4, 17, DEX_FONT_WHITE, gStringVar4);
    CopyWindowToVram(WIN_TITLE, COPYWIN_GFX);
}

// Column headers are narrow, so squeeze long labels
static u32 GetHeaderFont(u32 windowId, const u8 *str)
{
    if (GetStringWidth(FONT_SMALL_NARROW, str, 0) > WindowWidthPx(windowId) - 2)
        return FONT_SMALL_NARROWER;
    return FONT_SMALL_NARROW;
}

static void DrawHeaders(void)
{
    u32 i;

    for (i = 0; i < POKEDOKU_GRID_SIZE; i++)
    {
        u32 colWindow = WIN_COL_HEADER_0 + i;
        u32 rowWindow = WIN_ROW_HEADER_0 + i;
        const u8 *colName = gPokedokuCriteria[sUi->board->criteria[POKEDOKU_GRID_SIZE + i]].name;
        const u8 *rowName = gPokedokuCriteria[sUi->board->criteria[i]].name;

        FillWindowPixelBuffer(colWindow, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
        DexGame_PrintText(colWindow, GetHeaderFont(colWindow, colName), 2, 4,
                          (sUi->cursorCol == i + 1) ? DEX_FONT_HIGHLIGHT : DEX_FONT_WHITE, colName);
        CopyWindowToVram(colWindow, COPYWIN_GFX);

        FillWindowPixelBuffer(rowWindow, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
        DexGame_PrintText(rowWindow, FONT_SMALL_NARROW, 4, 4,
                          (sUi->cursorRow == i + 1) ? DEX_FONT_HIGHLIGHT : DEX_FONT_WHITE, rowName);
        CopyWindowToVram(rowWindow, COPYWIN_GFX);
    }
}

static void PrintSpeciesLine(const u8 *prefix, u32 species, u32 color)
{
    u8 *end = StringCopy(gStringVar4, prefix);
    StringCopy(end, GetSpeciesName(species));
    DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, color, gStringVar4);
}

// "<prefix>31%" on the info bar's second line
static void PrintPopularityLine(const u8 *prefix, u32 dex, u32 cell)
{
    u8 *end = StringCopy(gStringVar4, prefix);
    end = ConvertIntToDecimalStringN(end, Pokedoku_GetPopularityPercent(sUi->pool, dex, sUi->cellTotals[cell]), STR_CONV_MODE_LEFT_ALIGN, 3);
    StringCopy(end, COMPOUND_STRING("%"));
    DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 16, DEX_FONT_WHITE, gStringVar4);
}

static u8 *AppendNameAndPercent(u8 *end, u32 dex, u32 cell)
{
    end = StringCopy(end, GetSpeciesName(sUi->pool->species[dex]));
    end = StringCopy(end, COMPOUND_STRING(" "));
    end = ConvertIntToDecimalStringN(end, Pokedoku_GetPopularityPercent(sUi->pool, dex, sUi->cellTotals[cell]), STR_CONV_MODE_LEFT_ALIGN, 3);
    return StringCopy(end, COMPOUND_STRING("%"));
}

// Finished boards: the player's pick, then the cell's most and least popular answers
static void PrintCellResult(u32 cell)
{
    u32 species = sUi->board->guesses[cell];
    u32 mostDex, leastDex;
    u8 *end;

    if (sUi->board->correctMask & (1 << cell))
    {
        end = StringCopy(gStringVar4, COMPOUND_STRING("Correct: "));
        end = AppendNameAndPercent(end, DexPool_GetDexOfSpecies(species), cell);
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_GREEN, gStringVar4);
    }
    else if (species != SPECIES_NONE)
    {
        PrintSpeciesLine(COMPOUND_STRING("Wrong: "), species, DEX_FONT_RED);
    }

    FindPopularityExtremes(cell, &mostDex, &leastDex);
    if (mostDex == 0)
        return;
    if (mostDex == leastDex)
    {
        end = StringCopy(gStringVar4, COMPOUND_STRING("Only answer: "));
        AppendNameAndPercent(end, mostDex, cell);
    }
    else
    {
        end = StringCopy(gStringVar4, COMPOUND_STRING("Top: "));
        end = AppendNameAndPercent(end, mostDex, cell);
        end = StringCopy(end, COMPOUND_STRING("   Rarest: "));
        AppendNameAndPercent(end, leastDex, cell);
    }
    DexGame_PrintText(WIN_INFO, FONT_SMALL_NARROW, 4, 19, DEX_FONT_WHITE, gStringVar4);
}

static void PrintStats(void)
{
    struct PokedokuSave *board = sUi->board;
    bool32 streakAlive = board->lastFinishedDay + 1 >= sUi->today;
    u8 *end;

    if (sUi->practice)
    {
        end = StringCopy(gStringVar4, COMPOUND_STRING("Practice: "));
        end = ConvertIntToDecimalStringN(end, __builtin_popcount(board->correctMask), STR_CONV_MODE_LEFT_ALIGN, 1);
        end = StringCopy(end, COMPOUND_STRING("/9  Rarity "));
        ConvertIntToDecimalStringN(end, GetRarityScore(), STR_CONV_MODE_LEFT_ALIGN, 3);
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_WHITE, gStringVar4);
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 16, DEX_FONT_WHITE, COMPOUND_STRING("{START_BUTTON} New board  {B_BUTTON} Exit"));
        return;
    }

    end = StringCopy(gStringVar4, COMPOUND_STRING("Played "));
    end = ConvertIntToDecimalStringN(end, board->played, STR_CONV_MODE_LEFT_ALIGN, 5);
    end = StringCopy(end, COMPOUND_STRING("  Perfect "));
    end = ConvertIntToDecimalStringN(end, board->played ? (board->perfect * 100) / board->played : 0, STR_CONV_MODE_LEFT_ALIGN, 3);
    end = StringCopy(end, COMPOUND_STRING("%  Best rarity "));
    ConvertIntToDecimalStringN(end, board->bestRarity, STR_CONV_MODE_LEFT_ALIGN, 3);
    DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_WHITE, gStringVar4);

    end = StringCopy(gStringVar4, COMPOUND_STRING("Streak "));
    end = ConvertIntToDecimalStringN(end, streakAlive ? board->streak : 0, STR_CONV_MODE_LEFT_ALIGN, 5);
    end = StringCopy(end, COMPOUND_STRING("   Best "));
    end = ConvertIntToDecimalStringN(end, board->bestStreak, STR_CONV_MODE_LEFT_ALIGN, 5);
    end = StringCopy(end, COMPOUND_STRING("   Perfect streak "));
    ConvertIntToDecimalStringN(end, streakAlive ? board->perfectStreak : 0, STR_CONV_MODE_LEFT_ALIGN, 5);
    DexGame_PrintText(WIN_INFO, FONT_SMALL, 4, 17, DEX_FONT_WHITE, gStringVar4);
}

static void DrawInfo(void)
{
    const u8 *criteria = sUi->board->criteria;

    FillWindowPixelBuffer(WIN_INFO, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    if (sUi->mode == MODE_SAVE_MESSAGE || sUi->mode == MODE_SAVE)
    {
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_WHITE, COMPOUND_STRING("Saving…"));
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 16, DEX_FONT_WHITE, COMPOUND_STRING("Don't turn off the power."));
    }
    else if (sUi->showStats)
    {
        PrintStats();
    }
    else if (!IsCursorOnCell())
    {
        u32 criterion = (sUi->cursorRow == 0) ? criteria[POKEDOKU_GRID_SIZE + sUi->cursorCol - 1] : criteria[sUi->cursorRow - 1];
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_WHITE, gPokedokuCriteria[criterion].description);
    }
    else
    {
        u32 cell = GetCursorCell();
        u32 species = sUi->board->guesses[cell];

        if (sUi->board->finished)
        {
            PrintCellResult(cell);
        }
        else if (sUi->board->correctMask & (1 << cell))
        {
            PrintSpeciesLine(COMPOUND_STRING("Correct: "), species, DEX_FONT_GREEN);
            PrintPopularityLine(COMPOUND_STRING("Popularity: "), DexPool_GetDexOfSpecies(species), cell);
        }
        else if (species != SPECIES_NONE)
        {
            PrintSpeciesLine(COMPOUND_STRING("Wrong: "), species, DEX_FONT_RED);
        }
        else
        {
            DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_WHITE, gPokedokuCriteria[criteria[sUi->cursorRow - 1]].description);
            DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 16, DEX_FONT_WHITE, gPokedokuCriteria[criteria[POKEDOKU_GRID_SIZE + sUi->cursorCol - 1]].description);
        }
    }

    CopyWindowToVram(WIN_INFO, COPYWIN_GFX);
}

static void DrawBoard(void)
{
    u32 cell;

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
        DrawCell(cell);
    DexGame_CopyBoxesToVram();
    DrawTitle();
    DrawHeaders();
    DrawInfo();
}

// Correct cells show their species. Wrong ones stay empty, marked by their color.
static void DrawCellIcon(u32 cell)
{
    if (!(sUi->board->correctMask & (1 << cell)) || sUi->iconSpriteIds[cell] != SPRITE_NONE)
        return;

    sUi->iconSpriteIds[cell] = DexGame_CreateIcon(sUi->board->guesses[cell],
                                                  GetCellX(cell) * 8 + CELL_WIDTH * 4,
                                                  GetCellY(cell) * 8 + CELL_HEIGHT * 4);
}

#else

void GetPokedokuStatus(void)
{
    gSpecialVar_Result = DEX_GAME_STATUS_DISABLED;
}

void PreparePokedoku(void)
{
    gSpecialVar_Result = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES;
}

void OpenPokedoku(void)
{
    DexGame_ResumeScriptNextFrame();
}

void ClaimPokedokuReward(void)
{
    gSpecialVar_Result = DEX_GAME_NO_REWARD;
}

#endif // POKEDOKU_ENABLED
