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
    QUEST_COND_QUEST,
    QUEST_COND_QUESTS_COMPLETE,
    QUEST_COND_FLAGS_COUNT,
    QUEST_COND_DEX_COUNT,
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

enum QuestDexKind
{
    QUEST_DEX_REGIONAL,
    QUEST_DEX_NATIONAL,
};

// Ops for QUEST_COND_QUEST
enum QuestCompareOp
{
    QUEST_CMP_STATUS_GE,
    QUEST_CMP_STATUS_EQ,
    QUEST_CMP_STAGE_GE,     // active at or past the stage, or complete
    QUEST_CMP_OUTCOME,      // complete with the outcome
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
    u8 type;            // enum QuestConditionType
    u8 op;              // enum QuestVarOp / PARTY_COND_* / enum QuestDexState / enum QuestCompareOp
    u8 listCount;       // entries in list
    bool8 progress;     // show x/y
    u16 id;             // flag, var, item, species, type, ability, nature, quest, enum QuestDexKind
    u16 value;          // threshold / count / status / stage / outcome
    const u16 *list;    // flags (FLAGS_COUNT) or quest ids (QUESTS_COMPLETE)
};

struct QuestNpc
{
    u16 map;
    u8 localId;
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
    const u8 *journal;  // NULL for no journal entry
    const struct QuestObjective *objectives;
    const struct QuestNpc *turnIns;
    u8 objectiveCount;
    u8 turnInCount;
    u8 paths;           // bit per path this stage is on
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
    u8 category;
    u8 iconType;
    u16 icon;
    u8 parent;          // QUEST_NONE unless this is a task
    bool8 isTask;
    const struct QuestNpc *givers;
    const struct QuestReward *rewards;
    const struct QuestStage *stages;
    const struct QuestOutcome *outcomes;
    u8 giverCount, rewardCount, stageCount, outcomeCount;
};

extern const struct Quest gQuests[];

// Data access
bool32 Quest_IsValid(u32 questId);
const struct Quest *Quest_GetInfo(u32 questId);
const struct QuestStage *Quest_GetCurrentStage(u32 questId);
bool8 Quest_IsTask(u32 questId);

// Save state
u8 Quest_GetStatus(u32 questId);
u8 Quest_GetStage(u32 questId);
u8 Quest_GetOutcome(u32 questId);
u8 Quest_GetPath(u32 questId);
bool8 Quest_IsStageOnPath(u32 questId, u32 stage, u32 path);
bool8 Quest_IsStagePassed(u32 questId, u32 stage);
bool8 Quest_IsShown(u32 questId);
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
bool8 Quest_Unlock(u32 questId);
bool8 Quest_Start(u32 questId);
bool8 Quest_StartAt(u32 questId, u32 stage, u32 path);
bool8 Quest_SetStage(u32 questId, u32 stage);
bool8 Quest_SetPath(u32 questId, u32 path);
bool8 Quest_CompleteObjective(u32 questId, u32 objective);
bool8 Quest_Complete(u32 questId, u32 outcome);
bool8 Quest_Close(u32 questId);

// Debug: raw writes that skip transition rules
void Quest_DebugSetStatus(u32 questId, u32 status);
void Quest_DebugSetStage(u32 questId, u32 stage);
void Quest_DebugSetOutcome(u32 questId, u32 outcome);
void Quest_DebugSetPath(u32 questId, u32 path);
void Quest_DebugToggleObjective(u32 questId, u32 objective);

// Conditions
bool8 Quest_EvaluateCondition(const struct QuestCondition *condition);
bool8 Quest_GetConditionProgress(const struct QuestCondition *condition, u16 *current, u16 *target);
bool8 Quest_PartyMonMatches(u32 slot, u32 key, u32 value);
u8 Quest_FindPartyMatch(u32 key, u32 value); // PARTY_SIZE if none

// Progress
void Quest_CheckProgress(void);
void Quest_InitProgressCache(void);

// Guidance. The tracked target is the tracked quest's, or the pinned lead's.
bool8 Quest_GetTarget(u32 questId, u16 *map, u8 *localId);
bool8 Quest_GetTrackedTarget(u16 *map, u8 *localId);
u16 Quest_GetMapSec(u16 map);

#endif // GUARD_QUEST_H
