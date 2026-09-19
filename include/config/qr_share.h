#ifndef GUARD_CONFIG_QR_SHARE_H
#define GUARD_CONFIG_QR_SHARE_H

// QR Share: shows a QR code encoding the player's trainer info, party and selected flags/vars
// as a URL. See notes/feature-qr/qr-share.md. Flag, var and background tables live in src/data/qr_share.h.

#define QR_SHARE_MODE_TEAM   0 // Code links to the team share site (QR_SHARE_BASE_URL + the player's data).
#define QR_SHARE_MODE_STATIC 1 // Code links to QR_SHARE_STATIC_URL, e.g. your game's listing or docs.
#define QR_SHARE_MODE        QR_SHARE_MODE_TEAM

// Used in QR_SHARE_MODE_STATIC. These placeholders are filled in when the code is shown:
// {MAJOR}, {MINOR}, {PATCH} and {VERSION} (MAJOR.MINOR.PATCH, or QR_SHARE_VERSION_STRING if set).
// e.g. "https://mygame.dev/docs/v{MAJOR}.{MINOR}/" -> "https://mygame.dev/docs/v1.0/"
// An all-uppercase URL makes a smaller code, but only if the site accepts uppercase paths.
#define QR_SHARE_STATIC_URL      "https://mygame.dev/"

// Must end in "/P/". All uppercase uses the compact QR alphanumeric mode; any lowercase switches the
// whole URL to byte mode (~45% larger code, e.g. version 7 -> 9 for a typical team).
#define QR_SHARE_BASE_URL        "https://mygame.pages.dev/P/"
// First byte of every code; picks which data/v<N>/ folder the site loads. Bump it only when IDs or the
// tables in src/data/qr_share.h change, then re-run tools/qr_share/export.py and keep the old folders.
#define QR_SHARE_DATA_VERSION    1

// Release version shown on the site, e.g. 1.0.0. Each part is 0-255.
// These can point at your own macros, e.g. EXPANSION_VERSION_MAJOR from constants/expansion.h.
#define QR_SHARE_VERSION_MAJOR   1
#define QR_SHARE_VERSION_MINOR   0
#define QR_SHARE_VERSION_PATCH   0
// If not empty, shown instead of MAJOR.MINOR.PATCH, e.g. "BETA 3" or "2025.06.1".
// Up to 15 characters from A-Z 0-9 . - + and space (lowercase is converted to uppercase).
#define QR_SHARE_VERSION_STRING  ""
#define QR_SHARE_START_MENU      TRUE  // Adds a "SHARE" entry to the Start menu.
// SHARE only appears in the Start menu once this flag is set. 0 = always shown (when QR_SHARE_START_MENU is TRUE).
#define QR_SHARE_FLAG_START_MENU 0
// Adds the flags and vars from src/data/qr_share.h to the code (shown on the site with ?debug).
// Anyone with a shared link can view them, so consider FALSE for public releases to avoid spoilers.
#define QR_SHARE_INCLUDE_DEBUG_DATA TRUE
// On the website, a Pokemon holding its Mega Stone is drawn as its Mega form (share image and party card).
// Only affects tools/qr_share/export.py; re-run it after changing this. Needs P_MEGA_EVOLUTIONS.
#define QR_SHARE_SHOW_MEGA_FORMS TRUE
// Var holding the chosen background ID. 0 disables backgrounds (background 0 is always used).
#define QR_SHARE_VAR_BACKGROUND  VAR_UNUSED_0x404E

#endif // GUARD_CONFIG_QR_SHARE_H
