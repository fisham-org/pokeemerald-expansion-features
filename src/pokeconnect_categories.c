#include "global.h"
#include "battle_main.h"
#include "battle_message.h"
#include "malloc.h"
#include "pokeconnect.h"
#include "pokemon.h"
#include "random.h"
#include "string_util.h"
#include "constants/abilities.h"
#include "constants/characters.h"
#include "constants/items.h"
#include "constants/pokemon.h"

#define SET_END 0xFFFF

static const u8 sTypes[] =
{
    TYPE_NORMAL, TYPE_FIGHTING, TYPE_FLYING, TYPE_POISON, TYPE_GROUND, TYPE_ROCK,
    TYPE_BUG, TYPE_GHOST, TYPE_STEEL, TYPE_FIRE, TYPE_WATER, TYPE_GRASS,
    TYPE_ELECTRIC, TYPE_PSYCHIC, TYPE_ICE, TYPE_DRAGON, TYPE_DARK, TYPE_FAIRY,
};

static const u8 sEggGroups[] =
{
    EGG_GROUP_MONSTER, EGG_GROUP_WATER_1, EGG_GROUP_BUG, EGG_GROUP_FLYING, EGG_GROUP_FIELD,
    EGG_GROUP_FAIRY, EGG_GROUP_GRASS, EGG_GROUP_HUMAN_LIKE, EGG_GROUP_WATER_3, EGG_GROUP_MINERAL,
    EGG_GROUP_AMORPHOUS, EGG_GROUP_WATER_2, EGG_GROUP_DRAGON, EGG_GROUP_NO_EGGS_DISCOVERED,
};

// Well-known abilities. Only a species' two regular slots count, not its hidden ability.
static const u16 sAbilities[] =
{
    ABILITY_LEVITATE, ABILITY_INTIMIDATE, ABILITY_SWIFT_SWIM, ABILITY_CHLOROPHYLL,
    ABILITY_STURDY, ABILITY_KEEN_EYE, ABILITY_OVERGROW, ABILITY_BLAZE,
    ABILITY_TORRENT, ABILITY_SWARM, ABILITY_STATIC, ABILITY_FLASH_FIRE,
    ABILITY_THICK_FAT, ABILITY_INNER_FOCUS, ABILITY_SYNCHRONIZE, ABILITY_CUTE_CHARM,
    ABILITY_ROCK_HEAD, ABILITY_POISON_POINT, ABILITY_SHELL_ARMOR, ABILITY_PRESSURE,
    ABILITY_SAND_VEIL, ABILITY_RUN_AWAY, ABILITY_WATER_ABSORB, ABILITY_GUTS,
    ABILITY_TECHNICIAN,
};

static const u8 sGenders[] = { MON_GENDERLESS, MON_FEMALE, MON_MALE };

static const u16 sSetNumberWords[] =
{
    NATIONAL_DEX_TENTACOOL, NATIONAL_DEX_TENTACRUEL, NATIONAL_DEX_NINETALES, NATIONAL_DEX_PHIONE,
    NATIONAL_DEX_HONEDGE, NATIONAL_DEX_COTTONEE, NATIONAL_DEX_LUNATONE, SET_END,
};

static const u16 sSetPikachuClones[] =
{
    NATIONAL_DEX_PIKACHU, NATIONAL_DEX_PLUSLE, NATIONAL_DEX_MINUN, NATIONAL_DEX_PACHIRISU, NATIONAL_DEX_EMOLGA,
    NATIONAL_DEX_DEDENNE, NATIONAL_DEX_TOGEDEMARU, NATIONAL_DEX_MORPEKO, NATIONAL_DEX_PAWMI, SET_END,
};

static const u16 sSetFirstPartners[] =
{
    NATIONAL_DEX_BULBASAUR, NATIONAL_DEX_CHARMANDER, NATIONAL_DEX_SQUIRTLE,
    NATIONAL_DEX_CHIKORITA, NATIONAL_DEX_CYNDAQUIL, NATIONAL_DEX_TOTODILE,
    NATIONAL_DEX_TREECKO, NATIONAL_DEX_TORCHIC, NATIONAL_DEX_MUDKIP,
    NATIONAL_DEX_TURTWIG, NATIONAL_DEX_CHIMCHAR, NATIONAL_DEX_PIPLUP,
    NATIONAL_DEX_SNIVY, NATIONAL_DEX_TEPIG, NATIONAL_DEX_OSHAWOTT,
    NATIONAL_DEX_CHESPIN, NATIONAL_DEX_FENNEKIN, NATIONAL_DEX_FROAKIE,
    NATIONAL_DEX_ROWLET, NATIONAL_DEX_LITTEN, NATIONAL_DEX_POPPLIO,
    NATIONAL_DEX_GROOKEY, NATIONAL_DEX_SCORBUNNY, NATIONAL_DEX_SOBBLE,
    NATIONAL_DEX_SPRIGATITO, NATIONAL_DEX_FUECOCO, NATIONAL_DEX_QUAXLY, SET_END,
};

