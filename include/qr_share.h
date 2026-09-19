#ifndef GUARD_QR_SHARE_H
#define GUARD_QR_SHARE_H

#include "main.h"
#include "config/qr_share.h"

struct QrShareVar
{
    u16 varId;
    u8 bits;
};

struct QrShareBackground
{
    const u8 *name;
    u16 unlockFlag; // 0 = always unlocked
};

// Worst case: 241 payload bytes -> 386 base32 chars, plus the base URL.
#define QR_SHARE_URL_MAX 512

// Writes the full ASCII URL (NUL-terminated) to dst. Returns its length.
u32 QrShare_BuildUrl(char *dst);
// Writes template with its {MAJOR}/{MINOR}/{PATCH}/{VERSION} placeholders filled in to dst, cut to
// QR_SHARE_URL_MAX - 1 characters. Returns its length.
u32 QrShare_BuildStaticUrl(char *dst, const char *template);
u32 QrShare_GetVersionCharIndex(char c);
// Opens the QR screen; returns to callback when closed.
void QrShare_Open(MainCallback callback);
struct ScriptContext;
void QrShare_SetPartyMonNickname(struct ScriptContext *ctx);
// Specials
void QrShare_ShowScreen(void);
void QrShare_PushUnlockedBackgrounds(void);
void QrShare_SetBackground(void);

#endif // GUARD_QR_SHARE_H
