#include "global.h"
#include "event_data.h"
#include "pokedex.h"
#include "pokemon.h"
#include "qr_share.h"
#include "malloc.h"
#include "qrcodegen.h"
#include "string_util.h"
#include "test/test.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/moves.h"

// Must match GOLDEN_URL in tools/qr_share/test/decode.test.mjs, which decodes it and checks the fields.
static const char sGoldenUrl[] = QR_SHARE_BASE_URL "AEAIAAB4PO6TTAOIGABQAQDEIFB7A65QIEAEI4MIAAK24NP53YDJSBYOAAGUEEFUAAAAAV27O6";

static void SetupQrShareState(void)
{
    struct Pokemon *party = gParties[B_TRAINER_PLAYER];
    u32 item = ITEM_LEFTOVERS, move, shiny = TRUE;
    u8 nickname[POKEMON_NAME_LENGTH + 1];

    StringCopy(gSaveBlock2Ptr->playerName, COMPOUND_STRING("MAY"));
    gSaveBlock2Ptr->playerGender = FEMALE;
    gSaveBlock2Ptr->playerTrainerId[0] = 0x39;
    gSaveBlock2Ptr->playerTrainerId[1] = 0x30; // 12345
    gSaveBlock2Ptr->playTimeHours = 12;
    gSaveBlock2Ptr->playTimeMinutes = 34;

    FlagClear(FLAG_BADGE01_GET);
    FlagSet(FLAG_BADGE02_GET);
    FlagSet(FLAG_BADGE03_GET);
    FlagClear(FLAG_BADGE04_GET);
    FlagClear(FLAG_BADGE05_GET);
    FlagClear(FLAG_BADGE06_GET);
    FlagClear(FLAG_BADGE07_GET);
    FlagClear(FLAG_BADGE08_GET);

    memset(gSaveBlock1Ptr->dexSeen, 0, sizeof(gSaveBlock1Ptr->dexSeen));
    memset(gSaveBlock1Ptr->dexCaught, 0, sizeof(gSaveBlock1Ptr->dexCaught));
    GetSetPokedexFlag(NATIONAL_DEX_TREECKO, FLAG_SET_SEEN);
    GetSetPokedexFlag(NATIONAL_DEX_TREECKO, FLAG_SET_CAUGHT);
    GetSetPokedexFlag(NATIONAL_DEX_ZIGZAGOON, FLAG_SET_SEEN);
    GetSetPokedexFlag(NATIONAL_DEX_ZIGZAGOON, FLAG_SET_CAUGHT);
    GetSetPokedexFlag(NATIONAL_DEX_WURMPLE, FLAG_SET_SEEN);

    FlagClear(FLAG_TEMP_1);
    FlagSet(FLAG_TEMP_2);
    FlagSet(FLAG_TEMP_3);
    VarSet(VAR_TEMP_0, 2);
    VarSet(VAR_TEMP_1, 0xBEEF);
    VarSet(QR_SHARE_VAR_BACKGROUND, 1);

    ZeroPlayerPartyMons();

    // Slot 0: nicknamed, shiny, holding an item.
    CreateMon(&party[0], SPECIES_TREECKO, 15, 0x00000002, OTID_STRUCT_PRESET(0x12345678)); // personality % 25 = NATURE_BRAVE
    StringCopy(nickname, COMPOUND_STRING("LEAFY"));
    SetMonData(&party[0], MON_DATA_NICKNAME, nickname);
    SetMonData(&party[0], MON_DATA_HELD_ITEM, &item);
    SetMonData(&party[0], MON_DATA_IS_SHINY, &shiny);
    move = MOVE_POUND;       SetMonData(&party[0], MON_DATA_MOVE1, &move);
    move = MOVE_ABSORB;      SetMonData(&party[0], MON_DATA_MOVE2, &move);
    move = MOVE_QUICK_ATTACK; SetMonData(&party[0], MON_DATA_MOVE3, &move);
    move = MOVE_NONE;        SetMonData(&party[0], MON_DATA_MOVE4, &move);

    // Slot 1: egg, skipped.
    CreateMon(&party[1], SPECIES_WURMPLE, 5, 0, OTID_STRUCT_PRESET(0x12345678));
    shiny = TRUE;
    SetMonData(&party[1], MON_DATA_IS_EGG, &shiny);

    // Slot 2: plain, no nickname.
    CreateMon(&party[2], SPECIES_ZIGZAGOON, 7, 0x00000000, OTID_STRUCT_PRESET(0x12345678)); // NATURE_HARDY
    move = MOVE_TACKLE;      SetMonData(&party[2], MON_DATA_MOVE1, &move);
    move = MOVE_GROWL;       SetMonData(&party[2], MON_DATA_MOVE2, &move);
    move = MOVE_NONE;        SetMonData(&party[2], MON_DATA_MOVE3, &move);
    move = MOVE_NONE;        SetMonData(&party[2], MON_DATA_MOVE4, &move);
}