static const u16 sSetEarlyBirds[] =
{
    NATIONAL_DEX_PIDGEY, NATIONAL_DEX_SPEAROW, NATIONAL_DEX_HOOTHOOT, NATIONAL_DEX_TAILLOW, NATIONAL_DEX_STARLY,
    NATIONAL_DEX_PIDOVE, NATIONAL_DEX_FLETCHLING, NATIONAL_DEX_PIKIPEK, NATIONAL_DEX_ROOKIDEE, NATIONAL_DEX_WATTREL, SET_END,
};

static const u16 sSetEarlyMammals[] =
{
    NATIONAL_DEX_RATTATA, NATIONAL_DEX_SENTRET, NATIONAL_DEX_ZIGZAGOON, NATIONAL_DEX_BIDOOF, NATIONAL_DEX_PATRAT,
    NATIONAL_DEX_BUNNELBY, NATIONAL_DEX_YUNGOOS, NATIONAL_DEX_SKWOVET, NATIONAL_DEX_LECHONK, SET_END,
};

static const u16 sSetFossils[] =
{
    NATIONAL_DEX_OMANYTE, NATIONAL_DEX_KABUTO, NATIONAL_DEX_AERODACTYL, NATIONAL_DEX_LILEEP, NATIONAL_DEX_ANORITH,
    NATIONAL_DEX_CRANIDOS, NATIONAL_DEX_SHIELDON, NATIONAL_DEX_TIRTOUGA, NATIONAL_DEX_ARCHEN, NATIONAL_DEX_TYRUNT,
    NATIONAL_DEX_AMAURA, NATIONAL_DEX_DRACOZOLT, NATIONAL_DEX_ARCTOZOLT, NATIONAL_DEX_DRACOVISH, NATIONAL_DEX_ARCTOVISH, SET_END,
};

static const u16 sSetCoverLegendaries[] =
{
    NATIONAL_DEX_LUGIA, NATIONAL_DEX_HO_OH, NATIONAL_DEX_KYOGRE, NATIONAL_DEX_GROUDON, NATIONAL_DEX_RAYQUAZA,
    NATIONAL_DEX_DIALGA, NATIONAL_DEX_PALKIA, NATIONAL_DEX_GIRATINA, NATIONAL_DEX_RESHIRAM, NATIONAL_DEX_ZEKROM,
    NATIONAL_DEX_XERNEAS, NATIONAL_DEX_YVELTAL, NATIONAL_DEX_SOLGALEO, NATIONAL_DEX_LUNALA, NATIONAL_DEX_ZACIAN,
    NATIONAL_DEX_ZAMAZENTA, NATIONAL_DEX_KORAIDON, NATIONAL_DEX_MIRAIDON, SET_END,
};

static const u16 sSetFood[] =
{
    NATIONAL_DEX_APPLIN, NATIONAL_DEX_MILCERY, NATIONAL_DEX_SWIRLIX, NATIONAL_DEX_TATSUGIRI,
    NATIONAL_DEX_SMOLIV, NATIONAL_DEX_CAPSAKID, NATIONAL_DEX_CHERUBI, NATIONAL_DEX_VANILLITE, SET_END,
};

// Hand-written sets: add an entry to enum PokeconnectSet and here. A set needs 4+ species in
// the answer pool, and 4+ that fit none of a puzzle's other categories, to be used.
static const struct { const u16 *species; const u8 *name; } sSets[PCON_SET_COUNT] =
{
    [PCON_SET_NUMBER_WORDS]      = { sSetNumberWords,      COMPOUND_STRING("Hide a number: TEN, NINE, ONE") },
    [PCON_SET_PIKACHU_CLONES]    = { sSetPikachuClones,    COMPOUND_STRING("Pikachu clones") },
    [PCON_SET_FIRST_PARTNERS]    = { sSetFirstPartners,    COMPOUND_STRING("First partner Pokémon") },
    [PCON_SET_EARLY_BIRDS]       = { sSetEarlyBirds,       COMPOUND_STRING("Early-route birds") },
    [PCON_SET_EARLY_MAMMALS]     = { sSetEarlyMammals,     COMPOUND_STRING("Early-route mammals") },
    [PCON_SET_FOSSILS]           = { sSetFossils,          COMPOUND_STRING("Revived from fossils") },
    [PCON_SET_COVER_LEGENDARIES] = { sSetCoverLegendaries, COMPOUND_STRING("Box art Legendaries") },
    [PCON_SET_FOOD]              = { sSetFood,             COMPOUND_STRING("Based on food") },
};

#define NUM_DUAL_TYPES (ARRAY_COUNT(sTypes) * (ARRAY_COUNT(sTypes) - 1) / 2)

