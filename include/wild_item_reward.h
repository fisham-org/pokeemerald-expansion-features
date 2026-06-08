#ifndef GUARD_WILD_ITEM_REWARD_H
#define GUARD_WILD_ITEM_REWARD_H

// One entry in a family's item pool. Every entry in a pool has an equal chance
// of being selected.
struct FamilyItemDrop
{
    enum Item item;
    u16 quantity;
};

// Associates a family (keyed by its base, non-evolved species) with an item pool.
struct FamilyDropPool
{
    enum Species baseSpecies;
    const struct FamilyItemDrop *drops;
    u8 count;
};

extern const struct FamilyDropPool gFamilyDropPools[];
extern const u32 gFamilyDropPoolCount;

// Battle-script native entry point, called at the end of a won wild battle.
void BS_TryGiveWildItemReward(void);

#endif // GUARD_WILD_ITEM_REWARD_H
