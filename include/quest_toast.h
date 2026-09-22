#ifndef GUARD_QUEST_TOAST_H
#define GUARD_QUEST_TOAST_H

// Order matches the icons in graphics/quest_log/toast.png
enum QuestToastType
{
    QUEST_TOAST_NEW_LEAD,       // id is a note
    QUEST_TOAST_AVAILABLE,
    QUEST_TOAST_STARTED,
    QUEST_TOAST_UPDATED,
    QUEST_TOAST_PROGRESS,
    QUEST_TOAST_COMPLETE,
    QUEST_TOAST_CLOSED,
    QUEST_TOAST_TASK_COMPLETE,
    QUEST_TOAST_PROFILE,        // id is a note
    QUEST_TOAST_TYPE_COUNT,
};

#if QUEST_TOASTS
// id is a quest, or a note for QUEST_TOAST_NEW_LEAD and QUEST_TOAST_PROFILE
void QuestToast_Queue(u32 type, u32 id, u32 objective);
void QuestToast_Update(void);
u32 QuestToast_GetQueueCount(void);
u32 QuestToast_GetQueuedType(u32 index);
void QuestToast_ClearQueue(void);
#else
static inline void QuestToast_Queue(u32 type, u32 id, u32 objective) {}
static inline void QuestToast_Update(void) {}
static inline u32 QuestToast_GetQueueCount(void) { return 0; }
static inline u32 QuestToast_GetQueuedType(u32 index) { return QUEST_TOAST_TYPE_COUNT; }
static inline void QuestToast_ClearQueue(void) {}
#endif

#endif // GUARD_QUEST_TOAST_H
