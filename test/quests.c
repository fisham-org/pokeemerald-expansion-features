#include "global.h"
#include "event_data.h"
#include "item.h"
#include "pokedex.h"
#include "pokemon.h"
#include "quest.h"
#include "quest_guidance.h"
#include "quest_note.h"
#include "quest_toast.h"
#include "event_object_movement.h"
#include "sprite.h"
#include "test/overworld_script.h"
#include "test/test.h"
#include "constants/abilities.h"
#include "constants/event_objects.h"
#include "constants/items.h"
#include "constants/map_event_ids.h"
#include "constants/party_menu.h"
#include "constants/pokemon.h"

#define TEST_FLAG FLAG_UNUSED_0x020
#define TEST_VAR  VAR_UNUSED_0x404E

// *******************************
// Conditions (independent of quest content)

TEST("Quest flag condition follows the flag")
{
    struct QuestCondition condition = { .type = QUEST_COND_FLAG, .id = TEST_FLAG };

    EXPECT(!Quest_EvaluateCondition(&condition));
    FlagSet(TEST_FLAG);
    EXPECT(Quest_EvaluateCondition(&condition));
}

TEST("Quest var condition supports ge, eq and lt")
{
    struct QuestCondition ge = { .type = QUEST_COND_VAR, .op = QUEST_VAR_GE, .id = TEST_VAR, .value = 3 };
    struct QuestCondition eq = { .type = QUEST_COND_VAR, .op = QUEST_VAR_EQ, .id = TEST_VAR, .value = 3 };
    struct QuestCondition lt = { .type = QUEST_COND_VAR, .op = QUEST_VAR_LT, .id = TEST_VAR, .value = 3 };

    VarSet(TEST_VAR, 2);
    EXPECT(!Quest_EvaluateCondition(&ge));
    EXPECT(!Quest_EvaluateCondition(&eq));
    EXPECT(Quest_EvaluateCondition(&lt));

    VarSet(TEST_VAR, 3);
    EXPECT(Quest_EvaluateCondition(&ge));
    EXPECT(Quest_EvaluateCondition(&eq));
    EXPECT(!Quest_EvaluateCondition(&lt));

    VarSet(TEST_VAR, 4);
    EXPECT(Quest_EvaluateCondition(&ge));
    EXPECT(!Quest_EvaluateCondition(&eq));
}

TEST("Quest var condition reports x/y progress only when enabled")
{
    struct QuestCondition progress = { .type = QUEST_COND_VAR, .op = QUEST_VAR_GE, .id = TEST_VAR, .value = 5, .progress = TRUE };
    struct QuestCondition noProgress = { .type = QUEST_COND_VAR, .op = QUEST_VAR_GE, .id = TEST_VAR, .value = 5 };
    u16 current = 0, target = 0;

    VarSet(TEST_VAR, 2);
    EXPECT(Quest_GetConditionProgress(&progress, &current, &target));
    EXPECT_EQ(current, 2);
    EXPECT_EQ(target, 5);
    EXPECT(!Quest_GetConditionProgress(&noProgress, &current, &target));
}

TEST("Quest item condition counts the bag")
{
    struct QuestCondition condition = { .type = QUEST_COND_ITEM, .id = ITEM_POTION, .value = 3, .progress = TRUE };
    u16 current = 0, target = 0;

    AddBagItem(ITEM_POTION, 2);
    EXPECT(!Quest_EvaluateCondition(&condition));
    EXPECT(Quest_GetConditionProgress(&condition, &current, &target));
    EXPECT_EQ(current, 2);
    EXPECT_EQ(target, 3);

    AddBagItem(ITEM_POTION, 1);
    EXPECT(Quest_EvaluateCondition(&condition));
}

TEST("Quest party condition matches species, type, ability and nature")
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][1];
    struct QuestCondition species = { .type = QUEST_COND_PARTY, .op = PARTY_COND_SPECIES, .id = SPECIES_WOBBUFFET };
    struct QuestCondition type = { .type = QUEST_COND_PARTY, .op = PARTY_COND_TYPE, .id = TYPE_PSYCHIC };
    struct QuestCondition wrongType = { .type = QUEST_COND_PARTY, .op = PARTY_COND_TYPE, .id = TYPE_FIRE };
    struct QuestCondition nature = { .type = QUEST_COND_PARTY, .op = PARTY_COND_NATURE, .id = NATURE_MODEST };
    struct QuestCondition ability = { .type = QUEST_COND_PARTY, .op = PARTY_COND_ABILITY };

    ASSUME(GetSpeciesType(SPECIES_WOBBUFFET, 0) == TYPE_PSYCHIC);
    ASSUME(GetSpeciesType(SPECIES_WOBBUFFET, 1) == TYPE_PSYCHIC);

    EXPECT(!Quest_EvaluateCondition(&species));

    CreateMon(mon, SPECIES_WOBBUFFET, 5, NATURE_MODEST, OTID_STRUCT_PLAYER_ID);
    ability.id = GetMonAbility(mon);

    EXPECT(Quest_EvaluateCondition(&species));
    EXPECT(Quest_EvaluateCondition(&type));
    EXPECT(!Quest_EvaluateCondition(&wrongType));
    EXPECT(Quest_EvaluateCondition(&nature));
    EXPECT(Quest_EvaluateCondition(&ability));
    EXPECT_EQ(Quest_FindPartyMatch(PARTY_COND_SPECIES, SPECIES_WOBBUFFET), 1);
    EXPECT(!Quest_PartyMonMatches(0, PARTY_COND_SPECIES, SPECIES_WOBBUFFET));
}

