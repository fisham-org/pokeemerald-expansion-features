#include "global.h"
#include "malloc.h"
#include "pokedoku.h"
#include "pokemon.h"
#include "random.h"
#include "constants/pokemon.h"

enum PokedokuKind
{
    KIND_TYPE,
    KIND_GEN,
    KIND_STAGE,
    KIND_LEGENDARY,
    KIND_MYTHICAL,
    KIND_ULTRA_BEAST,
    KIND_PARADOX,
    KIND_COLOR,
    KIND_EGG_GROUP,
    KIND_BASE_STAT_100, // param: stat
    KIND_BST_AT_LEAST,
    KIND_BST_UNDER,
    KIND_WEIGHT_AT_LEAST, // hectograms
    KIND_WEIGHT_UNDER,
    KIND_HEIGHT_AT_LEAST, // decimeters
    KIND_HEIGHT_UNDER,
    KIND_MONOTYPE,
    KIND_GENDERLESS,
};

#define TYPE_CRIT(id, name) [CRIT_TYPE_##id] = { POKEDOKU_GROUP_TYPE, KIND_TYPE, TYPE_##id, COMPOUND_STRING(name), COMPOUND_STRING("Has the " name " type.") }
#define GEN_CRIT(n, desc)   [CRIT_GEN_##n] = { POKEDOKU_GROUP_GEN, KIND_GEN, n, COMPOUND_STRING("Gen " #n), COMPOUND_STRING(desc) }
#define COLOR_CRIT(id, name) [CRIT_COLOR_##id] = { POKEDOKU_GROUP_COLOR, KIND_COLOR, BODY_COLOR_##id, COMPOUND_STRING("Color\n" name), COMPOUND_STRING("Its Pokédex color is " name ".") }
#define EGG_CRIT(id, name) [CRIT_EGG_##id] = { POKEDOKU_GROUP_EGG, KIND_EGG_GROUP, EGG_GROUP_##id, COMPOUND_STRING("Egg Group\n" name), COMPOUND_STRING("In the " name " Egg Group.") }
#define STAT_CRIT(id, stat, name) [CRIT_##id##_100] = { POKEDOKU_GROUP_STATS, KIND_BASE_STAT_100, stat, COMPOUND_STRING(name "\n100+"), COMPOUND_STRING("Base " name " of 100 or more.") }

