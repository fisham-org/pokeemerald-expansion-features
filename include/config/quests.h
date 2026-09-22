#ifndef GUARD_CONFIG_QUESTS_H
#define GUARD_CONFIG_QUESTS_H

// The quest data model and scripting API are always compiled.
// These toggles only control the presentation layer.
#define QUEST_TOASTS                TRUE // Show a toast at the top of the screen when quest state changes.
#define QUEST_NPC_MARKERS           TRUE // Show available / turn-in / objective target markers above NPCs.
#define QUEST_TOWN_MAP_PIN          TRUE // Pin the tracked quest's target on the Town Map.
#define QUEST_START_MENU_UNREAD     TRUE // Show an indicator next to QUESTS in the start menu while a quest is unread.

#endif // GUARD_CONFIG_QUESTS_H
