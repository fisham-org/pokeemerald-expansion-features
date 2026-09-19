#include "global.h"
#include "bg.h"
#include "event_data.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "overworld.h"
#include "palette.h"
#include "pokedex.h"
#include "pokemon.h"
#include "qr_share.h"
#include "qrcodegen.h"
#include "scanline_effect.h"
#include "script.h"
#include "script_menu.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/rgb.h"
#include "constants/script_menu.h"
#include "constants/songs.h"

#include "data/qr_share.h"

// Payload layout: see notes/feature-qr/qr-share.md. tools/qr_share/web/decode.js must mirror it.

// Bits needed to store values 0..(n - 1).
#define BITS_FOR(n) ((n) <= 2 ? 1 : (n) <= 4 ? 2 : (n) <= 8 ? 3 : (n) <= 16 ? 4 : (n) <= 32 ? 5 \
                   : (n) <= 64 ? 6 : (n) <= 128 ? 7 : (n) <= 256 ? 8 : (n) <= 512 ? 9 : (n) <= 1024 ? 10 \
                   : (n) <= 2048 ? 11 : (n) <= 4096 ? 12 : (n) <= 8192 ? 13 : (n) <= 16384 ? 14 : (n) <= 32768 ? 15 : 16)

#define BITS_SPECIES    BITS_FOR(NUM_SPECIES)
#define BITS_ITEM       BITS_FOR(ITEMS_COUNT)
#define BITS_MOVE       BITS_FOR(MOVES_COUNT)
#define BITS_ABILITY    BITS_FOR(ABILITIES_COUNT)
#define BITS_DEX_COUNT  BITS_FOR(NATIONAL_DEX_COUNT + 1)

#define PAYLOAD_MAX_BYTES 241

// Largest possible payload: custom version string, full-length names and every var at 16 bits.
#define HEADER_MAX_BITS (8 + 1 + 4 + 15 * 6 + 3 + PLAYER_NAME_LENGTH * 8 + 1 + 16 + 8 + BITS_DEX_COUNT * 2 + 10 + 6 + 5 + 3)
#define MON_MAX_BITS    (BITS_SPECIES + 7 + BITS_ITEM + BITS_ABILITY + MAX_MON_MOVES * BITS_MOVE + 5 + 1 + 2 + 1 + 4 + POKEMON_NAME_LENGTH * 8)
#define DEBUG_MAX_BITS  (1 + (QR_SHARE_INCLUDE_DEBUG_DATA ? ARRAY_COUNT(sQrShareFlags) + ARRAY_COUNT(sQrShareVars) * 16 : 0))

STATIC_ASSERT(HEADER_MAX_BITS + PARTY_SIZE * MON_MAX_BITS + DEBUG_MAX_BITS <= PAYLOAD_MAX_BYTES * 8, QrSharePayloadTooLarge);
// Base URL + the largest possible payload (5 bits per base32 character) + NUL must fit the URL buffer.
STATIC_ASSERT(sizeof(QR_SHARE_BASE_URL) + (PAYLOAD_MAX_BYTES * 8 + 4) / 5 <= QR_SHARE_URL_MAX, QrShareBaseUrlTooLong);
STATIC_ASSERT(sizeof(QR_SHARE_VERSION_STRING) - 1 <= 15, QrShareVersionStringTooLong);
STATIC_ASSERT(ARRAY_COUNT(sQrShareFlags) <= 64, QrShareTooManyFlags);
STATIC_ASSERT(ARRAY_COUNT(sQrShareVars) <= 32, QrShareTooManyVars);
STATIC_ASSERT(ARRAY_COUNT(sQrShareBackgrounds) <= 32, QrShareTooManyBackgrounds);

struct BitWriter
{
    u8 *buffer;
    u32 bitPos;
};

static const char sBase32Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
// 6 bits per character. Must match VERSION_ALPHABET in tools/qr_share/web/decode.js.
static const char sVersionAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-+ ";

static void WriteBits(struct BitWriter *writer, u32 value, u32 bits)
{
    while (bits-- && writer->bitPos < PAYLOAD_MAX_BYTES * 8)
    {
        if ((value >> bits) & 1)
            writer->buffer[writer->bitPos >> 3] |= 0x80 >> (writer->bitPos & 7);
        writer->bitPos++;
    }
}

static void WriteString(struct BitWriter *writer, const u8 *str, u32 lengthBits)
{
    u32 i, length = StringLength(str);

    WriteBits(writer, length, lengthBits);
    for (i = 0; i < length; i++)
        WriteBits(writer, str[i], 8);
}

