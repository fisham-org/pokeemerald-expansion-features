#include "global.h"
#include "dex_minigame.h"
#include "event_data.h"
#include "main.h"
#include "malloc.h"
#include "pokeconnect.h"
#include "pokemon.h"
#include "random.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/songs.h"

#if POKECONNECT_ENABLED

// Layout, in tiles. A 4x4 grid of tiles fills the screen above the info bar. Found groups
// become a full-width row at the top of the grid, in the order they were found.
#define GRID_LEFT       1
#define CELL_WIDTH      7
#define CELL_HEIGHT     4
#define GRID_WIDTH      (CELL_WIDTH * POKECONNECT_GROUP_SIZE)
#define GRID_HEIGHT     (CELL_HEIGHT * POKECONNECT_NUM_GROUPS)

#define ALL_GROUPS_MASK ((1 << POKECONNECT_NUM_GROUPS) - 1)

enum PokeconnectMode
{
    MODE_BOARD,
    MODE_SAVE_MESSAGE,
    MODE_SAVE,
};

enum PokeconnectWindows
{
    WIN_GRID,
    WIN_INFO,
};

struct PokeconnectUi
{
    struct DexPool *pool;
    struct PokeconnectSave *game; // The save block's game, or sPracticeGame
    const u8 *message;            // Shown in the info bar until the next input
    u16 today;
    u16 selectedMask;             // Tiles, not positions
    u8 mode;
    u8 cursor;                    // Position in the grid
    u8 messageColor;
    u8 iconSpriteIds[POKECONNECT_NUM_TILES]; // By position
    bool8 practice;
    bool8 showStats;
};

static EWRAM_DATA struct PokeconnectUi *sUi = NULL;
static EWRAM_DATA u8 sRequestedMode = 0;
static EWRAM_DATA struct PokeconnectSave sPracticeGame = {0};
static EWRAM_DATA bool8 sPracticeGameReady = FALSE;

static const struct WindowTemplate sWindowTemplates[] =
{
    [WIN_GRID] = { .bg = 0, .tilemapLeft = GRID_LEFT, .tilemapTop = 0,           .width = GRID_WIDTH, .height = GRID_HEIGHT, .paletteNum = 15, .baseBlock = 1 },
    [WIN_INFO] = { .bg = 0, .tilemapLeft = 0,         .tilemapTop = GRID_HEIGHT, .width = 30,         .height = 4,           .paletteNum = 15, .baseBlock = 1 + GRID_WIDTH * GRID_HEIGHT },
    DUMMY_WIN_TEMPLATE
};

// Group colors, easiest first
static const u8 sGroupPalettes[POKECONNECT_NUM_GROUPS] = { DEX_BOX_PARTIAL, DEX_BOX_CORRECT, DEX_BOX_BLUE, DEX_BOX_PURPLE };

static bool32 Pokeconnect_Init(void);
static void Pokeconnect_Draw(void);
static void Task_PokeconnectMainInput(u8 taskId);
static void Pokeconnect_Free(void);
static void DrawGrid(void);
static void DrawTile(u32 position);
static void DrawInfo(void);
static void DrawCursor(void);

static const u16 sAccentPalette[] = INCBIN_U16("graphics/dex_minigames/pokeconnect.gbapal");

static const struct DexGameScreen sPokeconnectScreen =
{
    .windows = sWindowTemplates,
    .accentPalette = sAccentPalette,
    .init = Pokeconnect_Init,
    .draw = Pokeconnect_Draw,
    .inputTask = Task_PokeconnectMainInput,
    .free = Pokeconnect_Free,
};

#define sSave (&gSaveBlock3Ptr->pokeconnect)

static u32 GetDailyStatus(u16 *today)
{
    return DexGame_GetDailyStatus(sSave->active, sSave->day, sSave->solvedMask != 0 || sSave->mistakes != 0, sSave->finished, today);
}

void GetPokeconnectStatus(void)
{
    u16 today;
    gSpecialVar_Result = GetDailyStatus(&today);
}

// The puzzle itself is already in the game, from Pokeconnect_GeneratePuzzle
static void StartGame(struct PokeconnectSave *game, u32 day, u32 answerPool)
{
    game->day = day;
    memset(game->wrongGuesses, 0, sizeof(game->wrongGuesses));
    game->solvedMask = 0;
    game->mistakes = 0;
    game->answerPool = answerPool;
    game->active = TRUE;
    game->finished = FALSE;
    game->won = FALSE;
}