TEST("Quest party condition ignores eggs")
{
    struct QuestCondition condition = { .type = QUEST_COND_PARTY, .op = PARTY_COND_SPECIES, .id = SPECIES_WOBBUFFET };
    bool8 isEgg = TRUE;

    CreateMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 5, 0, OTID_STRUCT_PLAYER_ID);
    SetMonData(&gParties[B_TRAINER_PLAYER][0], MON_DATA_IS_EGG, &isEgg);
    EXPECT(!Quest_EvaluateCondition(&condition));
}

TEST("Quest dex condition checks seen and caught")
{
    struct QuestCondition seen = { .type = QUEST_COND_DEX, .op = QUEST_DEX_SEEN, .id = SPECIES_WOBBUFFET };
    struct QuestCondition caught = { .type = QUEST_COND_DEX, .op = QUEST_DEX_CAUGHT, .id = SPECIES_WOBBUFFET };

    EXPECT(!Quest_EvaluateCondition(&seen));
    EXPECT(!Quest_EvaluateCondition(&caught));

    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_WOBBUFFET), FLAG_SET_SEEN);
    EXPECT(Quest_EvaluateCondition(&seen));
    EXPECT(!Quest_EvaluateCondition(&caught));

    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_WOBBUFFET), FLAG_SET_CAUGHT);
    EXPECT(Quest_EvaluateCondition(&caught));
}

TEST("Quest flags_count condition counts set flags with x/y progress")
{
    static const u16 flags[] = { FLAG_UNUSED_0x020, FLAG_UNUSED_0x021, FLAG_UNUSED_0x022 };
    struct QuestCondition condition = { .type = QUEST_COND_FLAGS_COUNT, .list = flags, .listCount = ARRAY_COUNT(flags), .value = 2, .progress = TRUE };
    u16 current = 0, target = 0;

    FlagSet(FLAG_UNUSED_0x021);
    EXPECT(!Quest_EvaluateCondition(&condition));
    EXPECT(Quest_GetConditionProgress(&condition, &current, &target));
    EXPECT_EQ(current, 1);
    EXPECT_EQ(target, 2);

    FlagSet(FLAG_UNUSED_0x022);
    EXPECT(Quest_EvaluateCondition(&condition));
}

TEST("Quest dex_count condition counts the national and regional dex")
{
    struct QuestCondition national = { .type = QUEST_COND_DEX_COUNT, .op = QUEST_DEX_CAUGHT, .id = QUEST_DEX_NATIONAL, .value = 2, .progress = TRUE };
    struct QuestCondition regionalSeen = { .type = QUEST_COND_DEX_COUNT, .op = QUEST_DEX_SEEN, .id = QUEST_DEX_REGIONAL, .value = 1 };
    u16 current = 0, target = 0;

    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_WOBBUFFET), FLAG_SET_CAUGHT);
    EXPECT(!Quest_EvaluateCondition(&national));
    EXPECT(Quest_GetConditionProgress(&national, &current, &target));
    EXPECT_EQ(current, 1);

    GetSetPokedexFlag(SpeciesToNationalPokedexNum(SPECIES_WYNAUT), FLAG_SET_CAUGHT);
    EXPECT(Quest_EvaluateCondition(&national));

    EXPECT_EQ(Quest_EvaluateCondition(&regionalSeen), GetRegionalPokedexCount(FLAG_GET_SEEN) >= 1);
}

TEST("checkchosenmoncondition fails when nothing was chosen")
{
    CreateMon(&gParties[B_TRAINER_PLAYER][0], SPECIES_WOBBUFFET, 5, 0, OTID_STRUCT_PLAYER_ID);
    VarSet(VAR_0x8000, SPECIES_WOBBUFFET);

    VarSet(VAR_0x8004, 0);
    RUN_OVERWORLD_SCRIPT(
        checkchosenmoncondition PARTY_COND_SPECIES, VAR_0x8000;
    );
    EXPECT_EQ(VarGet(VAR_RESULT), TRUE);

    VarSet(VAR_0x8004, PARTY_NOTHING_CHOSEN);
    RUN_OVERWORLD_SCRIPT(
        checkchosenmoncondition PARTY_COND_SPECIES, VAR_0x8000;
    );
    EXPECT_EQ(VarGet(VAR_RESULT), FALSE);
}

