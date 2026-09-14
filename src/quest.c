#include "global.h"
#include "event_data.h"
#include "item.h"
#include "overworld.h"
#include "pokedex.h"
#include "pokemon.h"
#include "quest.h"
#include "quest_guidance.h"
#include "quest_toast.h"
#include "script.h"
#include "string_util.h"
#include "constants/abilities.h"
#include "constants/event_objects.h"
#include "constants/items.h"
#include "constants/map_event_ids.h"
#include "constants/maps.h"
#include "constants/party_menu.h"
#include "constants/pokemon.h"
#include "constants/region_map_sections.h"
#include "constants/species.h"

#include "data/quests.h"

// Last-seen value of each objective's progress counter, used to decide when an
// "x/y" toast is due. RAM only; refilled silently on load and on state changes.
static EWRAM_DATA u8 sLastProgress[QUEST_MAX][QUEST_MAX_OBJECTIVES] = {0};

static inline struct QuestSaveEntry *GetEntry(u32 questId)
{
    return &gSaveBlock3Ptr->quests[questId];
}

// *******************************
// Data access

bool32 Quest_IsValid(u32 questId)
{
    return questId < QUEST_COUNT && gQuests[questId].stageCount != 0;
}

const struct Quest *Quest_GetInfo(u32 questId)
{
    return &gQuests[questId];
}

const struct QuestStage *Quest_GetCurrentStage(u32 questId)
{
    const struct Quest *quest = &gQuests[questId];
    u32 stage = GetEntry(questId)->stage;

    if (stage >= quest->stageCount)
        stage = quest->stageCount - 1;
    return &quest->stages[stage];
}

// *******************************
// Save state

u8 Quest_GetStatus(u32 questId)
{
    if (!Quest_IsValid(questId))
        return QUEST_STATUS_HIDDEN;
    return GetEntry(questId)->status;
}

u8 Quest_GetStage(u32 questId)
{
    if (!Quest_IsValid(questId))
        return 0;
    return GetEntry(questId)->stage;
}

u8 Quest_GetOutcome(u32 questId)
{
    if (!Quest_IsValid(questId))
        return 0;
    return GetEntry(questId)->outcome;
}

bool8 Quest_IsUnread(u32 questId)
{
    return Quest_IsValid(questId) && GetEntry(questId)->unread;
}

void Quest_ClearUnread(u32 questId)
{
    if (Quest_IsValid(questId))
        GetEntry(questId)->unread = FALSE;
}

bool8 Quest_HasUnread(void)
{
    for (u32 i = 0; i < QUEST_COUNT; i++)
    {
        if (Quest_GetStatus(i) != QUEST_STATUS_HIDDEN && GetEntry(i)->unread)
            return TRUE;
    }
    return FALSE;
}

bool8 Quest_IsObjectiveDone(u32 questId, u32 objective)
{
    if (!Quest_IsValid(questId) || objective >= QUEST_MAX_OBJECTIVES)
        return FALSE;
    return (GetEntry(questId)->objectivesDone >> objective) & 1;
}

bool8 Quest_IsObjectiveVisible(u32 questId, u32 objective)
{
    const struct QuestStage *stage;

    if (!Quest_IsValid(questId))
        return FALSE;
    stage = Quest_GetCurrentStage(questId);
    if (objective >= stage->objectiveCount)
        return FALSE;
    if (Quest_IsObjectiveDone(questId, objective))
        return TRUE;
    if (stage->objectives[objective].reveal.type == QUEST_COND_NONE)
        return TRUE;
    return Quest_EvaluateCondition(&stage->objectives[objective].reveal);
}

bool8 Quest_IsFinished(u32 questId)
{
    u32 status = Quest_GetStatus(questId);
    return status == QUEST_STATUS_COMPLETE || status == QUEST_STATUS_CLOSED;
}

bool8 Quest_IsTurnInReady(u32 questId)
{
    const struct QuestStage *stage;

    if (Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE)
        return FALSE;
    stage = Quest_GetCurrentStage(questId);
    if (stage->turnInMap == MAP_UNDEFINED)
        return FALSE;
    for (u32 i = 0; i < stage->objectiveCount; i++)
    {
        if (!stage->objectives[i].optional
         && !Quest_IsObjectiveDone(questId, i)
         && Quest_IsObjectiveVisible(questId, i))
            return FALSE;
    }
    return TRUE;
}