// Returns the character's index in sVersionAlphabet, or the index of the space if it isn't there.
u32 QrShare_GetVersionCharIndex(char c)
{
    u32 i;

    if (c >= 'a' && c <= 'z')
        c -= 'a' - 'A';
    for (i = 0; i < ARRAY_COUNT(sVersionAlphabet) - 1; i++)
    {
        if (sVersionAlphabet[i] == c)
            return i;
    }
    return ARRAY_COUNT(sVersionAlphabet) - 2;
}

static void WriteReleaseVersion(struct BitWriter *writer)
{
    static const char versionString[] = QR_SHARE_VERSION_STRING;
    u32 i;

    if (versionString[0] == '\0')
    {
        WriteBits(writer, 0, 1);
        WriteBits(writer, QR_SHARE_VERSION_MAJOR, 8);
        WriteBits(writer, QR_SHARE_VERSION_MINOR, 8);
        WriteBits(writer, QR_SHARE_VERSION_PATCH, 8);
    }
    else
    {
        WriteBits(writer, 1, 1);
        WriteBits(writer, ARRAY_COUNT(versionString) - 1, 4);
        for (i = 0; versionString[i] != '\0'; i++)
            WriteBits(writer, QrShare_GetVersionCharIndex(versionString[i]), 6);
    }
}

static u32 GetGenderId(struct Pokemon *mon)
{
    switch (GetMonGender(mon))
    {
    case MON_MALE:
        return 0;
    case MON_FEMALE:
        return 1;
    default:
        return 2;
    }
}

static void WriteHeader(struct BitWriter *writer, u32 partyCount)
{
    u32 i, badges = 0, background = 0;

    for (i = 0; i < NUM_BADGES; i++)
    {
        if (FlagGet(FLAG_BADGE01_GET + i))
            badges |= 1 << i;
    }
    if (QR_SHARE_VAR_BACKGROUND != 0)
        background = VarGet(QR_SHARE_VAR_BACKGROUND);

    WriteBits(writer, QR_SHARE_DATA_VERSION, 8);
    WriteReleaseVersion(writer);
    WriteString(writer, gSaveBlock2Ptr->playerName, 3);
    WriteBits(writer, gSaveBlock2Ptr->playerGender, 1);
    WriteBits(writer, gSaveBlock2Ptr->playerTrainerId[0] | (gSaveBlock2Ptr->playerTrainerId[1] << 8), 16);
    WriteBits(writer, badges, 8);
    WriteBits(writer, GetNationalPokedexCount(FLAG_GET_SEEN), BITS_DEX_COUNT);
    WriteBits(writer, GetNationalPokedexCount(FLAG_GET_CAUGHT), BITS_DEX_COUNT);
    WriteBits(writer, min(gSaveBlock2Ptr->playTimeHours, 999), 10);
    WriteBits(writer, gSaveBlock2Ptr->playTimeMinutes, 6);
    WriteBits(writer, background, 5);
    WriteBits(writer, partyCount, 3);
}

static void WriteMon(struct BitWriter *writer, struct Pokemon *mon)
{
    u32 i;
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    u8 nickname[POKEMON_NAME_LENGTH + 1];

    WriteBits(writer, species, BITS_SPECIES);
    WriteBits(writer, GetMonData(mon, MON_DATA_LEVEL), 7);
    WriteBits(writer, GetMonData(mon, MON_DATA_HELD_ITEM), BITS_ITEM);
    WriteBits(writer, GetMonAbility(mon), BITS_ABILITY);
    for (i = 0; i < MAX_MON_MOVES; i++)
        WriteBits(writer, GetMonData(mon, MON_DATA_MOVE1 + i), BITS_MOVE);
    WriteBits(writer, GetNature(mon), 5);
    WriteBits(writer, IsMonShiny(mon), 1);
    WriteBits(writer, GetGenderId(mon), 2);

    GetMonData(mon, MON_DATA_NICKNAME, nickname);
    if (StringCompare(nickname, GetSpeciesName(species)) != 0)
    {
        WriteBits(writer, 1, 1);
        WriteString(writer, nickname, 4);
    }
    else
    {
        WriteBits(writer, 0, 1);
    }
}

