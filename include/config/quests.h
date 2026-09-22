#ifndef GUARD_CONFIG_QUESTS_H
#define GUARD_CONFIG_QUESTS_H

// The quest data model and scripting API are always compiled.
// These toggles only control the presentation layer.
#define QUEST_TOASTS                TRUE // Show a toast at the top of the screen when quest state changes.
#define QUEST_NPC_MARKERS           TRUE // Show available / turn-in / objective target markers above NPCs.
#define QUEST_TOWN_MAP_PIN          TRUE // Pin the tracked quest's target on the Town Map.
#define QUEST_START_MENU_UNREAD     TRUE // Show an indicator next to QUESTS in the start menu while a quest is unread.
#define QUEST_LEADS                 TRUE // Show the Leads page in the quest log, "New lead" toasts, and allow pinning leads.
#define QUEST_PROFILES              TRUE // Show the Profiles page in the quest log and "Profile updated" toasts.

// Quest log list layout. The default shows a sprite per row (4 rows); compact drops the sprites (8 rows).
#define QUEST_LOG_COMPACT           FALSE // Use the compact list layout by default.
#define QUEST_LOG_LAYOUT_FLAG       0                     // If set to a flag, the log uses the other layout while the flag is set. Set it from a script or an options menu to let the player choose.

#endif // GUARD_CONFIG_QUESTS_H
