#include "global.h"
#include "friend_code.h"
#include "event_data.h"
#include "field_message_box.h"
#include "list_menu.h"
#include "main.h"
#include "malloc.h"
#include "naming_screen.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "script_menu.h"
#include "sound.h"
#include "string_util.h"
#include "text.h"
#include "constants/characters.h"
#include "constants/songs.h"

// A friend code is 100 bits, written as 20 base32 characters:
//   bits  0-1   format version
//   bits  2-8   title
//   bits  9-74  6 x species (11 bits each)
//   bits 75-80  shiny flags
//   bits 81-96  trainer ID
//   bit  97     reserved, must be 0
//   bits 98-99  checksum of bits 0-97
#define FRIEND_CODE_VERSION         0
#define FRIEND_CODE_BYTES           13
#define FRIEND_CODE_CHECKSUM_BIT    98

#define BITS_VERSION    2
#define BITS_TITLE      7
#define BITS_SPECIES    11
#define BITS_SHINY      PARTY_SIZE
#define BITS_TRAINER_ID 16
#define BITS_RESERVED   1
#define BITS_CHECKSUM   2
#define BITS_PER_CHAR   5

#define CODE_PARTS          4
#define CODE_PART_LENGTH    (FRIEND_CODE_LENGTH / CODE_PARTS)

#include "data/friend_code_titles.h"

#define FRIEND_TITLE_COUNT ARRAY_COUNT(gFriendTitles)

STATIC_ASSERT(FRIEND_TITLE_COUNT <= (1 << BITS_TITLE), FriendCodeTooManyTitles);
STATIC_ASSERT(NUM_SPECIES <= (1 << BITS_SPECIES), FriendCodeTooManySpecies);
STATIC_ASSERT(FRIEND_CODE_CHECKSUM_BIT + BITS_CHECKSUM == FRIEND_CODE_LENGTH * BITS_PER_CHAR, FriendCodeBitCount);
STATIC_ASSERT(sizeof(struct FriendRecord) == 24, FriendRecordSize);
STATIC_ASSERT(FRIEND_CODE_MAX_FRIENDS > 0 && FRIEND_CODE_MAX_FRIENDS < 256, FriendCodeMaxFriends);

// Base32 without 0, O, I or L, so no two characters look alike
static const u8 sAlphabet[] = _("123456789ABCDEFGHJKMNPQRSTUVWXYZ");

// XORed over bits 0-97 so that codes don't look patterned. Bits 98+ must be 0.
static const u8 sKeystream[FRIEND_CODE_BYTES] = {0x5A, 0xC3, 0x91, 0x2E, 0x7B, 0xE4, 0x08, 0xB6, 0x3D, 0xF1, 0x62, 0x9C, 0x03};

// Script state between the code entry, name entry and save steps
EWRAM_DATA static u8 sCodeInput[CODE_PARTS][CODE_PART_LENGTH + 1] = {0};
EWRAM_DATA static u8 sCodePart = 0;
EWRAM_DATA static u8 sNameInput[PLAYER_NAME_LENGTH + 1] = {0};
EWRAM_DATA static struct FriendCodeData sPendingFriend = {0};

// Friend Safari species, cached for the friend slot in sSafariCachedVar
EWRAM_DATA static u16 sSafariSpecies[PARTY_SIZE] = {0};
EWRAM_DATA static u8 sSafariSpeciesCount = 0;
EWRAM_DATA static u8 sSafariCachedVar = 0;

static void CB2_ReturnFromCodePart(void);

static void WriteBits(u8 *bytes, u32 *pos, u32 value, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++, (*pos)++)
    {
        if (value & (1 << i))
            bytes[*pos / 8] |= 1 << (*pos % 8);
        else
            bytes[*pos / 8] &= ~(1 << (*pos % 8));
    }
}

static u32 ReadBits(const u8 *bytes, u32 *pos, u32 count)
{
    u32 i, value = 0;
    for (i = 0; i < count; i++, (*pos)++)
    {
        if (bytes[*pos / 8] & (1 << (*pos % 8)))
            value |= 1 << i;
    }
    return value;
}

