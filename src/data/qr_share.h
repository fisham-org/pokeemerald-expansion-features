// QR Share tables. tools/qr_share/export.py parses these for labels, so keep one entry per line
// and use the FLAG_*/VAR_* constant names directly.

#if TESTING

static const u16 sQrShareFlags[] =
{
    FLAG_TEMP_1,
    FLAG_TEMP_2,
    FLAG_TEMP_3,
};

static const struct QrShareVar sQrShareVars[] =
{
    { VAR_TEMP_0, 2 },
    { VAR_TEMP_1, 16 },
};

#else

// Up to 64 flags, one bit each.
static const u16 sQrShareFlags[] =
{
    FLAG_SYS_GAME_CLEAR,
};

// Up to 32 vars. The second field is how many bits to store (1-16); larger values are truncated.
static const struct QrShareVar sQrShareVars[] =
{
    { VAR_STARTER_MON, 2 },
};

#endif

// Up to 32 backgrounds. Background 0 must always be unlocked (flag 0).
static const struct QrShareBackground sQrShareBackgrounds[] =
{
    { COMPOUND_STRING("Lab"), 0 },
    { COMPOUND_STRING("Frontier"), 0 },
};