TEST("checkpartycondition sets VAR_RESULT and the matching slot")
{
    CreateMon(&gParties[B_TRAINER_PLAYER][2], SPECIES_WOBBUFFET, 5, 0, OTID_STRUCT_PLAYER_ID);
    VarSet(VAR_0x8000, SPECIES_WOBBUFFET);

    RUN_OVERWORLD_SCRIPT(
        checkpartycondition PARTY_COND_SPECIES, VAR_0x8000;
    );
    EXPECT_EQ(VarGet(VAR_RESULT), TRUE);
    EXPECT_EQ(VarGet(VAR_0x8005), 2);
}

// *******************************
// Stage rules, latching and guidance.
// These use the example quests in src/data/quests and compile out if they are removed.

#if defined(QUEST_EXAMPLE_ERRAND) && defined(QUEST_EXAMPLE_DELIVERY)

TEST("Quest objective done bits latch after the condition turns false")
{
    ASSUME(Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->stages[0].objectives[0].condition.type == QUEST_COND_ITEM);
    ASSUME(Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->stages[0].objectives[0].condition.id == ITEM_POTION);

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    EXPECT(!Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 0));

    AddBagItem(ITEM_POTION, 3);
    Quest_CheckProgress();
    EXPECT(Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 0));

    RemoveBagItem(ITEM_POTION, 3);
    Quest_CheckProgress();
    EXPECT(Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 0));
}

TEST("Quest objectives only latch while visible")
{
    const struct QuestObjective *objective = &Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->stages[0].objectives[2];
    ASSUME(objective->reveal.type == QUEST_COND_FLAG);
    ASSUME(objective->condition.type == QUEST_COND_ITEM);

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    AddBagItem(objective->condition.id, objective->condition.value);
    Quest_CheckProgress();
    EXPECT(!Quest_IsObjectiveVisible(QUEST_EXAMPLE_ERRAND, 2));
    EXPECT(!Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 2));

    FlagSet(objective->reveal.id);
    Quest_CheckProgress();
    EXPECT(Quest_IsObjectiveVisible(QUEST_EXAMPLE_ERRAND, 2));
    EXPECT(Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 2));
}

TEST("setqueststage only moves forward and clears objective bits")
{
    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_DebugToggleObjective(QUEST_EXAMPLE_ERRAND, 0);
    EXPECT(Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 0));

    EXPECT(Quest_SetStage(QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_REPORT));
    EXPECT_EQ(Quest_GetStage(QUEST_EXAMPLE_ERRAND), STAGE_ERRAND_REPORT);
    EXPECT(!Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 0));

    EXPECT(!Quest_SetStage(QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_SUPPLIES));
    EXPECT_EQ(Quest_GetStage(QUEST_EXAMPLE_ERRAND), STAGE_ERRAND_REPORT);
}

TEST("Quest state changes follow the status rules")
{
    EXPECT(Quest_Unlock(QUEST_EXAMPLE_DELIVERY));
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_DELIVERY), QUEST_STATUS_AVAILABLE);
    EXPECT(!Quest_Unlock(QUEST_EXAMPLE_DELIVERY));

    EXPECT(Quest_Start(QUEST_EXAMPLE_DELIVERY));
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_DELIVERY), QUEST_STATUS_ACTIVE);
    EXPECT(!Quest_Start(QUEST_EXAMPLE_DELIVERY));
    EXPECT(!Quest_Unlock(QUEST_EXAMPLE_DELIVERY));

    EXPECT(Quest_Complete(QUEST_EXAMPLE_DELIVERY, OUTCOME_DELIVERY_DONE));
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_DELIVERY), QUEST_STATUS_COMPLETE);
    EXPECT_EQ(Quest_GetOutcome(QUEST_EXAMPLE_DELIVERY), OUTCOME_DELIVERY_DONE);
    EXPECT(!Quest_Close(QUEST_EXAMPLE_DELIVERY));
    EXPECT(Quest_IsUnread(QUEST_EXAMPLE_DELIVERY));
}

TEST("completequest closes listed quests but skips hidden ones")
{
    ASSUME(Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->outcomes[OUTCOME_ERRAND_SKIPPED].closesCount == 1);
    ASSUME(Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->outcomes[OUTCOME_ERRAND_SKIPPED].closes[0] == QUEST_EXAMPLE_DELIVERY);

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_Complete(QUEST_EXAMPLE_ERRAND, OUTCOME_ERRAND_SKIPPED);
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_DELIVERY), QUEST_STATUS_HIDDEN);
}

TEST("completequest closes listed quests that are open")
{
    Quest_Unlock(QUEST_EXAMPLE_DELIVERY);
    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_Complete(QUEST_EXAMPLE_ERRAND, OUTCOME_ERRAND_SKIPPED);
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_DELIVERY), QUEST_STATUS_CLOSED);
}

