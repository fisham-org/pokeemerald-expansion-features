// Daily rewards for the Pokédex minigames. When a daily puzzle's reward is claimed, the first
// tier whose score range includes the score is used, and one of its items is picked by weight.
// A score no tier covers gives nothing. The pick is seeded by the day, so resetting doesn't reroll it.
// These are examples: swap in your own tiers, messages and items.

#define REWARD_ITEMS(...) .items = (const struct DexRewardItem[]) { __VA_ARGS__ }, .itemCount = ARRAY_COUNT(((const struct DexRewardItem[]) { __VA_ARGS__ }))

// Score: squares correct (0-9)
static const struct DexRewardTier sPokedokuRewardTiers[] =
{
    {
        .minScore = 9, .maxScore = 9,
        .message = COMPOUND_STRING("A perfect board!"),
        REWARD_ITEMS(
            { ITEM_RARE_CANDY,  1, 4 },
            { ITEM_PP_UP,       1, 3 },
            { ITEM_BOTTLE_CAP,  1, 1 },
        ),
    },
    {
        .minScore = 6, .maxScore = 8,
        .message = COMPOUND_STRING("Great work!"),
        REWARD_ITEMS(
            { ITEM_ULTRA_BALL,   3, 2 },
            { ITEM_HYPER_POTION, 2, 2 },
            { ITEM_NUGGET,       1, 1 },
        ),
    },
    {
        .minScore = 0, .maxScore = 5,
        .message = COMPOUND_STRING("Thanks for playing!"),
        REWARD_ITEMS(
            { ITEM_POKE_BALL,    3, 2 },
            { ITEM_SUPER_POTION, 2, 2 },
        ),
    },
};

// Score: guesses a win took (1-8). Losses give no reward.
static const struct DexRewardTier sSquirdleRewardTiers[] =
{
    {
        .minScore = 1, .maxScore = 3,
        .message = COMPOUND_STRING("Amazing, and so quick!"),
        REWARD_ITEMS(
            { ITEM_RARE_CANDY,  1, 4 },
            { ITEM_PP_UP,       1, 3 },
            { ITEM_BOTTLE_CAP,  1, 1 },
        ),
    },
    {
        .minScore = 4, .maxScore = 6,
        .message = COMPOUND_STRING("Nicely done!"),
        REWARD_ITEMS(
            { ITEM_ULTRA_BALL,   3, 2 },
            { ITEM_HYPER_POTION, 2, 2 },
            { ITEM_NUGGET,       1, 1 },
        ),
    },
    {
        .minScore = 7, .maxScore = 8,
        .message = COMPOUND_STRING("You got there!"),
        REWARD_ITEMS(
            { ITEM_POKE_BALL,    3, 2 },
            { ITEM_SUPER_POTION, 2, 2 },
        ),
    },
};

#undef REWARD_ITEMS
