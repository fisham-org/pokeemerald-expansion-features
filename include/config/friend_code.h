#ifndef GUARD_CONFIG_FRIEND_CODE_H
#define GUARD_CONFIG_FRIEND_CODE_H

// Friend codes: a 20-character password that registers another player as a friend.

// Max number of friends that can be registered at once (1-255). Each friend uses 24 bytes of SaveBlock3.
// WARNING: USE_DEXNAV_SEARCH_LEVELS (include/config/dexnav.h) uses NUM_SPECIES bytes of SaveBlock3,
// which leaves no room for friend records.
#define FRIEND_CODE_MAX_FRIENDS     10

// Percent chance that watching TV shows a friend show instead of a normal show, when at least one
// friend is registered. Friend shows also air when no normal show is available. 0 disables friend shows.
#define FRIEND_CODE_TV_CHANCE       25

// Which Pokémon friends can't use. TRUE bans that group.
// A friend's code still stores their whole team; the bans only decide which Pokémon each feature uses.
// Note: FRONTIER covers every restricted legendary and mythical, so to allow e.g. Kyogre, set both
// FRONTIER and RESTRICTED_LEGENDARY to FALSE.
//
// _ENCOUNTER: left out of Friend Safari wild encounters. A player can't share a code unless at least
// one party Pokémon can appear in a Friend Safari.
#define FRIEND_SAFARI_BAN_FRONTIER_ENCOUNTER                TRUE    // isFrontierBanned, e.g. Mewtwo, Mew, Kyogre
#define FRIEND_SAFARI_BAN_RESTRICTED_LEGENDARY_ENCOUNTER    TRUE    // e.g. Mewtwo, Lugia, Rayquaza, Koraidon
#define FRIEND_SAFARI_BAN_SUB_LEGENDARY_ENCOUNTER           TRUE    // e.g. the legendary birds, Regis, Latias, Tapus
#define FRIEND_SAFARI_BAN_MYTHICAL_ENCOUNTER                TRUE    // e.g. Mew, Celebi, Jirachi, Pecharunt
#define FRIEND_SAFARI_BAN_ULTRA_BEAST_ENCOUNTER             TRUE    // e.g. Nihilego, Buzzwole, Poipole
#define FRIEND_SAFARI_BAN_PARADOX_ENCOUNTER                 TRUE    // e.g. Great Tusk, Iron Treads

// _BATTLE: left out of a friend's team when battling them. A player can't share a code unless at least
// one party Pokémon can battle. Friend battles are not built yet; these are read by
// FriendCode_IsBannedFromBattle.
#define FRIEND_SAFARI_BAN_FRONTIER_BATTLE                   FALSE
#define FRIEND_SAFARI_BAN_RESTRICTED_LEGENDARY_BATTLE       FALSE
#define FRIEND_SAFARI_BAN_SUB_LEGENDARY_BATTLE              FALSE
#define FRIEND_SAFARI_BAN_MYTHICAL_BATTLE                   FALSE
#define FRIEND_SAFARI_BAN_ULTRA_BEAST_BATTLE                FALSE
#define FRIEND_SAFARI_BAN_PARADOX_BATTLE                    FALSE

// Holds the active Friend Safari as (friend slot + 1). 0 means no friend has been chosen.
// VAR_UNUSED_0x404E is unused in Emerald builds, but FRLG builds use it for VAR_NATIONAL_DEX_FRLG.
#define VAR_FRIEND_SAFARI_FRIEND    VAR_UNUSED_0x404E

// The map whose grass encounters are replaced with the active friend's Pokémon.
#define FRIEND_SAFARI_MAP           MAP_SAFARI_MAP

#endif // GUARD_CONFIG_FRIEND_CODE_H