u32 QrShare_BuildUrl(char *dst)
{
    u8 payload[PAYLOAD_MAX_BYTES + 1] = {0}; // +1: the base32 loop reads one byte ahead.
    struct BitWriter writer = { payload, 0 };
    struct Pokemon *party[PARTY_SIZE];
    u32 i, partyCount = 0, length = 0;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SPECIES) != SPECIES_NONE
         && !GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_EGG))
            party[partyCount++] = &gParties[B_TRAINER_PLAYER][i];
    }

    WriteHeader(&writer, partyCount);
    for (i = 0; i < partyCount; i++)
        WriteMon(&writer, party[i]);
    WriteBits(&writer, QR_SHARE_INCLUDE_DEBUG_DATA, 1);
    if (QR_SHARE_INCLUDE_DEBUG_DATA)
    {
        for (i = 0; i < ARRAY_COUNT(sQrShareFlags); i++)
            WriteBits(&writer, FlagGet(sQrShareFlags[i]), 1);
        for (i = 0; i < ARRAY_COUNT(sQrShareVars); i++)
            WriteBits(&writer, VarGet(sQrShareVars[i].varId) & ((1 << sQrShareVars[i].bits) - 1), sQrShareVars[i].bits);
    }

    for (i = 0; QR_SHARE_BASE_URL[i] != '\0'; i++)
        dst[length++] = QR_SHARE_BASE_URL[i];

    // Base32: 5 bits per character, final character zero-padded.
    for (i = 0; i < writer.bitPos; i += 5)
    {
        u32 bits = (payload[i >> 3] << 8) | payload[(i >> 3) + 1];
        dst[length++] = sBase32Alphabet[(bits >> (11 - (i & 7))) & 0x1F];
    }
    dst[length] = '\0';
    return length;
}

static u32 AppendString(char *dst, u32 length, const char *str)
{
    while (*str != '\0' && length < QR_SHARE_URL_MAX - 1)
        dst[length++] = *str++;
    return length;
}

static u32 AppendNumber(char *dst, u32 length, u32 value)
{
    char digits[11];
    u32 i = ARRAY_COUNT(digits) - 1;

    digits[i] = '\0';
    do
    {
        digits[--i] = '0' + value % 10;
        value /= 10;
    } while (value != 0);
    return AppendString(dst, length, &digits[i]);
}

// Returns the placeholder's length if str starts with it, otherwise 0.
static u32 MatchPlaceholder(const char *str, const char *placeholder)
{
    u32 i;

    for (i = 0; placeholder[i] != '\0'; i++)
    {
        if (str[i] != placeholder[i])
            return 0;
    }
    return i;
}

u32 QrShare_BuildStaticUrl(char *dst, const char *template)
{
    u32 length = 0, matched;

    while (*template != '\0' && length < QR_SHARE_URL_MAX - 1)
    {
        if ((matched = MatchPlaceholder(template, "{MAJOR}")) != 0)
        {
            length = AppendNumber(dst, length, QR_SHARE_VERSION_MAJOR);
        }
        else if ((matched = MatchPlaceholder(template, "{MINOR}")) != 0)
        {
            length = AppendNumber(dst, length, QR_SHARE_VERSION_MINOR);
        }
        else if ((matched = MatchPlaceholder(template, "{PATCH}")) != 0)
        {
            length = AppendNumber(dst, length, QR_SHARE_VERSION_PATCH);
        }
        else if ((matched = MatchPlaceholder(template, "{VERSION}")) != 0)
        {
            if (QR_SHARE_VERSION_STRING[0] != '\0')
            {
                length = AppendString(dst, length, QR_SHARE_VERSION_STRING);
            }
            else
            {
                length = AppendNumber(dst, length, QR_SHARE_VERSION_MAJOR);
                length = AppendString(dst, length, ".");
                length = AppendNumber(dst, length, QR_SHARE_VERSION_MINOR);
                length = AppendString(dst, length, ".");
                length = AppendNumber(dst, length, QR_SHARE_VERSION_PATCH);
            }
        }
        else
        {
            dst[length++] = *template;
            matched = 1;
        }
        template += matched;
    }
    dst[length] = '\0';
    return length;
}

// Callnative for debug scripts; sets a party Pokemon's nickname.
// Usage: callnative QrShare_SetPartyMonNickname / .byte <slot> / .4byte <text>
void QrShare_SetPartyMonNickname(struct ScriptContext *ctx)
{
    u32 slot = ScriptReadByte(ctx);
    const u8 *nickname = (const u8 *)ScriptReadWord(ctx);

    SetMonData(&gParties[B_TRAINER_PLAYER][slot], MON_DATA_NICKNAME, nickname);
}

