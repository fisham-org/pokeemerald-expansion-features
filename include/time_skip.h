#ifndef GUARD_TIME_SKIP_H
#define GUARD_TIME_SKIP_H

#include "global.h"
#include "constants/rtc.h"

// Winds the fake RTC forward to the canonical time for the given time of day and
// re-applies the day/night tint immediately. Never winds backwards: asking for a
// time that has already passed today lands on it tomorrow.
void TimeSkip_ToTimeOfDay(enum TimeOfDay timeOfDay);

// The time TimeSkip_ToTimeOfDay() aims at, exposed so tests can assert the
// targets still land inside their band if the band constants move.
u32 TimeSkip_GetTargetHour(enum TimeOfDay timeOfDay);
u32 TimeSkip_GetTargetMinute(enum TimeOfDay timeOfDay);

void Script_KeepTextAboveFade(void);

#endif // GUARD_TIME_SKIP_H