const struct PokedokuCriterion gPokedokuCriteria[POKEDOKU_CRITERIA_COUNT] =
{
    TYPE_CRIT(NORMAL,   "Normal"),
    TYPE_CRIT(FIGHTING, "Fighting"),
    TYPE_CRIT(FLYING,   "Flying"),
    TYPE_CRIT(POISON,   "Poison"),
    TYPE_CRIT(GROUND,   "Ground"),
    TYPE_CRIT(ROCK,     "Rock"),
    TYPE_CRIT(BUG,      "Bug"),
    TYPE_CRIT(GHOST,    "Ghost"),
    TYPE_CRIT(STEEL,    "Steel"),
    TYPE_CRIT(FIRE,     "Fire"),
    TYPE_CRIT(WATER,    "Water"),
    TYPE_CRIT(GRASS,    "Grass"),
    TYPE_CRIT(ELECTRIC, "Electric"),
    TYPE_CRIT(PSYCHIC,  "Psychic"),
    TYPE_CRIT(ICE,      "Ice"),
    TYPE_CRIT(DRAGON,   "Dragon"),
    TYPE_CRIT(DARK,     "Dark"),
    TYPE_CRIT(FAIRY,    "Fairy"),

    GEN_CRIT(1, "Debuted in Gen 1 (Kanto)."),
    GEN_CRIT(2, "Debuted in Gen 2 (Johto)."),
    GEN_CRIT(3, "Debuted in Gen 3 (Hoenn)."),
    GEN_CRIT(4, "Debuted in Gen 4 (Sinnoh)."),
    GEN_CRIT(5, "Debuted in Gen 5 (Unova)."),
    GEN_CRIT(6, "Debuted in Gen 6 (Kalos)."),
    GEN_CRIT(7, "Debuted in Gen 7 (Alola)."),
    GEN_CRIT(8, "Debuted in Gen 8 (Galar)."),
    GEN_CRIT(9, "Debuted in Gen 9 (Paldea)."),

    [CRIT_STAGE_BASIC]  = { POKEDOKU_GROUP_STAGE, KIND_STAGE, DEX_STAGE_BASIC,  COMPOUND_STRING("Basic\nStage"),  COMPOUND_STRING("First stage of a line that evolves.") },
    [CRIT_STAGE_MIDDLE] = { POKEDOKU_GROUP_STAGE, KIND_STAGE, DEX_STAGE_MIDDLE, COMPOUND_STRING("Middle\nStage"), COMPOUND_STRING("Evolved, and can evolve again.") },
    [CRIT_STAGE_FINAL]  = { POKEDOKU_GROUP_STAGE, KIND_STAGE, DEX_STAGE_FINAL,  COMPOUND_STRING("Fully\nEvolved"), COMPOUND_STRING("Evolved, and can't evolve further.") },
    [CRIT_STAGE_SINGLE] = { POKEDOKU_GROUP_STAGE, KIND_STAGE, DEX_STAGE_SINGLE, COMPOUND_STRING("No\nEvolution"), COMPOUND_STRING("Doesn't evolve to or from anything.") },

    [CRIT_LEGENDARY]   = { POKEDOKU_GROUP_CLASS, KIND_LEGENDARY,   0, COMPOUND_STRING("Legendary"),     COMPOUND_STRING("A Legendary Pokémon.") },
    [CRIT_MYTHICAL]    = { POKEDOKU_GROUP_CLASS, KIND_MYTHICAL,    0, COMPOUND_STRING("Mythical"),      COMPOUND_STRING("A Mythical Pokémon.") },
    [CRIT_ULTRA_BEAST] = { POKEDOKU_GROUP_CLASS, KIND_ULTRA_BEAST, 0, COMPOUND_STRING("Ultra\nBeast"),  COMPOUND_STRING("An Ultra Beast.") },
    [CRIT_PARADOX]     = { POKEDOKU_GROUP_CLASS, KIND_PARADOX,     0, COMPOUND_STRING("Paradox"),       COMPOUND_STRING("A Paradox Pokémon.") },

    COLOR_CRIT(RED,    "Red"),
    COLOR_CRIT(BLUE,   "Blue"),
    COLOR_CRIT(YELLOW, "Yellow"),
    COLOR_CRIT(GREEN,  "Green"),
    COLOR_CRIT(BLACK,  "Black"),
    COLOR_CRIT(BROWN,  "Brown"),
    COLOR_CRIT(PURPLE, "Purple"),
    COLOR_CRIT(GRAY,   "Gray"),
    COLOR_CRIT(WHITE,  "White"),
    COLOR_CRIT(PINK,   "Pink"),

    EGG_CRIT(MONSTER,    "Monster"),
    EGG_CRIT(WATER_1,    "Water 1"),
    EGG_CRIT(BUG,        "Bug"),
    EGG_CRIT(FLYING,     "Flying"),
    EGG_CRIT(FIELD,      "Field"),
    EGG_CRIT(FAIRY,      "Fairy"),
    EGG_CRIT(GRASS,      "Grass"),
    EGG_CRIT(HUMAN_LIKE, "Human-Like"),
    EGG_CRIT(WATER_3,    "Water 3"),
    EGG_CRIT(MINERAL,    "Mineral"),
    EGG_CRIT(AMORPHOUS,  "Amorphous"),
    EGG_CRIT(WATER_2,    "Water 2"),
    EGG_CRIT(DRAGON,     "Dragon"),
    [CRIT_EGG_NO_EGGS] = { POKEDOKU_GROUP_EGG, KIND_EGG_GROUP, EGG_GROUP_NO_EGGS_DISCOVERED, COMPOUND_STRING("No Eggs\nDiscovered"), COMPOUND_STRING("In the No Eggs Discovered group.") },

    STAT_CRIT(HP,    STAT_HP,    "HP"),
    STAT_CRIT(ATK,   STAT_ATK,   "Attack"),
    STAT_CRIT(DEF,   STAT_DEF,   "Defense"),
    STAT_CRIT(SPEED, STAT_SPEED, "Speed"),
    STAT_CRIT(SPATK, STAT_SPATK, "Sp. Atk"),
    STAT_CRIT(SPDEF, STAT_SPDEF, "Sp. Def"),
    [CRIT_BST_500]       = { POKEDOKU_GROUP_STATS, KIND_BST_AT_LEAST, 500, COMPOUND_STRING("Base Stats\n500+"),      COMPOUND_STRING("Base stat total of 500 or more.") },
    [CRIT_BST_UNDER_300] = { POKEDOKU_GROUP_STATS, KIND_BST_UNDER,    300, COMPOUND_STRING("Base Stats\nUnder 300"), COMPOUND_STRING("Base stat total below 300.") },

    [CRIT_WEIGHT_100KG]      = { POKEDOKU_GROUP_BODY, KIND_WEIGHT_AT_LEAST, 1000, COMPOUND_STRING("Weight\n100kg+"),     COMPOUND_STRING("Weighs 100 kg or more.") },
    [CRIT_WEIGHT_UNDER_5KG]  = { POKEDOKU_GROUP_BODY, KIND_WEIGHT_UNDER,    50,   COMPOUND_STRING("Weight\nUnder 5kg"),  COMPOUND_STRING("Weighs less than 5 kg.") },
    [CRIT_HEIGHT_2M]         = { POKEDOKU_GROUP_BODY, KIND_HEIGHT_AT_LEAST, 20,   COMPOUND_STRING("Height\n2m+"),        COMPOUND_STRING("Is 2 m tall or more.") },
    [CRIT_HEIGHT_UNDER_50CM] = { POKEDOKU_GROUP_BODY, KIND_HEIGHT_UNDER,    5,    COMPOUND_STRING("Height\nUnder 0.5m"), COMPOUND_STRING("Is shorter than 0.5 m.") },

    [CRIT_MONOTYPE]   = { POKEDOKU_GROUP_MISC, KIND_MONOTYPE,   0, COMPOUND_STRING("Single\nType"), COMPOUND_STRING("Has only one type.") },
    [CRIT_GENDERLESS] = { POKEDOKU_GROUP_MISC, KIND_GENDERLESS, 0, COMPOUND_STRING("Gender-\nless"), COMPOUND_STRING("Has no gender.") },
};