TEST("Tracked quest is untracked when it finishes")
{
    EXPECT(!Quest_SetTracked(QUEST_EXAMPLE_DELIVERY));
    Quest_Start(QUEST_EXAMPLE_DELIVERY);
    EXPECT(Quest_SetTracked(QUEST_EXAMPLE_DELIVERY));
    EXPECT_EQ(Quest_GetTracked(), QUEST_EXAMPLE_DELIVERY);
    Quest_Complete(QUEST_EXAMPLE_DELIVERY, OUTCOME_DELIVERY_DONE);
    EXPECT_EQ(Quest_GetTracked(), QUEST_NONE);
}

TEST("Quest_GetTrackedTarget follows objective, turn-in, giver priority")
{
    const struct Quest *quest = Quest_GetInfo(QUEST_EXAMPLE_ERRAND);
    u16 map;
    u8 localId;

    ASSUME(quest->stages[0].objectives[1].targetMap == MAP_OLDALE_TOWN);
    ASSUME(quest->stages[0].objectives[1].condition.type == QUEST_COND_FLAG);
    ASSUME(quest->stages[0].turnInCount != 0);
    ASSUME(quest->stages[STAGE_ERRAND_REPORT].turnInCount == 0);
    ASSUME(quest->giverCount != 0);

    EXPECT(!Quest_GetTrackedTarget(&map, &localId));

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_SetTracked(QUEST_EXAMPLE_ERRAND);
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, MAP_OLDALE_TOWN);

    // Objective done: falls back to the stage's turn-in
    FlagSet(quest->stages[0].objectives[1].condition.id);
    Quest_CheckProgress();
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, quest->stages[0].turnIns[0].map);
    EXPECT_EQ(localId, quest->stages[0].turnIns[0].localId);

    // Stage without a turn-in, objective target done: falls back to the giver
    Quest_SetStage(QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_REPORT);
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, quest->stages[STAGE_ERRAND_REPORT].objectives[0].targetMap);
    Quest_DebugToggleObjective(QUEST_EXAMPLE_ERRAND, 0);
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, quest->givers[0].map);
    EXPECT_EQ(localId, quest->givers[0].localId);
}

TEST("Quest markers use turn-in, available, target priority")
{
    const struct Quest *delivery = Quest_GetInfo(QUEST_EXAMPLE_DELIVERY);
    const struct QuestNpc *giver = &delivery->givers[0];
    u8 category = 0xFF;

    ASSUME(delivery->stages[0].turnIns[0].map == giver->map);
    ASSUME(delivery->stages[0].turnIns[0].localId == giver->localId);
    ASSUME(delivery->stages[0].objectives[0].condition.type == QUEST_COND_VAR);

    EXPECT_EQ(QuestMarkers_GetType(giver->localId, MAP_NUM(giver->map), MAP_GROUP(giver->map), &category), QUEST_MARKER_NONE);

    Quest_Unlock(QUEST_EXAMPLE_DELIVERY);
    EXPECT_EQ(QuestMarkers_GetType(giver->localId, MAP_NUM(giver->map), MAP_GROUP(giver->map), &category), QUEST_MARKER_AVAILABLE);
    EXPECT_EQ(category, delivery->category);

    Quest_Start(QUEST_EXAMPLE_DELIVERY);
    EXPECT_EQ(QuestMarkers_GetType(giver->localId, MAP_NUM(giver->map), MAP_GROUP(giver->map), &category), QUEST_MARKER_NONE);

    VarSet(delivery->stages[0].objectives[0].condition.id, delivery->stages[0].objectives[0].condition.value);
    Quest_CheckProgress();
    EXPECT_EQ(QuestMarkers_GetType(giver->localId, MAP_NUM(giver->map), MAP_GROUP(giver->map), &category), QUEST_MARKER_TURN_IN);
}

TEST("Quest markers show objective targets")
{
    const struct QuestObjective *objective = &Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->stages[STAGE_ERRAND_REPORT].objectives[0];
    u8 category = 0xFF;

    ASSUME(objective->targetLocalId != LOCALID_NONE);

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_SetStage(QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_REPORT);
    EXPECT_EQ(QuestMarkers_GetType(objective->targetLocalId, MAP_NUM(objective->targetMap), MAP_GROUP(objective->targetMap), &category), QUEST_MARKER_TARGET);

    Quest_DebugToggleObjective(QUEST_EXAMPLE_ERRAND, 0);
    EXPECT_EQ(QuestMarkers_GetType(objective->targetLocalId, MAP_NUM(objective->targetMap), MAP_GROUP(objective->targetMap), &category), QUEST_MARKER_NONE);
}