// VAR_0x8004 is a DEX_GAME_MODE_* and VAR_0x8005 a DEX_POOL_* (every tile is from it).
// Makes today's daily puzzle (if it doesn't exist yet) or a practice puzzle, before the screen fades.
// VAR_RESULT = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES if the answer pool can't make a puzzle,
// or DEX_GAME_PREPARE_REWARD_WAITING if a new daily puzzle would replace one whose reward is unclaimed.
void PreparePokeconnect(void)
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
    if (pool == NULL || !Pokeconnect_GeneratePuzzle(pool, mode == DEX_GAME_MODE_DAILY ? today : Random32(),
                                                    mode == DEX_GAME_MODE_DAILY ? sSave : &sPracticeGame))
    {
        gSpecialVar_Result = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES;
    }
    else if (mode == DEX_GAME_MODE_DAILY)
    {
        StartGame(sSave, today, answerPool);
    }
    else
    {
        StartGame(&sPracticeGame, 0, answerPool);
        sPracticeGameReady = TRUE;
    }
    TRY_FREE_AND_SET_NULL(pool);
}

// VAR_0x8004 is a DEX_GAME_MODE_*. Opens the puzzle made by PreparePokeconnect. For daily mode, scripts
// should save the game (special SaveGame) first, since progress is saved after every guess.
// VAR_RESULT = FALSE if it didn't open (e.g. the day changed since PreparePokeconnect); the screen
// is still faded out then, so the script has to fade it back in.
void OpenPokeconnect(void)
{
    u16 today;

    sRequestedMode = gSpecialVar_0x8004;
    if (sRequestedMode == DEX_GAME_MODE_PRACTICE ? sPracticeGameReady : DexGame_CanOpenDaily(GetDailyStatus(&today)))
    {
        gSpecialVar_Result = TRUE;
        DexGame_OpenScreen(&sPokeconnectScreen);
    }
    else
    {
        gSpecialVar_Result = FALSE;
        DexGame_ResumeScriptNextFrame();
    }
}

// Once per won daily puzzle: VAR_RESULT = its mistakes (0-3), and VAR_0x8004/VAR_0x8005/STR_VAR_1 =
// the item, quantity and message rolled from its reward tier (VAR_0x8004 = ITEM_NONE if no tier
// covers the score). If the item doesn't fit in the bag, VAR_RESULT = DEX_GAME_REWARD_BAG_FULL and
// the reward stays waiting. With no reward waiting, VAR_RESULT = DEX_GAME_NO_REWARD.
void ClaimPokeconnectReward(void)
{
    if (sSave->rewardPending)
    {
        if (DexGame_ClaimReward(DEX_REWARDS_POKECONNECT, sSave->mistakes, sSave->day))
            sSave->rewardPending = FALSE;
    }
    else
    {
        gSpecialVar_Result = DEX_GAME_NO_REWARD;
    }
}

// Rows at the top of the grid showing a group: the found ones, or all of them once finished
static u32 GetGroupRowCount(void)
{
    return sUi->game->finished ? POKECONNECT_NUM_GROUPS : __builtin_popcount(sUi->game->solvedMask);
}

static u32 GetFirstTilePosition(void)
{
    return GetGroupRowCount() * POKECONNECT_GROUP_SIZE;
}

static u32 GetTileX(u32 position)
{
    return GRID_LEFT + (position % POKECONNECT_GROUP_SIZE) * CELL_WIDTH;
}

static u32 GetTileY(u32 position)
{
    return (position / POKECONNECT_GROUP_SIZE) * CELL_HEIGHT;
}

static u32 GetCursorTile(void)
{
    return sUi->game->order[sUi->cursor];
}

static bool32 Pokeconnect_Init(void)
{
    u32 i;

    sUi = AllocZeroed(sizeof(struct PokeconnectUi));
    if (sUi == NULL)
        return FALSE;

    sUi->practice = (sRequestedMode == DEX_GAME_MODE_PRACTICE);
    sUi->game = sUi->practice ? &sPracticeGame : sSave;
    sUi->pool = DexPool_Create(sUi->game->answerPool);
    if (sUi->pool == NULL)
        return FALSE;
    if (!sUi->practice)
        GetDailyStatus(&sUi->today);

    for (i = 0; i < POKECONNECT_NUM_TILES; i++)
        sUi->iconSpriteIds[i] = SPRITE_NONE;
    sUi->cursor = sUi->game->finished ? 0 : GetFirstTilePosition();
    return TRUE;
}