#undef TYPE_CRIT
#undef GEN_CRIT
#undef COLOR_CRIT
#undef EGG_CRIT
#undef STAT_CRIT

// How often each group is picked for a header. Types dominate, like the original game.
static const u8 sGroupWeights[POKEDOKU_GROUP_COUNT] =
{
    [POKEDOKU_GROUP_TYPE]  = 10,
    [POKEDOKU_GROUP_GEN]   = 3,
    [POKEDOKU_GROUP_STAGE] = 2,
    [POKEDOKU_GROUP_CLASS] = 1,
    [POKEDOKU_GROUP_COLOR] = 2,
    [POKEDOKU_GROUP_EGG]   = 2,
    [POKEDOKU_GROUP_STATS] = 2,
    [POKEDOKU_GROUP_BODY]  = 1,
    [POKEDOKU_GROUP_MISC]  = 1,
};

// Rows: Water, Normal, Poison. Columns: Gen 1, Basic, Fully Evolved. Solvable with Gen 1 alone.
const u8 gPokedokuFallbackBoard[POKEDOKU_GRID_SIZE * 2] =
{
    CRIT_TYPE_WATER, CRIT_TYPE_NORMAL, CRIT_TYPE_POISON,
    CRIT_GEN_1, CRIT_STAGE_BASIC, CRIT_STAGE_FINAL,
};