// FNV-1a over bits 0-97, folded to 2 bits
static u32 CalcChecksum(const u8 *bytes)
{
    u32 i, hash = 2166136261u;
    for (i = 0; i < FRIEND_CODE_BYTES; i++)
    {
        u8 byte = bytes[i];
        if (i == FRIEND_CODE_BYTES - 1)
            byte &= (1 << (FRIEND_CODE_CHECKSUM_BIT % 8)) - 1;
        hash = (hash ^ byte) * 16777619u;
    }
    hash ^= (hash >> 16);
    hash ^= (hash >> 8);
    hash ^= (hash >> 4);
    hash ^= (hash >> 2);
    return hash & ((1 << BITS_CHECKSUM) - 1);
}

static void ApplyKeystream(u8 *bytes)
{
    u32 i;
    for (i = 0; i < FRIEND_CODE_BYTES; i++)
        bytes[i] ^= sKeystream[i];
}

// Returns the 5-bit value of a code character, or -1 if it isn't one
static s32 CharToValue(u8 c)
{
    u32 i;

    if (c >= CHAR_a && c <= CHAR_z)
        c = c - CHAR_a + CHAR_A;
    if (c == CHAR_I || c == CHAR_L)
        c = CHAR_1;

    for (i = 0; i < ARRAY_COUNT(sAlphabet) - 1; i++)
    {
        if (sAlphabet[i] == c)
            return i;
    }
    return -1;
}

static bool32 IsBattleOnlyForm(enum Species species)
{
    return gSpeciesInfo[species].isMegaEvolution
        || gSpeciesInfo[species].isPrimalReversion
        || gSpeciesInfo[species].isUltraBurst
        || gSpeciesInfo[species].isGigantamax
        || gSpeciesInfo[species].isTeraForm;
}

// Set by the FRIEND_SAFARI_BAN_*_ENCOUNTER configs
static bool32 IsSafariBanned(enum Species species)
{
    return (FRIEND_SAFARI_BAN_FRONTIER_ENCOUNTER && gSpeciesInfo[species].isFrontierBanned)
        || (FRIEND_SAFARI_BAN_RESTRICTED_LEGENDARY_ENCOUNTER && gSpeciesInfo[species].isRestrictedLegendary)
        || (FRIEND_SAFARI_BAN_SUB_LEGENDARY_ENCOUNTER && gSpeciesInfo[species].isSubLegendary)
        || (FRIEND_SAFARI_BAN_MYTHICAL_ENCOUNTER && gSpeciesInfo[species].isMythical)
        || (FRIEND_SAFARI_BAN_ULTRA_BEAST_ENCOUNTER && gSpeciesInfo[species].isUltraBeast)
        || (FRIEND_SAFARI_BAN_PARADOX_ENCOUNTER && gSpeciesInfo[species].isParadox);
}

// Set by the FRIEND_SAFARI_BAN_*_BATTLE configs
bool32 FriendCode_IsBannedFromBattle(enum Species species)
{
    return (FRIEND_SAFARI_BAN_FRONTIER_BATTLE && gSpeciesInfo[species].isFrontierBanned)
        || (FRIEND_SAFARI_BAN_RESTRICTED_LEGENDARY_BATTLE && gSpeciesInfo[species].isRestrictedLegendary)
        || (FRIEND_SAFARI_BAN_SUB_LEGENDARY_BATTLE && gSpeciesInfo[species].isSubLegendary)
        || (FRIEND_SAFARI_BAN_MYTHICAL_BATTLE && gSpeciesInfo[species].isMythical)
        || (FRIEND_SAFARI_BAN_ULTRA_BEAST_BATTLE && gSpeciesInfo[species].isUltraBeast)
        || (FRIEND_SAFARI_BAN_PARADOX_BATTLE && gSpeciesInfo[species].isParadox);
}

static bool32 HasBattleSpecies(const u16 *team)
{
    u32 i;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (team[i] != SPECIES_NONE && !FriendCode_IsBannedFromBattle(team[i]))
            return TRUE;
    }
    return FALSE;
}

