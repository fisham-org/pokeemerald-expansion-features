#ifndef GUARD_QUEST_GUIDANCE_H
#define GUARD_QUEST_GUIDANCE_H

enum QuestMarkerType
{
    QUEST_MARKER_NONE,
    QUEST_MARKER_TURN_IN,
    QUEST_MARKER_AVAILABLE,
    QUEST_MARKER_TARGET,
};

u32 QuestMarkers_GetType(u32 localId, u32 mapNum, u32 mapGroup, u8 *category);

#if QUEST_NPC_MARKERS
void QuestMarkers_OnObjectSpawn(u32 objectEventId);
void QuestMarkers_Refresh(void);
#else
static inline void QuestMarkers_OnObjectSpawn(u32 objectEventId) {}
static inline void QuestMarkers_Refresh(void) {}
#endif

#if QUEST_TOWN_MAP_PIN
void QuestPin_CreateTownMapSprite(void);
#else
static inline void QuestPin_CreateTownMapSprite(void) {}
#endif

#endif // GUARD_QUEST_GUIDANCE_H