#define BITSET_WORDS ((NATIONAL_DEX_COUNT + 32) / 32)

struct PokedokuSolver
{
    u32 cells[POKEDOKU_NUM_CELLS][BITSET_WORDS];
    u32 visited[BITSET_WORDS];
    u16 assigned[POKEDOKU_NUM_CELLS]; // National Dex number matched to each cell
};

struct PokedokuGenCache
{
    u32 bits[POKEDOKU_CRITERIA_COUNT][BITSET_WORDS];
    bool8 computed[POKEDOKU_CRITERIA_COUNT];
};

bool32 Pokedoku_MatchesCriterion(const struct DexPool *pool, u32 dex, u32 criterion)
{
    const struct SpeciesInfo *info;
    u32 param = gPokedokuCriteria[criterion].param;

    if (dex == 0 || dex > NATIONAL_DEX_COUNT || pool->species[dex] == SPECIES_NONE)
        return FALSE;

    info = &gSpeciesInfo[pool->species[dex]];
    switch (gPokedokuCriteria[criterion].kind)
    {
    case KIND_TYPE:
        return info->types[0] == param || info->types[1] == param;
    case KIND_GEN:
        return DexPool_GetGeneration(dex) == param;
    case KIND_STAGE:
        return pool->stage[dex] == param;
    case KIND_LEGENDARY:
        return info->isRestrictedLegendary || info->isSubLegendary;
    case KIND_MYTHICAL:
        return info->isMythical;
    case KIND_ULTRA_BEAST:
        return info->isUltraBeast;
    case KIND_PARADOX:
        return info->isParadox;
    case KIND_COLOR:
        return info->bodyColor == param;
    case KIND_EGG_GROUP:
        return info->eggGroups[0] == param || info->eggGroups[1] == param;
    case KIND_BASE_STAT_100:
        return GetSpeciesBaseStat(pool->species[dex], param) >= 100;
    case KIND_BST_AT_LEAST:
        return GetSpeciesBaseStatTotal(pool->species[dex]) >= param;
    case KIND_BST_UNDER:
        return GetSpeciesBaseStatTotal(pool->species[dex]) < param;
    case KIND_WEIGHT_AT_LEAST:
        return info->weight >= param;
    case KIND_WEIGHT_UNDER:
        return info->weight < param;
    case KIND_HEIGHT_AT_LEAST:
        return info->height >= param;
    case KIND_HEIGHT_UNDER:
        return info->height < param;
    case KIND_MONOTYPE:
        return info->types[0] == info->types[1];
    case KIND_GENDERLESS:
        return info->genderRatio == MON_GENDERLESS;
    }
    return FALSE;
}

bool32 Pokedoku_IsValidGuess(const struct DexPool *pool, const u8 *criteria, u32 cell, u32 dex)
{
    return Pokedoku_MatchesCriterion(pool, dex, criteria[cell / POKEDOKU_GRID_SIZE])
        && Pokedoku_MatchesCriterion(pool, dex, criteria[POKEDOKU_GRID_SIZE + cell % POKEDOKU_GRID_SIZE]);
}

// Only available species count as answers when generating a board
static void FillCriterionBits(const struct DexPool *pool, u32 criterion, u32 *bits)
{
    u32 dex;

    memset(bits, 0, sizeof(u32) * BITSET_WORDS);
    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        if (pool->available[dex] && Pokedoku_MatchesCriterion(pool, dex, criterion))
            bits[dex / 32] |= 1u << (dex % 32);
    }
}

