#ifndef GUARD_DEX_MINIGAME_H
#define GUARD_DEX_MINIGAME_H

// Shared code for the Pokédex minigames (PokeDoku, Squirdle): the species pool,
// daily puzzle bookkeeping, a full-screen board with outlined boxes, and the species picker.

#include "constants/dex_minigames.h"

enum DexStage
{
    DEX_STAGE_SINGLE, // Doesn't evolve and has no pre-evolution
    DEX_STAGE_BASIC,  // Depth 0 in its evolution chain, can evolve
    DEX_STAGE_MIDDLE, // Depth 1+, can evolve
    DEX_STAGE_FINAL,  // Depth 1+, can't evolve
};

// Every species enabled in the Pokédex, by National Dex number. Index 0 is unused.
// "Available" species are the ones the player can answer with (see DEX_POOL_*).
struct DexPool
{
    u16 species[NATIONAL_DEX_COUNT + 1];    // Base form species, SPECIES_NONE if disabled
    u8 stage[NATIONAL_DEX_COUNT + 1];       // enum DexStage
    bool8 available[NATIONAL_DEX_COUNT + 1];
    u16 popularity[NATIONAL_DEX_COUNT + 1]; // Filled by DexPool_LoadPopularity, 0 if disabled
    u16 alphabetical[NATIONAL_DEX_COUNT];   // Available dex numbers sorted by name
    u16 alphabeticalCount;
    u16 count;                              // Number of available dex numbers
};

struct DexPool *DexPool_Create(u32 answerPool);
void DexPool_LoadPopularity(struct DexPool *pool);
u32 DexPool_GetGeneration(u32 dex);
u32 DexPool_GetNthDex(const struct DexPool *pool, u32 n);
u32 DexPool_StepAvailable(const struct DexPool *pool, u32 dex, s32 delta);
u32 DexPool_GetDexOfSpecies(u32 species);

// Daily rewards: each table is a list of score ranges, each with a weighted pool of items.
// Tables live in src/data/dex_minigame_rewards.h.
enum DexRewardTable
{
    DEX_REWARDS_POKEDOKU, // Score: squares correct (0-9)
    DEX_REWARDS_SQUIRDLE, // Score: guesses a win took (1-8)
};

struct DexRewardItem
{
    u16 item;
    u8 quantity;
    u8 weight; // Chance relative to the other items in the tier
};

struct DexRewardTier
{
    u8 minScore;
    u8 maxScore;
    const u8 *message; // Put in STR_VAR_1 for the script, e.g. "A perfect board!"
    const struct DexRewardItem *items;
    u8 itemCount;
};

bool32 DexGame_RollReward(enum DexRewardTable table, u32 score, u32 seed);
bool32 DexGame_ClaimReward(enum DexRewardTable table, u32 score, u32 seed);

u32 DexGame_GetDailyStatus(bool32 active, u32 savedDay, bool32 started, bool32 finished, u16 *today);
bool32 DexGame_CanOpenDaily(u32 status);
void DexGame_SaveProgress(void);
void DexGame_ResumeScriptNextFrame(void);

// Full-screen board. BG1 holds boxes (8x8 tile units), BG0 holds the game's text windows.
// WIN_* ids are up to each game; the templates must end with DUMMY_WIN_TEMPLATE.
struct DexGameScreen
{
    const struct WindowTemplate *windows;
    bool32 (*init)(void);     // Allocate game state. Returning FALSE exits.
    void (*draw)(void);       // Draw everything once windows exist
    void (*inputTask)(u8 taskId); // Runs once the fade-in finishes
    void (*free)(void);       // Free game state; also called after a failed init
};

void DexGame_OpenScreen(const struct DexGameScreen *screen);
void DexGame_ExitScreen(u8 taskId);

enum DexBoxPalette
{
    DEX_BOX_EMPTY,
    DEX_BOX_WRONG,
    DEX_BOX_CORRECT,
    DEX_BOX_PARTIAL,
    DEX_BOX_CURSOR_OFFSET, // Add to any of the above for a highlighted outline
};

enum DexFontColor
{
    DEX_FONT_WHITE,
    DEX_FONT_HIGHLIGHT,
    DEX_FONT_RED,
    DEX_FONT_GREEN,
    DEX_FONT_GRAY,
    DEX_FONT_DARK, // For text drawn on top of boxes
};

void DexGame_DrawBox(u32 x, u32 y, u32 width, u32 height, u32 palette);
void DexGame_ClearBoxes(u32 x, u32 y, u32 width, u32 height);
void DexGame_CopyBoxesToVram(void);
void DexGame_PrintText(u32 windowId, u32 font, u32 x, u32 y, u32 color, const u8 *str);
u8 DexGame_CreateIcon(u32 species, s16 x, s16 y);
void DexGame_DestroyIcon(u8 *spriteId);

#define DEX_PICKER_ICON_X   216
#define DEX_PICKER_ICON_Y   144

enum DexPickerResult
{
    DEX_PICKER_NONE,
    DEX_PICKER_SUBMIT,
    DEX_PICKER_CANCEL,
};

// Chooses a species in a 2-line window: by dex number like the debug menu (starting at
// the first enabled species), or by name with letter jumps. SELECT switches between the two.
struct DexPicker
{
    const struct DexPool *pool;
    bool32 (*isUsed)(u32 dex); // Used species are shown in gray and can't be submitted
    u16 dex;
    u16 alphabeticalIndex;
    u8 windowId;
    u8 digit;
    bool8 byNumber;
    u8 iconSpriteId;
    const u8 *message;
};

void DexPicker_Init(struct DexPicker *picker, const struct DexPool *pool, u32 windowId, bool32 (*isUsed)(u32 dex));
void DexPicker_Show(struct DexPicker *picker);
void DexPicker_Hide(struct DexPicker *picker);
enum DexPickerResult DexPicker_HandleInput(struct DexPicker *picker);

#endif // GUARD_DEX_MINIGAME_H