// Writes FRIEND_CODE_LENGTH characters and an EOS to dest
void FriendCode_Encode(const struct FriendCodeData *data, u8 *dest)
{
    u8 bytes[FRIEND_CODE_BYTES] = {0};
    u32 i, pos = 0, checksum;

    WriteBits(bytes, &pos, FRIEND_CODE_VERSION, BITS_VERSION);
    WriteBits(bytes, &pos, data->title, BITS_TITLE);
    for (i = 0; i < PARTY_SIZE; i++)
        WriteBits(bytes, &pos, data->species[i], BITS_SPECIES);
    WriteBits(bytes, &pos, data->shinyFlags, BITS_SHINY);
    WriteBits(bytes, &pos, data->trainerId, BITS_TRAINER_ID);
    WriteBits(bytes, &pos, 0, BITS_RESERVED);

    checksum = CalcChecksum(bytes);
    ApplyKeystream(bytes);
    pos = FRIEND_CODE_CHECKSUM_BIT;
    WriteBits(bytes, &pos, checksum, BITS_CHECKSUM);

    pos = 0;
    for (i = 0; i < FRIEND_CODE_LENGTH; i++)
        dest[i] = sAlphabet[ReadBits(bytes, &pos, BITS_PER_CHAR)];
    dest[FRIEND_CODE_LENGTH] = EOS;
}

// Reads a code, ignoring hyphens, spaces and letter case. Disabled species and battle-only forms
// are removed from the team rather than rejecting the code. A team needs at least one Pokémon that
// can appear in a Friend Safari and at least one that can battle.
enum FriendCodeDecodeResult FriendCode_Decode(const u8 *src, struct FriendCodeData *data)
{
    u8 bytes[FRIEND_CODE_BYTES] = {0};
    u32 i, pos, count = 0, checksum;
    bool32 hasSpecies = FALSE;
    u16 safariSpecies[PARTY_SIZE];

    for (; *src != EOS; src++)
    {
        s32 value;

        if (*src == CHAR_HYPHEN || *src == CHAR_SPACE)
            continue;
        value = CharToValue(*src);
        if (value < 0 || count == FRIEND_CODE_LENGTH)
            return FRIEND_CODE_DECODE_BAD_CHARACTERS;
        pos = count * BITS_PER_CHAR;
        WriteBits(bytes, &pos, value, BITS_PER_CHAR);
        count++;
    }
    if (count != FRIEND_CODE_LENGTH)
        return FRIEND_CODE_DECODE_BAD_CHARACTERS;

    pos = FRIEND_CODE_CHECKSUM_BIT;
    checksum = ReadBits(bytes, &pos, BITS_CHECKSUM);
    pos = FRIEND_CODE_CHECKSUM_BIT;
    WriteBits(bytes, &pos, 0, BITS_CHECKSUM);
    ApplyKeystream(bytes);
    if (CalcChecksum(bytes) != checksum)
        return FRIEND_CODE_DECODE_BAD_CHECKSUM;

    pos = 0;
    if (ReadBits(bytes, &pos, BITS_VERSION) != FRIEND_CODE_VERSION)
        return FRIEND_CODE_DECODE_BAD_VERSION;
    data->title = ReadBits(bytes, &pos, BITS_TITLE);
    for (i = 0; i < PARTY_SIZE; i++)
        data->species[i] = ReadBits(bytes, &pos, BITS_SPECIES);
    data->shinyFlags = ReadBits(bytes, &pos, BITS_SHINY);
    data->trainerId = ReadBits(bytes, &pos, BITS_TRAINER_ID);
    if (ReadBits(bytes, &pos, BITS_RESERVED) != 0)
        return FRIEND_CODE_DECODE_BAD_VERSION;

    if (data->title >= FRIEND_TITLE_COUNT)
        return FRIEND_CODE_DECODE_BAD_TITLE;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (data->species[i] >= NUM_SPECIES)
            return FRIEND_CODE_DECODE_BAD_SPECIES;
        if (data->species[i] != SPECIES_NONE && (!IsSpeciesEnabled(data->species[i]) || IsBattleOnlyForm(data->species[i])))
            data->species[i] = SPECIES_NONE;
        if (data->species[i] == SPECIES_NONE)
            data->shinyFlags &= ~(1 << i);
        else
            hasSpecies = TRUE;
    }
    if (!hasSpecies)
        return FRIEND_CODE_DECODE_EMPTY_TEAM;
    if (FriendSafari_BuildSpeciesList(data->species, safariSpecies) == 0)
        return FRIEND_CODE_DECODE_NO_SAFARI_SPECIES;
    if (!HasBattleSpecies(data->species))
        return FRIEND_CODE_DECODE_NO_BATTLE_SPECIES;

    return FRIEND_CODE_DECODE_OK;
}

