#include "global.h"
#include "dex_minigame.h"
#include "event_data.h"
#include "item.h"
#include "malloc.h"
#include "pokedex.h"
#include "pokemon.h"
#include "test/test.h"

TEST("Dex minigame evolution stages use depth in the evolution chain")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);

    EXPECT_EQ(pool->stage[NATIONAL_DEX_BULBASAUR], DEX_STAGE_BASIC);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_IVYSAUR], DEX_STAGE_MIDDLE);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_VENUSAUR], DEX_STAGE_FINAL);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_TAUROS], DEX_STAGE_SINGLE);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_PICHU], DEX_STAGE_BASIC);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_PIKACHU], DEX_STAGE_MIDDLE);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_EEVEE], DEX_STAGE_BASIC);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_VAPOREON], DEX_STAGE_FINAL);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_SHEDINJA], DEX_STAGE_FINAL);
    // Only Galarian Corsola evolves, so Corsola itself doesn't
    EXPECT_EQ(pool->stage[NATIONAL_DEX_CORSOLA], DEX_STAGE_SINGLE);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_CURSOLA], DEX_STAGE_FINAL);
    EXPECT_EQ(pool->stage[NATIONAL_DEX_PERRSERKER], DEX_STAGE_FINAL);

    Free(pool);
}

TEST("Dex minigame pool maps dex numbers to base species")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);

    EXPECT_EQ(pool->species[NATIONAL_DEX_PIKACHU], SPECIES_PIKACHU);
    EXPECT_EQ(pool->species[NATIONAL_DEX_VULPIX], SPECIES_VULPIX);
    EXPECT_EQ(pool->species[NATIONAL_DEX_DEOXYS], SPECIES_DEOXYS);
    EXPECT_EQ(DexPool_GetNthDex(pool, 0), NATIONAL_DEX_BULBASAUR);
    EXPECT_EQ(DexPool_GetGeneration(NATIONAL_DEX_MEW), 1);
    EXPECT_EQ(DexPool_GetGeneration(NATIONAL_DEX_CHIKORITA), 2);
    EXPECT_EQ(DexPool_GetGeneration(NATIONAL_DEX_DEOXYS), 3);
    EXPECT_EQ(DexPool_GetGeneration(NATIONAL_DEX_MELTAN), 7);
    EXPECT_EQ(DexPool_GetGeneration(NATIONAL_DEX_PECHARUNT), 9);

    Free(pool);
}

TEST("Dex minigame alphabetical list covers every enabled species in name order")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u32 i;

    EXPECT_EQ(pool->alphabeticalCount, pool->count);
    EXPECT_EQ(pool->alphabetical[0], NATIONAL_DEX_ABOMASNOW);
    for (i = 1; i < pool->alphabeticalCount; i++)
    {
        const u8 *prev = GetSpeciesName(pool->species[pool->alphabetical[i - 1]]);
        const u8 *curr = GetSpeciesName(pool->species[pool->alphabetical[i]]);
        EXPECT_LE(prev[0], curr[0]);
    }

    Free(pool);
}

TEST("Dex minigame answer pools only include seen or caught species")
{
    struct DexPool *pool;

    ResetPokedex();
    GetSetPokedexFlag(NATIONAL_DEX_TREECKO, FLAG_SET_SEEN);
    GetSetPokedexFlag(NATIONAL_DEX_MUDKIP, FLAG_SET_SEEN);
    GetSetPokedexFlag(NATIONAL_DEX_MUDKIP, FLAG_SET_CAUGHT);

    pool = DexPool_Create(DEX_POOL_SEEN);
    EXPECT_EQ(pool->count, 2);
    EXPECT_EQ(pool->alphabeticalCount, 2);
    EXPECT(pool->available[NATIONAL_DEX_TREECKO]);
    EXPECT(!pool->available[NATIONAL_DEX_TORCHIC]);
    EXPECT_EQ(pool->species[NATIONAL_DEX_TORCHIC], SPECIES_TORCHIC); // Still known, for popularity totals
    EXPECT_EQ(DexPool_GetNthDex(pool, 1), NATIONAL_DEX_MUDKIP);
    Free(pool);

    pool = DexPool_Create(DEX_POOL_CAUGHT);
    EXPECT_EQ(pool->count, 1);
    EXPECT_EQ(DexPool_GetNthDex(pool, 0), NATIONAL_DEX_MUDKIP);
    Free(pool);
}

TEST("Dex minigame popularity is at least 1 for every enabled species")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u32 dex;

    DexPool_LoadPopularity(pool);
    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        if (pool->species[dex] != SPECIES_NONE)
            EXPECT_GE(pool->popularity[dex], 1);
    }
    Free(pool);
}

