#ifndef GUARD_CONFIG_WILD_ITEM_REWARD_H
#define GUARD_CONFIG_WILD_ITEM_REWARD_H

// Scarlet/Violet-style species-family item rewards.
// After winning a wild battle, the player has a chance to receive an item from
// the defeated Pokémon's family item pool. These are awarded directly to the
// bag and are NOT held items. Pools are defined in src/data/wild_item_reward.h.

#define WILD_ITEM_DROPS                   TRUE // Master toggle. When FALSE the feature is fully disabled and nothing changes.
#define WILD_ITEM_DROP_CHANCE             100    // Base % chance to receive an item after winning a wild battle.
#define WILD_ITEM_DROP_ABILITY_MULTIPLIER 50   // Applied to the base chance as (chance * N) / 100 when the lead Pokémon has a listed ability. 100 = no boost. See WILD_ITEM_DROP_OVERFLOW_DOUBLES for how chances above 100% behave.
#define WILD_ITEM_DROP_INCLUDE_SPECIAL    FALSE // When FALSE, legendary/Battle Pyramid/Battle Pike battles are excluded, mirroring wild held-item behaviour.

// When FALSE, any drop chance above 100% is simply capped at 100% (a guaranteed single reward).
// When TRUE, the portion above 100% becomes a chance to multiply the awarded items. Each full
// 100% over grants a guaranteed extra copy and the remainder is a chance for one more. e.g. at
// 150% the player always receives the rolled drop and has a 50% chance for its quantity to double
// (potion x1 -> potion x2). When extra copies are awarded, a follow-up message is shown.

// When WILD_ITEM_DROP_ABILITY_MULTIPLIER is e.g. 400, this will result in the guaranteed 1 item 
// by default, and 3 guaranteed extra items for every fight.
#define WILD_ITEM_DROP_OVERFLOW_DOUBLES   TRUE

// Lead-Pokémon abilities that trigger WILD_ITEM_DROP_ABILITY_MULTIPLIER. Comma-separated list of ABILITY_* values.
#define WILD_ITEM_DROP_BOOST_ABILITIES    ABILITY_COMPOUND_EYES, ABILITY_SUPER_LUCK

#endif // GUARD_CONFIG_WILD_ITEM_REWARD_H
