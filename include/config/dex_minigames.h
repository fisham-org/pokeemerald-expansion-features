#ifndef GUARD_CONFIG_DEX_MINIGAMES_H
#define GUARD_CONFIG_DEX_MINIGAMES_H

// Pokédex minigames, opened with the scripts in data/scripts/pokedoku.inc and data/scripts/squirdle.inc.
// Disabling a game removes its save data from SaveBlock3 and makes its specials do nothing.

// PokeDoku: fill a 3x3 grid with Pokémon matching each row and column.
#define POKEDOKU_ENABLED                TRUE
#define POKEDOKU_MIN_ANSWERS_PER_CELL   3       // Every generated cell has at least this many valid species. The generator also guarantees 9 distinct species can fill the board.
#define POKEDOKU_MAX_GENERATION_TRIES   2000    // Random boards tried per pass. If none meets the minimum above, another pass allows 1 answer per cell (for small seen/caught pools), then a fixed fallback board is tried.
#define POKEDOKU_DEFAULT_ANSWER_POOL    DEX_POOL_ALL // Answer pool used by Pokedoku_EventScript_Terminal: DEX_POOL_ALL, DEX_POOL_SEEN or DEX_POOL_CAUGHT. Other scripts can pass their own.

// Where PokeDoku's popularity (for the rarity score) comes from, in percent. Must add up to 100.
#define POKEDOKU_POPULARITY_SURVEY      100      // A real-world popularity survey (src/data/dex_popularity.h)
#define POKEDOKU_POPULARITY_TRAINERS    0      // How many trainers in this game use the species
#define POKEDOKU_POPULARITY_WILD        0      // How common the species is in this game's wild encounters

// Squirdle: guess a mystery Pokémon from generation, type, height and weight clues.
#define SQUIRDLE_ENABLED                TRUE
#define SQUIRDLE_DEFAULT_ANSWER_POOL    DEX_POOL_ALL // Answer pool used by Squirdle_EventScript_Terminal. The target is always from the answer pool.

#endif // GUARD_CONFIG_DEX_MINIGAMES_H