// Kuhn's augmenting path step: find a species for this cell, bumping other cells if needed.
static bool32 TryAssignCell(struct PokedokuSolver *solver, u32 cell)
{
    u32 word, bit, dex, other;

    for (word = 0; word < BITSET_WORDS; word++)
    {
        u32 candidates = solver->cells[cell][word] & ~solver->visited[word];

        for (bit = 0; candidates != 0; bit++, candidates >>= 1)
        {
            if (!(candidates & 1))
                continue;
            dex = word * 32 + bit;
            solver->visited[word] |= 1u << bit;

            for (other = 0; other < POKEDOKU_NUM_CELLS; other++)
            {
                if (solver->assigned[other] == dex)
                    break;
            }
            if (other == POKEDOKU_NUM_CELLS || TryAssignCell(solver, other))
            {
                solver->assigned[cell] = dex;
                return TRUE;
            }
        }
    }
    return FALSE;
}

// critBits holds rows then columns. Checks the minimum answer count per cell,
// then that 9 different species can fill the board.
static bool32 IsSolvable(struct PokedokuSolver *solver, u32 (*const critBits[POKEDOKU_GRID_SIZE * 2]), u32 minAnswers)
{
    u32 cell, word, count;

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
    {
        const u32 *rowBits = critBits[cell / POKEDOKU_GRID_SIZE];
        const u32 *colBits = critBits[POKEDOKU_GRID_SIZE + cell % POKEDOKU_GRID_SIZE];

        count = 0;
        for (word = 0; word < BITSET_WORDS; word++)
        {
            solver->cells[cell][word] = rowBits[word] & colBits[word];
            count += __builtin_popcount(solver->cells[cell][word]);
        }
        if (count < minAnswers)
            return FALSE;
        solver->assigned[cell] = 0;
    }

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
    {
        memset(solver->visited, 0, sizeof(solver->visited));
        if (!TryAssignCell(solver, cell))
            return FALSE;
    }
    return TRUE;
}

bool32 Pokedoku_IsBoardSolvable(const struct DexPool *pool, const u8 *criteria, u32 minAnswers)
{
    struct PokedokuSolver *solver = Alloc(sizeof(struct PokedokuSolver));
    u32 (*bits)[BITSET_WORDS] = Alloc(sizeof(u32) * BITSET_WORDS * POKEDOKU_GRID_SIZE * 2);
    u32 *critBits[POKEDOKU_GRID_SIZE * 2];
    bool32 result = FALSE;
    u32 i;

    if (solver != NULL && bits != NULL)
    {
        for (i = 0; i < POKEDOKU_GRID_SIZE * 2; i++)
        {
            FillCriterionBits(pool, criteria[i], bits[i]);
            critBits[i] = bits[i];
        }
        result = IsSolvable(solver, critBits, minAnswers);
    }
    TRY_FREE_AND_SET_NULL(solver);
    TRY_FREE_AND_SET_NULL(bits);
    return result;
}

static void PickCriteria(rng_value_t *rng, const u8 *groupStart, const u8 *groupCount, u8 *criteria)
{
    u32 totalWeight = 0;
    u32 i, j, group, roll;
    bool32 rowGroups[POKEDOKU_GROUP_COUNT] = {0};

    for (group = 0; group < POKEDOKU_GROUP_COUNT; group++)
        totalWeight += sGroupWeights[group];

    for (i = 0; i < POKEDOKU_GRID_SIZE * 2; i++)
    {
        while (TRUE)
        {
            roll = LocalRandom(rng) % totalWeight;
            for (group = 0; roll >= sGroupWeights[group]; group++)
                roll -= sGroupWeights[group];

            // Two headers from the same group on opposite axes can't overlap (Gen 1 x Gen 2),
            // except for types, which make dual-type cells.
            if (i >= POKEDOKU_GRID_SIZE && group != POKEDOKU_GROUP_TYPE && rowGroups[group])
                continue;

            criteria[i] = groupStart[group] + LocalRandom(rng) % groupCount[group];
            for (j = 0; j < i; j++)
            {
                if (criteria[j] == criteria[i])
                    break;
            }
            if (j == i)
                break;
        }
        if (i < POKEDOKU_GRID_SIZE)
            rowGroups[group] = TRUE;
    }
}

