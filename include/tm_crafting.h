#ifndef GUARD_TM_CRAFTING_H
#define GUARD_TM_CRAFTING_H

struct ScriptContext;

// One material requirement within a recipe. Every recipe lists 1-4 of these.
struct TMRecipeMaterial
{
    enum Item item;
    u8 quantity;
};

// A craftable TM: the resulting TM item, its money cost, the flag that unlocks
// it (0 == always available), and its material requirements.
struct TMRecipe
{
    enum Item tm;
    u16 cost;
    u16 unlockFlag;
    const struct TMRecipeMaterial *materials;
    u8 materialCount;
};

extern const struct TMRecipe gTMRecipes[];
extern const u32 gTMRecipeCount;

// Result of TMCrafting_CheckCraft: whether a recipe can be crafted, or why not.
enum TMCraftResult
{
    TM_CRAFT_SUCCESS,
    TM_CRAFT_NO_MATERIALS,
    TM_CRAFT_NO_MONEY,
    TM_CRAFT_NO_SPACE,
};

// Shared rules used by both the menu and any script path (src/tm_crafting.c).
bool32 TMCrafting_RecipeIsUnlocked(const struct TMRecipe *recipe);
enum TMCraftResult TMCrafting_CheckCraft(const struct TMRecipe *recipe); // no side effects
void TMCrafting_Craft(const struct TMRecipe *recipe);                    // consumes materials/money, gives the TM

// Opens the full-screen crafting menu (src/tm_crafting_menu.c). Script callnative.
void TMCrafting_OpenMenu(struct ScriptContext *ctx);

#endif // GUARD_TM_CRAFTING_H
