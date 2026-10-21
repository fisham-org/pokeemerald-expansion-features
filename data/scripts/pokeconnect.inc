@ Assign to an NPC or terminal in Porymap. Which species count as answers is set by
@ POKECONNECT_DEFAULT_ANSWER_POOL (include/config/dex_minigames.h); copy this script and pass a
@ different DEX_POOL_* in VAR_0x8005 for terminals that should differ. Daily puzzles save after every guess,
@ so the game is saved before opening one.
Pokeconnect_EventScript_Terminal::
	lockall
	special GetPokeconnectStatus
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_DISABLED, Pokeconnect_EventScript_End
	call Pokeconnect_EventScript_GiveReward @ In case the player reset or had a full bag before getting it
	goto_if_eq VAR_RESULT, DEX_GAME_REWARD_BAG_FULL, Pokeconnect_EventScript_End
	message Pokeconnect_Text_Welcome
	waitmessage
	dynmultichoice 0, 0, FALSE, 3, 0, DYN_MULTICHOICE_CB_NONE, DexGame_Text_Daily, DexGame_Text_Practice, DexGame_Text_Cancel
	goto_if_eq VAR_RESULT, 0, Pokeconnect_EventScript_Daily
	goto_if_eq VAR_RESULT, 1, Pokeconnect_EventScript_Practice
	goto Pokeconnect_EventScript_End

Pokeconnect_EventScript_Daily:
	special GetPokeconnectStatus
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_CLOCK_LOCKED, Pokeconnect_EventScript_ClockLocked
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_NO_CLOCK, Pokeconnect_EventScript_NoClock
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_FINISHED, Pokeconnect_EventScript_Finished
	goto_if_eq VAR_RESULT, DEX_GAME_STATUS_IN_PROGRESS, Pokeconnect_EventScript_Resume
	@ DEX_GAME_STATUS_NEW and DEX_GAME_STATUS_NOT_STARTED (made but never played) both get the intro
	msgbox Pokeconnect_Text_Intro, MSGBOX_DEFAULT
	goto Pokeconnect_EventScript_PrepareDaily

Pokeconnect_EventScript_Resume:
	msgbox DexGame_Text_Resume, MSGBOX_DEFAULT
Pokeconnect_EventScript_PrepareDaily:
	setvar VAR_0x8004, DEX_GAME_MODE_DAILY
	setvar VAR_0x8005, POKECONNECT_DEFAULT_ANSWER_POOL
	special PreparePokeconnect
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES, Pokeconnect_EventScript_NotEnoughSpecies
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_REWARD_WAITING, Pokeconnect_EventScript_End
	call Common_EventScript_SaveGame
	goto_if_eq VAR_RESULT, FALSE, Pokeconnect_EventScript_End
	goto Pokeconnect_EventScript_OpenDaily

Pokeconnect_EventScript_Finished:
	msgbox DexGame_Text_Finished, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, Pokeconnect_EventScript_End
Pokeconnect_EventScript_OpenDaily:
	setvar VAR_0x8004, DEX_GAME_MODE_DAILY
	fadescreen FADE_TO_BLACK
	special OpenPokeconnect
	goto_if_eq VAR_RESULT, FALSE, DexGame_EventScript_DidNotOpen
	call Pokeconnect_EventScript_GiveReward
	releaseall
	end

Pokeconnect_EventScript_Practice:
	setvar VAR_0x8004, DEX_GAME_MODE_PRACTICE
	setvar VAR_0x8005, POKECONNECT_DEFAULT_ANSWER_POOL
	special PreparePokeconnect
	goto_if_eq VAR_RESULT, DEX_GAME_PREPARE_NOT_ENOUGH_SPECIES, Pokeconnect_EventScript_NotEnoughSpecies
	msgbox DexGame_Text_PracticeInfo, MSGBOX_DEFAULT
	fadescreen FADE_TO_BLACK
	special OpenPokeconnect
	goto_if_eq VAR_RESULT, FALSE, DexGame_EventScript_DidNotOpen
	releaseall
	end

@ ClaimPokeconnectReward rolls the day's reward from src/data/dex_minigame_rewards.h, once
Pokeconnect_EventScript_GiveReward:
	special ClaimPokeconnectReward
	goto_if_ne VAR_RESULT, DEX_GAME_NO_REWARD, DexGame_EventScript_GiveRolledReward
	return

Pokeconnect_EventScript_ClockLocked:
	msgbox DexGame_Text_ClockLocked, MSGBOX_DEFAULT
	releaseall
	end

Pokeconnect_EventScript_NotEnoughSpecies:
	msgbox DexGame_Text_NotEnoughSpecies, MSGBOX_DEFAULT
	releaseall
	end

Pokeconnect_EventScript_NoClock:
	msgbox DexGame_Text_NoClock, MSGBOX_DEFAULT
Pokeconnect_EventScript_End:
	releaseall
	end

Pokeconnect_Text_Welcome:
	.string "It's POKéCONNECT! Which would you\n"
	.string "like to play?$"

Pokeconnect_Text_Intro:
	.string "Sort 16 POKéMON into four groups\n"
	.string "of four that share something.\p"
	.string "Pick four with {A_BUTTON}, then press START.\n"
	.string "You can make 4 mistakes.\p"
	.string "Your progress is saved after\n"
	.string "every guess.$"
