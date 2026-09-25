#ifndef GUARD_FRIEND_CODE_H
#define GUARD_FRIEND_CODE_H

#include "constants/friend_code.h"

// Characters in a friend code, excluding the hyphens used for display
#define FRIEND_CODE_LENGTH          20
// A displayed code is "XXXXX-XXXXX-XXXXX-XXXXX"
#define FRIEND_CODE_DISPLAY_LENGTH  (FRIEND_CODE_LENGTH + 3)

// Result of FriendCode_Decode
enum FriendCodeDecodeResult
{
    FRIEND_CODE_DECODE_OK,
    FRIEND_CODE_DECODE_BAD_CHARACTERS,
    FRIEND_CODE_DECODE_BAD_CHECKSUM,
    FRIEND_CODE_DECODE_BAD_VERSION,
    FRIEND_CODE_DECODE_BAD_TITLE,
    FRIEND_CODE_DECODE_BAD_SPECIES,
    FRIEND_CODE_DECODE_EMPTY_TEAM,
    FRIEND_CODE_DECODE_NO_SAFARI_SPECIES,
    FRIEND_CODE_DECODE_NO_BATTLE_SPECIES,
};

struct FriendTitle
{
    const u8 *name;
    u8 gender;
    u16 unlockFlag; // 0 = always available
    const u8 *const *tvLines;
    u8 tvLineCount;
};

// The contents of a friend code
struct FriendCodeData
{
    u8 title;
    u16 trainerId;
    u16 species[PARTY_SIZE];
    u8 shinyFlags;
};

extern const struct FriendTitle gFriendTitles[];

void FriendCode_Encode(const struct FriendCodeData *data, u8 *dest);
enum FriendCodeDecodeResult FriendCode_Decode(const u8 *src, struct FriendCodeData *data);
u32 FriendSafari_BuildSpeciesList(const u16 *team, u16 *dest);
enum Species FriendSafari_GetWildSpecies(enum Species species, u32 slot);
bool32 FriendCode_TVShowAvailable(void);
bool32 FriendCode_IsBannedFromBattle(enum Species species);

#endif // GUARD_FRIEND_CODE_H
