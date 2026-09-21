#ifndef GUARD_POKEDOKU_H
#define GUARD_POKEDOKU_H

#include "dex_minigame.h"

enum PokedokuGroup
{
    POKEDOKU_GROUP_TYPE,
    POKEDOKU_GROUP_GEN,
    POKEDOKU_GROUP_STAGE,
    POKEDOKU_GROUP_CLASS,
    POKEDOKU_GROUP_COLOR,
    POKEDOKU_GROUP_EGG,
    POKEDOKU_GROUP_STATS,
    POKEDOKU_GROUP_BODY,
    POKEDOKU_GROUP_MISC,
    POKEDOKU_GROUP_COUNT
};

// Criteria must stay grouped: all entries of a group are contiguous and in group order.
enum PokedokuCriterionId
{
    CRIT_TYPE_NORMAL,
    CRIT_TYPE_FIGHTING,
    CRIT_TYPE_FLYING,
    CRIT_TYPE_POISON,
    CRIT_TYPE_GROUND,
    CRIT_TYPE_ROCK,
    CRIT_TYPE_BUG,
    CRIT_TYPE_GHOST,
    CRIT_TYPE_STEEL,
    CRIT_TYPE_FIRE,
    CRIT_TYPE_WATER,
    CRIT_TYPE_GRASS,
    CRIT_TYPE_ELECTRIC,
    CRIT_TYPE_PSYCHIC,
    CRIT_TYPE_ICE,
    CRIT_TYPE_DRAGON,
    CRIT_TYPE_DARK,
    CRIT_TYPE_FAIRY,
    CRIT_GEN_1,
    CRIT_GEN_2,
    CRIT_GEN_3,
    CRIT_GEN_4,
    CRIT_GEN_5,
    CRIT_GEN_6,
    CRIT_GEN_7,
    CRIT_GEN_8,
    CRIT_GEN_9,
    CRIT_STAGE_BASIC,
    CRIT_STAGE_MIDDLE,
    CRIT_STAGE_FINAL,
    CRIT_STAGE_SINGLE,
    CRIT_LEGENDARY,
    CRIT_MYTHICAL,
    CRIT_ULTRA_BEAST,
    CRIT_PARADOX,
    CRIT_COLOR_RED,
    CRIT_COLOR_BLUE,
    CRIT_COLOR_YELLOW,
    CRIT_COLOR_GREEN,
    CRIT_COLOR_BLACK,
    CRIT_COLOR_BROWN,
    CRIT_COLOR_PURPLE,
    CRIT_COLOR_GRAY,
    CRIT_COLOR_WHITE,
    CRIT_COLOR_PINK,
    CRIT_EGG_MONSTER,
    CRIT_EGG_WATER_1,
    CRIT_EGG_BUG,
    CRIT_EGG_FLYING,
    CRIT_EGG_FIELD,
    CRIT_EGG_FAIRY,
    CRIT_EGG_GRASS,
    CRIT_EGG_HUMAN_LIKE,
    CRIT_EGG_WATER_3,
    CRIT_EGG_MINERAL,
    CRIT_EGG_AMORPHOUS,
    CRIT_EGG_WATER_2,
    CRIT_EGG_DRAGON,
    CRIT_EGG_NO_EGGS,
    CRIT_HP_100,
    CRIT_ATK_100,
    CRIT_DEF_100,
    CRIT_SPEED_100,
    CRIT_SPATK_100,
    CRIT_SPDEF_100,
    CRIT_BST_500,
    CRIT_BST_UNDER_300,
    CRIT_WEIGHT_100KG,
    CRIT_WEIGHT_UNDER_5KG,
    CRIT_HEIGHT_2M,
    CRIT_HEIGHT_UNDER_50CM,
    CRIT_MONOTYPE,
    CRIT_GENDERLESS,
    POKEDOKU_CRITERIA_COUNT
};

struct PokedokuCriterion
{
    u8 group;
    u8 kind;
    u16 param;
    const u8 *name;        // Header label, up to 2 short lines
    const u8 *description; // Shown in the info bar, 1 line
};

extern const struct PokedokuCriterion gPokedokuCriteria[POKEDOKU_CRITERIA_COUNT];
extern const u8 gPokedokuFallbackBoard[POKEDOKU_GRID_SIZE * 2];

bool32 Pokedoku_MatchesCriterion(const struct DexPool *pool, u32 dex, u32 criterion);
bool32 Pokedoku_IsValidGuess(const struct DexPool *pool, const u8 *criteria, u32 cell, u32 dex);
bool32 Pokedoku_IsBoardSolvable(const struct DexPool *pool, const u8 *criteria, u32 minAnswers);
bool32 Pokedoku_GenerateBoard(const struct DexPool *pool, u32 seed, u8 *criteria);
u32 Pokedoku_GetCellPopularityTotal(const struct DexPool *pool, const u8 *criteria, u32 cell);
u32 Pokedoku_GetPopularityPercent(const struct DexPool *pool, u32 dex, u32 cellTotal);
u32 Pokedoku_GetRarityScore(const struct DexPool *pool, const struct PokedokuSave *board, const u32 *cellTotals);

void GetPokedokuStatus(void);
void PreparePokedoku(void);
void OpenPokedoku(void);
void ClaimPokedokuReward(void);

#endif // GUARD_POKEDOKU_H