u8 Quest_CountByStatus(u32 category, u32 status)
{
    u32 count = 0;
    for (u32 i = 0; i < QUEST_COUNT; i++)
    {
        if (!Quest_IsValid(i))
            continue;
        if (category != QUEST_CATEGORY_ANY && gQuests[i].category != category)
            continue;
        if (GetEntry(i)->status == status)
            count++;
    }
    return count;
}

u8 Quest_GetTracked(void)
{
    u32 questId = gSaveBlock3Ptr->trackedQuest - 1;
    if (gSaveBlock3Ptr->trackedQuest == 0 || Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE)
        return QUEST_NONE;
    return questId;
}

// Pass QUEST_NONE to untrack. Only active quests can be tracked.
bool8 Quest_SetTracked(u32 questId)
{
    if (questId == QUEST_NONE)
    {
        gSaveBlock3Ptr->trackedQuest = 0;
        return TRUE;
    }
    if (Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE)
        return FALSE;
    gSaveBlock3Ptr->trackedQuest = questId + 1;
    return TRUE;
}

static void FillLastProgress(u32 questId)
{
    const struct QuestStage *stage;
    u16 current, target;

    memset(sLastProgress[questId], 0, sizeof(sLastProgress[questId]));
    if (Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE)
        return;
    stage = Quest_GetCurrentStage(questId);
    for (u32 i = 0; i < stage->objectiveCount; i++)
    {
        if (Quest_GetConditionProgress(&stage->objectives[i].condition, &current, &target))
            sLastProgress[questId][i] = min(current, 255);
    }
}

void Quest_InitProgressCache(void)
{
    memset(sLastProgress, 0, sizeof(sLastProgress));
    for (u32 i = 0; i < QUEST_COUNT; i++)
        FillLastProgress(i);
}

void Quest_ResetAll(void)
{
    memset(gSaveBlock3Ptr->quests, 0, sizeof(gSaveBlock3Ptr->quests));
    gSaveBlock3Ptr->trackedQuest = 0;
    Quest_InitProgressCache();
    QuestMarkers_Refresh();
}

// *******************************
// State changes

static void SetStatus(u32 questId, u32 status, u32 toastType)
{
    struct QuestSaveEntry *entry = GetEntry(questId);

    entry->status = status;
    entry->objectivesDone = 0;
    entry->unread = TRUE;
    if (status != QUEST_STATUS_ACTIVE && gSaveBlock3Ptr->trackedQuest == questId + 1)
        gSaveBlock3Ptr->trackedQuest = 0;
    FillLastProgress(questId);
    QuestToast_Queue(toastType, questId, 0);
}

static bool32 IsOpenStatus(u32 status)
{
    return status == QUEST_STATUS_LEAD
        || status == QUEST_STATUS_AVAILABLE
        || status == QUEST_STATUS_ACTIVE;
}

bool8 Quest_AddLead(u32 questId)
{
    if (Quest_GetStatus(questId) != QUEST_STATUS_HIDDEN || !Quest_IsValid(questId))
        return FALSE;
    SetStatus(questId, QUEST_STATUS_LEAD, QUEST_TOAST_LEAD);
    QuestMarkers_Refresh();
    return TRUE;
}

bool8 Quest_Unlock(u32 questId)
{
    u32 status = Quest_GetStatus(questId);
    if (!Quest_IsValid(questId) || (status != QUEST_STATUS_HIDDEN && status != QUEST_STATUS_LEAD))
        return FALSE;
    SetStatus(questId, QUEST_STATUS_AVAILABLE, QUEST_TOAST_AVAILABLE);
    QuestMarkers_Refresh();
    return TRUE;
}

bool8 Quest_Start(u32 questId)
{
    if (!Quest_IsValid(questId) || Quest_GetStatus(questId) > QUEST_STATUS_AVAILABLE)
        return FALSE;
    GetEntry(questId)->stage = 0;
    SetStatus(questId, QUEST_STATUS_ACTIVE, QUEST_TOAST_STARTED);
    QuestMarkers_Refresh();
    return TRUE;
}

// Stages only move forward.
bool8 Quest_SetStage(u32 questId, u32 stage)
{
    struct QuestSaveEntry *entry = GetEntry(questId);

    if (Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE
     || stage <= entry->stage
     || stage >= gQuests[questId].stageCount)
        return FALSE;
    entry->stage = stage;
    SetStatus(questId, QUEST_STATUS_ACTIVE, QUEST_TOAST_UPDATED);
    QuestMarkers_Refresh();
    return TRUE;
}

