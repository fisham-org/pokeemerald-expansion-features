#ifndef GUARD_QUEST_NOTE_H
#define GUARD_QUEST_NOTE_H

#include "quest.h"

// A note is one thing the player has learned. A note with a target (points_to) is a lead
// until its resolvedBy condition is true. A note with a subject is part of that character's profile.

struct QuestSubject
{
    const u8 *name;
    u16 graphicsId;     // OBJ_EVENT_GFX_*
};

struct QuestNote
{
    const u8 *text;
    u16 targetMap;      // MAP_* or MAP_UNDEFINED; set for leads
    u8 targetLocalId;   // LOCALID_* or LOCALID_NONE
    u8 subject;         // SUBJECT_* or SUBJECT_NONE
    struct QuestCondition resolvedBy;
};

extern const struct QuestNote gQuestNotes[];
extern const struct QuestSubject gQuestSubjects[];

bool32 QuestNote_IsValid(u32 noteId);
const struct QuestNote *QuestNote_GetInfo(u32 noteId);
bool8 QuestNote_IsKnown(u32 noteId);
bool8 QuestNote_IsLead(u32 noteId);
bool8 QuestNote_IsOpenLead(u32 noteId);
bool8 QuestNote_Take(u32 noteId);
u16 QuestNote_GetTracked(void);
bool8 QuestNote_SetTracked(u32 noteId);
void QuestNote_ResetAll(void);
void QuestNote_DebugToggle(u32 noteId);

bool32 QuestSubject_IsValid(u32 subjectId);
const struct QuestSubject *QuestSubject_GetInfo(u32 subjectId);
bool8 QuestSubject_IsKnown(u32 subjectId);
bool8 QuestSubject_IsUnread(u32 subjectId);
void QuestSubject_ClearUnread(u32 subjectId);
bool8 QuestSubject_HasAnyUnread(void);

#endif // GUARD_QUEST_NOTE_H