TEST("Loading a save fills last-seen progress without queueing toasts")
{
    const struct QuestCondition *condition = &Quest_GetInfo(QUEST_EXAMPLE_DELIVERY)->stages[0].objectives[0].condition;
    ASSUME(condition->type == QUEST_COND_VAR && condition->progress);
    ASSUME(condition->value > 4);

    Quest_Start(QUEST_EXAMPLE_DELIVERY);
    VarSet(condition->id, 3);

    // Simulate loading a save
    QuestToast_ClearQueue();
    Quest_InitProgressCache();
    Quest_CheckProgress();
    EXPECT_EQ(QuestToast_GetQueueCount(), 0);

    VarSet(condition->id, 4);
    Quest_CheckProgress();
    EXPECT_EQ(QuestToast_GetQueueCount(), 1);

    Quest_CheckProgress();
    EXPECT_EQ(QuestToast_GetQueueCount(), 1);
}

TEST("Quest script macros change and query quest state")
{
    RUN_OVERWORLD_SCRIPT(
        setvar VAR_RESULT, 0;
        startquest QUEST_EXAMPLE_ERRAND;
        setqueststage QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_REPORT;
        goto_if_quest_status QUEST_EXAMPLE_ERRAND, QUEST_STATUS_ACTIVE, Active;
        end;
    Active:
        goto_if_quest_stage_lt QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_REPORT, Fail;
        goto_if_quest_stage_eq QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_REPORT, StageOk;
        end;
    StageOk:
        completequest QUEST_EXAMPLE_ERRAND, OUTCOME_ERRAND_DONE;
        goto_if_quest_outcome QUEST_EXAMPLE_ERRAND, OUTCOME_ERRAND_DONE, Done;
        end;
    Done:
        setvar VAR_RESULT, 1;
        end;
    Fail:
        setvar VAR_RESULT, 2;
    );
    EXPECT_EQ(VarGet(VAR_RESULT), 1);
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_ERRAND), QUEST_STATUS_COMPLETE);
}

TEST("Quest_CountByStatus counts quests by category and status")
{
    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_Start(QUEST_EXAMPLE_DELIVERY);
    EXPECT_EQ(Quest_CountByStatus(QUEST_CATEGORY_ANY, QUEST_STATUS_ACTIVE), 2);
    EXPECT_EQ(Quest_CountByStatus(Quest_GetInfo(QUEST_EXAMPLE_DELIVERY)->category, QUEST_STATUS_ACTIVE),
              1 + (Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->category == Quest_GetInfo(QUEST_EXAMPLE_DELIVERY)->category));
}

TEST("Quest condition reads another quest's status, stage and outcome")
{
    struct QuestCondition started = { .type = QUEST_COND_QUEST, .op = QUEST_CMP_STATUS_GE, .id = QUEST_EXAMPLE_ERRAND, .value = QUEST_STATUS_ACTIVE };
    struct QuestCondition available = { .type = QUEST_COND_QUEST, .op = QUEST_CMP_STATUS_EQ, .id = QUEST_EXAMPLE_ERRAND, .value = QUEST_STATUS_AVAILABLE };
    struct QuestCondition atHelp = { .type = QUEST_COND_QUEST, .op = QUEST_CMP_STAGE_GE, .id = QUEST_EXAMPLE_ERRAND, .value = STAGE_ERRAND_HELP };
    struct QuestCondition done = { .type = QUEST_COND_QUEST, .op = QUEST_CMP_OUTCOME, .id = QUEST_EXAMPLE_ERRAND, .value = OUTCOME_ERRAND_DONE };

    EXPECT(!Quest_EvaluateCondition(&started));
    Quest_Unlock(QUEST_EXAMPLE_ERRAND);
    EXPECT(Quest_EvaluateCondition(&available));
    EXPECT(!Quest_EvaluateCondition(&started));

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    EXPECT(Quest_EvaluateCondition(&started));
    EXPECT(!Quest_EvaluateCondition(&atHelp));
    Quest_SetStage(QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_HELP);
    EXPECT(Quest_EvaluateCondition(&atHelp));
    EXPECT(!Quest_EvaluateCondition(&done));

    Quest_Complete(QUEST_EXAMPLE_ERRAND, OUTCOME_ERRAND_DONE);
    EXPECT(Quest_EvaluateCondition(&started));
    EXPECT(Quest_EvaluateCondition(&atHelp));
    EXPECT(Quest_EvaluateCondition(&done));
}

TEST("completeobjective ticks an objective once and only while active")
{
    ASSUME(Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->stages[STAGE_ERRAND_REPORT].objectives[0].condition.type == QUEST_COND_NONE);

    EXPECT(!Quest_CompleteObjective(QUEST_EXAMPLE_ERRAND, 0));
    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_SetStage(QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_REPORT);
    QuestToast_ClearQueue();

    EXPECT(Quest_CompleteObjective(QUEST_EXAMPLE_ERRAND, 0));
    EXPECT(Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 0));
    EXPECT_EQ(QuestToast_GetQueuedType(0), QUEST_TOAST_PROGRESS);
    EXPECT(!Quest_CompleteObjective(QUEST_EXAMPLE_ERRAND, 0));
    EXPECT(!Quest_CompleteObjective(QUEST_EXAMPLE_ERRAND, 1));
}

