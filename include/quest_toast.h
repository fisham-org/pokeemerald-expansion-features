#ifndef GUARD_QUEST_TOAST_H
#define GUARD_QUEST_TOAST_H

enum QuestToastType
{
    QUEST_TOAST_LEAD,
    QUEST_TOAST_AVAILABLE,
    QUEST_TOAST_STARTED,
    QUEST_TOAST_UPDATED,
    QUEST_TOAST_PROGRESS,
    QUEST_TOAST_COMPLETE,
    QUEST_TOAST_CLOSED,
    QUEST_TOAST_TYPE_COUNT,
};

#if QUEST_TOASTS
void QuestToast_Queue(u32 type, u32 questId, u32 objective);
void QuestToast_Update(void);
u32 QuestToast_GetQueueCount(void);
void QuestToast_ClearQueue(void);
#else
static inline void QuestToast_Queue(u32 type, u32 questId, u32 objective) {}
static inline void QuestToast_Update(void) {}
static inline u32 QuestToast_GetQueueCount(void) { return 0; }
static inline void QuestToast_ClearQueue(void) {}
#endif

#endif // GUARD_QUEST_TOAST_H