TEST("QR Share URL starts with the base URL and uses only base32 characters")
{
    char url[QR_SHARE_URL_MAX];
    u32 i, baseLength = sizeof(QR_SHARE_BASE_URL) - 1;
    u32 length;

    SetupQrShareState();
    length = QrShare_BuildUrl(url);

    EXPECT_LT(length, QR_SHARE_URL_MAX);
    EXPECT_EQ(memcmp(url, QR_SHARE_BASE_URL, baseLength), 0);
    for (i = baseLength; i < length; i++)
        EXPECT((url[i] >= 'A' && url[i] <= 'Z') || (url[i] >= '2' && url[i] <= '7'));
}

TEST("QR Share URL matches the golden URL")
{
    char url[QR_SHARE_URL_MAX];
    u32 length;

    SetupQrShareState();
    length = QrShare_BuildUrl(url);
    Test_MgbaPrintf("QR URL: %s", url);

    EXPECT_EQ(length, sizeof(sGoldenUrl) - 1);
    EXPECT_EQ(memcmp(url, sGoldenUrl, sizeof(sGoldenUrl)), 0);
}

struct QrBuffers
{
    char url[QR_SHARE_URL_MAX];
    u8 qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(13)];
    u8 temp[qrcodegen_BUFFER_LEN_FOR_VERSION(13)];
};

TEST("QR Share: a maximum-length URL fits the largest QR version the screen can show")
{
    struct QrBuffers *buffers = AllocZeroed(sizeof(*buffers));
    u32 i;

    for (i = 0; i < QR_SHARE_URL_MAX - 1; i++)
        buffers->url[i] = 'Z';

    EXPECT(qrcodegen_encodeText(buffers->url, buffers->temp, buffers->qrcode, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, 13, qrcodegen_Mask_AUTO, TRUE));
    Free(buffers);
}

TEST("QR Share version string only uses supported characters")
{
    static const char versionString[] = QR_SHARE_VERSION_STRING;
    u32 i;

    for (i = 0; versionString[i] != '\0'; i++)
        EXPECT(versionString[i] == ' ' || QrShare_GetVersionCharIndex(versionString[i]) != QrShare_GetVersionCharIndex(' '));
}

TEST("QR Share static URL fills in version placeholders and keeps other text")
{
    static const char expected[] = "https://x.dev/v1.0.0/1.0.0/{BAD}{MAJOR";
    char url[QR_SHARE_URL_MAX];
    u32 length;

    ASSUME(QR_SHARE_VERSION_MAJOR == 1 && QR_SHARE_VERSION_MINOR == 0 && QR_SHARE_VERSION_PATCH == 0);
    ASSUME(QR_SHARE_VERSION_STRING[0] == '\0');

    length = QrShare_BuildStaticUrl(url, "https://x.dev/v{MAJOR}.{MINOR}.{PATCH}/{VERSION}/{BAD}{MAJOR");

    EXPECT_EQ(length, sizeof(expected) - 1);
    EXPECT_EQ(memcmp(url, expected, sizeof(expected)), 0);
}

TEST("QR Share static URL is cut to fit the URL buffer")
{
    struct QrBuffers *buffers = AllocZeroed(sizeof(*buffers));
    char *template = AllocZeroed(QR_SHARE_URL_MAX * 2);
    u32 i;

    for (i = 0; i < QR_SHARE_URL_MAX * 2 - 1; i++)
        template[i] = (i % 2) ? '{' : 'A';

    EXPECT_EQ(QrShare_BuildStaticUrl(buffers->url, template), QR_SHARE_URL_MAX - 1);
    EXPECT_EQ(buffers->url[QR_SHARE_URL_MAX - 1], '\0');
    Free(template);
    Free(buffers);
}