#endif

#if defined(QUEST_EXAMPLE_BRANCH)

TEST("startquest can begin at a later stage and path with one toast")
{
    QuestToast_ClearQueue();
    EXPECT(!Quest_StartAt(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_HANDED_IN, 0));
    EXPECT(Quest_StartAt(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_HANDED_IN, 1));
    EXPECT_EQ(Quest_GetStage(QUEST_EXAMPLE_BRANCH), STAGE_BRANCH_HANDED_IN);
    EXPECT_EQ(Quest_GetPath(QUEST_EXAMPLE_BRANCH), 1);
    EXPECT_EQ(QuestToast_GetQueueCount(), 1);
    EXPECT_EQ(QuestToast_GetQueuedType(0), QUEST_TOAST_STARTED);
}

TEST("setquestpath leaves the default path once and setqueststage stays on the path")
{
    EXPECT(!Quest_SetPath(QUEST_EXAMPLE_BRANCH, 1));
    Quest_Start(QUEST_EXAMPLE_BRANCH);
    EXPECT(!Quest_SetStage(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_HANDED_IN));

    EXPECT(Quest_SetPath(QUEST_EXAMPLE_BRANCH, 1));
    EXPECT(!Quest_SetPath(QUEST_EXAMPLE_BRANCH, 2));
    EXPECT(!Quest_SetStage(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_SEARCH));
    EXPECT(Quest_SetStage(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_HANDED_IN));
    EXPECT(Quest_SetStage(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_RETURN));
}

TEST("The journal lists only passed stages on the quest's path")
{
    Quest_Start(QUEST_EXAMPLE_BRANCH);
    Quest_SetPath(QUEST_EXAMPLE_BRANCH, 1);
    Quest_SetStage(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_RETURN);

    EXPECT(Quest_IsStagePassed(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_ASK));
    EXPECT(!Quest_IsStagePassed(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_SEARCH));
    EXPECT(Quest_IsStagePassed(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_HANDED_IN));
    EXPECT(!Quest_IsStagePassed(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_RETURN));

    Quest_Complete(QUEST_EXAMPLE_BRANCH, OUTCOME_BRANCH_DONE);
    EXPECT(Quest_IsStagePassed(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_RETURN));
    EXPECT(!Quest_IsStagePassed(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_SEARCH));
}

TEST("Every giver and every turn-in gets a marker")
{
    const struct Quest *quest = Quest_GetInfo(QUEST_EXAMPLE_BRANCH);
    const struct QuestStage *last = &quest->stages[STAGE_BRANCH_RETURN];
    u8 category;

    ASSUME(quest->giverCount == 2);
    ASSUME(last->turnInCount == 2);

    Quest_Unlock(QUEST_EXAMPLE_BRANCH);
    for (u32 i = 0; i < quest->giverCount; i++)
        EXPECT_EQ(QuestMarkers_GetType(quest->givers[i].localId, MAP_NUM(quest->givers[i].map), MAP_GROUP(quest->givers[i].map), &category), QUEST_MARKER_AVAILABLE);

    Quest_Start(QUEST_EXAMPLE_BRANCH);
    Quest_SetStage(QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_RETURN);
    Quest_CompleteObjective(QUEST_EXAMPLE_BRANCH, 0);
    for (u32 i = 0; i < last->turnInCount; i++)
        EXPECT_EQ(QuestMarkers_GetType(last->turnIns[i].localId, MAP_NUM(last->turnIns[i].map), MAP_GROUP(last->turnIns[i].map), &category), QUEST_MARKER_TURN_IN);
}

TEST("Quest script macros start partway, set a path and branch on it")
{
    RUN_OVERWORLD_SCRIPT(
        setvar VAR_RESULT, 0;
        startquest QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_ASK;
        setquestpath QUEST_EXAMPLE_BRANCH, 1;
        setqueststage QUEST_EXAMPLE_BRANCH, STAGE_BRANCH_HANDED_IN;
        completeobjective QUEST_EXAMPLE_BRANCH, 0;
        goto_if_objective_done QUEST_EXAMPLE_BRANCH, 0, BranchTicked;
        end;
    BranchTicked:
        goto_if_quest_path QUEST_EXAMPLE_BRANCH, 1, BranchOnPath;
        end;
    BranchOnPath:
        setvar VAR_RESULT, 1;
    );
    EXPECT_EQ(VarGet(VAR_RESULT), 1);
    EXPECT_EQ(Quest_GetStage(QUEST_EXAMPLE_BRANCH), STAGE_BRANCH_HANDED_IN);
}

#endif

#if defined(QUEST_EXAMPLE_ERRAND) && defined(QUEST_EXAMPLE_TASK_NURSE) && defined(QUEST_EXAMPLE_TASK_CLERK)

