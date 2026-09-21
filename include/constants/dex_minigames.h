#ifndef GUARD_CONSTANTS_DEX_MINIGAMES_H
#define GUARD_CONSTANTS_DEX_MINIGAMES_H

// Returned in VAR_RESULT by GetPokedokuStatus and GetSquirdleStatus
#define DEX_GAME_STATUS_NEW          0 // Today's daily puzzle hasn't been started
#define DEX_GAME_STATUS_IN_PROGRESS  1 // Today's daily puzzle has guesses left
#define DEX_GAME_STATUS_FINISHED     2 // Today's daily puzzle is done; opening it shows the results
#define DEX_GAME_STATUS_CLOCK_LOCKED 3 // The clock went backwards; daily play is locked until the saved day passes
#define DEX_GAME_STATUS_NO_CLOCK     4 // The RTC is missing or broken
#define DEX_GAME_STATUS_DISABLED     5 // The game is disabled in include/config/dex_minigames.h
#define DEX_GAME_STATUS_NOT_STARTED  6 // Today's daily puzzle exists but has no guesses yet (e.g. the player declined the save)

// VAR_0x8004 for OpenPokedoku and OpenSquirdle
#define DEX_GAME_MODE_DAILY     0 // Saved after every guess, counts towards stats and rewards
#define DEX_GAME_MODE_PRACTICE  1 // Random puzzle, not saved, START begins a new one when finished

// VAR_0x8005 for PreparePokedoku and PrepareSquirdle: which species the player can answer with
#define DEX_POOL_ALL     0 // Every species enabled in the Pokédex
#define DEX_POOL_SEEN    1 // Species the player has seen
#define DEX_POOL_CAUGHT  2 // Species the player has caught

// Returned in VAR_RESULT by PreparePokedoku and PrepareSquirdle
#define DEX_GAME_PREPARE_OK                0
#define DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES 1 // The answer pool can't make a puzzle
#define DEX_GAME_PREPARE_REWARD_WAITING     2 // Daily only: the last daily puzzle's reward hasn't been claimed yet

// Returned by ClaimPokedokuReward and ClaimSquirdleReward instead of a score
#define DEX_GAME_NO_REWARD        0xFF // No reward is waiting
#define DEX_GAME_REWARD_BAG_FULL  0xFE // The reward doesn't fit in the bag; it stays waiting

#define POKEDOKU_GRID_SIZE  3
#define POKEDOKU_NUM_CELLS  (POKEDOKU_GRID_SIZE * POKEDOKU_GRID_SIZE)

#define SQUIRDLE_MAX_GUESSES 8

#endif // GUARD_CONSTANTS_DEX_MINIGAMES_H