TEST("Dex minigame popularity follows the survey")
{
    struct DexPool *pool;

    ASSUME(POKEDOKU_POPULARITY_SURVEY > 0);
    pool = DexPool_Create(DEX_POOL_ALL);
    DexPool_LoadPopularity(pool);
    // Most votes vs. none
    EXPECT_GT(pool->popularity[NATIONAL_DEX_MIMIKYU], pool->popularity[NATIONAL_DEX_TADBULB]);
    Free(pool);
}

TEST("Dex minigame popularity follows in-game wild encounters")
{
    struct DexPool *pool;

    ASSUME(POKEDOKU_POPULARITY_WILD > 0);
    pool = DexPool_Create(DEX_POOL_ALL);
    DexPool_LoadPopularity(pool);
    // Golbat got no survey votes but is common in Hoenn caves; Tadbulb got none and isn't wild in Hoenn
    EXPECT_GT(pool->popularity[NATIONAL_DEX_GOLBAT], pool->popularity[NATIONAL_DEX_TADBULB]);
    Free(pool);
}

TEST("Dex minigame rewards roll from the tier covering the score")
{
    u32 seed, item;
    bool32 sawRareCandy = FALSE, sawPpUp = FALSE, sawBottleCap = FALSE;

    for (seed = 8000; seed < 8100; seed++)
    {
        EXPECT(DexGame_RollReward(DEX_REWARDS_POKEDOKU, 9, seed));
        item = gSpecialVar_0x8004;
        EXPECT(item == ITEM_RARE_CANDY || item == ITEM_PP_UP || item == ITEM_BOTTLE_CAP);
        EXPECT_EQ(gSpecialVar_0x8005, 1);
        sawRareCandy |= (item == ITEM_RARE_CANDY);
        sawPpUp |= (item == ITEM_PP_UP);
        sawBottleCap |= (item == ITEM_BOTTLE_CAP);

        // The same day and score always give the same item
        DexGame_RollReward(DEX_REWARDS_POKEDOKU, 9, seed);
        EXPECT_EQ(gSpecialVar_0x8004, item);
    }
    EXPECT(sawRareCandy && sawPpUp && sawBottleCap);

    EXPECT(DexGame_RollReward(DEX_REWARDS_SQUIRDLE, 5, 8000));
    item = gSpecialVar_0x8004;
    EXPECT(item == ITEM_ULTRA_BALL || item == ITEM_HYPER_POTION || item == ITEM_NUGGET);
}

TEST("Dex minigame rewards give nothing for a score no tier covers")
{
    EXPECT(!DexGame_RollReward(DEX_REWARDS_SQUIRDLE, 0, 8000));
    EXPECT_EQ(gSpecialVar_0x8004, ITEM_NONE);
}

TEST("Dex minigame number steps reach the last available species past the end of the dex")
{
    struct DexPool *pool;

    ResetPokedex();
    GetSetPokedexFlag(NATIONAL_DEX_TREECKO, FLAG_SET_CAUGHT);
    GetSetPokedexFlag(NATIONAL_DEX_MUDKIP, FLAG_SET_CAUGHT);
    pool = DexPool_Create(DEX_POOL_CAUGHT);

    EXPECT_EQ(DexPool_StepAvailable(pool, NATIONAL_DEX_TREECKO, 1), NATIONAL_DEX_MUDKIP);
    EXPECT_EQ(DexPool_StepAvailable(pool, NATIONAL_DEX_TREECKO, 100), NATIONAL_DEX_MUDKIP);
    EXPECT_EQ(DexPool_StepAvailable(pool, NATIONAL_DEX_MUDKIP, -10), NATIONAL_DEX_TREECKO);
    EXPECT_EQ(DexPool_StepAvailable(pool, NATIONAL_DEX_MUDKIP, -1000), NATIONAL_DEX_TREECKO);
    // Nothing further in that direction: stays put
    EXPECT_EQ(DexPool_StepAvailable(pool, NATIONAL_DEX_MUDKIP, 1000), NATIONAL_DEX_MUDKIP);
    EXPECT_EQ(DexPool_StepAvailable(pool, NATIONAL_DEX_TREECKO, -1), NATIONAL_DEX_TREECKO);
    Free(pool);
}

TEST("Dex minigame reward claims succeed when the bag has room")
{
    ClearBag();
    EXPECT(DexGame_ClaimReward(DEX_REWARDS_POKEDOKU, 7, 8000));
    EXPECT_EQ(gSpecialVar_Result, 7);
    EXPECT_NE(gSpecialVar_0x8004, ITEM_NONE);
}
