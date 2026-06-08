#ifndef GUARD_CONFIG_TM_CRAFTING_H
#define GUARD_CONFIG_TM_CRAFTING_H

// Scarlet/Violet-style TM crafting. At a crafting machine/NPC the player can
// craft a TM if its recipe is unlocked (a flag), they hold the required
// materials, and they can afford the money cost. Recipes are defined in
// src/data/tm_crafting.h; materials are the ITEM_MATERIAL_* category items.

#define TM_CRAFTING            TRUE  // Master toggle. When FALSE the feature compiles out and the script does nothing.
#define TM_CRAFTING_UNLOCK_ALL FALSE // When TRUE, every recipe is craftable regardless of its unlock flag (useful for testing).

#endif // GUARD_CONFIG_TM_CRAFTING_H