// Background picker: see QrShare_EventScript_ChooseBackground in data/scripts/qr_share.pory.

// Special; pushes each unlocked background for dynmultistack. VAR_0x8004 = current background.
void QrShare_PushUnlockedBackgrounds(void)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sQrShareBackgrounds); i++)
    {
        if (sQrShareBackgrounds[i].unlockFlag == 0 || FlagGet(sQrShareBackgrounds[i].unlockFlag))
        {
            struct ListMenuItem item;
            u8 *name = Alloc(StringLength(sQrShareBackgrounds[i].name) + 1);

            StringCopy(name, sQrShareBackgrounds[i].name);
            item.name = name;
            item.id = i;
            MultichoiceDynamic_PushElement(item);
        }
    }
    gSpecialVar_0x8004 = QR_SHARE_VAR_BACKGROUND != 0 ? VarGet(QR_SHARE_VAR_BACKGROUND) : 0;
}

// Special; stores the dynmultistack choice.
void QrShare_SetBackground(void)
{
    if (QR_SHARE_VAR_BACKGROUND != 0 && gSpecialVar_Result != MULTI_B_PRESSED)
        VarSet(QR_SHARE_VAR_BACKGROUND, gSpecialVar_Result);
}

// QR screen: one BG with the code on the left (up to 160x160) and instructions on the right.

#define QR_MAX_VERSION 13 // Largest version that fits 160 px at 2 px per module.
#define QR_QUIET_ZONE  4
#define QR_AREA_SIZE   160

enum
{
    WIN_QR,
    WIN_TEXT,
};

enum
{
    COLOR_BACKGROUND,
    COLOR_WHITE,
    COLOR_BLACK,
    COLOR_GRAY,
};

struct QrShareScreen
{
    MainCallback savedCallback;
    char url[QR_SHARE_URL_MAX];
    u8 qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
    u8 temp[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION)];
};

static EWRAM_DATA struct QrShareScreen *sQrShareScreen = NULL;

static const struct BgTemplate sQrShareBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .priority = 0,
    },
};

static const struct WindowTemplate sQrShareWindowTemplates[] =
{
    [WIN_QR] =
    {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = QR_AREA_SIZE / 8,
        .height = QR_AREA_SIZE / 8,
        .paletteNum = 0,
        .baseBlock = 1,
    },
    [WIN_TEXT] =
    {
        .bg = 0,
        .tilemapLeft = QR_AREA_SIZE / 8,
        .tilemapTop = 0,
        .width = (DISPLAY_WIDTH - QR_AREA_SIZE) / 8,
        .height = DISPLAY_HEIGHT / 8,
        .paletteNum = 0,
        .baseBlock = 1 + (QR_AREA_SIZE / 8) * (QR_AREA_SIZE / 8),
    },
    DUMMY_WIN_TEMPLATE
};

static const u16 sQrSharePalette[] =
{
    [COLOR_BACKGROUND] = RGB(3, 6, 12),
    [COLOR_WHITE]      = RGB_WHITE,
    [COLOR_BLACK]      = RGB_BLACK,
    [COLOR_GRAY]       = RGB(12, 14, 18),
};

static const u8 sQrShareTextColors[] = {COLOR_BACKGROUND, COLOR_WHITE, COLOR_GRAY};

static const u8 sText_QrShareTitle[] = _("Share");
static const u8 sText_QrShareInstructions[] = _("Scan with a QR\ncode reader to\nshare your team!"); // add your instructions, line is roughly 16 characters long
static const u8 sText_QrShareError[] = _("Code too\nlong to show.");
static const u8 sText_QrShareBack[] = _("{B_BUTTON} Back");

static void CB2_QrShareSetup(void);
static void CB2_QrShareMain(void);
static void VBlankCB_QrShare(void);
static void Task_QrShareWaitFadeIn(u8 taskId);
static void Task_QrShareInput(u8 taskId);
static void Task_QrShareExit(u8 taskId);

void QrShare_Open(MainCallback callback)
{
    sQrShareScreen = AllocZeroed(sizeof(*sQrShareScreen));
    if (sQrShareScreen == NULL)
    {
        SetMainCallback2(callback);
        return;
    }
    sQrShareScreen->savedCallback = callback;
    gMain.state = 0;
    SetMainCallback2(CB2_QrShareSetup);
}

static void Task_QrShareOpenFromScript(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        CleanupOverworldWindowsAndTilemaps();
        QrShare_Open(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        DestroyTask(taskId);
    }
}

