#include "global.h"
#include "event_data.h"
#include "pokemon.h"
#include "move.h"
#include "scripting_util.h"

// Returns TRUE if move matches the specified property type
static bool32 MoveMatchesProperty(u16 move, u8 propertyType)
{
    switch (propertyType)
    {
    case MOVE_PROPERTY_SOUND:
        return IsSoundMove(move);
    case MOVE_PROPERTY_DANCE:
        return IsDanceMove(move);
    case MOVE_PROPERTY_PUNCHING:
        return IsPunchingMove(move);
    case MOVE_PROPERTY_BITING:
        return IsBitingMove(move);
    case MOVE_PROPERTY_PULSE:
        return IsPulseMove(move);
    case MOVE_PROPERTY_BALLISTIC:
        return IsBallisticMove(move);
    case MOVE_PROPERTY_POWDER:
        return IsPowderMove(move);
    case MOVE_PROPERTY_WIND:
        return IsWindMove(move);
    case MOVE_PROPERTY_SLICING:
        return IsSlicingMove(move);
    case MOVE_PROPERTY_HEALING:
        return IsHealingMove(move);
    case MOVE_PROPERTY_PHYSICAL:
        return GetMoveCategory(move) == DAMAGE_CATEGORY_PHYSICAL;
    case MOVE_PROPERTY_SPECIAL:
        return GetMoveCategory(move) == DAMAGE_CATEGORY_SPECIAL;
    case MOVE_PROPERTY_STATUS:
        return GetMoveCategory(move) == DAMAGE_CATEGORY_STATUS;
    default:
        return FALSE;
    }
}

// Counts how many party Pokemon know moves of the specified type
// Input:
//   VAR_0x8004 = Minimum number of Pokemon required (for early exit optimization, 0 = no optimization)
//   VAR_0x8005 = Move type (see TYPE_* constants in include/constants/pokemon.h)
//   VAR_0x8006 = Optional: Move ID to exclude (0 = don't exclude any)
// Output:
//   VAR_RESULT = Number of Pokemon with matching moves
void CountPartyMonsWithMoveType(void)
{
    struct Pokemon *mon;
    u16 move;
    u8 requiredCount = gSpecialVar_0x8004;
    u8 moveType = gSpecialVar_0x8005;
    u16 excludeMove = gSpecialVar_0x8006;
    u8 monCount = 0;
    u8 i, j;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        mon = &gPlayerParty[i];

        if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
            continue;
        if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
            continue;

        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            move = GetMonData(mon, MON_DATA_MOVE1 + j, NULL);

            if (move == MOVE_NONE)
                continue;

            if (excludeMove != 0 && move == excludeMove)
                continue;

            if (GetMoveType(move) == moveType)
            {
                monCount++;
                break;
            }
        }

        if (monCount >= requiredCount && requiredCount > 0)
            break;
    }

    gSpecialVar_Result = monCount;
}

// Counts how many party Pokemon know moves with power >= the specified threshold
// Input:
//   VAR_0x8004 = Minimum number of Pokemon required (for early exit optimization, 0 = no optimization)
//   VAR_0x8005 = Power threshold (e.g. 80 = moves with base power >= 80)
//   VAR_0x8006 = Optional: Move ID to exclude (0 = don't exclude any)
// Output:
//   VAR_RESULT = Number of Pokemon with matching moves
void CountPartyMonsWithMovePower(void)
{
    struct Pokemon *mon;
    u16 move;
    u8 requiredCount = gSpecialVar_0x8004;
    u8 powerThreshold = gSpecialVar_0x8005;
    u16 excludeMove = gSpecialVar_0x8006;
    u8 monCount = 0;
    u8 i, j;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        mon = &gPlayerParty[i];

        if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
            continue;
        if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
            continue;

        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            move = GetMonData(mon, MON_DATA_MOVE1 + j, NULL);

            if (move == MOVE_NONE)
                continue;

            if (excludeMove != 0 && move == excludeMove)
                continue;

            if (GetMovePower(move) >= powerThreshold)
            {
                monCount++;
                break;
            }
        }

        if (monCount >= requiredCount && requiredCount > 0)
            break;
    }

    gSpecialVar_Result = monCount;
}

// Counts how many party Pokemon know moves with the specified property
// Input:
//   VAR_0x8004 = Minimum number of Pokemon required (for early exit optimization, 0 = no optimization)
//   VAR_0x8005 = Move property type (see MOVE_PROPERTY_* constants in scripting_util.h)
//   VAR_0x8006 = Optional: Move ID to exclude (0 = don't exclude any)
// Output:
//   VAR_RESULT = Number of Pokemon with matching moves
void CountPartyMonsWithMoveProperty(void)
{
    struct Pokemon *mon;
    u16 move;
    u8 requiredCount = gSpecialVar_0x8004;
    u8 propertyType = gSpecialVar_0x8005;
    u16 excludeMove = gSpecialVar_0x8006;
    u8 monCount = 0;
    u8 i, j;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        mon = &gPlayerParty[i];

        // Skip empty slots and eggs
        if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
            continue;
        if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
            continue;

        // Check if this Pokemon knows any move with the property
        for (j = 0; j < MAX_MON_MOVES; j++)
        {
            move = GetMonData(mon, MON_DATA_MOVE1 + j, NULL);

            // Skip empty move slots
            if (move == MOVE_NONE)
                continue;

            // Skip excluded move if specified
            if (excludeMove != 0 && move == excludeMove)
                continue;

            // Check if move has the desired property
            if (MoveMatchesProperty(move, propertyType))
            {
                monCount++;
                break; // Count each Pokemon only once
            }
        }

        // Early exit optimization if we already have enough
        if (monCount >= requiredCount && requiredCount > 0)
            break;
    }

    gSpecialVar_Result = monCount;
}
