// ============================================================================
// HELD ITEM PROGRESSION TIERS
// ============================================================================
//
// Tier classification for held items. Used by the level-scaling system to
// decide whether a trainer-defined held item may be carried by a scaled-down
// Pokémon at a given level. Mirrors src/data/move_progression_tiers.h.
//
// Tiers:
//   ITEM_TIER_DEFAULT  - No gate (always allowed). Items omitted below default to this.
//   ITEM_TIER_MID      - Available at >= B_ITEM_TIER_MID_MIN_LEVEL
//   ITEM_TIER_LATE     - Available at >= B_ITEM_TIER_LATE_MIN_LEVEL
//   ITEM_TIER_ENDGAME  - Available at >= B_ITEM_TIER_ENDGAME_MIN_LEVEL
//
// When .scaleItems is opted in and the held item's tier is above what the
// scaled level allows, the item is stripped (set to ITEM_NONE). Anything not
// listed here is ITEM_TIER_DEFAULT and is never stripped — basic berries,
// type-boost held items, and other low-impact items intentionally fall here.

#include "constants/items.h"

const u8 gItemProgressionTier[ITEMS_COUNT] =
{
    // --- Healing / pinch berries: low impact, available early ---
    [ITEM_ORAN_BERRY]        = ITEM_TIER_DEFAULT,
    [ITEM_SITRUS_BERRY]      = ITEM_TIER_MID,
    [ITEM_LUM_BERRY]         = ITEM_TIER_MID,

    // --- Defensive / utility staples: mid-game ---
    [ITEM_LEFTOVERS]         = ITEM_TIER_LATE,
    [ITEM_SHELL_BELL]        = ITEM_TIER_MID,
    [ITEM_BRIGHT_POWDER]     = ITEM_TIER_MID,
    [ITEM_FOCUS_BAND]        = ITEM_TIER_MID,
    [ITEM_QUICK_CLAW]        = ITEM_TIER_MID,
    [ITEM_KINGS_ROCK]        = ITEM_TIER_MID,
    [ITEM_SCOPE_LENS]        = ITEM_TIER_MID,
    [ITEM_MENTAL_HERB]       = ITEM_TIER_MID,
    [ITEM_WHITE_HERB]        = ITEM_TIER_LATE,
    [ITEM_METRONOME]         = ITEM_TIER_MID,

    // --- Strong competitive items: late-game ---
    [ITEM_BLACK_SLUDGE]      = ITEM_TIER_LATE,
    [ITEM_ROCKY_HELMET]      = ITEM_TIER_LATE,
    [ITEM_AIR_BALLOON]       = ITEM_TIER_LATE,
    [ITEM_RED_CARD]          = ITEM_TIER_LATE,
    [ITEM_MUSCLE_BAND]       = ITEM_TIER_LATE,
    [ITEM_WISE_GLASSES]      = ITEM_TIER_LATE,
    [ITEM_EXPERT_BELT]       = ITEM_TIER_LATE,
    [ITEM_POWER_HERB]        = ITEM_TIER_LATE,
    [ITEM_LAGGING_TAIL]      = ITEM_TIER_LATE,
    [ITEM_HEAVY_DUTY_BOOTS]  = ITEM_TIER_LATE,
    [ITEM_FOCUS_SASH]        = ITEM_TIER_LATE,

    // --- Game-defining power items: endgame only ---
    [ITEM_CHOICE_BAND]       = ITEM_TIER_ENDGAME,
    [ITEM_CHOICE_SPECS]      = ITEM_TIER_ENDGAME,
    [ITEM_CHOICE_SCARF]      = ITEM_TIER_ENDGAME,
    [ITEM_LIFE_ORB]          = ITEM_TIER_ENDGAME,
    [ITEM_ASSAULT_VEST]      = ITEM_TIER_ENDGAME,
    [ITEM_WEAKNESS_POLICY]   = ITEM_TIER_ENDGAME,
    [ITEM_EVIOLITE]          = ITEM_TIER_ENDGAME,
};
