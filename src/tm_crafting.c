#include "global.h"
#include "event_data.h"
#include "item.h"
#include "money.h"
#include "tm_crafting.h"
#include "constants/items.h"

#include "data/tm_crafting.h"

// Shared crafting rules, used by the menu (src/tm_crafting_menu.c).

bool32 TMCrafting_RecipeIsUnlocked(const struct TMRecipe *recipe)
{
    if (TM_CRAFTING_UNLOCK_ALL || recipe->unlockFlag == 0)
        return TRUE;
    return FlagGet(recipe->unlockFlag);
}

// Validates a craft without changing anything; returns why it can't be made.
enum TMCraftResult TMCrafting_CheckCraft(const struct TMRecipe *recipe)
{
    u32 i;

    for (i = 0; i < recipe->materialCount; i++)
    {
        if (!CheckBagHasItem(recipe->materials[i].item, recipe->materials[i].quantity))
            return TM_CRAFT_NO_MATERIALS;
    }
    if (!IsEnoughMoney(&gSaveBlock1Ptr->money, recipe->cost))
        return TM_CRAFT_NO_MONEY;
    if (!CheckBagHasSpace(recipe->tm, 1))
        return TM_CRAFT_NO_SPACE;
    return TM_CRAFT_SUCCESS;
}

// Performs the craft: consumes the materials and money, adds the TM. Caller must
// have confirmed TMCrafting_CheckCraft() == TM_CRAFT_SUCCESS first.
void TMCrafting_Craft(const struct TMRecipe *recipe)
{
    u32 i;

    for (i = 0; i < recipe->materialCount; i++)
        RemoveBagItem(recipe->materials[i].item, recipe->materials[i].quantity);
    RemoveMoney(&gSaveBlock1Ptr->money, recipe->cost);
    AddBagItem(recipe->tm, 1);
}
