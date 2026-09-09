#include "global.h"
#include "battle_setup.h"
#include "data.h"
#include "follower_npc.h"
#include "pokemon.h"
#include "test/test.h"
#include "constants/battle_mode.h"

// Test builds exclude the real trainer table (see #if !TESTING in src/data.c), so
// gTrainers comes from test/battle/trainer_control.h and trainers are addressed by
// their numeric id there. These three cover the trainer shapes that
// ShouldTrainerBattleBeDouble branches on; the ASSUMPTIONS below fail loudly if
// upstream ever reshapes them.
#define TRAINER_SINGLES_MULTI_MON  3  // "Test1"
#define TRAINER_SINGLES_ONE_MON    1  // "RED"
#define TRAINER_DOUBLES            8  // "Test5"

ASSUMPTIONS
{
    ASSUME(OW_DOUBLE_APPROACH_WITH_ONE_MON == FALSE);
    ASSUME(FollowerNPCIsBattlePartner() == FALSE);
    ASSUME(GetTrainerBattleType(TRAINER_SINGLES_MULTI_MON) != TRAINER_BATTLE_TYPE_DOUBLES);
    ASSUME(GetTrainerPartySizeFromId(TRAINER_SINGLES_MULTI_MON) > 1);
    ASSUME(GetTrainerBattleType(TRAINER_SINGLES_ONE_MON) != TRAINER_BATTLE_TYPE_DOUBLES);
    ASSUME(GetTrainerPartySizeFromId(TRAINER_SINGLES_ONE_MON) == 1);
    ASSUME(GetTrainerBattleType(TRAINER_DOUBLES) == TRAINER_BATTLE_TYPE_DOUBLES);
    ASSUME(GetTrainerPartySizeFromId(TRAINER_DOUBLES) > 1);
}

// Gives the player healthyCount usable Pokémon followed by faintedCount at 0 HP.
static void SetPlayerParty(u32 healthyCount, u32 faintedCount)
{
    u32 zeroHp = 0;

    for (u32 i = 0; i < PARTY_SIZE; i++)
        ZeroMonData(&gParties[B_TRAINER_PLAYER][i]);

    for (u32 i = 0; i < healthyCount + faintedCount; i++)
    {
        CreateMon(&gParties[B_TRAINER_PLAYER][i], SPECIES_WOBBUFFET, 50, 0, OTID_STRUCT_PRESET(0));
        // CreateMon leaves HP at 0, which GetMonsStateToDoubles_2 reads as fainted.
        CalculateMonStats(&gParties[B_TRAINER_PLAYER][i]);
        if (i >= healthyCount)
            SetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_HP, &zeroHp);
    }

    gPartiesCount[B_TRAINER_PLAYER] = healthyCount + faintedCount;
}

// Loads a trainer battle the way the trainerbattle script command does, and returns
// whether the battle ended up as a double. TrainerBattleLoadArgs is where the player's
// battle mode preference is applied.
static bool32 LoadedAsDoubleBattle(u16 opponent, bool32 scriptRequestsDouble)
{
    TrainerBattleParameter params = {0};

    params.params.opponentA = opponent;
    params.params.isDoubleBattle = scriptRequestsDouble;
    TrainerBattleLoadArgs(params.data);

    return TRAINER_BATTLE_PARAM.isDoubleBattle;
}