static bool8 CloseQuest(u32 questId)
{
    if (!IsOpenStatus(Quest_GetStatus(questId)))
        return FALSE;
    SetStatus(questId, QUEST_STATUS_CLOSED, QUEST_TOAST_CLOSED);
    return TRUE;
}

bool8 Quest_Complete(u32 questId, u32 outcome)
{
    const struct QuestOutcome *questOutcome;

    if (!Quest_IsValid(questId) || Quest_IsFinished(questId) || outcome >= gQuests[questId].outcomeCount)
        return FALSE;
    GetEntry(questId)->outcome = outcome;
    SetStatus(questId, QUEST_STATUS_COMPLETE, QUEST_TOAST_COMPLETE);

    questOutcome = &gQuests[questId].outcomes[outcome];
    for (u32 i = 0; i < questOutcome->closesCount; i++)
        CloseQuest(questOutcome->closes[i]);

    QuestMarkers_Refresh();
    return TRUE;
}

bool8 Quest_Close(u32 questId)
{
    if (!CloseQuest(questId))
        return FALSE;
    QuestMarkers_Refresh();
    return TRUE;
}

// *******************************
// Debug

void Quest_DebugSetStatus(u32 questId, u32 status)
{
    if (!Quest_IsValid(questId))
        return;
    GetEntry(questId)->status = status;
    GetEntry(questId)->objectivesDone = 0;
    GetEntry(questId)->unread = TRUE;
    FillLastProgress(questId);
    QuestMarkers_Refresh();
}

void Quest_DebugSetStage(u32 questId, u32 stage)
{
    if (!Quest_IsValid(questId) || stage >= gQuests[questId].stageCount)
        return;
    GetEntry(questId)->stage = stage;
    GetEntry(questId)->objectivesDone = 0;
    GetEntry(questId)->unread = TRUE;
    FillLastProgress(questId);
    QuestMarkers_Refresh();
}

void Quest_DebugSetOutcome(u32 questId, u32 outcome)
{
    if (!Quest_IsValid(questId) || outcome >= gQuests[questId].outcomeCount)
        return;
    GetEntry(questId)->outcome = outcome;
    GetEntry(questId)->unread = TRUE;
}

void Quest_DebugToggleObjective(u32 questId, u32 objective)
{
    if (!Quest_IsValid(questId) || objective >= QUEST_MAX_OBJECTIVES)
        return;
    GetEntry(questId)->objectivesDone ^= 1 << objective;
    QuestMarkers_Refresh();
}

// *******************************
// Conditions

bool8 Quest_PartyMonMatches(u32 slot, u32 key, u32 value)
{
    struct Pokemon *mon;
    enum Species species;

    if (slot >= PARTY_SIZE)
        return FALSE;
    mon = &gParties[B_TRAINER_PLAYER][slot];
    species = GetMonData(mon, MON_DATA_SPECIES);
    if (species == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
        return FALSE;

    switch (key)
    {
    case PARTY_COND_SPECIES:
        return species == value;
    case PARTY_COND_TYPE:
        return GetSpeciesType(species, 0) == value || GetSpeciesType(species, 1) == value;
    case PARTY_COND_ABILITY:
        return GetMonAbility(mon) == value;
    case PARTY_COND_NATURE:
        return GetNature(mon) == value;
    }
    return FALSE;
}

u8 Quest_FindPartyMatch(u32 key, u32 value)
{
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        if (Quest_PartyMonMatches(i, key, value))
            return i;
    }
    return PARTY_SIZE;
}

bool8 Quest_EvaluateCondition(const struct QuestCondition *condition)
{
    switch (condition->type)
    {
    case QUEST_COND_FLAG:
        return FlagGet(condition->id);
    case QUEST_COND_VAR:
    {
        u16 value = VarGet(condition->id);
        switch (condition->op)
        {
        case QUEST_VAR_GE: return value >= condition->value;
        case QUEST_VAR_EQ: return value == condition->value;
        case QUEST_VAR_LT: return value < condition->value;
        }
        return FALSE;
    }
    case QUEST_COND_ITEM:
        return CountTotalItemQuantityInBag(condition->id) >= condition->value;
    case QUEST_COND_PARTY:
        return Quest_FindPartyMatch(condition->op, condition->id) != PARTY_SIZE;
    case QUEST_COND_DEX:
        return GetSetPokedexFlag(SpeciesToNationalPokedexNum(condition->id),
                                 condition->op == QUEST_DEX_SEEN ? FLAG_GET_SEEN : FLAG_GET_CAUGHT);
    }
    return FALSE;
}

