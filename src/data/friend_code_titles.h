// Titles a player can choose when sharing their friend code.
// The index of each title is stored in friend codes, so only ever add titles to the end of this
// table. Never reorder or remove them. There can be at most 128 titles.
//
// TV lines can use {STR_VAR_1} (title), {STR_VAR_2} (friend's name) and {STR_VAR_3} (a species
// from their team).

static const u8 *const sFriendTVLines_Generic[] =
{
    COMPOUND_STRING("FRIEND WATCH!\pWe spotted {STR_VAR_1} {STR_VAR_2}\ntraining with their {STR_VAR_3}.\pThey looked like they were having\na great time!"),
    COMPOUND_STRING("FRIEND WATCH!\pRumor has it {STR_VAR_1} {STR_VAR_2}\nnever travels without {STR_VAR_3}.\pNow that's a true partnership!"),
    COMPOUND_STRING("FRIEND WATCH!\p{STR_VAR_1} {STR_VAR_2} says their\n{STR_VAR_3} is the best POKéMON around.\pWhat do you think, viewers?"),
};

static const u8 *const sFriendTVLines_Champion[] =
{
    COMPOUND_STRING("BREAKING NEWS!\pI heard a trainer named {STR_VAR_2}\nbeat the ELITE FOUR!\pTheir {STR_VAR_3} was unstoppable!"),
    COMPOUND_STRING("FRIEND WATCH!\pCHAMPION {STR_VAR_2} was seen\ntraining with {STR_VAR_3} again.\pNo wonder they're on top!"),
};

#define GENERIC_TV_LINES .tvLines = sFriendTVLines_Generic, .tvLineCount = ARRAY_COUNT(sFriendTVLines_Generic)

const struct FriendTitle gFriendTitles[] =
{
    { .name = COMPOUND_STRING("POKéMON TRAINER"), .gender = MALE,   .unlockFlag = 0, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("POKéMON TRAINER"), .gender = FEMALE, .unlockFlag = 0, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("YOUNGSTER"),       .gender = MALE,   .unlockFlag = 0, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("LASS"),            .gender = FEMALE, .unlockFlag = 0, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("BUG CATCHER"),     .gender = MALE,   .unlockFlag = 0, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("PICNICKER"),       .gender = FEMALE, .unlockFlag = 0, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("ACE TRAINER"),     .gender = MALE,   .unlockFlag = FLAG_BADGE04_GET, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("ACE TRAINER"),     .gender = FEMALE, .unlockFlag = FLAG_BADGE04_GET, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("VETERAN"),         .gender = MALE,   .unlockFlag = FLAG_BADGE08_GET, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("VETERAN"),         .gender = FEMALE, .unlockFlag = FLAG_BADGE08_GET, GENERIC_TV_LINES },
    { .name = COMPOUND_STRING("CHAMPION"),        .gender = MALE,   .unlockFlag = FLAG_SYS_GAME_CLEAR, .tvLines = sFriendTVLines_Champion, .tvLineCount = ARRAY_COUNT(sFriendTVLines_Champion) },
    { .name = COMPOUND_STRING("CHAMPION"),        .gender = FEMALE, .unlockFlag = FLAG_SYS_GAME_CLEAR, .tvLines = sFriendTVLines_Champion, .tvLineCount = ARRAY_COUNT(sFriendTVLines_Champion) },
};

#undef GENERIC_TV_LINES
