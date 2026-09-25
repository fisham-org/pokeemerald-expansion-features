#include "global.h"
#include "friend_code.h"
#include "event_data.h"
#include "test/test.h"
#include "constants/characters.h"

void FriendCode_CountFriends(void);

static const u8 sTestAlphabet[] = _("123456789ABCDEFGHJKMNPQRSTUVWXYZ");

static u32 AlphabetIndex(u8 c)
{
    u32 i;
    for (i = 0; sTestAlphabet[i] != c; i++)
        ;
    return i;
}

TEST("Friend code round-trips a full team")
{
    struct FriendCodeData in = {
        .title = 3,
        .trainerId = 54321,
        .species = {SPECIES_BULBASAUR, SPECIES_PIKACHU, SPECIES_GARCHOMP, SPECIES_EEVEE, SPECIES_MEW, SPECIES_PECHARUNT},
        .shinyFlags = 0x25,
    };
    struct FriendCodeData out = {0};
    u8 code[FRIEND_CODE_LENGTH + 1];
    u32 i;

    FriendCode_Encode(&in, code);
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_OK);
    EXPECT_EQ(out.title, in.title);
    EXPECT_EQ(out.trainerId, in.trainerId);
    EXPECT_EQ(out.shinyFlags, in.shinyFlags);
    for (i = 0; i < PARTY_SIZE; i++)
        EXPECT_EQ(out.species[i], in.species[i]);
}

TEST("Friend code round-trips a partial team and clears shiny flags on empty slots")
{
    struct FriendCodeData in = {
        .title = 0,
        .trainerId = 0,
        .species = {SPECIES_NONE, SPECIES_TORCHIC, SPECIES_NONE, SPECIES_NONE, SPECIES_NONE, SPECIES_NONE},
        .shinyFlags = 0x3F,
    };
    struct FriendCodeData out = {0};
    u8 code[FRIEND_CODE_LENGTH + 1];

    FriendCode_Encode(&in, code);
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_OK);
    EXPECT_EQ(out.species[0], SPECIES_NONE);
    EXPECT_EQ(out.species[1], SPECIES_TORCHIC);
    EXPECT_EQ(out.shinyFlags, 0x02);
}

TEST("Friend code decoding ignores hyphens, spaces, case, and reads I and L as 1")
{
    struct FriendCodeData in = {
        .title = 1,
        .trainerId = 11111,
        .species = {SPECIES_MUDKIP, SPECIES_ZIGZAGOON},
    };
    struct FriendCodeData out = {0};
    u8 code[FRIEND_CODE_LENGTH + 1];
    u8 messy[FRIEND_CODE_LENGTH * 2];
    u32 i, j = 0;

    FriendCode_Encode(&in, code);
    for (i = 0; i < FRIEND_CODE_LENGTH; i++)
    {
        u8 c = code[i];
        if (i == 5 || i == 15)
            messy[j++] = CHAR_HYPHEN;
        if (i == 10)
            messy[j++] = CHAR_SPACE;
        if (c == CHAR_1)
            c = (i % 2) ? CHAR_I : CHAR_l;
        else if (c >= CHAR_A && c <= CHAR_Z)
            c = c - CHAR_A + CHAR_a;
        messy[j++] = c;
    }
    messy[j] = EOS;

    EXPECT_EQ(FriendCode_Decode(messy, &out), FRIEND_CODE_DECODE_OK);
    EXPECT_EQ(out.trainerId, in.trainerId);
    EXPECT_EQ(out.species[1], SPECIES_ZIGZAGOON);
}

TEST("Friend code decoding rejects bad characters and lengths")
{
    struct FriendCodeData in = {.species = {SPECIES_MUDKIP}};
    struct FriendCodeData out = {0};
    u8 code[FRIEND_CODE_LENGTH + 2];

    FriendCode_Encode(&in, code);
    code[4] = CHAR_0;
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_BAD_CHARACTERS);

    FriendCode_Encode(&in, code);
    code[4] = CHAR_O;
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_BAD_CHARACTERS);

    FriendCode_Encode(&in, code);
    code[FRIEND_CODE_LENGTH - 1] = EOS;
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_BAD_CHARACTERS);

    FriendCode_Encode(&in, code);
    code[FRIEND_CODE_LENGTH] = CHAR_A;
    code[FRIEND_CODE_LENGTH + 1] = EOS;
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_BAD_CHARACTERS);
}

