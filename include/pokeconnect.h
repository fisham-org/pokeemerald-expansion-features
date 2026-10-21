#ifndef GUARD_POKECONNECT_H
#define GUARD_POKECONNECT_H

#include "dex_minigame.h"

// Each category is a kind plus a param, e.g. PCON_KIND_TYPE + TYPE_FIRE
enum PokeconnectKind
{
    PCON_KIND_TYPE,
    PCON_KIND_COLOR,
    PCON_KIND_GEN,
    PCON_KIND_EGG_GROUP,
    PCON_KIND_STAGE,       // param: enum DexStage
    PCON_KIND_CLASS,       // param: enum PokeconnectClass
    PCON_KIND_ABILITY,     // Either of its non-hidden abilities
    PCON_KIND_EVO_METHOD,  // param: enum PokeconnectEvoMethod
    PCON_KIND_GENDER,      // param: MON_GENDERLESS, MON_FEMALE or MON_MALE
    PCON_KIND_FIRST_LETTER, // param: 0-25 for A-Z
    PCON_KIND_SHORT_NAME,  // param: max name length
    PCON_KIND_SET,         // param: enum PokeconnectSet, a hand-written species list
    PCON_KIND_DUAL_TYPE,   // param: type1 | (type2 << 8), exactly these two types. Last, as it isn't cached.
    PCON_KIND_COUNT
};

enum PokeconnectClass
{
    PCON_CLASS_LEGENDARY,
    PCON_CLASS_MYTHICAL,
    PCON_CLASS_ULTRA_BEAST,
    PCON_CLASS_PARADOX,
    PCON_CLASS_COUNT
};

enum PokeconnectEvoMethod
{
    PCON_EVO_STONE,
    PCON_EVO_TRADE,
    PCON_EVO_FRIENDSHIP,
    PCON_EVO_COUNT
};

// Hand-written sets, listed in src/pokeconnect_categories.c
enum PokeconnectSet
{
    PCON_SET_NUMBER_WORDS,
    PCON_SET_PIKACHU_CLONES,
    PCON_SET_FIRST_PARTNERS,
    PCON_SET_EARLY_BIRDS,
    PCON_SET_EARLY_MAMMALS,
    PCON_SET_FOSSILS,
    PCON_SET_COVER_LEGENDARIES,
    PCON_SET_FOOD,
    PCON_SET_COUNT
};

// Result of a guess of 4 tiles
#define PCON_GUESS_CORRECT    0
#define PCON_GUESS_ONE_AWAY   1 // 3 of the 4 tiles share a group
#define PCON_GUESS_WRONG      2

u32 Pokeconnect_GetCatalogSize(void);
struct PokeconnectCategory Pokeconnect_GetCatalogCategory(u32 index);
bool32 Pokeconnect_Matches(const struct DexPool *pool, u32 dex, struct PokeconnectCategory category);
void Pokeconnect_GetCategoryName(struct PokeconnectCategory category, u8 *dest);
bool32 Pokeconnect_GeneratePuzzle(const struct DexPool *pool, u32 seed, struct PokeconnectSave *game);
u32 Pokeconnect_CountSolutions(const struct DexPool *pool, const struct PokeconnectSave *game);
u32 Pokeconnect_CheckGuess(u32 tileMask);

void GetPokeconnectStatus(void);
void PreparePokeconnect(void);
void OpenPokeconnect(void);
void ClaimPokeconnectReward(void);

#endif // GUARD_POKECONNECT_H
