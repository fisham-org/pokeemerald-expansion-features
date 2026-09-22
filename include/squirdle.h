#ifndef GUARD_SQUIRDLE_H
#define GUARD_SQUIRDLE_H

#include "dex_minigame.h"

enum SquirdleAttribute
{
    SQUIRDLE_ATTR_GEN,
    SQUIRDLE_ATTR_TYPE_1,
    SQUIRDLE_ATTR_TYPE_2,
    SQUIRDLE_ATTR_HEIGHT,
    SQUIRDLE_ATTR_WEIGHT,
    SQUIRDLE_ATTR_BST,
    SQUIRDLE_ATTR_COLOR,
    SQUIRDLE_ATTR_COUNT
};

// How many of the attributes in SQUIRDLE_COLUMNS are shown
#define SQUIRDLE_NUM_COLUMNS 5

enum SquirdleClue
{
    SQUIRDLE_CLUE_CORRECT,
    SQUIRDLE_CLUE_WRONG,
    SQUIRDLE_CLUE_OTHER_SLOT, // Types only: the target has this type in its other slot
    SQUIRDLE_CLUE_HIGHER,     // The target's value is higher than the guess's
    SQUIRDLE_CLUE_LOWER,
};

void Squirdle_Compare(const struct DexPool *pool, u32 guessDex, u32 targetDex, u8 *clues);
bool32 Squirdle_IsSolved(const u8 *clues);
u32 Squirdle_GetDailyTarget(const struct DexPool *pool, u32 day);

void GetSquirdleStatus(void);
void PrepareSquirdle(void);
void OpenSquirdle(void);
void ClaimSquirdleReward(void);

#endif // GUARD_SQUIRDLE_H