TEST("Friend code decoding rejects a wrong checksum")
{
    struct FriendCodeData in = {.title = 2, .trainerId = 4242, .species = {SPECIES_RALTS}};
    struct FriendCodeData out = {0};
    u8 code[FRIEND_CODE_LENGTH + 1];
    u32 last;

    FriendCode_Encode(&in, code);
    // The checksum is bits 98-99: the top 2 bits of the last character
    last = AlphabetIndex(code[FRIEND_CODE_LENGTH - 1]);
    code[FRIEND_CODE_LENGTH - 1] = sTestAlphabet[last ^ 0x18];
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_BAD_CHECKSUM);
}

TEST("Friend code decoding rejects titles and species out of range")
{
    struct FriendCodeData in = {.title = 127, .species = {SPECIES_RALTS}};
    struct FriendCodeData out = {0};
    u8 code[FRIEND_CODE_LENGTH + 1];

    FriendCode_Encode(&in, code);
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_BAD_TITLE);

    in.title = 0;
    in.species[1] = 2047;
    FriendCode_Encode(&in, code);
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_BAD_SPECIES);
}

TEST("Friend code decoding removes battle-only forms and rejects empty teams")
{
    struct FriendCodeData in = {.species = {SPECIES_VENUSAUR_MEGA, SPECIES_PIKACHU}, .shinyFlags = 0x01};
    struct FriendCodeData out = {0};
    u8 code[FRIEND_CODE_LENGTH + 1];

    FriendCode_Encode(&in, code);
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_OK);
    EXPECT_EQ(out.species[0], SPECIES_NONE);
    EXPECT_EQ(out.species[1], SPECIES_PIKACHU);
    EXPECT_EQ(out.shinyFlags, 0);

    in.species[1] = SPECIES_NONE;
    FriendCode_Encode(&in, code);
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_EMPTY_TEAM);
}

TEST("Friend Safari species are distinct base forms in party order")
{
    u16 team[PARTY_SIZE] = {SPECIES_GARCHOMP, SPECIES_VAPOREON, SPECIES_JOLTEON, SPECIES_GIBLE, SPECIES_NONE, SPECIES_PIKACHU};
    u16 list[PARTY_SIZE] = {0};

    EXPECT_EQ(FriendSafari_BuildSpeciesList(team, list), 3);
    EXPECT_EQ(list[0], SPECIES_GIBLE);
    EXPECT_EQ(list[1], SPECIES_EEVEE);
    EXPECT_EQ(list[2], SPECIES_PICHU);
}

TEST("Friend code decodes known codes")
{
    struct FriendCodeData out = {0};

    EXPECT_EQ(FriendCode_Decode(COMPOUND_STRING("UBW4G-RBG17-UGBZ7-ZDPK8"), &out), FRIEND_CODE_DECODE_OK);
    EXPECT_EQ(out.title, 0);
    EXPECT_EQ(out.trainerId, 11111);
    EXPECT_EQ(out.species[0], SPECIES_PIKACHU);
    EXPECT_EQ(out.species[5], SPECIES_LAPRAS);
    EXPECT_EQ(out.shinyFlags, 0x01);

    EXPECT_EQ(FriendCode_Decode(COMPOUND_STRING("BUU1Z-JNGS7-3DVY5-YYGDQ"), &out), FRIEND_CODE_DECODE_OK);
    EXPECT_EQ(out.title, 4);
    EXPECT_EQ(out.trainerId, 22222);
    EXPECT_EQ(out.species[3], SPECIES_BEEDRILL);
    EXPECT_EQ(out.species[4], SPECIES_NONE);

    EXPECT_EQ(FriendCode_Decode(COMPOUND_STRING("QCF6M-Z7S59-MVTY4-F917E"), &out), FRIEND_CODE_DECODE_OK);
    EXPECT_EQ(out.title, 11);
    EXPECT_EQ(out.trainerId, 33333);
    EXPECT_EQ(out.species[4], SPECIES_MILOTIC);
    EXPECT_EQ(out.shinyFlags, 0x10);

    EXPECT_EQ(FriendCode_Decode(COMPOUND_STRING("BBFBX-JDG58-3DVY5-YYGDQ"), &out), FRIEND_CODE_DECODE_OK);
    EXPECT_EQ(out.trainerId, 22222);
    EXPECT_EQ(out.species[0], SPECIES_VOLCARONA);
}