TEST("(Battle mode) battle mode converts trainer battles")
{
    static const struct
    {
        const char *name;
        u8 battleMode;
        u16 opponent;
        bool8 scriptRequestsDouble;
        bool8 expectedDouble;
    } cases[] =
    {
        // BATTLE_MODE_SINGLES downgrades everything, including a trainer whose own data
        // says doubles and a script that explicitly asked for a double battle.
        {"singles mode, 1-mon trainer",                  BATTLE_MODE_SINGLES, TRAINER_SINGLES_ONE_MON,   FALSE, FALSE},
        {"singles mode, 1-mon trainer, script doubles",  BATTLE_MODE_SINGLES, TRAINER_SINGLES_ONE_MON,   TRUE,  FALSE},
        {"singles mode, singles trainer",                BATTLE_MODE_SINGLES, TRAINER_SINGLES_MULTI_MON, FALSE, FALSE},
        {"singles mode, singles trainer, script doubles",BATTLE_MODE_SINGLES, TRAINER_SINGLES_MULTI_MON, TRUE,  FALSE},
        {"singles mode, doubles trainer",                BATTLE_MODE_SINGLES, TRAINER_DOUBLES,           FALSE, FALSE},
        {"singles mode, doubles trainer, script doubles", BATTLE_MODE_SINGLES, TRAINER_DOUBLES,          TRUE,  FALSE},

        // BATTLE_MODE_DOUBLES promotes everything, except a trainer with a single
        // Pokémon, who cannot fill a double battle's two slots.
        {"doubles mode, 1-mon trainer",                  BATTLE_MODE_DOUBLES, TRAINER_SINGLES_ONE_MON,   FALSE, FALSE},
        {"doubles mode, 1-mon trainer, script doubles",  BATTLE_MODE_DOUBLES, TRAINER_SINGLES_ONE_MON,   TRUE,  FALSE},
        {"doubles mode, singles trainer",                BATTLE_MODE_DOUBLES, TRAINER_SINGLES_MULTI_MON, FALSE, TRUE},
        {"doubles mode, singles trainer, script doubles",BATTLE_MODE_DOUBLES, TRAINER_SINGLES_MULTI_MON, TRUE,  TRUE},
        {"doubles mode, doubles trainer",                BATTLE_MODE_DOUBLES, TRAINER_DOUBLES,           FALSE, TRUE},
        {"doubles mode, doubles trainer, script doubles", BATTLE_MODE_DOUBLES, TRAINER_DOUBLES,          TRUE,  TRUE},

        // BATTLE_MODE_MIXED keeps whatever the script and the trainer's data asked for,
        // which is the unmodified upstream behaviour.
        {"mixed mode, 1-mon trainer",                    BATTLE_MODE_MIXED,   TRAINER_SINGLES_ONE_MON,   FALSE, FALSE},
        {"mixed mode, 1-mon trainer, script doubles",    BATTLE_MODE_MIXED,   TRAINER_SINGLES_ONE_MON,   TRUE,  FALSE},
        {"mixed mode, singles trainer",                  BATTLE_MODE_MIXED,   TRAINER_SINGLES_MULTI_MON, FALSE, FALSE},
        {"mixed mode, singles trainer, script doubles",  BATTLE_MODE_MIXED,   TRAINER_SINGLES_MULTI_MON, TRUE,  TRUE},
        {"mixed mode, doubles trainer",                  BATTLE_MODE_MIXED,   TRAINER_DOUBLES,           FALSE, TRUE},
        {"mixed mode, doubles trainer, script doubles",  BATTLE_MODE_MIXED,   TRAINER_DOUBLES,           TRUE,  TRUE},
    };

    u32 caseIndex = 0;

    for (u32 i = 0; i < ARRAY_COUNT(cases); i++)
        PARAMETRIZE_LABEL("%s", cases[i].name) { caseIndex = i; }

    SetPlayerParty(2, 0);
    gSaveBlock2Ptr->battleMode = cases[caseIndex].battleMode;

    EXPECT_EQ(LoadedAsDoubleBattle(cases[caseIndex].opponent, cases[caseIndex].scriptRequestsDouble),
              cases[caseIndex].expectedDouble);
}

TEST("(Battle mode) a double battle requires two usable Pokémon")
{
    static const struct
    {
        const char *name;
        u8 healthyCount;
        u8 faintedCount;
        bool8 expectedDouble;
    } cases[] =
    {
        {"6 healthy",              6, 0, TRUE},
        {"2 healthy",              2, 0, TRUE},
        {"2 healthy, 1 fainted",   2, 1, TRUE},
        {"1 healthy",              1, 0, FALSE},
        {"1 healthy, 2 fainted",   1, 2, FALSE},
        {"0 healthy, 2 fainted",   0, 2, FALSE},
    };

    u32 caseIndex = 0;

    for (u32 i = 0; i < ARRAY_COUNT(cases); i++)
        PARAMETRIZE_LABEL("%s", cases[i].name) { caseIndex = i; }

    SetPlayerParty(cases[caseIndex].healthyCount, cases[caseIndex].faintedCount);

    // Forcing doubles must never leave the player stuck on the "not enough Pokémon" message,
    gSaveBlock2Ptr->battleMode = BATTLE_MODE_DOUBLES;
    EXPECT_EQ(LoadedAsDoubleBattle(TRAINER_SINGLES_MULTI_MON, FALSE), cases[caseIndex].expectedDouble);

    // and the same guard still protects an ordinary trainerbattle_double in mixed mode.
    gSaveBlock2Ptr->battleMode = BATTLE_MODE_MIXED;
    EXPECT_EQ(LoadedAsDoubleBattle(TRAINER_DOUBLES, TRUE), cases[caseIndex].expectedDouble);
}