// How many categories each kind has, and how often it's picked for a puzzle
static const struct { u16 count; u8 weight; } sKinds[PCON_KIND_COUNT] =
{
    [PCON_KIND_TYPE]         = { ARRAY_COUNT(sTypes),      4 },
    [PCON_KIND_COLOR]        = { BODY_COLOR_PINK + 1,      3 },
    [PCON_KIND_GEN]          = { 9,                        2 },
    [PCON_KIND_EGG_GROUP]    = { ARRAY_COUNT(sEggGroups),  1 },
    [PCON_KIND_STAGE]        = { DEX_STAGE_FINAL + 1,      1 },
    [PCON_KIND_CLASS]        = { PCON_CLASS_COUNT,         2 },
    [PCON_KIND_ABILITY]      = { ARRAY_COUNT(sAbilities),  3 },
    [PCON_KIND_EVO_METHOD]   = { PCON_EVO_COUNT,           2 },
    [PCON_KIND_GENDER]       = { ARRAY_COUNT(sGenders),    1 },
    [PCON_KIND_FIRST_LETTER] = { 26,                       1 },
    [PCON_KIND_SET]          = { PCON_SET_COUNT,           3 },
    [PCON_KIND_BEST_STAT]    = { NUM_STATS,                2 },
    [PCON_KIND_PURE_TYPE]    = { ARRAY_COUNT(sTypes),      2 },
    [PCON_KIND_FORM]         = { PCON_FORM_COUNT,          2 },
    [PCON_KIND_BRANCH_EVO]   = { 1,                        1 },
    [PCON_KIND_DUAL_TYPE]    = { NUM_DUAL_TYPES,           2 },
};

// Every kind before PCON_KIND_DUAL_TYPE. Dual types are worked out from two type bitsets instead.
#define NUM_CACHED_CATEGORIES (ARRAY_COUNT(sTypes) + BODY_COLOR_PINK + 1 + 9 + ARRAY_COUNT(sEggGroups) \
                               + DEX_STAGE_FINAL + 1 + PCON_CLASS_COUNT + ARRAY_COUNT(sAbilities) + PCON_EVO_COUNT \
                               + ARRAY_COUNT(sGenders) + 26 + PCON_SET_COUNT \
                               + NUM_STATS + ARRAY_COUNT(sTypes) + PCON_FORM_COUNT + 1)
#define NUM_CATEGORIES (NUM_CACHED_CATEGORIES + NUM_DUAL_TYPES)

#define BITSET_WORDS ((NATIONAL_DEX_COUNT + 32) / 32)
#define NUM_QUADS 1820 // 16 choose 4
// Puzzles tried with the same 4 kinds before picking new ones. Re-rolling only the params
// gives kinds with few working categories a fair chance, rather than favouring broad kinds.
#define TRIES_PER_KINDS 20

struct PokeconnectGen
{
    u32 bits[NUM_CACHED_CATEGORIES][BITSET_WORDS];
    bool8 computed[NUM_CACHED_CATEGORIES];
    u32 dualBits[POKECONNECT_NUM_GROUPS][BITSET_WORDS];
    u16 quads[NUM_QUADS];
};

u32 Pokeconnect_GetCatalogSize(void)
{
    return NUM_CATEGORIES;
}

static u32 GetKindStart(u32 kind)
{
    u32 i, start = 0;

    for (i = 0; i < kind; i++)
        start += sKinds[i].count;
    return start;
}

static u32 GetDualTypeParam(u32 index)
{
    u32 first = 0;

    while (index >= ARRAY_COUNT(sTypes) - 1 - first)
    {
        index -= ARRAY_COUNT(sTypes) - 1 - first;
        first++;
    }
    return sTypes[first] | (sTypes[first + 1 + index] << 8);
}

static struct PokeconnectCategory MakeCategory(u32 kind, u32 index)
{
    struct PokeconnectCategory category = { .kind = kind };

    switch (kind)
    {
    case PCON_KIND_TYPE:
    case PCON_KIND_PURE_TYPE:
        category.param = sTypes[index];
        break;
    case PCON_KIND_GEN:
        category.param = index + 1;
        break;
    case PCON_KIND_EGG_GROUP:
        category.param = sEggGroups[index];
        break;
    case PCON_KIND_ABILITY:
        category.param = sAbilities[index];
        break;
    case PCON_KIND_GENDER:
        category.param = sGenders[index];
        break;
    case PCON_KIND_DUAL_TYPE:
        category.param = GetDualTypeParam(index);
        break;
    default:
        category.param = index;
        break;
    }
    return category;
}

struct PokeconnectCategory Pokeconnect_GetCatalogCategory(u32 index)
{
    u32 kind;

    for (kind = 0; index >= sKinds[kind].count; kind++)
        index -= sKinds[kind].count;
    return MakeCategory(kind, index);
}

static bool32 IsEvolutionStone(u32 item)
{
    return item >= ITEM_FIRE_STONE && item <= ITEM_DAWN_STONE;
}

static bool32 HasFriendshipCondition(const struct EvolutionParam *params)
{
    u32 i;

    for (i = 0; params != NULL && params[i].condition != CONDITIONS_END; i++)
    {
        if (params[i].condition == IF_MIN_FRIENDSHIP)
            return TRUE;
    }
    return FALSE;
}