static void Pokeconnect_Draw(void)
{
    DexGame_DrawBox(0, GRID_HEIGHT, 30, 4, DEX_BOX_EMPTY);
    DrawGrid();
    DrawInfo();
}

static void Pokeconnect_Free(void)
{
    sPracticeGameReady = FALSE;
    if (sUi != NULL)
    {
        TRY_FREE_AND_SET_NULL(sUi->pool);
        FREE_AND_SET_NULL(sUi);
    }
}

static void SetMessage(const u8 *message, u32 color)
{
    sUi->message = message;
    sUi->messageColor = color;
}

// Moves a group's tiles to just after the frontTiles positions already showing groups,
// keeping the order of the rest
static void MoveGroupToFront(u32 group, u32 frontTiles)
{
    u8 order[POKECONNECT_NUM_TILES];
    u32 i, count = frontTiles;

    memcpy(order, sUi->game->order, frontTiles);
    for (i = frontTiles; i < POKECONNECT_NUM_TILES; i++)
    {
        if (sUi->game->order[i] / POKECONNECT_GROUP_SIZE == group)
            order[count++] = sUi->game->order[i];
    }
    for (i = frontTiles; i < POKECONNECT_NUM_TILES; i++)
    {
        if (sUi->game->order[i] / POKECONNECT_GROUP_SIZE != group)
            order[count++] = sUi->game->order[i];
    }
    memcpy(sUi->game->order, order, sizeof(order));
}

static void RecordFinishedGame(bool32 won)
{
    struct PokeconnectSave *game = sUi->game;
    u32 group, frontTiles;

    // Reveal the groups the player didn't find, easiest first
    frontTiles = __builtin_popcount(game->solvedMask) * POKECONNECT_GROUP_SIZE;
    for (group = 0; group < POKECONNECT_NUM_GROUPS; group++)
    {
        if (!(game->solvedMask & (1 << group)))
        {
            MoveGroupToFront(group, frontTiles);
            frontTiles += POKECONNECT_GROUP_SIZE;
        }
    }

    game->finished = TRUE;
    game->won = won;
    sUi->selectedMask = 0;
    PlaySE(won ? SE_SUCCESS : SE_FAILURE);
    if (sUi->practice)
        return;

    if (won)
    {
        game->streak = (game->wins > 0 && game->lastWinDay + 1 == game->day) ? game->streak + 1 : 1;
        if (game->streak > game->bestStreak)
            game->bestStreak = game->streak;
        game->wins++;
        if (game->mistakes == 0)
            game->perfect++;
        game->lastWinDay = game->day;
        game->rewardPending = TRUE;
    }
    else
    {
        game->streak = 0;
    }
}

static void SubmitGuess(void)
{
    struct PokeconnectSave *game = sUi->game;
    u32 i, result, mask = sUi->selectedMask;

    for (i = 0; i < game->mistakes; i++)
    {
        if (game->wrongGuesses[i] == mask)
        {
            PlaySE(SE_BOO);
            SetMessage(COMPOUND_STRING("Already guessed!"), DEX_FONT_RED);
            DrawInfo();
            return;
        }
    }

    if (!sUi->practice && game->solvedMask == 0 && game->mistakes == 0)
        game->played++;

    result = Pokeconnect_CheckGuess(mask);
    if (result == PCON_GUESS_CORRECT)
    {
        u32 group = __builtin_ctz(mask) / POKECONNECT_GROUP_SIZE;

        MoveGroupToFront(group, GetFirstTilePosition());
        game->solvedMask |= 1 << group;
        sUi->selectedMask = 0;
        SetMessage(NULL, 0);
        if (game->solvedMask == ALL_GROUPS_MASK)
            RecordFinishedGame(TRUE);
        else
            PlaySE(SE_SUCCESS);
    }
    else
    {
        game->wrongGuesses[game->mistakes++] = mask;
        if (result == PCON_GUESS_ONE_AWAY)
            SetMessage(COMPOUND_STRING("One away…"), DEX_FONT_RED);
        else
            SetMessage(COMPOUND_STRING("Not quite."), DEX_FONT_RED);
        if (game->mistakes == POKECONNECT_MAX_MISTAKES)
            RecordFinishedGame(FALSE);
        else
            PlaySE(SE_FAILURE);
    }

    if (game->finished)
        sUi->cursor = 0;
    else if (sUi->cursor < GetFirstTilePosition())
        sUi->cursor = GetFirstTilePosition() + sUi->cursor % POKECONNECT_GROUP_SIZE;

    DrawGrid();
    sUi->mode = sUi->practice ? MODE_BOARD : MODE_SAVE_MESSAGE;
    DrawInfo();
}