// Returns FALSE if the condition has no x/y progress.
bool8 Quest_GetConditionProgress(const struct QuestCondition *condition, u16 *current, u16 *target)
{
    if (!condition->progress)
        return FALSE;

    switch (condition->type)
    {
    case QUEST_COND_VAR:
        *current = VarGet(condition->id);
        break;
    case QUEST_COND_ITEM:
        *current = CountTotalItemQuantityInBag(condition->id);
        break;
    default:
        return FALSE;
    }
    *target = condition->value;
    return TRUE;
}

// *******************************
// Progress

void Quest_CheckProgress(void)
{
    bool32 changed = FALSE;
    u16 current, target;

    for (u32 questId = 0; questId < QUEST_COUNT; questId++)
    {
        const struct QuestStage *stage;

        if (Quest_GetStatus(questId) != QUEST_STATUS_ACTIVE)
            continue;

        stage = Quest_GetCurrentStage(questId);
        for (u32 i = 0; i < stage->objectiveCount; i++)
        {
            const struct QuestCondition *condition = &stage->objectives[i].condition;
            bool32 progressed = FALSE;

            if (condition->type == QUEST_COND_NONE
             || Quest_IsObjectiveDone(questId, i)
             || !Quest_IsObjectiveVisible(questId, i))
                continue;

            if (Quest_GetConditionProgress(condition, &current, &target))
            {
                current = min(current, 255);
                if (current > sLastProgress[questId][i])
                    progressed = TRUE;
                sLastProgress[questId][i] = current;
            }

            if (Quest_EvaluateCondition(condition))
            {
                GetEntry(questId)->objectivesDone |= 1 << i;
                GetEntry(questId)->unread = TRUE;
                changed = TRUE;
                QuestToast_Queue(QUEST_TOAST_PROGRESS, questId, i);
            }
            else if (progressed)
            {
                QuestToast_Queue(QUEST_TOAST_PROGRESS, questId, i);
            }
        }
    }

    if (changed)
        QuestMarkers_Refresh();
}

// *******************************
// Guidance

// Objective target, then turn-in, then giver. Leads point at their area.
bool8 Quest_GetTarget(u32 questId, u16 *map, u8 *localId)
{
    const struct Quest *quest = &gQuests[questId];
    const struct QuestStage *stage;

    *map = MAP_UNDEFINED;
    *localId = LOCALID_NONE;

    switch (Quest_GetStatus(questId))
    {
    case QUEST_STATUS_LEAD:
        *map = quest->leadMap;
        break;
    case QUEST_STATUS_AVAILABLE:
        *map = quest->giverMap;
        *localId = quest->giverLocalId;
        break;
    case QUEST_STATUS_ACTIVE:
        stage = Quest_GetCurrentStage(questId);
        for (u32 i = 0; i < stage->objectiveCount; i++)
        {
            const struct QuestObjective *objective = &stage->objectives[i];
            if (objective->targetMap != MAP_UNDEFINED
             && !objective->optional
             && !Quest_IsObjectiveDone(questId, i)
             && Quest_IsObjectiveVisible(questId, i))
            {
                *map = objective->targetMap;
                *localId = objective->targetLocalId;
                return TRUE;
            }
        }
        if (stage->turnInMap != MAP_UNDEFINED)
        {
            *map = stage->turnInMap;
            *localId = stage->turnInLocalId;
        }
        else
        {
            *map = quest->giverMap;
            *localId = quest->giverLocalId;
        }
        break;
    }
    return *map != MAP_UNDEFINED;
}

bool8 Quest_GetTrackedTarget(u16 *map, u8 *localId)
{
    u32 questId = Quest_GetTracked();
    if (questId == QUEST_NONE)
    {
        *map = MAP_UNDEFINED;
        *localId = LOCALID_NONE;
        return FALSE;
    }
    return Quest_GetTarget(questId, map, localId);
}

u16 Quest_GetMapSec(u16 map)
{
    if (map == MAP_UNDEFINED)
        return MAPSEC_NONE;
    return Overworld_GetMapHeaderByGroupAndId(MAP_GROUP(map), MAP_NUM(map))->regionMapSectionId;
}

// *******************************
// Script natives

#define READ_QUEST_ID(ctx) VarGet(ScriptReadHalfword(ctx))

static void RequestStateChangeEffects(void)
{
    Script_RequestEffects(SCREFF_V1 | SCREFF_SAVE | SCREFF_HARDWARE);
}

void ScrCmd_addlead(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    RequestStateChangeEffects();
    Quest_AddLead(questId);
}

