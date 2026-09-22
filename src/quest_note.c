#include "global.h"
#include "event_data.h"
#include "quest.h"
#include "quest_note.h"
#include "quest_toast.h"
#include "constants/maps.h"

/*
 * Notes: things the player learns by talking to people, reading books or checking signs.
 * A note with a target is a lead until its resolvedBy condition is true. Resolved state is
 * not saved; it is read from the condition whenever it is needed.
 * A note with a subject belongs to that character's profile.
 * Data comes from src/data/notes/*.json via tools/quests/quests_to_header.py (generated into src/data/quests.h).
 */

static inline bool32 GetBit(const u8 *bits, u32 index)
{
    return (bits[index / 8] >> (index % 8)) & 1;
}

static inline void SetBit(u8 *bits, u32 index, bool32 value)
{
    if (value)
        bits[index / 8] |= 1 << (index % 8);
    else
        bits[index / 8] &= ~(1 << (index % 8));
}

bool32 QuestNote_IsValid(u32 noteId)
{
    return noteId < NOTE_COUNT && gQuestNotes[noteId].text != NULL;
}

const struct QuestNote *QuestNote_GetInfo(u32 noteId)
{
    return &gQuestNotes[noteId];
}

bool8 QuestNote_IsKnown(u32 noteId)
{
    return QuestNote_IsValid(noteId) && GetBit(gSaveBlock3Ptr->notesKnown, noteId);
}

bool8 QuestNote_IsLead(u32 noteId)
{
    return QuestNote_IsValid(noteId) && gQuestNotes[noteId].targetMap != MAP_UNDEFINED;
}

// A lead the player knows and has not followed up yet
bool8 QuestNote_IsOpenLead(u32 noteId)
{
    return QuestNote_IsKnown(noteId)
        && QuestNote_IsLead(noteId)
        && !Quest_EvaluateCondition(&gQuestNotes[noteId].resolvedBy);
}

bool8 QuestNote_Take(u32 noteId)
{
    const struct QuestNote *note;

    if (!QuestNote_IsValid(noteId) || QuestNote_IsKnown(noteId))
        return FALSE;

    note = &gQuestNotes[noteId];
    SetBit(gSaveBlock3Ptr->notesKnown, noteId, TRUE);
    if (note->subject != SUBJECT_NONE)
        SetBit(gSaveBlock3Ptr->subjectsUnread, note->subject, TRUE);

    if (QUEST_LEADS && QuestNote_IsOpenLead(noteId))
        QuestToast_Queue(QUEST_TOAST_NEW_LEAD, noteId, 0);
    else if (QUEST_PROFILES && note->subject != SUBJECT_NONE)
        QuestToast_Queue(QUEST_TOAST_PROFILE, noteId, 0);
    return TRUE;
}

// A pinned lead stops being tracked once it resolves.
u16 QuestNote_GetTracked(void)
{
    u32 noteId = gSaveBlock3Ptr->trackedNote - 1;
    if (gSaveBlock3Ptr->trackedNote == 0 || !QuestNote_IsOpenLead(noteId))
        return NOTE_NONE;
    return noteId;
}

// Pass NOTE_NONE to unpin. Pinning a lead untracks the tracked quest; they share the Town Map pin.
bool8 QuestNote_SetTracked(u32 noteId)
{
    if (noteId == NOTE_NONE)
    {
        gSaveBlock3Ptr->trackedNote = 0;
        return TRUE;
    }
    if (!QuestNote_IsOpenLead(noteId))
        return FALSE;
    gSaveBlock3Ptr->trackedNote = noteId + 1;
    gSaveBlock3Ptr->trackedQuest = 0;
    return TRUE;
}

void QuestNote_ResetAll(void)
{
    memset(gSaveBlock3Ptr->notesKnown, 0, sizeof(gSaveBlock3Ptr->notesKnown));
    memset(gSaveBlock3Ptr->subjectsUnread, 0, sizeof(gSaveBlock3Ptr->subjectsUnread));
    gSaveBlock3Ptr->trackedNote = 0;
}

void QuestNote_DebugToggle(u32 noteId)
{
    if (!QuestNote_IsValid(noteId))
        return;
    if (QuestNote_IsKnown(noteId))
        SetBit(gSaveBlock3Ptr->notesKnown, noteId, FALSE);
    else
        QuestNote_Take(noteId);
}

// *******************************
// Subjects

bool32 QuestSubject_IsValid(u32 subjectId)
{
    return subjectId < SUBJECT_COUNT && gQuestSubjects[subjectId].name != NULL;
}

const struct QuestSubject *QuestSubject_GetInfo(u32 subjectId)
{
    return &gQuestSubjects[subjectId];
}

// A profile appears once the player knows at least one note about the character.
bool8 QuestSubject_IsKnown(u32 subjectId)
{
    for (u32 i = 0; i < NOTE_COUNT; i++)
    {
        if (gQuestNotes[i].subject == subjectId && QuestNote_IsKnown(i))
            return TRUE;
    }
    return FALSE;
}

bool8 QuestSubject_IsUnread(u32 subjectId)
{
    return QuestSubject_IsValid(subjectId) && GetBit(gSaveBlock3Ptr->subjectsUnread, subjectId);
}

void QuestSubject_ClearUnread(u32 subjectId)
{
    if (QuestSubject_IsValid(subjectId))
        SetBit(gSaveBlock3Ptr->subjectsUnread, subjectId, FALSE);
}

bool8 QuestSubject_HasAnyUnread(void)
{
    for (u32 i = 0; i < SUBJECT_COUNT; i++)
    {
        if (QuestSubject_IsUnread(i))
            return TRUE;
    }
    return FALSE;
}
