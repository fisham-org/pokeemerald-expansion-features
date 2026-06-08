// Starter TM crafting recipes. See include/config/tm_crafting.h for toggles and
// notes/feature-tm-crafting/ for documentation. Materials are the consolidated
// ITEM_MATERIAL_* category items.
//
// NOTE: TMs here use this codebase's Gen-3 numbering (TM05 = Roar, TM06 = Toxic,
// ...), not Scarlet/Violet numbering. Costs are in money (Poké$). A recipe with
// unlockFlag == 0 is always craftable; otherwise it requires that flag to be set.
// Extend freely: add a materials array and a gTMRecipes[] entry.

static const struct TMRecipeMaterial sRecipeRoar[]        = { { ITEM_MATERIAL_FEATHER, 3 } };
static const struct TMRecipeMaterial sRecipeToxic[]       = { { ITEM_MATERIAL_TOXIN, 3 } };
static const struct TMRecipeMaterial sRecipeIceBeam[]     = { { ITEM_MATERIAL_SCALES, 3 }, { ITEM_MATERIAL_GOO, 1 } };
static const struct TMRecipeMaterial sRecipeThunderbolt[] = { { ITEM_MATERIAL_SPARK, 3 }, { ITEM_MATERIAL_SCALES, 2 } };
static const struct TMRecipeMaterial sRecipeEarthquake[]  = { { ITEM_MATERIAL_MINERAL, 4 }, { ITEM_MATERIAL_FANG, 1 } };
static const struct TMRecipeMaterial sRecipeDoubleTeam[]  = { { ITEM_MATERIAL_FUR, 2 }, { ITEM_MATERIAL_FEATHER, 2 } };
static const struct TMRecipeMaterial sRecipeAerialAce[]   = { { ITEM_MATERIAL_FEATHER, 3 }, { ITEM_MATERIAL_FANG, 3 } };
static const struct TMRecipeMaterial sRecipeRest[]        = { { ITEM_MATERIAL_FUR, 3 } };

const struct TMRecipe gTMRecipes[] =
{
    { ITEM_TM05, 200,  0,                sRecipeRoar,        ARRAY_COUNT(sRecipeRoar) },        // Roar (always available)
    { ITEM_TM06, 300,  FLAG_TM_RECIPE_1, sRecipeToxic,       ARRAY_COUNT(sRecipeToxic) },       // Toxic
    { ITEM_TM13, 1000, FLAG_TM_RECIPE_2, sRecipeIceBeam,     ARRAY_COUNT(sRecipeIceBeam) },     // Ice Beam
    { ITEM_TM24, 1000, FLAG_TM_RECIPE_3, sRecipeThunderbolt, ARRAY_COUNT(sRecipeThunderbolt) }, // Thunderbolt
    { ITEM_TM26, 1000, FLAG_TM_RECIPE_4, sRecipeEarthquake,  ARRAY_COUNT(sRecipeEarthquake) },  // Earthquake
    { ITEM_TM32, 400,  FLAG_TM_RECIPE_5, sRecipeDoubleTeam,  ARRAY_COUNT(sRecipeDoubleTeam) },  // Double Team
    { ITEM_TM40, 800,  FLAG_TM_RECIPE_6, sRecipeAerialAce,   ARRAY_COUNT(sRecipeAerialAce) },   // Aerial Ace
    { ITEM_TM44, 300,  FLAG_TM_RECIPE_7, sRecipeRest,        ARRAY_COUNT(sRecipeRest) },         // Rest
};

const u32 gTMRecipeCount = ARRAY_COUNT(gTMRecipes);
