#include "global.h"
#include "clock.h"
#include "fake_rtc.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "overworld.h"
#include "script.h"
#include "time_skip.h"
#include "constants/rtc.h"

// The time each band skips to. UpdateTimeOfDay() blends between two bands' tints
// weighted across the band, and weight 256 means "fully the starting colour", so
// landing on a band's first hour renders as the band *before* it. These sit
// inside their band instead:
//   MORNING 08:00 - the band midpoint, peak morning tint
//   DAY     12:00 - flat midday, no tint
//   EVENING 19:30 - evening is only 19:00-20:00, and 19:00 still renders as full
//                   day while 20:00 is already night, so the half hour is the
//                   only point in the band that actually looks like a sunset
//   NIGHT   21:00 - past the evening->night blend, full night
//
// Morning is derived so it tracks the band if it moves; the other three are
// tuned to the OW_TIMES_OF_DAY >= GEN_8 bands and need retuning if those change.
struct SkipTarget
{
    u8 hour;
    u8 minute;
};

static const struct SkipTarget sSkipTargets[TIMES_OF_DAY_COUNT] =
{
    [TIME_MORNING] = { MORNING_HOUR_BEGIN + ((MORNING_HOUR_END - MORNING_HOUR_BEGIN) / 2), 0 },
    [TIME_DAY]     = { 12,  0 },
    [TIME_EVENING] = { 19, 30 },
    [TIME_NIGHT]   = { 21,  0 },
};

// Rebuilds the day/night palette blend in place. The periodic refresh in
// OverworldBasic() only runs once per in-game minute, which is several real
// seconds away; callers jumping the clock need the new tint on this frame so it
// is already correct behind a hardware fade.
static void RefreshTimeOfDayPalettes(void)
{
    UpdateTimeOfDay(TRUE);
    FormChangeTimeUpdate();

    if (MapHasNaturalLight(gMapHeader.mapType))
        ApplyWeatherColorMapIfIdle(gWeatherPtr->colorMapIndex);
}

u32 TimeSkip_GetTargetHour(enum TimeOfDay timeOfDay)
{
    if (timeOfDay >= TIMES_OF_DAY_COUNT)
        timeOfDay = TIME_MORNING;

    return sSkipTargets[timeOfDay].hour;
}

u32 TimeSkip_GetTargetMinute(enum TimeOfDay timeOfDay)
{
    if (timeOfDay >= TIMES_OF_DAY_COUNT)
        timeOfDay = TIME_MORNING;

    return sSkipTargets[timeOfDay].minute;
}

void TimeSkip_ToTimeOfDay(enum TimeOfDay timeOfDay)
{
    const struct SkipTarget *target;

    if (!OW_USE_FAKE_RTC)
        return;

    if (timeOfDay >= TIMES_OF_DAY_COUNT)
        timeOfDay = TIME_MORNING;

    target = &sSkipTargets[timeOfDay];
    FakeRtc_ForwardTimeTo(target->hour, target->minute, 0);
    RefreshTimeOfDayPalettes();
}

bool8 ScrCmd_settimeofday(struct ScriptContext *ctx)
{
    u32 timeOfDay = ScriptReadWord(ctx);

    Script_RequestEffects(SCREFF_V1 | SCREFF_SAVE);

    TimeSkip_ToTimeOfDay(timeOfDay);

    return FALSE;
}

// After a hardware fade to black, lift BG0 - the text/window layer - out of the
// blend target so a message box drawn on top stays readable while the map and
// sprites stay dark. FadeScreenHardware() rebuilds BLDCNT from scratch on every
// call, so the following fade back in restores BG0 on its own.
void Script_KeepTextAboveFade(void)
{
    Script_RequestEffects(SCREFF_V1 | SCREFF_HARDWARE);

    SetGpuReg(REG_OFFSET_BLDCNT, GetGpuReg(REG_OFFSET_BLDCNT) & ~BLDCNT_TGT1_BG0);
}
