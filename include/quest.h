#ifndef GUARD_QUEST_H
#define GUARD_QUEST_H

#include "constants/quests.h"

enum QuestConditionType
{
    QUEST_COND_NONE,
    QUEST_COND_FLAG,
    QUEST_COND_VAR,
    QUEST_COND_ITEM,
    QUEST_COND_PARTY,
    QUEST_COND_DEX,
};

enum QuestVarOp
{
    QUEST_VAR_GE,
    QUEST_VAR_EQ,
    QUEST_VAR_LT,
};

enum QuestDexState
{
    QUEST_DEX_SEEN,
    QUEST_DEX_CAUGHT,
};

enum QuestIconType
{
    QUEST_ICON_OBJECT,
    QUEST_ICON_ITEM,
    QUEST_ICON_PKMN,
};

enum QuestRewardType
{
    QUEST_REWARD_ITEM,
    QUEST_REWARD_MONEY,
};

struct QuestCondition
{
    u8 type;        // enum QuestConditionType
    u8 op;          // enum QuestVarOp / PARTY_COND_* / enum QuestDexState
    u16 id;         // flag, var, item, species, type, ability, nature
    u16 value;      // threshold / count
    bool8 progress; // show x/y
};

struct QuestObjective
{
    const u8 *text;
    u16 targetMap;      // MAP_* or MAP_UNDEFINED
    u8 targetLocalId;   // LOCALID_* or LOCALID_NONE
    bool8 optional;
    struct QuestCondition condition;
    struct QuestCondition reveal;
};

struct QuestStage
{
    const u8 *title;    // NULL for a removed stage placeholder
    const u8 *journal;
    const struct QuestObjective *objectives;
    u16 turnInMap;
    u8 turnInLocalId;
    u8 objectiveCount;
};

struct QuestReward
{
    u8 type;            // enum QuestRewardType
    u16 item;
    u32 amount;         // item count or money
};

struct QuestOutcome
{
    const u8 *summary;
    const u8 *closes;   // QUEST_* ids closed by this outcome
    u8 closesCount;
};

struct Quest
{
    const u8 *name;
    const u8 *summary;
    const u8 *leadHint;
    u8 category;
    u8 iconType;
    u16 icon;
    u16 giverMap;
    u8 giverLocalId;
    u16 leadMap;
    const struct QuestReward *rewards;
    const struct QuestStage *stages;
    const struct QuestOutcome *outcomes;
    u8 rewardCount, stageCount, outcomeCount;
};

extern const struct Quest gQuests[];

// Data access
bool32 Quest_IsValid(u32 questId);
const struct Quest *Quest_GetInfo(u32 questId);
const struct QuestStage *Quest_GetCurrentStage(u32 questId);

// Save state
u8 Quest_GetStatus(u32 questId);
u8 Quest_GetStage(u32 questId);
u8 Quest_GetOutcome(u32 questId);
bool8 Quest_IsUnread(u32 questId);
void Quest_ClearUnread(u32 questId);
bool8 Quest_HasUnread(void);
bool8 Quest_IsObjectiveDone(u32 questId, u32 objective);
bool8 Quest_IsObjectiveVisible(u32 questId, u32 objective);
bool8 Quest_IsFinished(u32 questId);
bool8 Quest_IsTurnInReady(u32 questId);
u8 Quest_CountByStatus(u32 category, u32 status);
u8 Quest_GetTracked(void);
bool8 Quest_SetTracked(u32 questId);
void Quest_ResetAll(void);

// State changes. Each one sets unread, queues a toast and refreshes NPC markers.
bool8 Quest_AddLead(u32 questId);
bool8 Quest_Unlock(u32 questId);
bool8 Quest_Start(u32 questId);
bool8 Quest_SetStage(u32 questId, u32 stage);
bool8 Quest_Complete(u32 questId, u32 outcome);
bool8 Quest_Close(u32 questId);

// Debug: raw writes that skip transition rules
void Quest_DebugSetStatus(u32 questId, u32 status);
void Quest_DebugSetStage(u32 questId, u32 stage);
void Quest_DebugSetOutcome(u32 questId, u32 outcome);
void Quest_DebugToggleObjective(u32 questId, u32 objective);

// Conditions
bool8 Quest_EvaluateCondition(const struct QuestCondition *condition);
bool8 Quest_GetConditionProgress(const struct QuestCondition *condition, u16 *current, u16 *target);
bool8 Quest_PartyMonMatches(u32 slot, u32 key, u32 value);
u8 Quest_FindPartyMatch(u32 key, u32 value); // PARTY_SIZE if none

// Progress
void Quest_CheckProgress(void);
void Quest_InitProgressCache(void);

// Guidance
bool8 Quest_GetTarget(u32 questId, u16 *map, u8 *localId);
bool8 Quest_GetTrackedTarget(u16 *map, u8 *localId);
u16 Quest_GetMapSec(u16 map);

#endif // GUARD_QUEST_H