// Only the base form's evolutions count, like the pool's evolution stages
static bool32 EvolvesBy(u32 species, u32 dex, u32 method)
{
    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    u32 i;

    for (i = 0; evolutions != NULL && evolutions[i].method != EVOLUTIONS_END; i++)
    {
        enum Species target = evolutions[i].targetSpecies;

        if (evolutions[i].method == EVO_NONE || !IsSpeciesEnabled(target) || gSpeciesInfo[target].natDexNum == dex)
            continue;
        if (method == PCON_EVO_STONE && evolutions[i].method == EVO_ITEM && IsEvolutionStone(evolutions[i].param))
            return TRUE;
        if (method == PCON_EVO_TRADE && evolutions[i].method == EVO_TRADE)
            return TRUE;
        if (method == PCON_EVO_FRIENDSHIP && HasFriendshipCondition(evolutions[i].params))
            return TRUE;
    }
    return FALSE;
}

static bool32 CanEvolveIntoMultiple(u32 species, u32 dex)
{
    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    u32 i, firstDex = 0;

    for (i = 0; evolutions != NULL && evolutions[i].method != EVOLUTIONS_END; i++)
    {
        enum Species target = evolutions[i].targetSpecies;
        u32 targetDex = gSpeciesInfo[target].natDexNum;

        if (evolutions[i].method == EVO_NONE || !IsSpeciesEnabled(target) || targetDex == dex)
            continue;
        if (firstDex == 0)
            firstDex = targetDex;
        else if (targetDex != firstDex)
            return TRUE;
    }
    return FALSE;
}

static bool32 HasForm(u32 species, u32 form)
{
    const u16 *forms = GetSpeciesFormTable(species);
    u32 i;

    for (i = 0; forms != NULL && forms[i] != FORM_SPECIES_END; i++)
    {
        const struct SpeciesInfo *info = &gSpeciesInfo[forms[i]];

        if (!IsSpeciesEnabled(forms[i]))
            continue;
        if ((form == PCON_FORM_MEGA && info->isMegaEvolution)
         || (form == PCON_FORM_ALOLAN && info->isAlolanForm)
         || (form == PCON_FORM_GALARIAN && info->isGalarianForm)
         || (form == PCON_FORM_HISUIAN && info->isHisuianForm))
            return TRUE;
    }
    return FALSE;
}

static bool32 IsBestStat(u32 species, u32 stat)
{
    u32 i, best = GetSpeciesBaseStat(species, stat);

    for (i = 0; i < NUM_STATS; i++)
    {
        if (i != stat && GetSpeciesBaseStat(species, i) >= best)
            return FALSE;
    }
    return TRUE;
}

static bool32 IsInSet(u32 set, u32 dex)
{
    const u16 *species;

    for (species = sSets[set].species; *species != SET_END; species++)
    {
        if (*species == dex)
            return TRUE;
    }
    return FALSE;
}

bool32 Pokeconnect_Matches(const struct DexPool *pool, u32 dex, struct PokeconnectCategory category)
{
    const struct SpeciesInfo *info;
    u32 species, param = category.param;

    if (dex == 0 || dex > NATIONAL_DEX_COUNT || pool->species[dex] == SPECIES_NONE)
        return FALSE;

    species = pool->species[dex];
    info = &gSpeciesInfo[species];
    switch (category.kind)
    {
    case PCON_KIND_TYPE:
        return info->types[0] == param || info->types[1] == param;
    case PCON_KIND_COLOR:
        return info->bodyColor == param;
    case PCON_KIND_GEN:
        return DexPool_GetGeneration(dex) == param;
    case PCON_KIND_EGG_GROUP:
        return info->eggGroups[0] == param || info->eggGroups[1] == param;
    case PCON_KIND_STAGE:
        return pool->stage[dex] == param;
    case PCON_KIND_CLASS:
        switch (param)
        {
        case PCON_CLASS_LEGENDARY:
            return info->isRestrictedLegendary || info->isSubLegendary;
        case PCON_CLASS_MYTHICAL:
            return info->isMythical;
        case PCON_CLASS_ULTRA_BEAST:
            return info->isUltraBeast;
        case PCON_CLASS_PARADOX:
            return info->isParadox;
        case PCON_CLASS_BABY:
            return pool->stage[dex] == DEX_STAGE_BASIC
                && (info->eggGroups[0] == EGG_GROUP_NO_EGGS_DISCOVERED || info->eggGroups[1] == EGG_GROUP_NO_EGGS_DISCOVERED)
                && !info->isRestrictedLegendary && !info->isSubLegendary && !info->isMythical
                && !info->isUltraBeast && !info->isParadox;
        }
        return FALSE;
    case PCON_KIND_ABILITY:
        return info->abilities[0] == param || info->abilities[1] == param;
    case PCON_KIND_EVO_METHOD:
        return EvolvesBy(species, dex, param);
    case PCON_KIND_GENDER:
        return info->genderRatio == param;
    case PCON_KIND_FIRST_LETTER:
        return GetSpeciesName(species)[0] == CHAR_A + param;
    case PCON_KIND_SET:
        return IsInSet(param, dex);
    case PCON_KIND_BEST_STAT:
        return IsBestStat(species, param);
    case PCON_KIND_PURE_TYPE:
        return info->types[0] == param && info->types[1] == param;
    case PCON_KIND_FORM:
        return HasForm(species, param);
    case PCON_KIND_BRANCH_EVO:
        return CanEvolveIntoMultiple(species, dex);
    case PCON_KIND_DUAL_TYPE:
        return (info->types[0] == (param & 0xFF) && info->types[1] == (param >> 8))
            || (info->types[0] == (param >> 8) && info->types[1] == (param & 0xFF));
    }
    return FALSE;
}

