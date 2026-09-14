#ifndef GUARD_CONSTANTS_QUEST_TYPES_H
#define GUARD_CONSTANTS_QUEST_TYPES_H

// Hand-written quest constants. Quest, stage and outcome ids are generated
// into constants/quests.h by tools/quests/quests_to_header.py.
// These are #defines (not enums) so event scripts and tests can use them.

#define QUEST_MAX                   128 // Save slots. Do not lower once saves exist.
#define QUEST_MAX_OBJECTIVES        6   // Per stage. Bounded by the u8 objectivesDone bitfield.
#define QUEST_MAX_OUTCOMES          4   // Bounded by the 2-bit outcome field.
#define QUEST_NONE                  0xFF

// enum QuestStatus
#define QUEST_STATUS_HIDDEN         0 // not in the log
#define QUEST_STATUS_LEAD           1 // heard about; hint only
#define QUEST_STATUS_AVAILABLE      2 // giver known, not started
#define QUEST_STATUS_ACTIVE         3
#define QUEST_STATUS_COMPLETE       4
#define QUEST_STATUS_CLOSED         5 // closed by another quest's outcome

// enum QuestCategory
#define QUEST_CATEGORY_STORY        0
#define QUEST_CATEGORY_POKEMON      1
#define QUEST_CATEGORY_SIDE         2
#define QUEST_CATEGORY_COUNT        3
#define QUEST_CATEGORY_ANY          0xFF

// Party check keys, shared by the "party" condition and the party check macros
#define PARTY_COND_SPECIES          0
#define PARTY_COND_TYPE             1
#define PARTY_COND_ABILITY          2
#define PARTY_COND_NATURE           3

#endif // GUARD_CONSTANTS_QUEST_TYPES_H
