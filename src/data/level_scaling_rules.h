// ============================================================================
// PER-TRAINER LEVEL SCALING CONFIGURATION
// ============================================================================
//
// This file allows you to override the default scaling behavior for specific trainers.
//
// HOW IT WORKS:
// - Default behavior is defined in include/config/level_scaling.h
// - If B_TRAINER_SCALING_DEFAULT_MODE is set to anything other than LEVEL_SCALING_NONE,
//   ALL trainers will use that default configuration
// - Trainers listed here can override the default with custom rules
// - Every entry must set .mode (an entry without it is treated as absent)
// - Use LEVEL_SCALING_CONFIG_NONE to explicitly disable scaling for a specific trainer
//
// EXAMPLES:
//
// 1. Override with custom config:
//    [TRAINER_ROXANNE_1] = {
//        .mode = LEVEL_SCALING_TO_LEVEL_CAP,
//        .levelAugmentAdd = 2,         // +2 levels above cap
//        .levelVariationPct = 10,         // Up to 10% level reduction
//        .minLevel = 10,
//        .maxLevel = 50,
//        .manageEvolutions = TRUE,
//        .excludeFainted = FALSE,
//    },
//
// 2. Explicitly disable scaling (opt-out):
//    [TRAINER_WALLY_VR_1] = LEVEL_SCALING_CONFIG_NONE,
//
// 3. Opt-in to EV scaling and/or moveset filtering for a specific trainer:
//    [TRAINER_STEVEN] = {
//        .mode = LEVEL_SCALING_PARTY_AVG,
//        .scaleEVs = TRUE,        // Scale defined EVs proportionally to scaled level
//        .scaleMoves = TRUE,      // Drop trainer-defined moves illegal for the scaled species/level
//                                 // (filled from the scaled species' level-up learnset)
//        .manageEvolutions = TRUE,
//    },

#include "constants/opponents.h"

const struct LevelScalingConfig gTrainerLevelScalingRules[TRAINERS_COUNT] =
{
    // Trainers not listed here will use the default from config/level_scaling.h

    // ============================================================================
    // TEST TRAINERS - For debugging and testing level scaling
    // ============================================================================

    // Test 1: Scale to level cap
    [TRAINER_ROXANNE_1] = {
        .mode = LEVEL_SCALING_TO_LEVEL_CAP,
        .levelAugmentAdd = 0,
        .levelVariationPct = 0,
        .manageEvolutions = TRUE,
    },

    // Test 2: Scale to party average + 3
    [TRAINER_BRAWLY_1] = {
        .mode = LEVEL_SCALING_PARTY_AVG,
        .levelAugmentAdd = 3,
        .levelVariationPct = 0,
        .manageEvolutions = TRUE,
        .excludeFainted = TRUE,
    },

    // Test 3: Scale to party highest
    [TRAINER_WATTSON_1] = {
        .mode = LEVEL_SCALING_PARTY_HIGHEST,
        .levelAugmentAdd = 0,
        .levelVariationPct = 10,
        .manageEvolutions = TRUE,
    },

    // Test 4: Scale to party lowest + 5 (also exercises EV + moveset scaling)
    // With the debug Script 1 setup, party lowest = 10 → scaled level = 15.
    // - manageEvolutions devolves any fully-evolved mons
    // - scaleMoves drops any trainer-defined move not in the devolved species'
    //   level-up learnset at <= 15 AND not MOVE_TIER_DEFAULT, refilling from
    //   the species' learnset at <= 15
    // - scaleEVs caps total EVs at 15 * 10 = 150 (proportional to defined EVs)
    [TRAINER_FLANNERY_1] = {
        .mode = LEVEL_SCALING_PARTY_LOWEST,
        .levelAugmentAdd = 5,
        .levelVariationPct = 0,
        .manageEvolutions = TRUE,
        .scaleEVs = TRUE,
        .scaleMoves = TRUE,
    },

    // Test 5: Scale to party highest + augment with variation
    [TRAINER_NORMAN_1] = {
        .mode = LEVEL_SCALING_PARTY_HIGHEST,
        .levelAugmentAdd = 2,
        .levelVariationPct = 10,
        .minLevel = 25,
        .maxLevel = 50,
        .manageEvolutions = TRUE,
    },

    // Test 6: Evolution management test (should devolve illegal mons)
    [TRAINER_WINONA_1] = {
        .mode = LEVEL_SCALING_TO_LEVEL_CAP,
        .levelAugmentAdd = -20,  // Reduce levels significantly
        .levelVariationPct = 0,
        .manageEvolutions = TRUE,
    },
};
