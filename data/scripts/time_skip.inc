@ Shared "rest a while" spot. Point a bg_event sign at EventScript_RestSpot to
@ give the player somewhere to wind the clock forward from.
@
@ settimeofday never winds the clock backwards, so choosing a time that has
@ already passed today lands on it tomorrow. That is the intended behaviour: it
@ rolls over VAR_DAYS and fires the daily events, exactly as a night's rest should.
@
@ The message is shown while the screen is dark. fadescreenswapbuffers is a
@ hardware (BLDY) fade that darkens every layer, so keeptextabovefade lifts the
@ text layer back out of it; the fade back in restores the blend target itself.

EventScript_RestSpot::
	lockall
	dynmultipush Text_RestMorning, 0
	dynmultipush Text_RestDay, 1
	dynmultipush Text_RestEvening, 2
	dynmultipush Text_RestNight, 3
	dynmultistack 0, 0, FALSE, 4, FALSE, 0, DYN_MULTICHOICE_CB_NONE
	switch VAR_RESULT
	case 0, EventScript_RestUntilMorning
	case 1, EventScript_RestUntilDay
	case 2, EventScript_RestUntilEvening
	case 3, EventScript_RestUntilNight
	releaseall
	end

EventScript_RestUntilMorning::
	bufferstring STR_VAR_1, Text_RestedMorning
	settimeofday TIME_MORNING
	goto EventScript_RestCommon

EventScript_RestUntilDay::
	bufferstring STR_VAR_1, Text_RestedDay
	settimeofday TIME_DAY
	goto EventScript_RestCommon

EventScript_RestUntilEvening::
	bufferstring STR_VAR_1, Text_RestedEvening
	settimeofday TIME_EVENING
	goto EventScript_RestCommon

EventScript_RestUntilNight::
	bufferstring STR_VAR_1, Text_RestedNight
	settimeofday TIME_NIGHT
	goto EventScript_RestCommon

@ The clock has already moved by this point, so the tint is correct behind the
@ fade and the screen comes back showing the new time of day.
EventScript_RestCommon::
	closemessage
	fadescreenswapbuffers FADE_TO_BLACK
	delay 30
	keeptextabovefade
	msgbox Text_Rested, MSGBOX_DEFAULT
	closemessage
	delay 20
	fadescreenswapbuffers FADE_FROM_BLACK
	releaseall
	end

Text_RestMorning:
	.string "Until morning$"

Text_RestDay:
	.string "Until midday$"

Text_RestEvening:
	.string "Until evening$"

Text_RestNight:
	.string "Until night$"

Text_Rested:
	.string "You rested until {STR_VAR_1}.$"

Text_RestedMorning:
	.string "morning$"

Text_RestedDay:
	.string "midday$"

Text_RestedEvening:
	.string "evening$"

Text_RestedNight:
	.string "night$"
