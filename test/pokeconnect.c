#include "global.h"
#include "malloc.h"
#include "pokeconnect.h"
#include "pokedex.h"
#include "test/test.h"

static struct PokeconnectCategory Category(u32 kind, u32 param)
{
    struct PokeconnectCategory category = { .kind = kind, .param = param };
    return category;
}

static u32 CountPoolMatches(const struct DexPool *pool, struct PokeconnectCategory category)
{
    u32 dex, count = 0;

    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        if (pool->available[dex] && Pokeconnect_Matches(pool, dex, category))
            count++;
    }
    return count;
}

TEST("PokeConnect matches abilities, evolution methods and gender")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_KOFFING, Category(PCON_KIND_ABILITY, ABILITY_LEVITATE)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_PIKACHU, Category(PCON_KIND_ABILITY, ABILITY_LEVITATE)));
    // Hidden abilities don't count: Pikachu's is Lightning Rod
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_PIKACHU, Category(PCON_KIND_ABILITY, ABILITY_LIGHTNING_ROD)));

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_PIKACHU, Category(PCON_KIND_EVO_METHOD, PCON_EVO_STONE)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_EEVEE, Category(PCON_KIND_EVO_METHOD, PCON_EVO_STONE)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_EEVEE, Category(PCON_KIND_EVO_METHOD, PCON_EVO_FRIENDSHIP)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_GOLBAT, Category(PCON_KIND_EVO_METHOD, PCON_EVO_FRIENDSHIP)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_KADABRA, Category(PCON_KIND_EVO_METHOD, PCON_EVO_TRADE)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_ALAKAZAM, Category(PCON_KIND_EVO_METHOD, PCON_EVO_TRADE)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_BULBASAUR, Category(PCON_KIND_EVO_METHOD, PCON_EVO_STONE)));

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_MAGNEMITE, Category(PCON_KIND_GENDER, MON_GENDERLESS)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_CHANSEY, Category(PCON_KIND_GENDER, MON_FEMALE)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_TAUROS, Category(PCON_KIND_GENDER, MON_MALE)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_PIKACHU, Category(PCON_KIND_GENDER, MON_MALE)));

    Free(pool);
}

TEST("PokeConnect matches names, sets and exact dual types")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_ZUBAT, Category(PCON_KIND_FIRST_LETTER, 25)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_PIKACHU, Category(PCON_KIND_FIRST_LETTER, 25)));

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_PAWMI, Category(PCON_KIND_SET, PCON_SET_PIKACHU_CLONES)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_RAICHU, Category(PCON_KIND_SET, PCON_SET_PIKACHU_CLONES)));

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_SWAMPERT, Category(PCON_KIND_DUAL_TYPE, TYPE_WATER | (TYPE_GROUND << 8))));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_SWAMPERT, Category(PCON_KIND_DUAL_TYPE, TYPE_GROUND | (TYPE_WATER << 8))));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_MUDKIP, Category(PCON_KIND_DUAL_TYPE, TYPE_WATER | (TYPE_GROUND << 8))));

    Free(pool);
}

TEST("PokeConnect matches best stats, pure types, forms, branches and babies")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_JOLTEON, Category(PCON_KIND_BEST_STAT, STAT_SPEED)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_CHANSEY, Category(PCON_KIND_BEST_STAT, STAT_HP)));
    // A tie isn't a best stat: Shuckle's Defense and Sp. Def are both 230
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_SHUCKLE, Category(PCON_KIND_BEST_STAT, STAT_DEF)));

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_CHARMANDER, Category(PCON_KIND_PURE_TYPE, TYPE_FIRE)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_CHARIZARD, Category(PCON_KIND_PURE_TYPE, TYPE_FIRE)));

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_CHARIZARD, Category(PCON_KIND_FORM, PCON_FORM_MEGA)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_CHARMANDER, Category(PCON_KIND_FORM, PCON_FORM_MEGA)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_RATTATA, Category(PCON_KIND_FORM, PCON_FORM_ALOLAN)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_MEOWTH, Category(PCON_KIND_FORM, PCON_FORM_GALARIAN)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_GROWLITHE, Category(PCON_KIND_FORM, PCON_FORM_HISUIAN)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_PIDGEY, Category(PCON_KIND_FORM, PCON_FORM_ALOLAN)));

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_EEVEE, Category(PCON_KIND_BRANCH_EVO, 0)));
    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_WURMPLE, Category(PCON_KIND_BRANCH_EVO, 0)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_POLIWAG, Category(PCON_KIND_BRANCH_EVO, 0)));
    // Regional evolutions keep the same dex number, so don't count
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_EXEGGCUTE, Category(PCON_KIND_BRANCH_EVO, 0)));

    EXPECT(Pokeconnect_Matches(pool, NATIONAL_DEX_PICHU, Category(PCON_KIND_CLASS, PCON_CLASS_BABY)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_PIKACHU, Category(PCON_KIND_CLASS, PCON_CLASS_BABY)));
    EXPECT(!Pokeconnect_Matches(pool, NATIONAL_DEX_COSMOG, Category(PCON_KIND_CLASS, PCON_CLASS_BABY)));

    Free(pool);
}

