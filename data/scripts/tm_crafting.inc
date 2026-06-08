TMCrafting_EventScript_Machine::
# 11 "data/scripts/tm_crafting.pory"
	lock
# 12 "data/scripts/tm_crafting.pory"
	faceplayer
# 13 "data/scripts/tm_crafting.pory"
	msgbox TMCrafting_EventScript_Machine_Text_0
# 14 "data/scripts/tm_crafting.pory"
	goto TMCrafting_EventScript_Loop
	return


TMCrafting_EventScript_Loop::
# 19 "data/scripts/tm_crafting.pory"
	callnative TMCrafting_BuildList
# 20 "data/scripts/tm_crafting.pory"
	compare VAR_RESULT, 0
	goto_if_eq TMCrafting_EventScript_Loop_2
TMCrafting_EventScript_Loop_1:
# 25 "data/scripts/tm_crafting.pory"
	dynmultistack 0, 0, FALSE, 6, FALSE, 0, DYN_MULTICHOICE_CB_SHOW_ITEM
# 27 "data/scripts/tm_crafting.pory"
	callnative TMCrafting_PrepareSelection
# 28 "data/scripts/tm_crafting.pory"
	compare VAR_RESULT, FALSE
	goto_if_eq TMCrafting_EventScript_Loop_5
TMCrafting_EventScript_Loop_4:
# 31 "data/scripts/tm_crafting.pory"
	msgbox TMCrafting_EventScript_Loop_Text_1, MSGBOX_YESNO
# 32 "data/scripts/tm_crafting.pory"
	compare VAR_RESULT, NO
	goto_if_eq TMCrafting_EventScript_Loop_8
TMCrafting_EventScript_Loop_7:
# 35 "data/scripts/tm_crafting.pory"
	callnative TMCrafting_TryCraft
# 36 "data/scripts/tm_crafting.pory"
	switch VAR_RESULT
# 37 "data/scripts/tm_crafting.pory"
	case 0, TMCrafting_EventScript_Loop_12
# 40 "data/scripts/tm_crafting.pory"
	case 1, TMCrafting_EventScript_Loop_13
# 42 "data/scripts/tm_crafting.pory"
	case 2, TMCrafting_EventScript_Loop_14
# 44 "data/scripts/tm_crafting.pory"
	case 3, TMCrafting_EventScript_Loop_15
TMCrafting_EventScript_Loop_10:
# 47 "data/scripts/tm_crafting.pory"
	goto TMCrafting_EventScript_Loop
	return

TMCrafting_EventScript_Loop_2:
# 21 "data/scripts/tm_crafting.pory"
	msgbox TMCrafting_EventScript_Loop_Text_0
# 22 "data/scripts/tm_crafting.pory"
	goto TMCrafting_EventScript_Done
	goto TMCrafting_EventScript_Loop_1

TMCrafting_EventScript_Loop_5:
# 29 "data/scripts/tm_crafting.pory"
	goto TMCrafting_EventScript_Done
	goto TMCrafting_EventScript_Loop_4

TMCrafting_EventScript_Loop_8:
# 33 "data/scripts/tm_crafting.pory"
	goto TMCrafting_EventScript_Loop
	goto TMCrafting_EventScript_Loop_7

TMCrafting_EventScript_Loop_12:
# 38 "data/scripts/tm_crafting.pory"
	playse SE_SHOP
# 39 "data/scripts/tm_crafting.pory"
	msgbox TMCrafting_EventScript_Loop_Text_2
	goto TMCrafting_EventScript_Loop_10

TMCrafting_EventScript_Loop_13:
# 41 "data/scripts/tm_crafting.pory"
	msgbox TMCrafting_EventScript_Loop_Text_3
	goto TMCrafting_EventScript_Loop_10

TMCrafting_EventScript_Loop_14:
# 43 "data/scripts/tm_crafting.pory"
	msgbox TMCrafting_EventScript_Loop_Text_4
	goto TMCrafting_EventScript_Loop_10

TMCrafting_EventScript_Loop_15:
# 45 "data/scripts/tm_crafting.pory"
	msgbox TMCrafting_EventScript_Loop_Text_5
	goto TMCrafting_EventScript_Loop_10


TMCrafting_EventScript_Done::
# 51 "data/scripts/tm_crafting.pory"
	msgbox TMCrafting_EventScript_Done_Text_0
# 52 "data/scripts/tm_crafting.pory"
	release
	end


TMCrafting_EventScript_Machine_Text_0:
# 13 "data/scripts/tm_crafting.pory"
	.string "This is the TM Machine. Shall I craft a\n"
	.string "TM for you?$"

TMCrafting_EventScript_Loop_Text_0:
# 21 "data/scripts/tm_crafting.pory"
	.string "There are no TM recipes available right\n"
	.string "now.$"

TMCrafting_EventScript_Loop_Text_1:
# 31 "data/scripts/tm_crafting.pory"
	.string "Craft {STR_VAR_1}?\p"
	.string "It needs {STR_VAR_3}, and costs\n"
	.string "¥{STR_VAR_2}.$"

TMCrafting_EventScript_Loop_Text_2:
# 39 "data/scripts/tm_crafting.pory"
	.string "Here you go! One {STR_VAR_1},\n"
	.string "freshly crafted!$"

TMCrafting_EventScript_Loop_Text_3:
# 41 "data/scripts/tm_crafting.pory"
	.string "You don't have the materials needed for\n"
	.string "{STR_VAR_1}.$"

TMCrafting_EventScript_Loop_Text_4:
# 43 "data/scripts/tm_crafting.pory"
	.string "You can't afford to craft\n"
	.string "{STR_VAR_1}.$"

TMCrafting_EventScript_Loop_Text_5:
# 45 "data/scripts/tm_crafting.pory"
	.string "Your Bag has no room for\n"
	.string "{STR_VAR_1}.$"

TMCrafting_EventScript_Done_Text_0:
# 51 "data/scripts/tm_crafting.pory"
	.string "Come back anytime!$"