static const u8 *const sColorNames[] =
{
    [BODY_COLOR_RED]    = COMPOUND_STRING("Red"),
    [BODY_COLOR_BLUE]   = COMPOUND_STRING("Blue"),
    [BODY_COLOR_YELLOW] = COMPOUND_STRING("Yellow"),
    [BODY_COLOR_GREEN]  = COMPOUND_STRING("Green"),
    [BODY_COLOR_BLACK]  = COMPOUND_STRING("Black"),
    [BODY_COLOR_BROWN]  = COMPOUND_STRING("Brown"),
    [BODY_COLOR_PURPLE] = COMPOUND_STRING("Purple"),
    [BODY_COLOR_GRAY]   = COMPOUND_STRING("Gray"),
    [BODY_COLOR_WHITE]  = COMPOUND_STRING("White"),
    [BODY_COLOR_PINK]   = COMPOUND_STRING("Pink"),
};

static const u8 *const sEggGroupNames[] =
{
    [EGG_GROUP_MONSTER]            = COMPOUND_STRING("Monster"),
    [EGG_GROUP_WATER_1]            = COMPOUND_STRING("Water 1"),
    [EGG_GROUP_BUG]                = COMPOUND_STRING("Bug"),
    [EGG_GROUP_FLYING]             = COMPOUND_STRING("Flying"),
    [EGG_GROUP_FIELD]              = COMPOUND_STRING("Field"),
    [EGG_GROUP_FAIRY]              = COMPOUND_STRING("Fairy"),
    [EGG_GROUP_GRASS]              = COMPOUND_STRING("Grass"),
    [EGG_GROUP_HUMAN_LIKE]         = COMPOUND_STRING("Human-Like"),
    [EGG_GROUP_WATER_3]            = COMPOUND_STRING("Water 3"),
    [EGG_GROUP_MINERAL]            = COMPOUND_STRING("Mineral"),
    [EGG_GROUP_AMORPHOUS]          = COMPOUND_STRING("Amorphous"),
    [EGG_GROUP_WATER_2]            = COMPOUND_STRING("Water 2"),
    [EGG_GROUP_DRAGON]             = COMPOUND_STRING("Dragon"),
    [EGG_GROUP_NO_EGGS_DISCOVERED] = COMPOUND_STRING("No Eggs Discovered"),
};

static const u8 *const sStageNames[] =
{
    [DEX_STAGE_SINGLE] = COMPOUND_STRING("Doesn't evolve"),
    [DEX_STAGE_BASIC]  = COMPOUND_STRING("Unevolved, but can evolve"),
    [DEX_STAGE_MIDDLE] = COMPOUND_STRING("Middle evolution stage"),
    [DEX_STAGE_FINAL]  = COMPOUND_STRING("Fully evolved"),
};

static const u8 *const sClassNames[] =
{
    [PCON_CLASS_LEGENDARY]   = COMPOUND_STRING("Legendary Pokémon"),
    [PCON_CLASS_MYTHICAL]    = COMPOUND_STRING("Mythical Pokémon"),
    [PCON_CLASS_ULTRA_BEAST] = COMPOUND_STRING("Ultra Beasts"),
    [PCON_CLASS_PARADOX]     = COMPOUND_STRING("Paradox Pokémon"),
    [PCON_CLASS_BABY]        = COMPOUND_STRING("Baby Pokémon"),
};

static const u8 *const sFormNames[] =
{
    [PCON_FORM_MEGA]       = COMPOUND_STRING("Has a Mega Evolution"),
    [PCON_FORM_ALOLAN]     = COMPOUND_STRING("Has an Alolan form"),
    [PCON_FORM_GALARIAN]   = COMPOUND_STRING("Has a Galarian form"),
    [PCON_FORM_HISUIAN]    = COMPOUND_STRING("Has a Hisuian form"),
};

static const u8 *const sEvoMethodNames[] =
{
    [PCON_EVO_STONE]      = COMPOUND_STRING("Evolves using a stone"),
    [PCON_EVO_TRADE]      = COMPOUND_STRING("Evolves by trading"),
    [PCON_EVO_FRIENDSHIP] = COMPOUND_STRING("Evolves with high friendship"),
};