TEST("A task done before its parent starts is silent and counts once the parent's stage opens")
{
    const struct QuestNpc *giver = &Quest_GetInfo(QUEST_EXAMPLE_TASK_NURSE)->givers[0];
    const struct QuestCondition *counted = &Quest_GetInfo(QUEST_EXAMPLE_ERRAND)->stages[STAGE_ERRAND_HELP].objectives[0].condition;
    u8 category;
    u16 current, target;

    ASSUME(Quest_IsTask(QUEST_EXAMPLE_TASK_NURSE));
    ASSUME(counted->type == QUEST_COND_QUESTS_COMPLETE && counted->value == 1);

    QuestToast_ClearQueue();
    Quest_Unlock(QUEST_EXAMPLE_TASK_NURSE);
    EXPECT_EQ(QuestMarkers_GetType(giver->localId, MAP_NUM(giver->map), MAP_GROUP(giver->map), &category), QUEST_MARKER_NONE);
    Quest_Start(QUEST_EXAMPLE_TASK_NURSE);
    Quest_Complete(QUEST_EXAMPLE_TASK_NURSE, 0);
    EXPECT_EQ(QuestToast_GetQueueCount(), 0);
    EXPECT(!Quest_HasUnread());

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_SetStage(QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_HELP);
    EXPECT(Quest_GetConditionProgress(counted, &current, &target));
    EXPECT_EQ(current, 1);
    EXPECT_EQ(target, 1);
    Quest_CheckProgress();
    EXPECT(Quest_IsObjectiveDone(QUEST_EXAMPLE_ERRAND, 0));
}

TEST("A task shows markers and a Task complete toast once its parent has started")
{
    const struct QuestNpc *giver = &Quest_GetInfo(QUEST_EXAMPLE_TASK_CLERK)->givers[0];
    u8 category;

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_Unlock(QUEST_EXAMPLE_TASK_CLERK);
    EXPECT_EQ(QuestMarkers_GetType(giver->localId, MAP_NUM(giver->map), MAP_GROUP(giver->map), &category), QUEST_MARKER_AVAILABLE);

    QuestToast_ClearQueue();
    RUN_OVERWORLD_SCRIPT(
        startquest QUEST_EXAMPLE_TASK_CLERK;
        completetask QUEST_EXAMPLE_TASK_CLERK;
    );
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_TASK_CLERK), QUEST_STATUS_COMPLETE);
    EXPECT_EQ(QuestToast_GetQueueCount(), 1);
    EXPECT_EQ(QuestToast_GetQueuedType(0), QUEST_TOAST_TASK_COMPLETE);
}

#endif

// *******************************
// Notes, leads and profiles. These use the example notes in src/data/notes.

#if defined(NOTE_EXAMPLE_FLYER_RUMOUR) && defined(NOTE_EXAMPLE_BIRCH_FIELDWORK) && defined(QUEST_EXAMPLE_DELIVERY)

TEST("takenote learns a note once and marks the character's profile unread")
{
    u32 subject = QuestNote_GetInfo(NOTE_EXAMPLE_BIRCH_FIELDWORK)->subject;

    ASSUME(subject != SUBJECT_NONE);
    ASSUME(!QuestNote_IsLead(NOTE_EXAMPLE_BIRCH_FIELDWORK));

    EXPECT(!QuestSubject_IsKnown(subject));
    QuestToast_ClearQueue();
    EXPECT(QuestNote_Take(NOTE_EXAMPLE_BIRCH_FIELDWORK));
    EXPECT(!QuestNote_Take(NOTE_EXAMPLE_BIRCH_FIELDWORK));
    EXPECT(QuestNote_IsKnown(NOTE_EXAMPLE_BIRCH_FIELDWORK));
    EXPECT(QuestSubject_IsKnown(subject));
    EXPECT(QuestSubject_IsUnread(subject));
    EXPECT(Quest_HasUnread());
    EXPECT_EQ(QuestToast_GetQueueCount(), 1);
    EXPECT_EQ(QuestToast_GetQueuedType(0), QUEST_TOAST_PROFILE);

    QuestSubject_ClearUnread(subject);
    EXPECT(!Quest_HasUnread());
}

TEST("A lead resolves on its condition and cannot get stuck")
{
    ASSUME(QuestNote_IsLead(NOTE_EXAMPLE_FLYER_RUMOUR));
    ASSUME(QuestNote_GetInfo(NOTE_EXAMPLE_FLYER_RUMOUR)->resolvedBy.id == QUEST_EXAMPLE_DELIVERY);

    QuestToast_ClearQueue();
    EXPECT(!QuestNote_IsOpenLead(NOTE_EXAMPLE_FLYER_RUMOUR));
    QuestNote_Take(NOTE_EXAMPLE_FLYER_RUMOUR);
    EXPECT(QuestNote_IsOpenLead(NOTE_EXAMPLE_FLYER_RUMOUR));
    EXPECT_EQ(QuestToast_GetQueuedType(0), QUEST_TOAST_NEW_LEAD);

    // Closed counts as resolved too, so the lead never gets stuck
    Quest_Unlock(QUEST_EXAMPLE_DELIVERY);
    EXPECT(QuestNote_IsOpenLead(NOTE_EXAMPLE_FLYER_RUMOUR));
    Quest_Close(QUEST_EXAMPLE_DELIVERY);
    EXPECT(!QuestNote_IsOpenLead(NOTE_EXAMPLE_FLYER_RUMOUR));
}