static void ToggleCursorTile(void)
{
    u32 tile = GetCursorTile();

    if (sUi->selectedMask & (1 << tile))
    {
        sUi->selectedMask &= ~(1 << tile);
    }
    else if (__builtin_popcount(sUi->selectedMask) < POKECONNECT_GROUP_SIZE)
    {
        sUi->selectedMask |= 1 << tile;
    }
    else
    {
        PlaySE(SE_BOO);
        SetMessage(COMPOUND_STRING("Only 4 at a time!"), DEX_FONT_RED);
        DrawInfo();
        return;
    }
    PlaySE(SE_SELECT);
    SetMessage(NULL, 0);
    DrawTile(sUi->cursor);
    DexGame_CopyBoxesToVram();
    DrawInfo();
}

static void ClearSelection(void)
{
    u32 position;

    PlaySE(SE_SELECT);
    sUi->selectedMask = 0;
    SetMessage(NULL, 0);
    for (position = GetFirstTilePosition(); position < POKECONNECT_NUM_TILES; position++)
        DrawTile(position);
    DexGame_CopyBoxesToVram();
    DrawInfo();
}

static void ShuffleTiles(void)
{
    u32 first = GetFirstTilePosition();
    u32 i, j;
    u8 temp;

    PlaySE(SE_SELECT);
    for (i = POKECONNECT_NUM_TILES - 1; i > first; i--)
    {
        j = first + Random() % (i - first + 1);
        SWAP(sUi->game->order[i], sUi->game->order[j], temp);
    }
    SetMessage(NULL, 0);
    DrawGrid();
    DrawInfo();
}

static void MoveCursor(s32 rowDelta, s32 colDelta)
{
    s32 row = sUi->cursor / POKECONNECT_GROUP_SIZE + rowDelta;
    s32 col = sUi->cursor % POKECONNECT_GROUP_SIZE + colDelta;

    if (row < (s32)GetGroupRowCount() || row >= POKECONNECT_NUM_GROUPS || col < 0 || col >= POKECONNECT_GROUP_SIZE)
        return;

    PlaySE(SE_SELECT);
    sUi->cursor = row * POKECONNECT_GROUP_SIZE + col;
    SetMessage(NULL, 0);
    DrawCursor();
    DrawInfo();
}

static void StartNewPracticeGame(void)
{
    // The answer pool made one puzzle already, so this only fails if it's very small and unlucky
    if (!Pokeconnect_GeneratePuzzle(sUi->pool, Random32(), sUi->game))
    {
        PlaySE(SE_BOO);
        return;
    }

    PlaySE(SE_SELECT);
    StartGame(sUi->game, 0, sUi->game->answerPool);
    sUi->cursor = 0;
    sUi->selectedMask = 0;
    SetMessage(NULL, 0);
    DrawGrid();
    DrawInfo();
}