void Pokeconnect_GetCategoryName(struct PokeconnectCategory category, u8 *dest)
{
    u32 param = category.param;

    switch (category.kind)
    {
    case PCON_KIND_TYPE:
        dest = StringCopy(dest, gTypesInfo[param].name);
        StringCopy(dest, COMPOUND_STRING(" type"));
        break;
    case PCON_KIND_COLOR:
        dest = StringCopy(dest, COMPOUND_STRING("Pokédex color "));
        StringCopy(dest, sColorNames[param]);
        break;
    case PCON_KIND_GEN:
        dest = StringCopy(dest, COMPOUND_STRING("Debuted in Gen "));
        ConvertIntToDecimalStringN(dest, param, STR_CONV_MODE_LEFT_ALIGN, 1);
        break;
    case PCON_KIND_EGG_GROUP:
        dest = StringCopy(dest, sEggGroupNames[param]);
        StringCopy(dest, COMPOUND_STRING(" Egg Group"));
        break;
    case PCON_KIND_STAGE:
        StringCopy(dest, sStageNames[param]);
        break;
    case PCON_KIND_CLASS:
        StringCopy(dest, sClassNames[param]);
        break;
    case PCON_KIND_ABILITY:
        dest = StringCopy(dest, COMPOUND_STRING("Ability: "));
        StringCopy(dest, gAbilitiesInfo[param].name);
        break;
    case PCON_KIND_EVO_METHOD:
        StringCopy(dest, sEvoMethodNames[param]);
        break;
    case PCON_KIND_GENDER:
        if (param == MON_GENDERLESS)
            StringCopy(dest, COMPOUND_STRING("Genderless"));
        else if (param == MON_FEMALE)
            StringCopy(dest, COMPOUND_STRING("Always female"));
        else
            StringCopy(dest, COMPOUND_STRING("Always male"));
        break;
    case PCON_KIND_FIRST_LETTER:
        dest = StringCopy(dest, COMPOUND_STRING("Name starts with "));
        *dest++ = CHAR_A + param;
        *dest = EOS;
        break;
    case PCON_KIND_SET:
        StringCopy(dest, sSets[param].name);
        break;
    case PCON_KIND_BEST_STAT:
        dest = StringCopy(dest, COMPOUND_STRING("Best base stat: "));
        StringCopy(dest, gStatNamesTable[param]);
        break;
    case PCON_KIND_PURE_TYPE:
        dest = StringCopy(dest, COMPOUND_STRING("Pure "));
        dest = StringCopy(dest, gTypesInfo[param].name);
        StringCopy(dest, COMPOUND_STRING(" type"));
        break;
    case PCON_KIND_FORM:
        StringCopy(dest, sFormNames[param]);
        break;
    case PCON_KIND_BRANCH_EVO:
        StringCopy(dest, COMPOUND_STRING("Can evolve into 2+ Pokémon"));
        break;
    case PCON_KIND_DUAL_TYPE:
        dest = StringCopy(dest, gTypesInfo[param & 0xFF].name);
        dest = StringCopy(dest, COMPOUND_STRING("/"));
        dest = StringCopy(dest, gTypesInfo[param >> 8].name);
        StringCopy(dest, COMPOUND_STRING(" type"));
        break;
    default:
        *dest = EOS;
        break;
    }
}

u32 Pokeconnect_CheckGuess(u32 tileMask)
{
    u32 group, best = 0;

    for (group = 0; group < POKECONNECT_NUM_GROUPS; group++)
    {
        u32 count = __builtin_popcount(tileMask & (((1 << POKECONNECT_GROUP_SIZE) - 1) << (group * POKECONNECT_GROUP_SIZE)));
        best = max(best, count);
    }
    if (best == POKECONNECT_GROUP_SIZE)
        return PCON_GUESS_CORRECT;
    if (best == POKECONNECT_GROUP_SIZE - 1)
        return PCON_GUESS_ONE_AWAY;
    return PCON_GUESS_WRONG;
}

// ---------------------------------------------------------------------------
// Puzzle generation
// ---------------------------------------------------------------------------

static u32 CountBits(const u32 *bits)
{
    u32 word, count = 0;

    for (word = 0; word < BITSET_WORDS; word++)
        count += __builtin_popcount(bits[word]);
    return count;
}

static void FillBits(const struct DexPool *pool, struct PokeconnectCategory category, u32 *bits)
{
    u32 dex;

    memset(bits, 0, sizeof(u32) * BITSET_WORDS);
    for (dex = 1; dex <= NATIONAL_DEX_COUNT; dex++)
    {
        if (pool->available[dex] && Pokeconnect_Matches(pool, dex, category))
            bits[dex / 32] |= 1u << (dex % 32);
    }
}

