#include "global.h"
#include "clock.h"
#include "event_data.h"
#include "fake_rtc.h"
#include "overworld.h"
#include "rtc.h"
#include "time_skip.h"
#include "test/overworld_script.h"
#include "test/test.h"
#include "config/overworld.h"
#include "constants/rtc.h"
#include "constants/songs.h"

ASSUMPTIONS
{
    ASSUME(OW_USE_FAKE_RTC);
}

static void SetClock(u32 day, u32 hour, u32 minute)
{
    RtcCalcLocalTimeOffset(day, hour, minute, 0);
}

// The band every hour of the day should report, derived straight from the
// MORNING/DAY/EVENING/NIGHT_HOUR_* constants rather than hardcoded, so this
// keeps working if the bands are ever retuned.
static enum TimeOfDay ExpectedTimeOfDay(u32 hour)
{
    if (IsBetweenHours(hour, MORNING_HOUR_BEGIN, MORNING_HOUR_END))
        return TIME_MORNING;
    if (IsBetweenHours(hour, EVENING_HOUR_BEGIN, EVENING_HOUR_END))
        return TIME_EVENING;
    if (IsBetweenHours(hour, NIGHT_HOUR_BEGIN, NIGHT_HOUR_END))
        return TIME_NIGHT;
    return TIME_DAY;
}

TEST("Every hour of the day reports exactly one time of day")
{
    u32 hour = 0;

    for (u32 i = 0; i < HOURS_PER_DAY; i++)
        PARAMETRIZE_LABEL("%02d:00", i) { hour = i; }

    SetClock(0, hour, 0);
    EXPECT_EQ(GetTimeOfDay(), ExpectedTimeOfDay(hour));
}

TEST("Bands tile the whole day with no gap")
{
    u32 seen[TIMES_OF_DAY_COUNT] = {0};
    u32 total = 0;

    for (u32 hour = 0; hour < HOURS_PER_DAY; hour++)
    {
        SetClock(0, hour, 0);
        seen[GetTimeOfDay()]++;
    }

    // Every band must own at least one hour, and they must add up to the day.
    for (u32 i = 0; i < TIMES_OF_DAY_COUNT; i++)
    {
        EXPECT_GT(seen[i], 0);
        total += seen[i];
    }
    EXPECT_EQ(total, HOURS_PER_DAY);
}

TEST("Each skip target lands inside the band it names")
{
    enum TimeOfDay timeOfDay = TIME_MORNING;

    for (u32 i = 0; i < TIMES_OF_DAY_COUNT; i++)
        PARAMETRIZE_LABEL("band %d", i) { timeOfDay = i; }

    SetClock(0, TimeSkip_GetTargetHour(timeOfDay), TimeSkip_GetTargetMinute(timeOfDay));
    EXPECT_EQ(GetTimeOfDay(), timeOfDay);
}

TEST("settimeofday reaches every band from every band")
{
    enum TimeOfDay from = TIME_MORNING, to = TIME_MORNING;

    for (u32 f = 0; f < TIMES_OF_DAY_COUNT; f++)
        for (u32 t = 0; t < TIMES_OF_DAY_COUNT; t++)
            PARAMETRIZE_LABEL("%d -> %d", f, t) { from = f; to = t; }

    SetClock(0, TimeSkip_GetTargetHour(from), TimeSkip_GetTargetMinute(from));
    EXPECT_EQ(GetTimeOfDay(), from);

    TimeSkip_ToTimeOfDay(to);
    EXPECT_EQ(GetTimeOfDay(), to);
}

TEST("A skip never winds the clock backwards")
{
    enum TimeOfDay timeOfDay = TIME_MORNING;
    u32 startHour, beforeMinutes, afterMinutes;
    struct Time before;

    for (u32 i = 0; i < TIMES_OF_DAY_COUNT; i++)
        PARAMETRIZE_LABEL("band %d", i) { timeOfDay = i; }

    // Start one hour past the target, so a naive implementation would rewind.
    startHour = (TimeSkip_GetTargetHour(timeOfDay) + 1) % HOURS_PER_DAY;

    SetClock(0, startHour, 0);
    RtcCalcLocalTime();
    before = gLocalTime;

    TimeSkip_ToTimeOfDay(timeOfDay);
    RtcCalcLocalTime();

    beforeMinutes = (before.days * HOURS_PER_DAY + before.hours) * MINUTES_PER_HOUR + before.minutes;
    afterMinutes = (gLocalTime.days * HOURS_PER_DAY + gLocalTime.hours) * MINUTES_PER_HOUR + gLocalTime.minutes;

    EXPECT_GT(afterMinutes, beforeMinutes);
    EXPECT_EQ(GetTimeOfDay(), timeOfDay);
}

