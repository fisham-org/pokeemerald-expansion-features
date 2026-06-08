// Data for the wild item reward feature. See include/config/wild_item_reward.h
// for the toggles and notes/feature-wild-item-reward/ for documentation.

// Lead-Pokémon abilities that boost the drop chance, derived from the config list.
static const u16 sWildItemBoostAbilities[] = { WILD_ITEM_DROP_BOOST_ABILITIES };

#if TESTING
// Example family used only by test/battle/wild_item_reward.c. A distinctive
// item keeps the expected reward message deterministic.
static const struct FamilyItemDrop sLechonkTestPool[] =
{
    { ITEM_RARE_CANDY, 1 },
};
#endif

// Item pools keyed by the family's BASE (non-evolved) species. Any species in
// the family (e.g. an evolved form) resolves back to this base species.
//
// Ships empty for production builds. To add a family, define its pool and add
// an entry here, for example:
//
//   static const struct FamilyItemDrop sLechonkPool[] =
//   {
//       { ITEM_LECHONK_HAIR, 1 },
//       { ITEM_LECHONK_HAIR, 3 },
//       { ITEM_POTION,       1 },
//   };
//   ...
//   { SPECIES_LECHONK, sLechonkPool, ARRAY_COUNT(sLechonkPool) },

// Drops are the consolidated TM-crafting category materials (see
// notes/feature-wild-item-reward/sv-material-categories.md): Poochyena -> Fang,
// Wurmple -> Silk.
static const struct FamilyItemDrop sPoochyenaPool[] =
{
    { ITEM_MATERIAL_FANG, 1 },
};

static const struct FamilyItemDrop sWurmplePool[] =
{
    { ITEM_MATERIAL_SILK, 1 },
};

const struct FamilyDropPool gFamilyDropPools[] =
{
#if TESTING
    { SPECIES_LECHONK, sLechonkTestPool, ARRAY_COUNT(sLechonkTestPool) },
#endif
    { SPECIES_POOCHYENA, sPoochyenaPool, ARRAY_COUNT(sPoochyenaPool) },
    { SPECIES_WURMPLE,   sWurmplePool,   ARRAY_COUNT(sWurmplePool) },
};

const u32 gFamilyDropPoolCount = ARRAY_COUNT(gFamilyDropPools);