TEST("Friend list ignores and clears leftover save data")
{
    memset(gSaveBlock3Ptr->friends, 0, sizeof(gSaveBlock3Ptr->friends));
    // Leftover data from another feature's save, as seen in a real save file
    gSaveBlock3Ptr->friends[0].title = 34;
    gSaveBlock3Ptr->friends[0].species[0] = 34;
    gSaveBlock3Ptr->friends[0].species[1] = 8704;
    gSaveBlock3Ptr->friends[2].title = 3;
    gSaveBlock3Ptr->friends[2].species[4] = 243;

    FriendCode_CountFriends();
    EXPECT_EQ(gSpecialVar_Result, 0);
    EXPECT_EQ(gSaveBlock3Ptr->friends[0].title, 0);
    EXPECT_EQ(gSaveBlock3Ptr->friends[2].species[4], SPECIES_NONE);
}

TEST("Friend Safari leaves out legendary, mythical, Ultra Beast and Paradox Pokémon")
{
    ASSUME(FRIEND_SAFARI_BAN_RESTRICTED_LEGENDARY_ENCOUNTER && FRIEND_SAFARI_BAN_SUB_LEGENDARY_ENCOUNTER && FRIEND_SAFARI_BAN_MYTHICAL_ENCOUNTER);
    ASSUME(FRIEND_SAFARI_BAN_ULTRA_BEAST_ENCOUNTER && FRIEND_SAFARI_BAN_PARADOX_ENCOUNTER);
    u16 team[PARTY_SIZE] = {SPECIES_ARTICUNO, SPECIES_SOLGALEO, SPECIES_NIHILEGO, SPECIES_GREAT_TUSK, SPECIES_MELMETAL, SPECIES_RAICHU};
    u16 list[PARTY_SIZE] = {0};

    EXPECT_EQ(FriendSafari_BuildSpeciesList(team, list), 1);
    EXPECT_EQ(list[0], SPECIES_PICHU);
}

TEST("Friend code decoding rejects a team with no Friend Safari Pokémon")
{
    ASSUME(FRIEND_SAFARI_BAN_RESTRICTED_LEGENDARY_ENCOUNTER && FRIEND_SAFARI_BAN_ULTRA_BEAST_ENCOUNTER && FRIEND_SAFARI_BAN_PARADOX_ENCOUNTER);
    struct FriendCodeData in = {.species = {SPECIES_MEWTWO, SPECIES_KYOGRE, SPECIES_NIHILEGO, SPECIES_GREAT_TUSK}};
    struct FriendCodeData out = {0};
    u8 code[FRIEND_CODE_LENGTH + 1];

    FriendCode_Encode(&in, code);
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_NO_SAFARI_SPECIES);

    in.species[4] = SPECIES_TORCHIC;
    FriendCode_Encode(&in, code);
    EXPECT_EQ(FriendCode_Decode(code, &out), FRIEND_CODE_DECODE_OK);
}

TEST("Friend battle bans follow the config")
{
    EXPECT_EQ(FriendCode_IsBannedFromBattle(SPECIES_TORCHIC), FALSE);
    EXPECT_EQ(FriendCode_IsBannedFromBattle(SPECIES_ARTICUNO), FRIEND_SAFARI_BAN_SUB_LEGENDARY_BATTLE);
    EXPECT_EQ(FriendCode_IsBannedFromBattle(SPECIES_MEW), FRIEND_SAFARI_BAN_FRONTIER_BATTLE || FRIEND_SAFARI_BAN_MYTHICAL_BATTLE);
    EXPECT_EQ(FriendCode_IsBannedFromBattle(SPECIES_NIHILEGO), FRIEND_SAFARI_BAN_ULTRA_BEAST_BATTLE);
    EXPECT_EQ(FriendCode_IsBannedFromBattle(SPECIES_GREAT_TUSK), FRIEND_SAFARI_BAN_PARADOX_BATTLE);
}