// Converts a team to its distinct base forms, in party order, leaving out any that are banned
// from the Safari. Returns how many were written.
u32 FriendSafari_BuildSpeciesList(const u16 *team, u16 *dest)
{
    u32 i, j, count = 0;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        enum Species species = team[i];
        enum Species preEvolution;

        if (species == SPECIES_NONE)
            continue;
        // Evolution lines are at most 3 stages; the bound guards against cyclic data
        for (j = 0; j < 3 && (preEvolution = GetSpeciesPreEvolution(species)) != SPECIES_NONE; j++)
            species = preEvolution;
        if (IsSafariBanned(team[i]) || IsSafariBanned(species))
            continue;
        for (j = 0; j < count && dest[j] != species; j++)
            ;
        if (j == count)
            dest[count++] = species;
    }
    return count;
}

static struct FriendRecord *GetFriend(u32 slot)
{
    return &gSaveBlock3Ptr->friends[slot];
}

// Guards against leftover save data, e.g. from a save made before friend codes existed
#define FRIEND_RECORD_MARKER 0xA5

static bool32 IsFriendValid(const struct FriendRecord *friend)
{
    u32 i;
    bool32 hasSpecies = FALSE;

    if (friend->marker != FRIEND_RECORD_MARKER || friend->title >= FRIEND_TITLE_COUNT)
        return FALSE;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (friend->species[i] >= NUM_SPECIES)
            return FALSE;
        if (friend->species[i] != SPECIES_NONE)
            hasSpecies = TRUE;
    }
    return hasSpecies;
}

// Friends are kept contiguous from slot 0
static u32 CountFriends(void)
{
    u32 count;
    for (count = 0; count < FRIEND_CODE_MAX_FRIENDS && IsFriendValid(GetFriend(count)); count++)
        ;
    return count;
}

// Moves valid friends to the front of the list and clears everything else
static void SanitizeFriends(void)
{
    u32 i, count = 0;

    for (i = 0; i < FRIEND_CODE_MAX_FRIENDS; i++)
    {
        if (IsFriendValid(GetFriend(i)))
        {
            if (i != count)
                *GetFriend(count) = *GetFriend(i);
            count++;
        }
    }
    for (i = count; i < FRIEND_CODE_MAX_FRIENDS; i++)
        memset(GetFriend(i), 0, sizeof(struct FriendRecord));
}

static void CopyFriendData(struct FriendRecord *friend, const struct FriendCodeData *data)
{
    u32 i;
    friend->title = data->title;
    friend->trainerId = data->trainerId;
    for (i = 0; i < PARTY_SIZE; i++)
        friend->species[i] = data->species[i];
    friend->shinyFlags = data->shinyFlags;
    friend->marker = FRIEND_RECORD_MARKER;
}

static void CopyFriendName(struct FriendRecord *friend, const u8 *name)
{
    u32 i;
    for (i = 0; i < PLAYER_NAME_LENGTH; i++)
        friend->name[i] = name[i];
}

static u8 *BufferFriendName(u8 *dest, const struct FriendRecord *friend)
{
    u32 i;
    for (i = 0; i < PLAYER_NAME_LENGTH && friend->name[i] != EOS; i++)
        dest[i] = friend->name[i];
    dest[i] = EOS;
    return &dest[i];
}

// "TITLE NAME"
static void BufferFriendTitleAndName(u8 *dest, const struct FriendRecord *friend)
{
    dest = StringCopy(dest, gFriendTitles[friend->title].name);
    *dest++ = CHAR_SPACE;
    BufferFriendName(dest, friend);
}

static void PushListItem(const u8 *name, u32 id)
{
    struct ListMenuItem item;
    u8 *buffer = Alloc(32);

    StringCopy(buffer, name);
    item.name = buffer;
    item.id = id;
    MultichoiceDynamic_PushElement(item);
}

