@ Assign to an NPC or terminal in Porymap. Which species count as answers is set by
@ POKEDOKU_DEFAULT_ANSWER_POOL (include/config/dex_minigames.h); copy this script and pass a
@ different DEX_POOL_* in VAR_0x8005 for terminals that should differ. Daily boards save after every guess,
@ so the game is saved before opening one.
Pokedoku_EventScript_Terminal::
	lockall
	special GetPokedokuStatus
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_DISABLED, Pokedoku_EventScript_End
	call Pokedoku_EventScript_GiveReward @ In case the player reset or had a full bag before getting it
	goto_if_eq VAR_RESULT, DEX_GAME_REWARD_BAG_FULL, Pokedoku_EventScript_End
	message Pokedoku_Text_Welcome
	waitmessage
	dynmultichoice 0, 0, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, DexGame_Text_Daily, DexGame_Text_Practice, DexGame_Text_Cancel
	goto_if_eq VAR_RESULT, 0, Pokedoku_EventScript_Daily
	goto_if_eq VAR_RESULT, 1, Pokedoku_EventScript_Practice
	goto Pokedoku_EventScript_End

Pokedoku_EventScript_Daily:
	special GetPokedokuStatus
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_CLOCK_LOCKED, Pokedoku_EventScript_ClockLocked
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_NO_CLOCK, Pokedoku_EventScript_NoClock
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_FINISHED, Pokedoku_EventScript_Finished
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_IN_PROGRESS, Pokedoku_EventScript_Resume
	@ DEX_GAME_STATUS_NEW and DEX_GAME_STATUS_NOT_STARTED (made but never played) both get the intro
	msgbox Pokedoku_Text_Intro, MSGBOX_DEFAULT
	goto Pokedoku_EventScript_PrepareDaily

Pokedoku_EventScript_Resume:
	msgbox DexGame_Text_Resume, MSGBOX_DEFAULT
Pokedoku_EventScript_PrepareDaily:
	setvar VAR_0x8004, DEX_GAME_MODE_DAILY
	setvar VAR_0x8005, POKEDOKU_DEFAULT_ANSWER_POOL
	special PreparePokedoku
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES, Pokedoku_EventScript_NotEnoughSpecies
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_REWARD_WAITING, Pokedoku_EventScript_End
	call Common_EventScript_SaveGame
	goto_if_eq VAR_RESULT, FALSE, Pokedoku_EventScript_End
	goto Pokedoku_EventScript_OpenDaily

Pokedoku_EventScript_Finished:
	msgbox DexGame_Text_Finished, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Pokedoku_EventScript_End
Pokedoku_EventScript_OpenDaily:
	setvar VAR_0x8004, DEX_GAME_MODE_DAILY
	fadescreen FADE_TO_BLACK
	special OpenPokedoku
	goto_if_eq VAR_RESULT, FALSE, DexGame_EventScript_DidNotOpen
	call Pokedoku_EventScript_GiveReward
	releaseall
	end

Pokedoku_EventScript_Practice:
	setvar VAR_0x8004, DEX_GAME_MODE_PRACTICE
	setvar VAR_0x8005, POKEDOKU_DEFAULT_ANSWER_POOL
	special PreparePokedoku
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES, Pokedoku_EventScript_NotEnoughSpecies
	msgbox DexGame_Text_PracticeInfo, MSGBOX_DEFAULT
	fadescreen FADE_TO_BLACK
	special OpenPokedoku
	goto_if_eq VAR_RESULT, FALSE, DexGame_EventScript_DidNotOpen
	releaseall
	end

@ ClaimPokedokuReward rolls the day's reward from src/data/dex_minigame_rewards.h, once
Pokedoku_EventScript_GiveReward:
	special ClaimPokedokuReward
	goto_if_ne VAR_RESULT, DEX_GAME_NO_REWARD, DexGame_EventScript_GiveRolledReward
	return

Pokedoku_EventScript_ClockLocked:
	msgbox DexGame_Text_ClockLocked, MSGBOX_DEFAULT
	releaseall
	end

Pokedoku_EventScript_NotEnoughSpecies:
	msgbox DexGame_Text_NotEnoughSpecies, MSGBOX_DEFAULT
	releaseall
	end

Pokedoku_EventScript_NoClock:
	msgbox DexGame_Text_NoClock, MSGBOX_DEFAULT
Pokedoku_EventScript_End:
	releaseall
	end

Pokedoku_Text_Welcome:
	.string "It's POKéDOKU! Which would you\n"
	.string "like to play?$"

Pokedoku_Text_Intro:
	.string "Name a POKéMON that fits the row\n"
	.string "and column of each square.\p"
	.string "You get one guess per square, and\n"
	.string "each POKéMON can only be used once.\p"
	.string "Your progress is saved after\n"
	.string "every guess.$"
