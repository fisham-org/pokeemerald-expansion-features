#ifndef GUARD_CONSTANTS_QUEST_TYPES_H
#define GUARD_CONSTANTS_QUEST_TYPES_H

// Hand-written quest constants. Quest, stage, outcome, note and subject ids are generated
// into constants/quests.h by tools/quests/quests_to_header.py.
// These are #defines (not enums) so event scripts and tests can use them.

#define QUEST_MAX                   128 // Save slots. Do not lower once saves exist.
#define QUEST_MAX_OBJECTIVES        6   // Per stage. Bounded by the u8 objectivesDone bitfield.
#define QUEST_MAX_OUTCOMES          4   // Bounded by the 2-bit outcome field.
#define QUEST_MAX_PATHS             4   // Bounded by the 2-bit path field.
#define QUEST_NONE                  0xFF

#define NOTE_MAX                    256 // Save bits. Do not lower once saves exist.
#define SUBJECT_MAX                 64  // Save bits. Do not lower once saves exist.
#define NOTE_NONE                   0xFFFF
#define SUBJECT_NONE                0xFF

// enum QuestStatus. Ordered so "status at least ACTIVE" also covers finished quests.
#define QUEST_STATUS_HIDDEN         0 // not in the log
#define QUEST_STATUS_AVAILABLE      1 // giver known, not started
#define QUEST_STATUS_ACTIVE         2
#define QUEST_STATUS_COMPLETE       3
#define QUEST_STATUS_CLOSED         4 // closed by another quest's outcome
#define QUEST_STATUS_COUNT          5

// enum QuestCategory
#define QUEST_CATEGORY_MAIN         0
#define QUEST_CATEGORY_SIDE         1
#define QUEST_CATEGORY_COUNT        2
#define QUEST_CATEGORY_ANY          0xFF

// Party check keys, shared by the "party" condition and the party check macros
#define PARTY_COND_SPECIES          0
#define PARTY_COND_TYPE             1
#define PARTY_COND_ABILITY          2
#define PARTY_COND_NATURE           3

#endif // GUARD_CONSTANTS_QUEST_TYPES_H
