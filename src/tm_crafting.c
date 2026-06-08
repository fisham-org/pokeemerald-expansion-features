#include "global.h"
#include "event_data.h"
#include "item.h"
#include "list_menu.h"
#include "malloc.h"
#include "money.h"
#include "move.h"
#include "script.h"
#include "script_menu.h"
#include "string_util.h"
#include "tm_crafting.h"
#include "constants/items.h"
#include "constants/script_menu.h"

#include "data/tm_crafting.h"

// Index into gTMRecipes of the recipe the player highlighted in the menu; set by
// TMCrafting_PrepareSelection and consumed by TMCrafting_TryCraft.
static u32 sSelectedRecipe;

static const u8 sText_SpaceX[]    = _(" x");
static const u8 sText_HaveOpen[]  = _(" (have ");
static const u8 sText_CloseParen[] = _(")");
static const u8 sText_CommaSpace[] = _(", ");

static bool32 RecipeIsUnlocked(const struct TMRecipe *recipe)
{
    if (TM_CRAFTING_UNLOCK_ALL || recipe->unlockFlag == 0)
        return TRUE;
    return FlagGet(recipe->unlockFlag);
}

// Writes "TM06 Toxic" (TM item name + the move it teaches) to dst.
static void BuildTMLabel(u8 *dst, const struct TMRecipe *recipe)
{
    u8 *str = StringCopy(dst, GetItemName(recipe->tm));
    *str++ = CHAR_SPACE;
    StringCopy(str, GetMoveName(GetItemTMHMMoveId(recipe->tm)));
}

// callnative: push every unlocked recipe onto the dynamic multichoice stack and
// report how many were added in VAR_RESULT. The script shows the list with
// `dynmultistack` (each row's id is the TM item id, so its icon is drawn).
void TMCrafting_BuildList(struct ScriptContext *ctx)
{
    u32 i, count = 0;

    if (!TM_CRAFTING)
    {
        gSpecialVar_Result = 0;
        return;
    }

    for (i = 0; i < gTMRecipeCount; i++)
    {
        struct ListMenuItem item;
        u8 *name;

        if (!RecipeIsUnlocked(&gTMRecipes[i]))
            continue;

        name = Alloc(32);
        BuildTMLabel(name, &gTMRecipes[i]);
        item.name = name;
        item.id = gTMRecipes[i].tm;
        MultichoiceDynamic_PushElement(item);
        count++;
    }

    gSpecialVar_Result = count;
}

// callnative: after the menu, VAR_RESULT holds the chosen TM item id (or
// MULTI_B_PRESSED). Resolve it to a recipe, buffer its details for the confirm
// prompt, and set VAR_RESULT to TRUE (valid) or FALSE (cancelled).
void TMCrafting_PrepareSelection(struct ScriptContext *ctx)
{
    u32 i;
    u16 chosenTM = gSpecialVar_Result;

    for (i = 0; i < gTMRecipeCount; i++)
    {
        const struct TMRecipe *recipe = &gTMRecipes[i];
        u8 *str;
        u32 j;

        if (recipe->tm != chosenTM || !RecipeIsUnlocked(recipe))
            continue;

        sSelectedRecipe = i;
        BuildTMLabel(gStringVar1, recipe);                                       // "TM06 Toxic"
        ConvertIntToDecimalStringN(gStringVar2, recipe->cost, STR_CONV_MODE_LEFT_ALIGN, 6); // cost

        str = gStringVar3;                                                       // material list
        for (j = 0; j < recipe->materialCount; j++)
        {
            const struct TMRecipeMaterial *mat = &recipe->materials[j];
            if (j != 0)
                str = StringAppend(str, sText_CommaSpace);
            str = StringCopy(str, GetItemName(mat->item));
            str = StringAppend(str, sText_SpaceX);
            str = ConvertIntToDecimalStringN(str, mat->quantity, STR_CONV_MODE_LEFT_ALIGN, 3);
            str = StringAppend(str, sText_HaveOpen);
            str = ConvertIntToDecimalStringN(str, CountTotalItemQuantityInBag(mat->item), STR_CONV_MODE_LEFT_ALIGN, 3);
            str = StringAppend(str, sText_CloseParen);
        }

        gSpecialVar_Result = TRUE;
        return;
    }

    gSpecialVar_Result = FALSE; // B pressed or stale selection
}

// callnative: validate and perform the craft for the previously selected recipe.
// Sets VAR_RESULT to a TMCraftResult and buffers the TM name in gStringVar1.
void TMCrafting_TryCraft(struct ScriptContext *ctx)
{
    const struct TMRecipe *recipe = &gTMRecipes[sSelectedRecipe];
    u32 i;

    BuildTMLabel(gStringVar1, recipe);

    for (i = 0; i < recipe->materialCount; i++)
    {
        if (!CheckBagHasItem(recipe->materials[i].item, recipe->materials[i].quantity))
        {
            gSpecialVar_Result = TM_CRAFT_NO_MATERIALS;
            return;
        }
    }

    if (!IsEnoughMoney(&gSaveBlock1Ptr->money, recipe->cost))
    {
        gSpecialVar_Result = TM_CRAFT_NO_MONEY;
        return;
    }

    if (!CheckBagHasSpace(recipe->tm, 1))
    {
        gSpecialVar_Result = TM_CRAFT_NO_SPACE;
        return;
    }

    for (i = 0; i < recipe->materialCount; i++)
        RemoveBagItem(recipe->materials[i].item, recipe->materials[i].quantity);
    RemoveMoney(&gSaveBlock1Ptr->money, recipe->cost);
    AddBagItem(recipe->tm, 1);
    gSpecialVar_Result = TM_CRAFT_SUCCESS;
}
