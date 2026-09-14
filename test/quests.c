#include "global.h"
#include "event_data.h"
#include "item.h"
#include "pokedex.h"
#include "pokemon.h"
#include "quest.h"
#include "quest_guidance.h"
#include "quest_toast.h"
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
    EXPECT(Quest_AddLead(QUEST_EXAMPLE_DELIVERY));
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_DELIVERY), QUEST_STATUS_LEAD);
    EXPECT(!Quest_AddLead(QUEST_EXAMPLE_DELIVERY));

    EXPECT(Quest_Unlock(QUEST_EXAMPLE_DELIVERY));
    EXPECT_EQ(Quest_GetStatus(QUEST_EXAMPLE_DELIVERY), QUEST_STATUS_AVAILABLE);

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
    ASSUME(quest->stages[0].turnInMap != MAP_UNDEFINED);
    ASSUME(quest->stages[1].turnInMap == MAP_UNDEFINED);
    ASSUME(quest->giverMap != MAP_UNDEFINED);

    EXPECT(!Quest_GetTrackedTarget(&map, &localId));

    Quest_Start(QUEST_EXAMPLE_ERRAND);
    Quest_SetTracked(QUEST_EXAMPLE_ERRAND);
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, MAP_OLDALE_TOWN);

    // Objective done: falls back to the stage's turn-in
    FlagSet(quest->stages[0].objectives[1].condition.id);
    Quest_CheckProgress();
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, quest->stages[0].turnInMap);
    EXPECT_EQ(localId, quest->stages[0].turnInLocalId);

    // Stage without a turn-in, objective target done: falls back to the giver
    Quest_SetStage(QUEST_EXAMPLE_ERRAND, STAGE_ERRAND_REPORT);
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, quest->stages[1].objectives[0].targetMap);
    Quest_DebugToggleObjective(QUEST_EXAMPLE_ERRAND, 0);
    EXPECT(Quest_GetTrackedTarget(&map, &localId));
    EXPECT_EQ(map, quest->giverMap);
    EXPECT_EQ(localId, quest->giverLocalId);
}

TEST("Quest markers use turn-in, available, target priority")
{
    const struct Quest *delivery = Quest_GetInfo(QUEST_EXAMPLE_DELIVERY);
    u16 giverMap = delivery->giverMap;
    u8 category = 0xFF;

    ASSUME(delivery->stages[0].turnInMap == giverMap);
    ASSUME(delivery->stages[0].turnInLocalId == delivery->giverLocalId);
    ASSUME(delivery->stages[0].objectives[0].condition.type == QUEST_COND_VAR);

    EXPECT_EQ(QuestMarkers_GetType(delivery->giverLocalId, MAP_NUM(giverMap), MAP_GROUP(giverMap), &category), QUEST_MARKER_NONE);

    Quest_Unlock(QUEST_EXAMPLE_DELIVERY);
    EXPECT_EQ(QuestMarkers_GetType(delivery->giverLocalId, MAP_NUM(giverMap), MAP_GROUP(giverMap), &category), QUEST_MARKER_AVAILABLE);
    EXPECT_EQ(category, delivery->category);

    Quest_Start(QUEST_EXAMPLE_DELIVERY);
    EXPECT_EQ(QuestMarkers_GetType(delivery->giverLocalId, MAP_NUM(giverMap), MAP_GROUP(giverMap), &category), QUEST_MARKER_NONE);

    VarSet(delivery->stages[0].objectives[0].condition.id, delivery->stages[0].objectives[0].condition.value);
    Quest_CheckProgress();
    EXPECT_EQ(QuestMarkers_GetType(delivery->giverLocalId, MAP_NUM(giverMap), MAP_GROUP(giverMap), &category), QUEST_MARKER_TURN_IN);
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

#endif