// Sharing your code

// Pushes each title the player has unlocked for dynmultistack. The item id is the title index.
void FriendCode_PushUnlockedTitles(void)
{
    u32 i;
    u8 name[32];

    for (i = 0; i < FRIEND_TITLE_COUNT; i++)
    {
        if (gFriendTitles[i].unlockFlag != 0 && !FlagGet(gFriendTitles[i].unlockFlag))
            continue;
        StringCopy(name, gFriendTitles[i].name);
        StringAppend(name, gFriendTitles[i].gender == MALE ? COMPOUND_STRING(" ♂") : COMPOUND_STRING(" ♀"));
        PushListItem(name, i);
    }
}

// In:  VAR_0x8004 = title
// Out: VAR_RESULT = FRIEND_CODE_SHARE_*, gStringVar1 = "XXXXX-XXXXX-XXXXX-XXXXX" if the code can be shared
void FriendCode_BufferOwnCode(void)
{
    struct FriendCodeData data = {0};
    u8 code[FRIEND_CODE_LENGTH + 1];
    u16 safariSpecies[PARTY_SIZE];
    u32 i, j;

    data.title = gSpecialVar_0x8004;
    data.trainerId = gSaveBlock2Ptr->playerTrainerId[0] | (gSaveBlock2Ptr->playerTrainerId[1] << 8);
    gSpecialVar_Result = FRIEND_CODE_SHARE_NO_POKEMON;
    for (i = 0; i < PARTY_SIZE; i++)
    {
        enum Species species = GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_SPECIES_OR_EGG);

        if (species == SPECIES_NONE || species == SPECIES_EGG)
            continue;
        data.species[i] = species;
        if (GetMonData(&gParties[B_TRAINER_PLAYER][i], MON_DATA_IS_SHINY))
            data.shinyFlags |= 1 << i;
        gSpecialVar_Result = FRIEND_CODE_SHARE_OK;
    }
    if (gSpecialVar_Result == FRIEND_CODE_SHARE_NO_POKEMON)
        return;
    if (FriendSafari_BuildSpeciesList(data.species, safariSpecies) == 0)
    {
        gSpecialVar_Result = FRIEND_CODE_SHARE_NO_SAFARI_POKEMON;
        return;
    }
    if (!HasBattleSpecies(data.species))
    {
        gSpecialVar_Result = FRIEND_CODE_SHARE_NO_BATTLE_POKEMON;
        return;
    }

    FriendCode_Encode(&data, code);
    for (i = 0, j = 0; i < FRIEND_CODE_LENGTH; i++)
    {
        if (i != 0 && i % 5 == 0)
            gStringVar1[j++] = CHAR_HYPHEN;
        gStringVar1[j++] = code[i];
    }
    gStringVar1[j] = EOS;
}

// Registering a friend

enum
{
    CODE_PART_INVALID,
    CODE_PART_OK,
    CODE_PART_EMPTY,
};

static u32 CheckCodePart(const u8 *part)
{
    u32 i;

    if (part[0] == EOS)
        return CODE_PART_EMPTY;
    for (i = 0; i < CODE_PART_LENGTH; i++)
    {
        if (part[i] == EOS || CharToValue(part[i]) < 0)
            return CODE_PART_INVALID;
    }
    return CODE_PART_OK;
}

static void OpenCodePartScreen(void)
{
    ConvertIntToDecimalStringN(gStringVar1, sCodePart + 1, STR_CONV_MODE_LEFT_ALIGN, 1);
    DoNamingScreen(NAMING_SCREEN_FRIEND_CODE, sCodeInput[sCodePart], 0, 0, 0, CB2_ReturnFromCodePart);
}

// Moves on to the next part, reopens a part that isn't 5 valid characters, or cancels on a blank part
static void CB2_ReturnFromCodePart(void)
{
    switch (CheckCodePart(sCodeInput[sCodePart]))
    {
    case CODE_PART_EMPTY:
        gSpecialVar_Result = FALSE;
        SetMainCallback2(CB2_ReturnToFieldContinueScript);
        return;
    case CODE_PART_INVALID:
        PlaySE(SE_FAILURE);
        break;
    case CODE_PART_OK:
        sCodePart++;
        break;
    }

    if (sCodePart == CODE_PARTS)
    {
        gSpecialVar_Result = TRUE;
        SetMainCallback2(CB2_ReturnToFieldContinueScript);
    }
    else
    {
        OpenCodePartScreen();
    }
}

