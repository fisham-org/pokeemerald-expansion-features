#include "global.h"
#include "malloc.h"
#include "pokedex.h"
#include "pokedoku.h"
#include "test/test.h"

TEST("PokeDoku criteria match species data")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);

    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_CHARIZARD, CRIT_TYPE_FIRE));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_CHARIZARD, CRIT_TYPE_FLYING));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_CHARIZARD, CRIT_GEN_1));
    EXPECT(!Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_CHARIZARD, CRIT_MONOTYPE));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_CHARIZARD, CRIT_BST_500));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_PIKACHU, CRIT_COLOR_YELLOW));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_TREECKO, CRIT_GEN_3));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_MEWTWO, CRIT_LEGENDARY));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_MEW, CRIT_MYTHICAL));
    EXPECT(!Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_MEW, CRIT_LEGENDARY));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_MAGNEMITE, CRIT_GENDERLESS));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_SNORLAX, CRIT_WEIGHT_100KG));
    EXPECT(Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_DITTO, CRIT_TYPE_NORMAL));
    EXPECT(!Pokedoku_MatchesCriterion(pool, NATIONAL_DEX_DITTO, CRIT_EGG_NO_EGGS));

    Free(pool);
}

TEST("PokeDoku criteria are grouped contiguously")
{
    u32 i;
    for (i = 1; i < POKEDOKU_CRITERIA_COUNT; i++)
        EXPECT_GE(gPokedokuCriteria[i].group, gPokedokuCriteria[i - 1].group);
}

TEST("PokeDoku fallback board is solvable")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    EXPECT(Pokedoku_IsBoardSolvable(pool, gPokedokuFallbackBoard, POKEDOKU_MIN_ANSWERS_PER_CELL));
    Free(pool);
}

TEST("PokeDoku daily boards are solvable, deterministic and change each day")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u8 board[POKEDOKU_GRID_SIZE * 2], again[POKEDOKU_GRID_SIZE * 2], tomorrow[POKEDOKU_GRID_SIZE * 2];
    u32 day = 0;

    for (u32 i = 0; i < 25; i++)
        PARAMETRIZE { day = 8000 + i * 37; }

    EXPECT(Pokedoku_GenerateBoard(pool, day, board));
    EXPECT(Pokedoku_IsBoardSolvable(pool, board, POKEDOKU_MIN_ANSWERS_PER_CELL));
    Pokedoku_GenerateBoard(pool, day, again);
    EXPECT(memcmp(board, again, sizeof(board)) == 0);
    Pokedoku_GenerateBoard(pool, day + 1, tomorrow);
    EXPECT(memcmp(board, tomorrow, sizeof(board)) != 0);

    Free(pool);
}

TEST("PokeDoku cell popularity totals cover every valid answer")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u32 cell, dex, sum;

    DexPool_LoadPopularity(pool);
    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
    {
        sum = 0;
        for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
        {
            if (Pokedoku_IsValidGuess(pool, gPokedokuFallbackBoard, cell, dex))
                sum += pool->popularity[dex];
        }
        EXPECT_EQ(sum, Pokedoku_GetCellPopularityTotal(pool, gPokedokuFallbackBoard, cell));
        EXPECT_GE(Pokedoku_GetPopularityPercent(pool, NATIONAL_DEX_TENTACOOL, sum), 1);
    }
    Free(pool);
}

TEST("PokeDoku rarity score adds popularity for correct cells and 100 for the rest")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    struct PokedokuSave board = {0};
    u32 totals[POKEDOKU_NUM_CELLS], cell;

    DexPool_LoadPopularity(pool);
    memcpy(board.criteria, gPokedokuFallbackBoard, sizeof(board.criteria));
    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
        totals[cell] = Pokedoku_GetCellPopularityTotal(pool, board.criteria, cell);
    EXPECT_EQ(Pokedoku_GetRarityScore(pool, &board, totals), 900);

    // Cell 0 is Water x Gen 1
    board.guesses[0] = SPECIES_SQUIRTLE;
    board.correctMask = 1;
    EXPECT_EQ(Pokedoku_GetRarityScore(pool, &board, totals),
              800 + Pokedoku_GetPopularityPercent(pool, NATIONAL_DEX_SQUIRTLE, totals[0]));
    EXPECT_LT(Pokedoku_GetRarityScore(pool, &board, totals), 900);
    Free(pool);
}

TEST("PokeDoku boards only use species from the answer pool")
{
    struct DexPool *pool;
    u8 board[POKEDOKU_GRID_SIZE * 2];
    u32 dex;

    ResetPokedex();
    pool = DexPool_Create(DEX_POOL_SEEN);
    EXPECT(!Pokedoku_GenerateBoard(pool, 8000, board)); // Nothing seen: no board is possible
    Free(pool);

    for (dex = 1; dex <= NATIONAL_DEX_MEW; dex++)
        GetSetPokedexFlag(dex, FLAG_SET_SEEN);
    pool = DexPool_Create(DEX_POOL_SEEN);
    EXPECT(Pokedoku_GenerateBoard(pool, 8000, board));
    EXPECT(Pokedoku_IsBoardSolvable(pool, board, 1));
    Free(pool);
}