TEST("Skipping to a time already passed today rolls over to tomorrow")
{
    u32 dayBefore;

    // 30 minutes past the morning target, so morning is behind us.
    SetClock(0, TimeSkip_GetTargetHour(TIME_MORNING), TimeSkip_GetTargetMinute(TIME_MORNING) + 30);
    RtcCalcLocalTime();
    dayBefore = gLocalTime.days;

    TimeSkip_ToTimeOfDay(TIME_MORNING);
    RtcCalcLocalTime();

    EXPECT_EQ(gLocalTime.days, dayBefore + 1);
    EXPECT_EQ(gLocalTime.hours, TimeSkip_GetTargetHour(TIME_MORNING));
}

TEST("The settimeofday script command moves the clock")
{
    SetClock(0, TimeSkip_GetTargetHour(TIME_DAY), 0);
    EXPECT_EQ(GetTimeOfDay(), TIME_DAY);

    RUN_OVERWORLD_SCRIPT(
        settimeofday TIME_NIGHT;
    );
    EXPECT_EQ(GetTimeOfDay(), TIME_NIGHT);
}

TEST("Pausing the fake RTC stops the clock, resuming restarts it")
{
    struct SiiRtcInfo *rtc;
    u32 secondBefore, minuteBefore;

    // OW_FLAG_PAUSE_TIME ships as 0, and GetFlagPointer() returns NULL for flag
    // 0, so pausefakertc is a silent no-op until a real flag is assigned.
    ASSUME(OW_FLAG_PAUSE_TIME != 0);

    SetClock(0, TimeSkip_GetTargetHour(TIME_DAY), 0);

    RUN_OVERWORLD_SCRIPT(pausefakertc;);
    EXPECT(FlagGet(OW_FLAG_PAUSE_TIME));

    rtc = FakeRtc_GetCurrentTime();
    secondBefore = rtc->second;
    minuteBefore = rtc->minute;
    FakeRtc_TickTimeForward();
    EXPECT_EQ(rtc->second, secondBefore);
    EXPECT_EQ(rtc->minute, minuteBefore);

    RUN_OVERWORLD_SCRIPT(resumefakertc;);
    EXPECT(!FlagGet(OW_FLAG_PAUSE_TIME));

    FakeRtc_TickTimeForward();
    EXPECT_NE(rtc->second, secondBefore);
}

TEST("One tick advances the clock by the configured ratio")
{
    struct SiiRtcInfo *rtc;
    u32 before, after;

    SetClock(0, TimeSkip_GetTargetHour(TIME_DAY), 0);
    FlagClear(OW_FLAG_PAUSE_TIME);

    rtc = FakeRtc_GetCurrentTime();
    before = (rtc->hour * MINUTES_PER_HOUR + rtc->minute) * SECONDS_PER_MINUTE + rtc->second;

    FakeRtc_TickTimeForward();

    after = (rtc->hour * MINUTES_PER_HOUR + rtc->minute) * SECONDS_PER_MINUTE + rtc->second;
    EXPECT_EQ(after - before, FakeRtc_GetSecondsRatio());
}

TEST("Night music lookup passes through a track outside the table")
{
    // MUS_DUMMY is 0 against START_MUS 350, so the unguarded lookup indexed
    // hundreds of entries before the table.
    SetClock(0, TimeSkip_GetTargetHour(TIME_NIGHT), 0);
    EXPECT_EQ(GetTimeOfDay(), TIME_NIGHT);

    EXPECT_EQ(Test_GetNightMusicFromTrack(MUS_DUMMY), MUS_DUMMY);
    EXPECT_EQ(Test_GetNightMusicFromTrack(END_MUS), END_MUS);
}

TEST("Night music lookup is a no-op outside of night")
{
    SetClock(0, TimeSkip_GetTargetHour(TIME_DAY), 0);
    EXPECT_EQ(GetTimeOfDay(), TIME_DAY);

    EXPECT_EQ(Test_GetNightMusicFromTrack(MUS_DUMMY), MUS_DUMMY);
    EXPECT_EQ(Test_GetNightMusicFromTrack(MUS_POKE_CENTER), MUS_POKE_CENTER);
}