// Enters the 4 parts of a code, one naming screen each.
// Out: VAR_RESULT = FALSE if the player cancelled
void FriendCode_EnterCode(void)
{
    memset(sCodeInput, EOS, sizeof(sCodeInput));
    sCodePart = 0;
    OpenCodePartScreen();
}

// Out: VAR_RESULT = FRIEND_CODE_RESULT_*
//      For FRIEND_CODE_RESULT_UPDATE: VAR_0x8005 = slot, gStringVar1 = friend's name
//      For FRIEND_CODE_RESULT_NEW and _UPDATE: gStringVar2 = title
void FriendCode_ProcessEnteredCode(void)
{
    u8 code[FRIEND_CODE_LENGTH + 1];
    u8 *end = code;
    u16 ownId = gSaveBlock2Ptr->playerTrainerId[0] | (gSaveBlock2Ptr->playerTrainerId[1] << 8);
    u32 i;

    for (i = 0; i < CODE_PARTS; i++)
        end = StringCopy(end, sCodeInput[i]);
    if (FriendCode_Decode(code, &sPendingFriend) != FRIEND_CODE_DECODE_OK)
    {
        gSpecialVar_Result = FRIEND_CODE_RESULT_INVALID;
        return;
    }
    if (sPendingFriend.trainerId == ownId)
    {
        gSpecialVar_Result = FRIEND_CODE_RESULT_OWN_CODE;
        return;
    }

    StringCopy(gStringVar2, gFriendTitles[sPendingFriend.title].name);
    for (i = 0; i < CountFriends(); i++)
    {
        if (GetFriend(i)->trainerId == sPendingFriend.trainerId)
        {
            gSpecialVar_0x8005 = i;
            BufferFriendName(gStringVar1, GetFriend(i));
            gSpecialVar_Result = FRIEND_CODE_RESULT_UPDATE;
            return;
        }
    }
    gSpecialVar_Result = (CountFriends() < FRIEND_CODE_MAX_FRIENDS) ? FRIEND_CODE_RESULT_NEW : FRIEND_CODE_RESULT_LIST_FULL;
}

// In: VAR_0x8005 = slot, from FriendCode_ProcessEnteredCode
void FriendCode_UpdateFriend(void)
{
    CopyFriendData(GetFriend(gSpecialVar_0x8005), &sPendingFriend);
}

void FriendCode_EnterFriendName(void)
{
    memset(sNameInput, EOS, sizeof(sNameInput));
    DoNamingScreen(NAMING_SCREEN_FRIEND, sNameInput, 0, 0, 0, CB2_ReturnToFieldContinueScript);
}

// Saves the pending friend with the name just entered.
// Out: VAR_RESULT = FALSE if no name was entered, gStringVar1 = "TITLE NAME"
void FriendCode_SaveNewFriend(void)
{
    u32 slot = CountFriends();
    struct FriendRecord *friend;

    if (sNameInput[0] == EOS || slot >= FRIEND_CODE_MAX_FRIENDS)
    {
        gSpecialVar_Result = FALSE;
        return;
    }
    friend = GetFriend(slot);
    CopyFriendData(friend, &sPendingFriend);
    CopyFriendName(friend, sNameInput);
    BufferFriendTitleAndName(gStringVar1, friend);
    gSpecialVar_Result = TRUE;
}

// Managing friends

// Also clears any invalid records, so call this before showing the friend list
void FriendCode_CountFriends(void)
{
    SanitizeFriends();
    gSpecialVar_Result = CountFriends();
}

// Pushes "TITLE NAME" for each friend for dynmultistack. The item id is the slot.
void FriendCode_PushFriendList(void)
{
    u32 i;
    u8 name[32];

    for (i = 0; i < CountFriends(); i++)
    {
        BufferFriendTitleAndName(name, GetFriend(i));
        PushListItem(name, i);
    }
}