TEST("A lead that is already resolved gives no New lead toast")
{
    ASSUME(QuestNote_GetInfo(NOTE_EXAMPLE_FLYER_RUMOUR)->subject == SUBJECT_NONE);

    Quest_Start(QUEST_EXAMPLE_DELIVERY);
    QuestToast_ClearQueue();
    QuestNote_Take(NOTE_EXAMPLE_FLYER_RUMOUR);
    EXPECT(QuestNote_IsKnown(NOTE_EXAMPLE_FLYER_RUMOUR));
    EXPECT_EQ(QuestToast_GetQueueCount(), 0);
}

TEST("A pinned lead shares the Town Map pin with the tracked quest")
{
    const struct QuestNote *note = QuestNote_GetInfo(NOTE_EXAMPLE_FLYER_RUMOUR);
    u16 map;
    u8 localId;

    EXPECT(!QuestNote_SetTracked(NOTE_EXAMPLE_FLYER_RUMOUR));
    QuestNote_Take(NOTE_EXAMPLE_FLYER_RUMOUR);
    EXPECT(QuestNote_SetTracked(NOTE_EXAMPLE_FLYER_RUMOUR));
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, note->targetMap);
    EXPECT_EQ(localId, note->targetLocalId);

    // Tracking a quest unpins the lead
    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_SetTracked(QUEST_EXAMPLE_ERRAND);
    EXPECT_EQ(QuestNote_GetTracked(), NOTE_NONE);

    // A lead stops being pinned once it resolves
    QuestNote_SetTracked(NOTE_EXAMPLE_FLYER_RUMOUR);
    EXPECT_EQ(Quest_GetTracked(), QUEST_NONE);
    Quest_Start(QUEST_EXAMPLE_DELIVERY);
    EXPECT_EQ(QuestNote_GetTracked(), NOTE_NONE);
}

TEST("takenote and goto_if_note_known work from scripts")
{
    RUN_OVERWORLD_SCRIPT(
        setvar VAR_RESULT, 0;
        goto_if_note_known NOTE_EXAMPLE_BIRCH_FIELDWORK, NoteKnownTooEarly;
        takenote NOTE_EXAMPLE_BIRCH_FIELDWORK;
        goto_if_note_known NOTE_EXAMPLE_BIRCH_FIELDWORK, NoteKnown;
        end;
    NoteKnown:
        setvar VAR_RESULT, 1;
        end;
    NoteKnownTooEarly:
        setvar VAR_RESULT, 2;
    );
    EXPECT_EQ(VarGet(VAR_RESULT), 1);
}

#endif

#if QUEST_NPC_MARKERS && defined(QUEST_EXAMPLE_TASK_CLERK)

TEST("A marker sprite is created above its NPC and follows it")
{
    const struct QuestNpc *giver = &Quest_GetInfo(QUEST_EXAMPLE_TASK_CLERK)->givers[0];
    struct ObjectEvent *objectEvent = &gObjectEvents[1];
    u32 markerId = MAX_SPRITES;
    u8 npcSpriteId;

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_Unlock(QUEST_EXAMPLE_TASK_CLERK);
    FreeAllSpritePalettes();
    ResetSpriteData();

    npcSpriteId = CreateObjectGraphicsSprite(OBJ_EVENT_GFX_MART_EMPLOYEE, SpriteCallbackDummy, 40, 60, 0);
    memset(objectEvent, 0, sizeof(*objectEvent));
    objectEvent->active = TRUE;
    objectEvent->localId = giver->localId;
    objectEvent->mapNum = MAP_NUM(giver->map);
    objectEvent->mapGroup = MAP_GROUP(giver->map);
    objectEvent->spriteId = npcSpriteId;

    QuestMarkers_OnObjectSpawn(1);
    for (u32 i = 0; i < MAX_SPRITES; i++)
    {
        if (gSprites[i].inUse && i != npcSpriteId)
            markerId = i;
    }
    EXPECT_NE(markerId, MAX_SPRITES);

    gSprites[npcSpriteId].x = 56;
    AnimateSprites();
    EXPECT(gSprites[markerId].inUse);
    EXPECT_EQ(gSprites[markerId].x, 56);
    EXPECT_LT(gSprites[markerId].y, gSprites[npcSpriteId].y + gSprites[npcSpriteId].centerToCornerVecY);
    // The marker's palette is really loaded, not a fallback slot
    EXPECT_NE(GetSpritePaletteTagByPaletteNum((u32)gSprites[markerId].oam.paletteNum), TAG_NONE);
}

#endif