TEST("PokeConnect forms and Baby Pokémon each have 4+ enabled species")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u32 form;

    for (form = 0; form < PCON_FORM_COUNT; form++)
        EXPECT_GE(CountPoolMatches(pool, Category(PCON_KIND_FORM, form)), POKECONNECT_GROUP_SIZE);
    EXPECT_GE(CountPoolMatches(pool, Category(PCON_KIND_CLASS, PCON_CLASS_BABY)), POKECONNECT_GROUP_SIZE);

    Free(pool);
}

TEST("PokeConnect hand-written sets each have 4+ enabled species")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    u32 set;

    for (set = 0; set < PCON_SET_COUNT; set++)
        EXPECT_GE(CountPoolMatches(pool, Category(PCON_KIND_SET, set)), POKECONNECT_GROUP_SIZE);

    Free(pool);
}

TEST("PokeConnect guesses are correct, one away or wrong")
{
    EXPECT_EQ(Pokeconnect_CheckGuess(0x000F), PCON_GUESS_CORRECT);
    EXPECT_EQ(Pokeconnect_CheckGuess(0xF000), PCON_GUESS_CORRECT);
    EXPECT_EQ(Pokeconnect_CheckGuess(0x0017), PCON_GUESS_ONE_AWAY);
    EXPECT_EQ(Pokeconnect_CheckGuess(0x1110), PCON_GUESS_WRONG);
    EXPECT_EQ(Pokeconnect_CheckGuess(0x0033), PCON_GUESS_WRONG);
}

TEST("PokeConnect puzzles have one solution, with each tile fitting only its group")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    struct PokeconnectSave *game = AllocZeroed(sizeof(struct PokeconnectSave));
    u32 seed, tile, group, i;

    for (seed = 0; seed < 8; seed++)
    {
        u32 seenTiles = 0;

        EXPECT(Pokeconnect_GeneratePuzzle(pool, seed, game));
        EXPECT_EQ(Pokeconnect_CountSolutions(pool, game), 1);

        for (tile = 0; tile < POKECONNECT_NUM_TILES; tile++)
        {
            u32 dex = DexPool_GetDexOfSpecies(game->species[tile]);

            EXPECT(pool->available[dex]);
            for (group = 0; group < POKECONNECT_NUM_GROUPS; group++)
                EXPECT_EQ(Pokeconnect_Matches(pool, dex, game->categories[group]), group == tile / POKECONNECT_GROUP_SIZE);
            for (i = 0; i < tile; i++)
                EXPECT_NE(game->species[i], game->species[tile]);
            seenTiles |= 1 << game->order[tile];
        }
        EXPECT_EQ(seenTiles, (1 << POKECONNECT_NUM_TILES) - 1);

        // Easiest first: groups are ordered by how many species fit in the answer pool
        for (group = 1; group < POKECONNECT_NUM_GROUPS; group++)
            EXPECT_GE(CountPoolMatches(pool, game->categories[group - 1]), CountPoolMatches(pool, game->categories[group]));
    }

    Free(game);
    Free(pool);
}

TEST("PokeConnect puzzles are the same for the same seed")
{
    struct DexPool *pool = DexPool_Create(DEX_POOL_ALL);
    struct PokeconnectSave *first = AllocZeroed(sizeof(struct PokeconnectSave));
    struct PokeconnectSave *second = AllocZeroed(sizeof(struct PokeconnectSave));

    EXPECT(Pokeconnect_GeneratePuzzle(pool, 1234, first));
    EXPECT(Pokeconnect_GeneratePuzzle(pool, 1234, second));
    EXPECT_EQ(memcmp(first->species, second->species, sizeof(first->species)), 0);
    EXPECT_EQ(memcmp(first->categories, second->categories, sizeof(first->categories)), 0);
    EXPECT_EQ(memcmp(first->order, second->order, sizeof(first->order)), 0);

    Free(first);
    Free(second);
    Free(pool);
}