// In:  VAR_0x8004 = slot
// Out: gStringVar1 = "TITLE NAME", gStringVar2 = trainer ID, gStringVar3 = team
void FriendCode_BufferFriendDetails(void)
{
    struct FriendRecord *friend = GetFriend(gSpecialVar_0x8004);
    u8 *dest = gStringVar3;
    u32 i, count = 0;

    BufferFriendTitleAndName(gStringVar1, friend);
    ConvertIntToDecimalStringN(gStringVar2, friend->trainerId, STR_CONV_MODE_LEADING_ZEROS, 5);
    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (friend->species[i] == SPECIES_NONE)
            continue;
        if (count != 0)
        {
            *dest++ = CHAR_COMMA;
            if (count == 2)
                *dest++ = CHAR_NEWLINE;
            else if (count == 4)
                *dest++ = CHAR_PROMPT_SCROLL;
            else
                *dest++ = CHAR_SPACE;
        }
        dest = StringCopy(dest, GetSpeciesName(friend->species[i]));
        count++;
    }
    *dest = EOS;
}

// Renames a friend with the name just entered.
// In:  VAR_0x8004 = slot
// Out: VAR_RESULT = FALSE if no name was entered
void FriendCode_RenameFriend(void)
{
    gSpecialVar_Result = (sNameInput[0] != EOS);
    if (gSpecialVar_Result)
        CopyFriendName(GetFriend(gSpecialVar_0x8004), sNameInput);
}

// In: VAR_0x8004 = slot
void FriendCode_DeleteFriend(void)
{
    u32 slot;
    for (slot = gSpecialVar_0x8004; slot < FRIEND_CODE_MAX_FRIENDS - 1; slot++)
        *GetFriend(slot) = *GetFriend(slot + 1);
    memset(GetFriend(FRIEND_CODE_MAX_FRIENDS - 1), 0, sizeof(struct FriendRecord));
}

// Friend Safari

// In: VAR_0x8004 = slot
void FriendSafari_SetActiveFriend(void)
{
    VarSet(VAR_FRIEND_SAFARI_FRIEND, gSpecialVar_0x8004 + 1);
    sSafariCachedVar = 0;
}

// Replaces a grass encounter on the Friend Safari map with one of the active friend's Pokémon
enum Species FriendSafari_GetWildSpecies(enum Species species, u32 slot)
{
    u32 var = VarGet(VAR_FRIEND_SAFARI_FRIEND);

    if (gSaveBlock1Ptr->location.mapGroup != MAP_GROUP(FRIEND_SAFARI_MAP)
     || gSaveBlock1Ptr->location.mapNum != MAP_NUM(FRIEND_SAFARI_MAP)
     || var == 0 || var > CountFriends())
        return species;

    if (sSafariCachedVar != var)
    {
        sSafariSpeciesCount = FriendSafari_BuildSpeciesList(GetFriend(var - 1)->species, sSafariSpecies);
        sSafariCachedVar = var;
    }
    if (sSafariSpeciesCount == 0)
        return species;
    return sSafariSpecies[slot % sSafariSpeciesCount];
}

// TV

bool32 FriendCode_TVShowAvailable(void)
{
    return FRIEND_CODE_TV_CHANCE > 0 && CountFriends() > 0;
}

void FriendCode_ShouldDoTVShow(void)
{
    gSpecialVar_Result = FriendCode_TVShowAvailable() && (Random() % 100) < FRIEND_CODE_TV_CHANCE;
}

void FriendCode_DoTVShow(void)
{
    struct FriendRecord *friend = GetFriend(Random() % CountFriends());
    const struct FriendTitle *title = &gFriendTitles[friend->title];
    u16 team[PARTY_SIZE];
    u32 i, count = 0;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        if (friend->species[i] != SPECIES_NONE)
            team[count++] = friend->species[i];
    }

    StringCopy(gStringVar1, title->name);
    BufferFriendName(gStringVar2, friend);
    StringCopy(gStringVar3, GetSpeciesName(team[Random() % count]));
    StringExpandPlaceholders(gStringVar4, title->tvLines[Random() % title->tvLineCount]);
    ShowFieldMessage(gStringVar4);
}
