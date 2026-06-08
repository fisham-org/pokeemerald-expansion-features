#include "global.h"
#include "test/battle.h"

// These tests rely on the example Lechonk family pool defined under #if TESTING
// in src/data/wild_item_reward.h, which awards ITEM_RARE_CANDY. The feature is
// force-enabled for test builds via include/config/test.h (WILD_ITEM_DROPS).
//
// The reward message is printed at the end of a won wild battle, after EXP, by
// BattleScript_PayDayMoneyAndPickUpItems. The drop-chance gate uses
// RNG_WILD_ITEM_REWARD; the pool entry / double-battle target use
// RNG_WILD_ITEM_REWARD_SELECT.

WILD_BATTLE_TEST("Wild item reward: grants a family-pool item after a won wild battle")
{
    PASSES_RANDOMLY(WILD_ITEM_DROP_CHANCE, 100, RNG_WILD_ITEM_REWARD);
    GIVEN {
        PLAYER(SPECIES_ZIGZAGOON) { Moves(MOVE_TACKLE); }
        OPPONENT(SPECIES_LECHONK) { HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_TACKLE); }
    } SCENE {
        MESSAGE("You obtained Rare Candy!");
    }
}

WILD_BATTLE_TEST("Wild item reward: resolves the family pool from an evolved form")
{
    PASSES_RANDOMLY(WILD_ITEM_DROP_CHANCE, 100, RNG_WILD_ITEM_REWARD);
    GIVEN {
        PLAYER(SPECIES_ZIGZAGOON) { Moves(MOVE_TACKLE); }
        OPPONENT(SPECIES_OINKOLOGNE_M) { HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_TACKLE); }
    } SCENE {
        MESSAGE("You obtained Rare Candy!");
    }
}

WILD_BATTLE_TEST("Wild item reward: a chance-boosting lead ability raises the drop rate")
{
    // Base chance is 100%, so the ability multiplier (150%) keeps the drop
    // guaranteed; the reward gate caps at 100%.
    GIVEN {
        PLAYER(SPECIES_VENONAT) { Ability(ABILITY_COMPOUND_EYES); Moves(MOVE_TACKLE); }
        OPPONENT(SPECIES_LECHONK) { HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_TACKLE); }
    } SCENE {
        MESSAGE("You obtained Rare Candy!");
    }
}

WILD_BATTLE_TEST("Wild item reward: overflow chance can double the awarded items")
{
    // Compound Eyes pushes the chance to 150%: the drop is guaranteed (gate caps
    // at 100%) and the 50% overflow rolls RNG_WILD_ITEM_REWARD_DOUBLE for a copy.
    PASSES_RANDOMLY(WILD_ITEM_DROP_ABILITY_MULTIPLIER - 100, 100, RNG_WILD_ITEM_REWARD_DOUBLE);
    GIVEN {
        PLAYER(SPECIES_VENONAT) { Ability(ABILITY_COMPOUND_EYES); Moves(MOVE_TACKLE); }
        OPPONENT(SPECIES_LECHONK) { HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_TACKLE); }
    } SCENE {
        MESSAGE("You obtained Rare Candy!");
        MESSAGE("Venonat found an extra Rare Candy (x1)!");
    }
}

WILD_BATTLE_TEST("Wild item reward: no reward when the defeated family has no pool")
{
    GIVEN {
        PLAYER(SPECIES_ZIGZAGOON) { Moves(MOVE_TACKLE); }
        OPPONENT(SPECIES_WOBBUFFET) { HP(1); }
    } WHEN {
        TURN { MOVE(player, MOVE_TACKLE); }
    } SCENE {
        NONE_OF { MESSAGE("You obtained Rare Candy!"); }
    }
}
