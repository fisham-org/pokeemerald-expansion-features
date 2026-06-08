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

// Status codes written to VAR_RESULT by TMCrafting_TryCraft, matched in the
// crafting script (data/scripts/tm_crafting.pory).
enum TMCraftResult
{
    TM_CRAFT_SUCCESS,
    TM_CRAFT_NO_MATERIALS,
    TM_CRAFT_NO_MONEY,
    TM_CRAFT_NO_SPACE,
};

// Script callnative entry points (src/tm_crafting.c).
void TMCrafting_BuildList(struct ScriptContext *ctx);
void TMCrafting_PrepareSelection(struct ScriptContext *ctx);
void TMCrafting_TryCraft(struct ScriptContext *ctx);

#endif // GUARD_TM_CRAFTING_H