// Special; follow with waitstate.
void QrShare_ShowScreen(void)
{
    FadeScreen(FADE_TO_BLACK, 0);
    CreateTask(Task_QrShareOpenFromScript, 0);
}

static void DrawQrCode(void)
{
    s32 x, y, size, scale, origin;

    FillWindowPixelBuffer(WIN_QR, PIXEL_FILL(COLOR_BACKGROUND));
    if (QR_SHARE_MODE == QR_SHARE_MODE_STATIC)
        QrShare_BuildStaticUrl(sQrShareScreen->url, QR_SHARE_STATIC_URL);
    else
        QrShare_BuildUrl(sQrShareScreen->url);
    if (!qrcodegen_encodeText(sQrShareScreen->url, sQrShareScreen->temp, sQrShareScreen->qrcode,
                              qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, QR_MAX_VERSION, qrcodegen_Mask_AUTO, TRUE))
    {
        AddTextPrinterParameterized4(WIN_QR, FONT_NORMAL, 8, 8, 0, 0, sQrShareTextColors, TEXT_SKIP_DRAW, sText_QrShareError);
        return;
    }

    size = qrcodegen_getSize(sQrShareScreen->qrcode);
    scale = QR_AREA_SIZE / (size + 2 * QR_QUIET_ZONE);
    origin = (QR_AREA_SIZE - (size + 2 * QR_QUIET_ZONE) * scale) / 2;

    FillWindowPixelRect(WIN_QR, COLOR_WHITE, origin, origin, (size + 2 * QR_QUIET_ZONE) * scale, (size + 2 * QR_QUIET_ZONE) * scale);
    origin += QR_QUIET_ZONE * scale;
    for (y = 0; y < size; y++)
    {
        for (x = 0; x < size; x++)
        {
            if (qrcodegen_getModule(sQrShareScreen->qrcode, x, y))
                FillWindowPixelRect(WIN_QR, COLOR_BLACK, origin + x * scale, origin + y * scale, scale, scale);
        }
    }
}

static void DrawText(void)
{
    FillWindowPixelBuffer(WIN_TEXT, PIXEL_FILL(COLOR_BACKGROUND));
    AddTextPrinterParameterized4(WIN_TEXT, FONT_NORMAL, 2, 4, 0, 0, sQrShareTextColors, TEXT_SKIP_DRAW, sText_QrShareTitle);
    AddTextPrinterParameterized4(WIN_TEXT, FONT_SMALL, 2, 24, 0, 0, sQrShareTextColors, TEXT_SKIP_DRAW, sText_QrShareInstructions);
    AddTextPrinterParameterized4(WIN_TEXT, FONT_SMALL, 2, DISPLAY_HEIGHT - 16, 0, 0, sQrShareTextColors, TEXT_SKIP_DRAW, sText_QrShareBack);
}

static void CB2_QrShareSetup(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankHBlankCallbacksToNull();
        ClearScheduledBgCopiesToVram();
        ScanlineEffect_Stop();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        gMain.state++;
        break;
    case 1:
        ResetAllBgsCoordinates();
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sQrShareBgTemplates, ARRAY_COUNT(sQrShareBgTemplates));
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP);
        InitWindows(sQrShareWindowTemplates);
        DeactivateAllTextPrinters();
        LoadPalette(sQrSharePalette, BG_PLTT_ID(0), sizeof(sQrSharePalette));
        gMain.state++;
        break;
    case 2:
        DrawQrCode();
        DrawText();
        PutWindowTilemap(WIN_QR);
        PutWindowTilemap(WIN_TEXT);
        CopyWindowToVram(WIN_QR, COPYWIN_FULL);
        CopyWindowToVram(WIN_TEXT, COPYWIN_FULL);
        ShowBg(0);
        gMain.state++;
        break;
    case 3:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        CreateTask(Task_QrShareWaitFadeIn, 0);
        SetVBlankCallback(VBlankCB_QrShare);
        SetMainCallback2(CB2_QrShareMain);
        break;
    }
}

static void CB2_QrShareMain(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void VBlankCB_QrShare(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void Task_QrShareWaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_QrShareInput;
}

static void Task_QrShareInput(u8 taskId)
{
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_QrShareExit;
    }
}

static void Task_QrShareExit(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(sQrShareScreen->savedCallback);
        FREE_AND_SET_NULL(sQrShareScreen);
        FreeAllWindowBuffers();
        DestroyTask(taskId);
    }
}
