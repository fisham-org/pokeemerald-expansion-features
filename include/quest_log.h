#ifndef GUARD_QUEST_LOG_H
#define GUARD_QUEST_LOG_H

void QuestLog_Open(MainCallback returnCallback);
void Task_QuestLog_OpenFromStartMenu(u8 taskId);
void QuestLog_BlitUnreadIndicator(u32 windowId, u32 x, u32 y);

#endif // GUARD_QUEST_LOG_H
