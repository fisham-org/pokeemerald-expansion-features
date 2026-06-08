#include "global.h"
#include "battle.h"
#include "battle_message.h"
#include "battle_util.h"
#include "item.h"
#include "pokemon.h"
#include "random.h"
#include "wild_item_reward.h"
#include "constants/abilities.h"
#include "constants/battle.h"
#include "constants/items.h"
#include "constants/species.h"

#include "data/wild_item_reward.h"

extern const u8 BattleScript_WildItemReward[];
extern const u8 BattleScript_WildItemRewardDoubled[];

enum WildItemRewardResult
{
    WILD_ITEM_REWARD_NONE,
    WILD_ITEM_REWARD_SINGLE,
    WILD_ITEM_REWARD_DOUBLED,
};

// Walks pre-evolutions back to the family's base (non-evolved) species, which is
// the key used to look up the family's item pool.
static enum Species GetFamilyBaseSpecies(enum Species species)
{
    enum Species prevo;
    while ((prevo = GetSpeciesPreEvolution(species)) != SPECIES_NONE)
        species = prevo;
    return species;
}

static const struct FamilyDropPool *FindFamilyPool(enum Species baseSpecies)
{
    u32 i;
    for (i = 0; i < gFamilyDropPoolCount; i++)
    {
        if (gFamilyDropPools[i].baseSpecies == baseSpecies)
            return &gFamilyDropPools[i];
    }
    return NULL;
}

static bool32 LeadMonHasBoostAbility(void)
{
    u32 i;
    enum Ability ability;

    if (GetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_SANITY_IS_EGG))
        return FALSE;

    ability = GetMonAbility(&gParties[B_TRAINER_PLAYER][0]);
    for (i = 0; i < ARRAY_COUNT(sWildItemBoostAbilities); i++)
    {
        if (ability == sWildItemBoostAbilities[i])
            return TRUE;
    }
    return FALSE;
}

// Picks the defeated wild Pokémon whose family pool the reward is drawn from. In
// a wild double battle one of the two is chosen at random.
static enum Species GetRewardSpecies(void)
{
    u32 slot = 0;
    if (WILD_DOUBLE_BATTLE)
        slot = RandomUniform(RNG_WILD_ITEM_REWARD_SELECT, 0, 1);
    return GetMonData(&gParties[B_TRAINER_OPPONENT_A][slot], MON_DATA_SPECIES);
}

// When WILD_ITEM_DROP_OVERFLOW_DOUBLES is enabled and the drop chance exceeds
// 100%, the surplus is rolled into extra copies of the awarded drop: each full
// 100% over grants a guaranteed extra copy and the remainder is a chance for one
// more. Returns the number of extra copies (0 = no doubling).
static u32 RollExtraCopies(u32 chance)
{
    u32 overflow, extraCopies;

    if (!WILD_ITEM_DROP_OVERFLOW_DOUBLES || chance <= 100)
        return 0;

    overflow = chance - 100;
    extraCopies = overflow / 100;
    overflow %= 100;
    if (overflow != 0 && RandomPercentage(RNG_WILD_ITEM_REWARD_DOUBLE, overflow))
        extraCopies++;
    return extraCopies;
}

// Returns whether an item was awarded (and, if so, whether extra copies were
// granted) so the caller can pick the matching message.
static enum WildItemRewardResult TryGiveWildItemReward(void)
{
    enum Species baseSpecies;
    const struct FamilyDropPool *pool;
    const struct FamilyItemDrop *drop;
    u32 chance, baseChance, extraCopies, extraQuantity;

    if (!WILD_ITEM_DROPS)
        return WILD_ITEM_REWARD_NONE;
    if (gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED_LINK))
        return WILD_ITEM_REWARD_NONE;
    if (!WILD_ITEM_DROP_INCLUDE_SPECIAL
        && (gBattleTypeFlags & (BATTLE_TYPE_LEGENDARY | BATTLE_TYPE_PYRAMID | BATTLE_TYPE_PIKE)))
        return WILD_ITEM_REWARD_NONE;

    baseSpecies = GetFamilyBaseSpecies(GetRewardSpecies());
    pool = FindFamilyPool(baseSpecies);
    if (pool == NULL || pool->count == 0)
        return WILD_ITEM_REWARD_NONE;

    chance = WILD_ITEM_DROP_CHANCE;
    if (LeadMonHasBoostAbility())
        chance = (chance * WILD_ITEM_DROP_ABILITY_MULTIPLIER) / 100;

    // The gate for receiving any item caps at 100%; anything above only matters
    // when overflow-doubling is enabled (handled in RollExtraCopies).
    baseChance = (chance > 100) ? 100 : chance;
    if (!RandomPercentage(RNG_WILD_ITEM_REWARD, baseChance))
        return WILD_ITEM_REWARD_NONE;

    drop = &pool->drops[RandomUniform(RNG_WILD_ITEM_REWARD_SELECT, 0, pool->count - 1)];
    if (!CheckBagHasSpace(drop->item, drop->quantity))
        return WILD_ITEM_REWARD_NONE;

    AddBagItem(drop->item, drop->quantity);
    PREPARE_ITEM_BUFFER(gBattleTextBuff1, drop->item);

    extraCopies = RollExtraCopies(chance);
    extraQuantity = extraCopies * drop->quantity;
    if (extraQuantity == 0 || !CheckBagHasSpace(drop->item, extraQuantity))
        return WILD_ITEM_REWARD_SINGLE;

    AddBagItem(drop->item, extraQuantity);
    PREPARE_MON_NICK_WITH_PREFIX_BUFFER(gBattleTextBuff2, GetBattlerAtPosition(B_POSITION_PLAYER_LEFT), 0);
    PREPARE_HWORD_NUMBER_BUFFER(gBattleTextBuff3, 3, extraQuantity);
    return WILD_ITEM_REWARD_DOUBLED;
}

// Battle-script native command, invoked at the end of a won wild battle (after
// all EXP messages). Replicates the packed layout of a `callnative` instruction
// so the feature stays self-contained in this file.
void BS_TryGiveWildItemReward(void)
{
    const struct __attribute__((packed)) {
        u8 opcode;
        void (*func)(void);
        const u8 nextInstr[0];
    } *cmd = (const void *)gBattlescriptCurrInstr;

    switch (TryGiveWildItemReward())
    {
    case WILD_ITEM_REWARD_DOUBLED:
        BattleScriptPush(cmd->nextInstr);
        gBattlescriptCurrInstr = BattleScript_WildItemRewardDoubled;
        break;
    case WILD_ITEM_REWARD_SINGLE:
        BattleScriptPush(cmd->nextInstr);
        gBattlescriptCurrInstr = BattleScript_WildItemReward;
        break;
    case WILD_ITEM_REWARD_NONE:
    default:
        gBattlescriptCurrInstr = cmd->nextInstr;
        break;
    }
}
