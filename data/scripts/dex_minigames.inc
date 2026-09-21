@ Shared scripts and text for the Pokédex minigames (pokedoku.inc, squirdle.inc)

@ Gives the item a Claim*Reward special rolled from src/data/dex_minigame_rewards.h.
@ If it doesn't fit, the reward stays waiting and VAR_RESULT is left as DEX_GAME_REWARD_BAG_FULL.
DexGame_EventScript_GiveRolledReward:
	goto_if_eq VAR_RESULT, DEX_GAME_REWARD_BAG_FULL, DexGame_EventScript_RewardBagFull
	goto_if_eq VAR_0x8004, ITEM_NONE, DexGame_EventScript_NoRolledReward
	msgbox DexGame_Text_Reward, MSGBOX_DEFAULT
	giveitem VAR_0x8004, VAR_0x8005
DexGame_EventScript_NoRolledReward:
	return

DexGame_EventScript_RewardBagFull:
	msgbox DexGame_Text_RewardBagFull, MSGBOX_DEFAULT
	setvar VAR_RESULT, DEX_GAME_REWARD_BAG_FULL
	return

@ Open* didn't open (the day changed while a prompt was up), so the screen is still black
DexGame_EventScript_DidNotOpen:
	fadescreen FADE_FROM_BLACK
	msgbox DexGame_Text_DayChanged, MSGBOX_DEFAULT
	releaseall
	end

DexGame_Text_RewardBagFull:
	.string "You've got a prize waiting, but your\n"
	.string "BAG is full.\p"
	.string "Make some room and come back for it!$"

DexGame_Text_DayChanged:
	.string "Oh! The date just changed, so there's\n"
	.string "a new puzzle. Talk to me again to\l"
	.string "play it!$"

DexGame_Text_Reward:
	.string "{STR_VAR_1}\n"
	.string "Here's a prize.$"

DexGame_Text_Daily:
	.string "Daily puzzle$"

DexGame_Text_Practice:
	.string "Practice$"

DexGame_Text_Cancel:
	.string "Cancel$"

DexGame_Text_PracticeInfo:
	.string "Practice puzzles are random and\n"
	.string "aren't saved. Press START for a new\l"
	.string "one when you finish.$"

DexGame_Text_Resume:
	.string "Resuming today's puzzle.$"

DexGame_Text_Finished:
	.string "You've finished today's puzzle.\n"
	.string "View your results?$"

DexGame_Text_ClockLocked:
	.string "The clock seems to have gone back\n"
	.string "in time… Come back tomorrow!$"

DexGame_Text_NoClock:
	.string "The internal clock isn't working,\n"
	.string "so there's no daily puzzle.$"

DexGame_Text_NotEnoughSpecies:
	.string "You haven't found enough POKéMON to\n"
	.string "make a puzzle yet.\p"
	.string "Come back after seeing or catching\n"
	.string "some more!$"
