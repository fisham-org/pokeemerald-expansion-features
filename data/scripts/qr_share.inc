@ Lets the player pick one of their unlocked QR Share backgrounds (src/data/qr_share.h).
@ Use with: call QrShare_EventScript_ChooseBackground
QrShare_EventScript_ChooseBackground::
	special QrShare_PushUnlockedBackgrounds
	dynmultistack 0, 0, FALSE, 6, FALSE, VAR_0x8004, DYN_MULTICHOICE_CB_NONE
	special QrShare_SetBackground
	return
