#include "global.h"
#include "malloc.h"
#include "pokedex.h"
#include "squirdle.h"
#include "test/test.h"

TEST("Squirdle compares generation, height and weight directionally")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u8 clues[SQUIRDLE_ATTR_COUNT];

    // Guess Charizard (Gen 1, Fire/Flying, 1.7 m, 90.5 kg) for Blaziken (Gen 3, Fire/Fighting, 1.9 m, 52.0 kg)
    Squirdle_Compare(pool, NATIONAL_DEX_CHARIZARD, NATIONAL_DEX_BLAZIKEN, clues);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_GEN], SQUIRDLE_CLUE_HIGHER);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_1], SQUIRDLE_CLUE_CORRECT);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_2], SQUIRDLE_CLUE_WRONG);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_HEIGHT], SQUIRDLE_CLUE_HIGHER);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_WEIGHT], SQUIRDLE_CLUE_LOWER);
    EXPECT(!Squirdle_IsSolved(clues));

    Squirdle_Compare(pool, NATIONAL_DEX_BLAZIKEN, NATIONAL_DEX_CHARIZARD, clues);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_GEN], SQUIRDLE_CLUE_LOWER);

    Free(pool);
}

TEST("Squirdle compares base stat total directionally and color exactly")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u8 clues[SQUIRDLE_ATTR_COUNT];

    // Guess Charizard (534 BST, Red) for Blaziken (530 BST, Red)
    Squirdle_Compare(pool, NATIONAL_DEX_CHARIZARD, NATIONAL_DEX_BLAZIKEN, clues);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_BST], SQUIRDLE_CLUE_LOWER);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_COLOR], SQUIRDLE_CLUE_CORRECT);

    // Guess Squirtle (314 BST, Blue) for Charizard (534 BST, Red)
    Squirdle_Compare(pool, NATIONAL_DEX_SQUIRTLE, NATIONAL_DEX_CHARIZARD, clues);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_BST], SQUIRDLE_CLUE_HIGHER);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_COLOR], SQUIRDLE_CLUE_WRONG);

    Free(pool);
}

TEST("Squirdle marks a type in the target's other slot")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u8 clues[SQUIRDLE_ATTR_COUNT];

    // Tornadus is pure Flying; Charizard has Flying as its second type
    Squirdle_Compare(pool, NATIONAL_DEX_TORNADUS, NATIONAL_DEX_CHARIZARD, clues);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_1], SQUIRDLE_CLUE_OTHER_SLOT);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_2], SQUIRDLE_CLUE_WRONG);

    // Pidgey (Normal/Flying) has Flying in the same slot as Charizard
    Squirdle_Compare(pool, NATIONAL_DEX_PIDGEY, NATIONAL_DEX_CHARIZARD, clues);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_1], SQUIRDLE_CLUE_WRONG);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_2], SQUIRDLE_CLUE_CORRECT);

    Free(pool);
}

TEST("Squirdle treats a single-typed Pokemon's second type as None")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u8 clues[SQUIRDLE_ATTR_COUNT];

    // Pikachu and Raichu are both pure Electric
    Squirdle_Compare(pool, NATIONAL_DEX_PIKACHU, NATIONAL_DEX_RAICHU, clues);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_1], SQUIRDLE_CLUE_CORRECT);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_2], SQUIRDLE_CLUE_CORRECT);

    // Pure Fire Charmander's None doesn't count as being in Charizard's other slot
    Squirdle_Compare(pool, NATIONAL_DEX_CHARMANDER, NATIONAL_DEX_CHARIZARD, clues);
    EXPECT_EQ(clues[SQUIRDLE_ATTR_TYPE_2], SQUIRDLE_CLUE_WRONG);

    Free(pool);
}

TEST("Squirdle is solved by guessing the target")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u8 clues[SQUIRDLE_ATTR_COUNT];

    Squirdle_Compare(pool, NATIONAL_DEX_MUDKIP, NATIONAL_DEX_MUDKIP, clues);
    EXPECT(Squirdle_IsSolved(clues));

    Free(pool);
}

TEST("Squirdle daily targets are deterministic, enabled and vary by day")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u32 day, target, changes = 0, previous = 0;

    for (day = 8000; day < 8030; day++)
    {
        target = Squirdle_GetDailyTarget(pool, day);
        EXPECT_NE(pool->species[target], SPECIES_NONE);
        EXPECT_EQ(target, Squirdle_GetDailyTarget(pool, day));
        if (target != previous)
            changes++;
        previous = target;
    }
    EXPECT_GE(changes, 28);

    Free(pool);
}

TEST("Squirdle daily targets come from the answer pool")
{
    struct DexPool *pool;
    u32 day;

    ResetPokedex();
    GetSetPokedexFlag(NATIONAL_DEX_TREECKO, FLAG_SET_CAUGHT);
    GetSetPokedexFlag(NATIONAL_DEX_TORCHIC, FLAG_SET_CAUGHT);
    pool = DexPool_Create(DEX_POOL_CAUGHT);
    for (day = 8000; day < 8010; day++)
    {
        u32 target = Squirdle_GetDailyTarget(pool, day);
        EXPECT(target == NATIONAL_DEX_TREECKO || target == NATIONAL_DEX_TORCHIC);
    }
    Free(pool);
}