static const u32 *GetCachedBits(struct PokeconnectGen *gen, const struct DexPool *pool, u32 index)
{
    if (!gen->computed[index])
    {
        FillBits(pool, Pokeconnect_GetCatalogCategory(index), gen->bits[index]);
        gen->computed[index] = TRUE;
    }
    return gen->bits[index];
}

static u32 FindTypeIndex(u32 type)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sTypes) && sTypes[i] != type; i++)
        ;
    return i;
}

// Available species in the category. Dual types are built in the group's dualBits slot.
static const u32 *GetCategoryBits(struct PokeconnectGen *gen, const struct DexPool *pool, u32 index, u32 group)
{
    struct PokeconnectCategory category;
    const u32 *bits1, *bits2;
    u32 word;

    if (index < NUM_CACHED_CATEGORIES)
        return GetCachedBits(gen, pool, index);

    category = Pokeconnect_GetCatalogCategory(index);
    bits1 = GetCachedBits(gen, pool, GetKindStart(PCON_KIND_TYPE) + FindTypeIndex(category.param & 0xFF));
    bits2 = GetCachedBits(gen, pool, GetKindStart(PCON_KIND_TYPE) + FindTypeIndex(category.param >> 8));
    for (word = 0; word < BITSET_WORDS; word++)
        gen->dualBits[group][word] = bits1[word] & bits2[word];
    return gen->dualBits[group];
}

// The nth set bit, counting from 0
static u32 GetNthBit(const u32 *bits, u32 n)
{
    u32 word, bit;

    for (word = 0; word < BITSET_WORDS; word++)
    {
        u32 count = __builtin_popcount(bits[word]);

        if (n >= count)
        {
            n -= count;
            continue;
        }
        for (bit = 0; bit < 32; bit++)
        {
            if ((bits[word] & (1u << bit)) && n-- == 0)
                return word * 32 + bit;
        }
    }
    return 0;
}

static u32 CountExactCovers(const u16 *quads, u32 numQuads, u32 covered, u32 limit)
{
    u32 i, lowest, count = 0;

    if (covered == (1 << POKECONNECT_NUM_TILES) - 1)
        return 1;

    lowest = __builtin_ctz(~covered);
    for (i = 0; i < numQuads && count < limit; i++)
    {
        if ((quads[i] & (1 << lowest)) && !(quads[i] & covered))
            count += CountExactCovers(quads, numQuads, covered | quads[i], limit - count);
    }
    return count;
}

// How many ways the tiles split into 4 groups of 4 that each fit some category in the catalog,
// stopping at 2. A fair puzzle has exactly 1. quads needs room for NUM_QUADS.
static u32 CountSolutionsOfTiles(const struct DexPool *pool, const u16 *dexes, u16 *quads)
{
    u16 masks[NUM_CATEGORIES];
    u32 numMasks = 0, numQuads = 0;
    u32 i, tile, a, b, c, d;

    for (i = 0; i < NUM_CATEGORIES; i++)
    {
        struct PokeconnectCategory category = Pokeconnect_GetCatalogCategory(i);
        u32 mask = 0;

        for (tile = 0; tile < POKECONNECT_NUM_TILES; tile++)
        {
            if (Pokeconnect_Matches(pool, dexes[tile], category))
                mask |= 1 << tile;
        }
        if (__builtin_popcount(mask) >= POKECONNECT_GROUP_SIZE)
            masks[numMasks++] = mask;
    }

    for (a = 0; a < POKECONNECT_NUM_TILES; a++)
    {
        for (b = a + 1; b < POKECONNECT_NUM_TILES; b++)
        {
            for (c = b + 1; c < POKECONNECT_NUM_TILES; c++)
            {
                for (d = c + 1; d < POKECONNECT_NUM_TILES; d++)
                {
                    u32 quad = (1 << a) | (1 << b) | (1 << c) | (1 << d);

                    for (i = 0; i < numMasks; i++)
                    {
                        if ((masks[i] & quad) == quad)
                        {
                            quads[numQuads++] = quad;
                            break;
                        }
                    }
                }
            }
        }
    }
    return CountExactCovers(quads, numQuads, 0, 2);
}

u32 Pokeconnect_CountSolutions(const struct DexPool *pool, const struct PokeconnectSave *game)
{
    u16 *quads = Alloc(sizeof(u16) * NUM_QUADS);
    u16 dexes[POKECONNECT_NUM_TILES];
    u32 tile, result = 0;

    if (quads != NULL)
    {
        for (tile = 0; tile < POKECONNECT_NUM_TILES; tile++)
            dexes[tile] = DexPool_GetDexOfSpecies(game->species[tile]);
        result = CountSolutionsOfTiles(pool, dexes, quads);
        Free(quads);
    }
    return result;
}

static u32 PickKind(rng_value_t *rng, u32 usedKinds)
{
    u32 kind, totalWeight = 0, roll;

    for (kind = 0; kind < PCON_KIND_COUNT; kind++)
    {
        if (!(usedKinds & (1 << kind)))
            totalWeight += sKinds[kind].weight;
    }
    roll = LocalRandom(rng) % totalWeight;
    for (kind = 0; ; kind++)
    {
        if (usedKinds & (1 << kind))
            continue;
        if (roll < sKinds[kind].weight)
            return kind;
        roll -= sKinds[kind].weight;
    }
}