// Deterministic for a given seed (the day, for daily boards) and answer pool.
// Tries for boards with POKEDOKU_MIN_ANSWERS_PER_CELL answers per cell, then (for small
// seen/caught pools) boards with at least 1, then the fallback board. Returns FALSE if
// the answer pool can't make any solvable board.
bool32 Pokedoku_GenerateBoard(const struct DexPool *pool, u32 seed, u8 *criteria)
{
    static const u8 sMinAnswersPerPass[] = {POKEDOKU_MIN_ANSWERS_PER_CELL, 1};
    struct PokedokuGenCache *cache = AllocZeroed(sizeof(struct PokedokuGenCache));
    struct PokedokuSolver *solver = Alloc(sizeof(struct PokedokuSolver));
    u8 groupStart[POKEDOKU_GROUP_COUNT] = {0};
    u8 groupCount[POKEDOKU_GROUP_COUNT] = {0};
    u32 *critBits[POKEDOKU_GRID_SIZE * 2];
    rng_value_t rng = LocalRandomSeed(seed * 2654435761u + 0xD0C0);
    bool32 found = FALSE;
    u32 i, pass, try;

    for (i = POKEDOKU_CRITERIA_COUNT; i-- > 0;)
    {
        groupStart[gPokedokuCriteria[i].group] = i;
        groupCount[gPokedokuCriteria[i].group]++;
    }

    for (pass = 0; pass < ARRAY_COUNT(sMinAnswersPerPass) && cache != NULL && solver != NULL && !found; pass++)
    {
        for (try = 0; try < POKEDOKU_MAX_GENERATION_TRIES && !found; try++)
        {
            PickCriteria(&rng, groupStart, groupCount, criteria);
            for (i = 0; i < POKEDOKU_GRID_SIZE * 2; i++)
            {
                if (!cache->computed[criteria[i]])
                {
                    FillCriterionBits(pool, criteria[i], cache->bits[criteria[i]]);
                    cache->computed[criteria[i]] = TRUE;
                }
                critBits[i] = cache->bits[criteria[i]];
            }
            found = IsSolvable(solver, critBits, sMinAnswersPerPass[pass]);
        }
    }

    TRY_FREE_AND_SET_NULL(cache);
    TRY_FREE_AND_SET_NULL(solver);

    if (!found && Pokedoku_IsBoardSolvable(pool, gPokedokuFallbackBoard, 1))
    {
        memcpy(criteria, gPokedokuFallbackBoard, sizeof(gPokedokuFallbackBoard));
        found = TRUE;
    }
    return found;
}

// The popularity of every enabled species (not just available ones) that fits a cell.
// A pick's popularity percent is its share of this total.
u32 Pokedoku_GetCellPopularityTotal(const struct DexPool *pool, const u8 *criteria, u32 cell)
{
    u32 dex, total = 0;

    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        if (pool->species[dex] != SPECIES_NONE && Pokedoku_IsValidGuess(pool, criteria, cell, dex))
            total += pool->popularity[dex];
    }
    return total;
}

// Rounded, and at least 1% so every correct pick adds to the rarity score
u32 Pokedoku_GetPopularityPercent(const struct DexPool *pool, u32 dex, u32 cellTotal)
{
    if (cellTotal == 0)
        return 0;
    return max(1, (pool->popularity[dex] * 100 + cellTotal / 2) / cellTotal);
}

// Lower is better: each correct cell adds its pick's popularity percent, each other cell adds 100
u32 Pokedoku_GetRarityScore(const struct DexPool *pool, const struct PokedokuSave *board, const u32 *cellTotals)
{
    u32 cell, score = 0;

    for (cell = 0; cell < POKEDOKU_NUM_CELLS; cell++)
    {
        if (board->correctMask & (1 << cell))
            score += Pokedoku_GetPopularityPercent(pool, DexPool_GetDexOfSpecies(board->guesses[cell]), cellTotals[cell]);
        else
            score += 100;
    }
    return score;
}
