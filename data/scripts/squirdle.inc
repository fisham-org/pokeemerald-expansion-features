@ Assign to an NPC or terminal in Porymap. Which species count as answers is set by
@ SQUIRDLE_DEFAULT_ANSWER_POOL (include/config/dex_minigames.h); copy this script and pass a
@ different DEX_POOL_* in VAR_0x8005 for terminals that should differ. Daily puzzles save after every guess,
@ so the game is saved before opening one.
Squirdle_EventScript_Terminal::
	lockall
	special GetSquirdleStatus
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_DISABLED, Squirdle_EventScript_End
	call Squirdle_EventScript_GiveReward @ In case the player reset or had a full bag before getting it
	goto_if_eq VAR_RESULT, DEX_GAME_REWARD_BAG_FULL, Squirdle_EventScript_End
	message Squirdle_Text_Welcome
	waitmessage
	dynmultichoice 0, 0, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, DexGame_Text_Daily, DexGame_Text_Practice, DexGame_Text_Cancel
	goto_if_eq VAR_RESULT, 0, Squirdle_EventScript_Daily
	goto_if_eq VAR_RESULT, 1, Squirdle_EventScript_Practice
	goto Squirdle_EventScript_End

Squirdle_EventScript_Daily:
	special GetSquirdleStatus
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_CLOCK_LOCKED, Squirdle_EventScript_ClockLocked
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_NO_CLOCK, Squirdle_EventScript_NoClock
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_FINISHED, Squirdle_EventScript_Finished
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_IN_PROGRESS, Squirdle_EventScript_Resume
	@ DEX_GAME_STATUS_NEW and DEX_GAME_STATUS_NOT_STARTED (made but never played) both get the intro
	msgbox Squirdle_Text_Intro, MSGBOX_DEFAULT
	goto Squirdle_EventScript_PrepareDaily

Squirdle_EventScript_Resume:
	msgbox DexGame_Text_Resume, MSGBOX_DEFAULT
Squirdle_EventScript_PrepareDaily:
	setvar VAR_0x8004, DEX_GAME_MODE_DAILY
	setvar VAR_0x8005, SQUIRDLE_DEFAULT_ANSWER_POOL
	special PrepareSquirdle
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES, Squirdle_EventScript_NotEnoughSpecies
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_REWARD_WAITING, Squirdle_EventScript_End
	call Common_EventScript_SaveGame
	goto_if_eq VAR_RESULT, FALSE, Squirdle_EventScript_End
	goto Squirdle_EventScript_OpenDaily

Squirdle_EventScript_Finished:
	msgbox DexGame_Text_Finished, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Squirdle_EventScript_End
Squirdle_EventScript_OpenDaily:
	setvar VAR_0x8004, DEX_GAME_MODE_DAILY
	fadescreen FADE_TO_BLACK
	special OpenSquirdle
	goto_if_eq VAR_RESULT, FALSE, DexGame_EventScript_DidNotOpen
	call Squirdle_EventScript_GiveReward
	releaseall
	end

Squirdle_EventScript_Practice:
	setvar VAR_0x8004, DEX_GAME_MODE_PRACTICE
	setvar VAR_0x8005, SQUIRDLE_DEFAULT_ANSWER_POOL
	special PrepareSquirdle
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES, Squirdle_EventScript_NotEnoughSpecies
	msgbox DexGame_Text_PracticeInfo, MSGBOX_DEFAULT
	fadescreen FADE_TO_BLACK
	special OpenSquirdle
	goto_if_eq VAR_RESULT, FALSE, DexGame_EventScript_DidNotOpen
	releaseall
	end

@ ClaimSquirdleReward rolls the day's reward from src/data/dex_minigame_rewards.h, once
Squirdle_EventScript_GiveReward:
	special ClaimSquirdleReward
	goto_if_ne VAR_RESULT, DEX_GAME_NO_REWARD, DexGame_EventScript_GiveRolledReward
	return

Squirdle_EventScript_ClockLocked:
	msgbox DexGame_Text_ClockLocked, MSGBOX_DEFAULT
	releaseall
	end

Squirdle_EventScript_NotEnoughSpecies:
	msgbox DexGame_Text_NotEnoughSpecies, MSGBOX_DEFAULT
	releaseall
	end

Squirdle_EventScript_NoClock:
	msgbox DexGame_Text_NoClock, MSGBOX_DEFAULT
Squirdle_EventScript_End:
	releaseall
	end

Squirdle_Text_Welcome:
	.string "It's SQUIRDLE! Which would you\n"
	.string "like to play?$"

Squirdle_Text_Intro:
	.string "Guess the mystery POKéMON in 8\n"
	.string "tries or fewer.\p"
	.string "Each guess shows how its generation,\n"
	.string "types, height and weight compare.\p"
	.string "Your progress is saved after\n"
	.string "every guess.$"
