#ifndef GUARD_SCRIPTING_UTIL_H
#define GUARD_SCRIPTING_UTIL_H

// Move property types for Special_CountPartyMonsWithMoveProperty
// These correspond to the move flags defined in include/move.h
#define MOVE_PROPERTY_SOUND     0  // Sound-based moves (blocked by Soundproof)
#define MOVE_PROPERTY_DANCE     1  // Dance moves (copied by Dancer ability)
#define MOVE_PROPERTY_PUNCHING  2  // Punching moves (boosted by Iron Fist)
#define MOVE_PROPERTY_BITING    3  // Biting moves (boosted by Strong Jaw)
#define MOVE_PROPERTY_PULSE     4  // Pulse moves (boosted by Mega Launcher)
#define MOVE_PROPERTY_BALLISTIC 5  // Ballistic moves (blocked by Bulletproof)
#define MOVE_PROPERTY_POWDER    6  // Powder moves (don't affect Grass types)
#define MOVE_PROPERTY_WIND      7  // Wind moves (boosted by Wind Power)
#define MOVE_PROPERTY_SLICING   8  // Slicing moves (boosted by Sharpness)
#define MOVE_PROPERTY_HEALING   9  // Healing moves (blocked by Heal Block)
#define MOVE_PROPERTY_PHYSICAL  10 // Physical damage category
#define MOVE_PROPERTY_SPECIAL   11 // Special damage category
#define MOVE_PROPERTY_STATUS    12 // Status damage category

#endif // GUARD_SCRIPTING_UTIL_H