void ScrCmd_unlockquest(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    RequestStateChangeEffects();
    Quest_Unlock(questId);
}

void ScrCmd_startquest(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    RequestStateChangeEffects();
    Quest_Start(questId);
}

void ScrCmd_setqueststage(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    u32 stage = ScriptReadByte(ctx);
    RequestStateChangeEffects();
    Quest_SetStage(questId, stage);
}

void ScrCmd_completequest(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    u32 outcome = ScriptReadByte(ctx);
    RequestStateChangeEffects();
    Quest_Complete(questId, outcome);
}

void ScrCmd_closequest(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    RequestStateChangeEffects();
    Quest_Close(questId);
}

void ScrCmd_questupdate(struct ScriptContext *ctx)
{
    RequestStateChangeEffects();
    Quest_CheckProgress();
}

void ScrCmd_checkqueststatus(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    u32 status = ScriptReadByte(ctx);
    Script_RequestEffects(SCREFF_V1);
    ctx->comparisonResult = (Quest_GetStatus(questId) == status);
}

// Must match the values used by the goto_if_quest_stage_* macros in asm/macros/event.inc
enum { QUEST_STAGE_CMP_EQ, QUEST_STAGE_CMP_GE, QUEST_STAGE_CMP_LT };

// Compares the stored stage regardless of status; a quest that was never started is at stage 0.
void ScrCmd_checkqueststage(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    u32 stage = ScriptReadByte(ctx);
    u32 cmp = ScriptReadByte(ctx);
    u32 current = Quest_GetStage(questId);
    bool32 result = FALSE;

    Script_RequestEffects(SCREFF_V1);
    switch (cmp)
    {
    case QUEST_STAGE_CMP_EQ: result = current == stage; break;
    case QUEST_STAGE_CMP_GE: result = current >= stage; break;
    case QUEST_STAGE_CMP_LT: result = current < stage; break;
    }
    ctx->comparisonResult = result;
}

void ScrCmd_checkquestoutcome(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    u32 outcome = ScriptReadByte(ctx);
    Script_RequestEffects(SCREFF_V1);
    ctx->comparisonResult = (Quest_GetStatus(questId) == QUEST_STATUS_COMPLETE && Quest_GetOutcome(questId) == outcome);
}

void ScrCmd_checkobjectivedone(struct ScriptContext *ctx)
{
    u32 questId = READ_QUEST_ID(ctx);
    u32 objective = ScriptReadByte(ctx);
    Script_RequestEffects(SCREFF_V1);
    ctx->comparisonResult = Quest_IsObjectiveDone(questId, objective);
}

void ScrCmd_bufferquestname(struct ScriptContext *ctx)
{
    u8 stringVarIndex = ScriptReadByte(ctx);
    u32 questId = READ_QUEST_ID(ctx);
    Script_RequestEffects(SCREFF_V1);
    if (Quest_IsValid(questId))
        StringCopy(GetStringVar(stringVarIndex), gQuests[questId].name);
}

void ScrCmd_bufferquestcount(struct ScriptContext *ctx)
{
    u8 stringVarIndex = ScriptReadByte(ctx);
    u32 category = ScriptReadByte(ctx);
    u32 status = ScriptReadByte(ctx);
    Script_RequestEffects(SCREFF_V1);
    ConvertIntToDecimalStringN(GetStringVar(stringVarIndex), Quest_CountByStatus(category, status), STR_CONV_MODE_LEFT_ALIGN, 3);
}

void ScrCmd_checkpartycondition(struct ScriptContext *ctx)
{
    u32 key = ScriptReadByte(ctx);
    u32 value = VarGet(ScriptReadHalfword(ctx));
    u32 slot = Quest_FindPartyMatch(key, value);

    Script_RequestEffects(SCREFF_V1);
    gSpecialVar_Result = (slot != PARTY_SIZE);
    if (slot != PARTY_SIZE)
        gSpecialVar_0x8005 = slot;
}

void ScrCmd_checkchosenmoncondition(struct ScriptContext *ctx)
{
    u32 key = ScriptReadByte(ctx);
    u32 value = VarGet(ScriptReadHalfword(ctx));

    Script_RequestEffects(SCREFF_V1);
    if (gSpecialVar_0x8004 == PARTY_NOTHING_CHOSEN)
        gSpecialVar_Result = FALSE;
    else
        gSpecialVar_Result = Quest_PartyMonMatches(gSpecialVar_0x8004, key, value);
}