static void HandleBoardInput(u8 taskId)
{
    if (sUi->game->finished)
    {
        if (JOY_NEW(B_BUTTON))
        {
            DexGame_ExitScreen(taskId);
        }
        else if (JOY_NEW(START_BUTTON) && sUi->practice)
        {
            StartNewPracticeGame();
        }
        else if (JOY_NEW(SELECT_BUTTON) && !sUi->practice)
        {
            sUi->showStats = !sUi->showStats;
            DrawInfo();
        }
    }
    else if (JOY_NEW(B_BUTTON))
    {
        if (sUi->selectedMask != 0)
            ClearSelection();
        else
            DexGame_ExitScreen(taskId);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        ToggleCursorTile();
    }
    else if (JOY_NEW(START_BUTTON))
    {
        if (__builtin_popcount(sUi->selectedMask) == POKECONNECT_GROUP_SIZE)
        {
            SubmitGuess();
        }
        else
        {
            PlaySE(SE_BOO);
            SetMessage(COMPOUND_STRING("Pick 4 Pokémon first."), DEX_FONT_RED);
            DrawInfo();
        }
    }
    else if (JOY_NEW(SELECT_BUTTON))
    {
        ShuffleTiles();
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

static void Task_PokeconnectMainInput(u8 taskId)
{
    switch (sUi->mode)
    {
    case MODE_BOARD:
        HandleBoardInput(taskId);
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

static void DrawTile(u32 position)
{
    u32 palette = (sUi->selectedMask & (1 << sUi->game->order[position])) ? DEX_BOX_SELECTED : DEX_BOX_EMPTY;

    DexGame_DrawBox(GetTileX(position), GetTileY(position), CELL_WIDTH, CELL_HEIGHT, palette);
}

static u32 GetFittingFont(const u8 *str, u32 font, u32 narrowerFont)
{
    return GetStringWidth(font, str, 0) <= GRID_WIDTH * 8 - 8 ? font : narrowerFont;
}

static void DrawGroupRow(u32 row)
{
    struct PokeconnectSave *game = sUi->game;
    u32 group = game->order[row * POKECONNECT_GROUP_SIZE] / POKECONNECT_GROUP_SIZE;
    u32 i, y = row * CELL_HEIGHT * 8;
    u8 *end;

    DexGame_DrawBox(GRID_LEFT, row * CELL_HEIGHT, GRID_WIDTH, CELL_HEIGHT, sGroupPalettes[group]);

    Pokeconnect_GetCategoryName(game->categories[group], gStringVar4);
    DexGame_PrintText(WIN_GRID, GetFittingFont(gStringVar4, FONT_NORMAL, FONT_NARROW), 4, y + 2, DEX_FONT_NORMAL, gStringVar4);

    end = gStringVar4;
    for (i = 0; i < POKECONNECT_GROUP_SIZE; i++)
    {
        if (i != 0)
            end = StringCopy(end, COMPOUND_STRING(", "));
        end = StringCopy(end, GetSpeciesName(game->species[game->order[row * POKECONNECT_GROUP_SIZE + i]]));
    }
    DexGame_PrintText(WIN_GRID, GetFittingFont(gStringVar4, FONT_SMALL, FONT_SMALL_NARROW), 4, y + 16, DEX_FONT_NORMAL, gStringVar4);
}

static void DrawGrid(void)
{
    u32 row, position;

    FillWindowPixelBuffer(WIN_GRID, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));
    DexGame_ClearBoxes(0, 0, 30, GRID_HEIGHT);
    for (position = 0; position < POKECONNECT_NUM_TILES; position++)
        DexGame_DestroyIcon(&sUi->iconSpriteIds[position]);

    for (row = 0; row < GetGroupRowCount(); row++)
        DrawGroupRow(row);
    for (position = GetFirstTilePosition(); position < POKECONNECT_NUM_TILES; position++)
    {
        DrawTile(position);
        sUi->iconSpriteIds[position] = DexGame_CreateIcon(sUi->game->species[sUi->game->order[position]],
                                                          GetTileX(position) * 8 + CELL_WIDTH * 4,
                                                          GetTileY(position) * 8 + CELL_HEIGHT * 4);
    }
    CopyWindowToVram(WIN_GRID, COPYWIN_GFX);
    DexGame_CopyBoxesToVram();
    DrawCursor();
}

static void DrawCursor(void)
{
    if (sUi->game->finished)
        DexGame_HideCursor();
    else
        DexGame_ShowCursor(GetTileX(sUi->cursor), GetTileY(sUi->cursor), CELL_WIDTH, CELL_HEIGHT);
}

static void PrintStats(void)
{
    struct PokeconnectSave *game = sUi->game;
    u8 *end;

    end = StringCopy(gStringVar4, COMPOUND_STRING("Played "));
    end = ConvertIntToDecimalStringN(end, game->played, STR_CONV_MODE_LEFT_ALIGN, 5);
    end = StringCopy(end, COMPOUND_STRING("  Won "));
    end = ConvertIntToDecimalStringN(end, game->played ? (game->wins * 100) / game->played : 0, STR_CONV_MODE_LEFT_ALIGN, 3);
    end = StringCopy(end, COMPOUND_STRING("%  Streak "));
    end = ConvertIntToDecimalStringN(end, (game->lastWinDay + 1 >= sUi->today) ? game->streak : 0, STR_CONV_MODE_LEFT_ALIGN, 5);
    end = StringCopy(end, COMPOUND_STRING("  Best "));
    ConvertIntToDecimalStringN(end, game->bestStreak, STR_CONV_MODE_LEFT_ALIGN, 5);
    DexGame_PrintText(WIN_INFO, FONT_SMALL, 4, 2, DEX_FONT_NORMAL, gStringVar4);

    end = StringCopy(gStringVar4, COMPOUND_STRING("Perfect "));
    end = ConvertIntToDecimalStringN(end, game->perfect, STR_CONV_MODE_LEFT_ALIGN, 5);
    StringCopy(end, COMPOUND_STRING("   {SELECT_BUTTON} Back  {B_BUTTON} Exit"));
    DexGame_PrintText(WIN_INFO, FONT_SMALL, 4, 16, DEX_FONT_NORMAL, gStringVar4);
}

static void PrintResult(void)
{
    struct PokeconnectSave *game = sUi->game;
    u8 *end;

    if (game->won && game->mistakes == 0)
    {
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_GREEN, COMPOUND_STRING("Perfect! No mistakes!"));
    }
    else if (game->won)
    {
        end = StringCopy(gStringVar4, COMPOUND_STRING("Solved with "));
        end = ConvertIntToDecimalStringN(end, game->mistakes, STR_CONV_MODE_LEFT_ALIGN, 1);
        StringCopy(end, game->mistakes == 1 ? COMPOUND_STRING(" mistake!") : COMPOUND_STRING(" mistakes!"));
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_GREEN, gStringVar4);
    }
    else
    {
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_RED, COMPOUND_STRING("Out of mistakes!"));
    }

    DexGame_PrintText(WIN_INFO, FONT_SMALL, 4, 16, DEX_FONT_NORMAL,
                      sUi->practice ? COMPOUND_STRING("{START_BUTTON} New puzzle  {B_BUTTON} Exit")
                                    : COMPOUND_STRING("{SELECT_BUTTON} Stats  {B_BUTTON} Exit"));
}