static void PickKinds(rng_value_t *rng, u8 *kinds)
{
    u32 group, usedKinds = 0;

    for (group = 0; group < POKECONNECT_NUM_GROUPS; group++)
    {
        kinds[group] = PickKind(rng, usedKinds);
        usedKinds |= 1 << kinds[group];
    }
}

// Picks a category of each of the 4 kinds, then 4 species for each that fit none of the other 3.
// Fills dexes and indices (catalog index of each group, in tile order) on success.
static bool32 TryMakePuzzle(struct PokeconnectGen *gen, const struct DexPool *pool, rng_value_t *rng, const u8 *kinds, u16 *dexes, u16 *indices, u16 *sizes)
{
    const u32 *bits[POKECONNECT_NUM_GROUPS];
    u32 exclusive[BITSET_WORDS];
    u32 group, other, word, i;

    for (group = 0; group < POKECONNECT_NUM_GROUPS; group++)
    {
        u32 kind = kinds[group];

        indices[group] = GetKindStart(kind) + LocalRandom(rng) % sKinds[kind].count;
        bits[group] = GetCategoryBits(gen, pool, indices[group], group);
        sizes[group] = CountBits(bits[group]);
        if (sizes[group] < POKECONNECT_GROUP_SIZE)
            return FALSE;
    }

    for (group = 0; group < POKECONNECT_NUM_GROUPS; group++)
    {
        u32 count;

        for (word = 0; word < BITSET_WORDS; word++)
        {
            exclusive[word] = bits[group][word];
            for (other = 0; other < POKECONNECT_NUM_GROUPS; other++)
            {
                if (other != group)
                    exclusive[word] &= ~bits[other][word];
            }
        }

        count = CountBits(exclusive);
        if (count < POKECONNECT_GROUP_SIZE)
            return FALSE;
        for (i = 0; i < POKECONNECT_GROUP_SIZE; i++)
        {
            u32 dex = GetNthBit(exclusive, LocalRandom(rng) % count--);

            exclusive[dex / 32] &= ~(1u << (dex % 32));
            dexes[group * POKECONNECT_GROUP_SIZE + i] = dex;
        }
    }

    return CountSolutionsOfTiles(pool, dexes, gen->quads) == 1;
}

// Deterministic for a given seed (the day, for daily puzzles) and answer pool. Groups are
// stored easiest first, where categories with more species in the answer pool are easier.
// Returns FALSE if no puzzle was found within POKECONNECT_MAX_GENERATION_TRIES.
bool32 Pokeconnect_GeneratePuzzle(const struct DexPool *pool, u32 seed, struct PokeconnectSave *game)
{
    struct PokeconnectGen *gen = AllocZeroed(sizeof(struct PokeconnectGen));
    rng_value_t rng = LocalRandomSeed(seed * 2246822519u + 0xC0EC);
    u16 dexes[POKECONNECT_NUM_TILES], indices[POKECONNECT_NUM_GROUPS], sizes[POKECONNECT_NUM_GROUPS];
    u8 rank[POKECONNECT_NUM_GROUPS] = {0, 1, 2, 3};
    u8 kinds[POKECONNECT_NUM_GROUPS];
    bool32 found = FALSE;
    u32 try, i, j;
    u8 temp;

    for (try = 0; try < POKECONNECT_MAX_GENERATION_TRIES && gen != NULL && !found; try++)
    {
        if (try % TRIES_PER_KINDS == 0)
            PickKinds(&rng, kinds);
        found = TryMakePuzzle(gen, pool, &rng, kinds, dexes, indices, sizes);
    }
    TRY_FREE_AND_SET_NULL(gen);
    if (!found)
        return FALSE;

    // Easiest first
    for (i = 1; i < POKECONNECT_NUM_GROUPS; i++)
    {
        for (j = i; j > 0 && sizes[rank[j]] > sizes[rank[j - 1]]; j--)
            SWAP(rank[j], rank[j - 1], temp);
    }

    for (i = 0; i < POKECONNECT_NUM_GROUPS; i++)
    {
        game->categories[i] = Pokeconnect_GetCatalogCategory(indices[rank[i]]);
        for (j = 0; j < POKECONNECT_GROUP_SIZE; j++)
            game->species[i * POKECONNECT_GROUP_SIZE + j] = pool->species[dexes[rank[i] * POKECONNECT_GROUP_SIZE + j]];
    }

    for (i = 0; i < POKECONNECT_NUM_TILES; i++)
        game->order[i] = i;
    for (i = POKECONNECT_NUM_TILES - 1; i > 0; i--)
    {
        j = LocalRandom(&rng) % (i + 1);
        SWAP(game->order[i], game->order[j], temp);
    }
    return TRUE;
}