static void PrintBoardInfo(void)
{
    u8 *end;
    u32 x;

    DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_NORMAL, GetSpeciesName(sUi->game->species[GetCursorTile()]));

    end = StringCopy(gStringVar4, COMPOUND_STRING("Mistakes left: "));
    ConvertIntToDecimalStringN(end, POKECONNECT_MAX_MISTAKES - sUi->game->mistakes, STR_CONV_MODE_LEFT_ALIGN, 1);
    x = 30 * 8 - 4 - GetStringWidth(FONT_NORMAL, gStringVar4, 0);
    DexGame_PrintText(WIN_INFO, FONT_NORMAL, x, 1, sUi->game->mistakes == POKECONNECT_MAX_MISTAKES - 1 ? DEX_FONT_RED : DEX_FONT_NORMAL, gStringVar4);

    if (sUi->message != NULL)
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 15, sUi->messageColor, sUi->message);
    else if (sUi->selectedMask != 0)
        DexGame_PrintText(WIN_INFO, FONT_SMALL, 4, 16, DEX_FONT_NORMAL,
                          COMPOUND_STRING("{A_BUTTON} Pick  {START_BUTTON} Submit  {SELECT_BUTTON} Shuffle  {B_BUTTON} Clear"));
    else
        DexGame_PrintText(WIN_INFO, FONT_SMALL, 4, 16, DEX_FONT_NORMAL,
                          COMPOUND_STRING("{A_BUTTON} Pick  {START_BUTTON} Submit  {SELECT_BUTTON} Shuffle  {B_BUTTON} Exit"));
}

static void DrawInfo(void)
{
    FillWindowPixelBuffer(WIN_INFO, PIXEL_FILL(TEXT_COLOR_TRANSPARENT));

    if (sUi->mode == MODE_SAVE_MESSAGE || sUi->mode == MODE_SAVE)
    {
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 1, DEX_FONT_NORMAL, COMPOUND_STRING("Saving…"));
        DexGame_PrintText(WIN_INFO, FONT_NORMAL, 4, 15, DEX_FONT_NORMAL, COMPOUND_STRING("Don't turn off the power."));
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
        PrintBoardInfo();
    }

    CopyWindowToVram(WIN_INFO, COPYWIN_GFX);
}

#else

void GetPokeconnectStatus(void)
{
    gSpecialVar_Result = DEX_GAME_STATUS_DISABLED;
}

void PreparePokeconnect(void)
{
    gSpecialVar_Result = DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES;
}

void OpenPokeconnect(void)
{
    DexGame_ResumeScriptNextFrame();
}

void ClaimPokeconnectReward(void)
{
    gSpecialVar_Result = DEX_GAME_NO_REWARD;
}

#endif // POKECONNECT_ENABLED
