/*======================================================================*\
|*		Editor mined						*|
|*		keyboard mappings to commands and characters		*|
\*======================================================================*/

#include "mined.h"
#include "io.h"


/*======================================================================*\
|*			Key and function key -> command mapping		*|
\*======================================================================*/

/* The mapping between input codes and functions */

#define Sch (voidfunc) Scharacter

voidfunc key_map [256] = {
	/* characters to functions map */
  /* ^@ ^A ^B ^C ^D ^E ^F ^G */
		MARK, MPW, DCC, PGDNkey, RTkey, UPkey, MNW, GOHOP,
  /* ^H ^I ^J ^K ^L ^M ^N ^O */
		DPC, Sch, Sch, DLN, HOP, SNL, RES, APPNL,
  /* ^P ^Q ^R ^S ^T ^U ^V ^W */
		PASTEstay, HOP, PGUPkey, LFkey, DNW, CUT, CTRLINS, SU,
  /* ^X ^Y ^Z ^[ ^\ ^] ^^ ^_ */
		DNkey, COPY, SD, ESCAPE, CANCEL, MARK, DPW, (voidfunc) BADch,
  /* 20-2F 040-057 */	Sch, Sch, Sdoublequote, Sch, Sch, Sch, Sch, Ssinglequote,
		Sch, Sch, Sch, Sch, Sch, Sdash, Sch, Sch,
  /* 30-3F 060-077 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* 40-4F 100-117 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* 50-5F 120-137 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* 60-6F 140-157 */	Sgrave, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* 70-7F 160-177 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, 
		Sch, Sch, Sch, Sch, Sch, Sch, Sch, DELchar,
  /* 80-8F 200-217 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* 90-9F 220-237 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* A0-AF 240-257 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* B0-BF 260-277 */	Sch, Sch, Sch, Sch, Sacute, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* C0-CF 300-317 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* D0-DF 320-337 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* E0-EF 340-357 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
  /* F0-FF 360-377 */	Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch, Sch,
};

voidfunc mined_key_map [32] = {
	/* control characters to functions map */
  /* ^@-^G */	MARK, MPW, DCC, PGDNkey, RTkey, UPkey, MNW, GOHOP,
  /* ^H-^O */	DPC, Sch, Sch, DLN, HOP, SNL, RES, APPNL,
  /* ^P-^W */	PASTEstay, HOP, PGUPkey, LFkey, DNW, CUT, CTRLINS, SU,
  /* ^X-^_ */	DNkey, COPY, SD, ESCAPE, CANCEL, MARK, DPW, (voidfunc) BADch,
};

voidfunc ws_key_map [32] = {
	/* control characters to functions map */
  /* ^@-^G */	I, MPW, JUS, PGDNkey, RTkey, UPkey, MNW, DCC,
  /* ^H-^O */	LFkey, Sch, Sch, ctrlK, RES, SNL, APPNL, ctrlO,
  /* ^P-^W */	CTRLINS, ctrlQ, PGUPkey, LFkey, DNW, (voidfunc) BADch, TOGINS, SU,
  /* ^X-^_ */	DNkey, DLINE, SD, ESCAPE, CANCEL, (voidfunc) BADch, (voidfunc) BADch, (voidfunc) BADch,
};

voidfunc pico_key_map [32] = {
	/* control characters to functions map */
  /* ^@-^G */	MNW, MOVLBEG , LFkey, FS, DCC, MOVLEND , RTkey, HELP,
  /* ^H-^O */	DPC, Sch, JUS, DLN, RDwin, SNL, DNkey, WTU,
  /* ^P-^W */	UPkey, I, INSFILE, I, SPELL, PASTE, PGDNkey, SFW,
  /* ^X-^_ */	QUED, PGUPkey, I, ESCAPE, CANCEL, MARK, toggleMARK, CTRLINS,
};

voidfunc emacs_key_map [32] = {
	/* control characters to functions map */
  /* ^@-^G */	MARK, MOVLBEG , LFkey, I, DCC, MOVLEND , RTkey, CANCEL,
  /* ^H-^O */	HELP, Sch, Sch, DLN, RD, SNL, DNkey, APPNL,
  /* ^P-^W */	UPkey, CTRLINS, SRV, SFW, I, (voidfunc) REPT, PGDNkey, CUT,
  /* ^X-^_ */	EMAX, PASTE, SUSP, META, HOP, I, HOP, UNDO,
};

voidfunc windows_key_map [32] = {
	/* control characters to functions map */
  /* ^@-^G */	MARK, I, I, COPY, I, I, SFW, GOHOP,
  /* ^H-^O */	REPL, Sch, Sch, I, I, SNL, I, EDIT,
  /* ^P-^W */	PRINT, QUED, I, WT, I, I, PASTE, CLOSEFILE,
  /* ^X-^_ */	CUT, I, I, ESCAPE, CANCEL, I, I, CTRLINS,
};


/* mapping DOS extended function key codes -> functions
   keys as delivered by djgpp function getxkey, listed in keys.h
   first half of map is also valid for function getkey (with leading 0 byte)
   a leading ':' means: special MSDOS key mapping
*/
struct pc_fkeyentry pc_xkey_map [512] = {
					{I},
/* #define K_Alt_Escape           0x101 */	{EXED},
					{I},
/* #define K_Control_At           0x103 */	{MARK},
					{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
/* #define K_Alt_Backspace        0x10e */	{DPC, ctrl_mask},
/* #define K_BackTab              0x10f */	{MPW},
/* #define K_Alt_Q                0x110 */	{I},
/* #define K_Alt_W                0x111 */	{I},
/* #define K_Alt_E                0x112 */	{EDITMENU},
/* #define K_Alt_R                0x113 */	{I},
/* #define K_Alt_T                0x114 */	{Stag},
/* #define K_Alt_Y                0x115 */	{I},
/* #define K_Alt_U                0x116 */	{I},
/* #define K_Alt_I                0x117 */	{I},
/* #define K_Alt_O                0x118 */	{OPTIONSMENU},
/* #define K_Alt_P                0x119 */	{PARAMENU},
/* #define K_Alt_LBracket         0x11a */	{I},
/* #define K_Alt_RBracket         0x11b */	{I},
/* #define K_Alt_Return           0x11c */	{SNL, alt_mask},
					{I},
/* #define K_Alt_A                0x11e */	{I},
/* #define K_Alt_S                0x11f */	{SEARCHMENU},
/* #define K_Alt_D                0x120 */	{I},
/* #define K_Alt_F                0x121 */	{FILEMENU},
/* #define K_Alt_G                0x122 */	{I},
/* #define K_Alt_H                0x123 */	{HTML},
/* #define K_Alt_J                0x124 */	{I},
/* #define K_Alt_K                0x125 */	{toggleKEYMAP},
/* #define K_Alt_L                0x126 */	{I},
/* #define K_Alt_Semicolon        0x127 */	{I},
/* #define K_Alt_Quote            0x128 */	{I},
/* #define K_Alt_Backquote        0x129 */	{I},
					{I},
/* #define K_Alt_Backslash        0x12b */	{I},
/* #define K_Alt_Z                0x12c */	{I},
/* #define K_Alt_X                0x12d */	{AltX},
/* #define K_Alt_C                0x12e */	{I},
/* #define K_Alt_V                0x12f */	{I},
/* #define K_Alt_B                0x130 */	{I},
/* #define K_Alt_N                0x131 */	{I},
/* #define K_Alt_M                0x132 */	{I},
/* #define K_Alt_Comma            0x133 */	{I},
/* #define K_Alt_Period           0x134 */	{I},
/* #define K_Alt_Slash            0x135 */	{I},
					{I},
/* #define K_Alt_KPStar           0x137 */	{I},
					{I},
/* : getkey: Alt_Control_Space      0x139 */	{MARK},
					{I},
/* #define K_F1                   0x13b */	{F1},
/* #define K_F2                   0x13c */	{F2},
/* #define K_F3                   0x13d */	{F3},
/* #define K_F4                   0x13e */	{F4},
/* #define K_F5                   0x13f */	{F5},
/* #define K_F6                   0x140 */	{F6},
/* #define K_F7                   0x141 */	{F7},
/* #define K_F8                   0x142 */	{F8},
/* #define K_F9                   0x143 */	{F9},
/* #define K_F10                  0x144 */	{F10},
					{I}, {I},
/* #define K_Home                 0x147 */	{HOMEkey},
/* #define K_Up                   0x148 */	{UPkey},
/* #define K_PageUp               0x149 */	{PGUPkey},
/* : #define K_Alt_KPMinus          0x14a */	{screenlesslines, alt_mask},
/* #define K_Left                 0x14b */	{LFkey},
/* #define K_Center               0x14c */	{HOP},
/* #define K_Right                0x14d */	{RTkey},
/* : #define K_Alt_KPPlus           0x14e */	{screenmorelines, alt_mask},
/* #define K_End                  0x14f */	{ENDkey},
/* #define K_Down                 0x150 */	{DNkey},
/* #define K_PageDown             0x151 */	{PGDNkey},
/* #define K_Insert               0x152 */	{INSkey},
/* #define K_Delete               0x153 */	{DELkey},
/* #define K_Shift_F1             0x154 */	{F1, shift_mask},
/* #define K_Shift_F2             0x155 */	{F2, shift_mask},
/* #define K_Shift_F3             0x156 */	{F3, shift_mask},
/* #define K_Shift_F4             0x157 */	{F4, shift_mask},
/* #define K_Shift_F5             0x158 */	{F5, shift_mask},
/* #define K_Shift_F6             0x159 */	{F6, shift_mask},
/* #define K_Shift_F7             0x15a */	{F7, shift_mask},
/* #define K_Shift_F8             0x15b */	{F8, shift_mask},
/* #define K_Shift_F9             0x15c */	{F9, shift_mask},
/* #define K_Shift_F10            0x15d */	{F10, shift_mask},
/* #define K_Control_F1           0x15e */	{F1, ctrl_mask},
/* #define K_Control_F2           0x15f */	{F2, ctrl_mask},
/* #define K_Control_F3           0x160 */	{F3, ctrl_mask},
/* #define K_Control_F4           0x161 */	{F4, ctrl_mask},
/* #define K_Control_F5           0x162 */	{F5, ctrl_mask},
/* #define K_Control_F6           0x163 */	{F6, ctrl_mask},
/* #define K_Control_F7           0x164 */	{F7, ctrl_mask},
/* #define K_Control_F8           0x165 */	{F8, ctrl_mask},
/* #define K_Control_F9           0x166 */	{F9, ctrl_mask},
/* #define K_Control_F10          0x167 */	{F10, ctrl_mask},
/* #define K_Alt_F1               0x168 */	{F1, alt_mask},
/* #define K_Alt_F2               0x169 */	{F2, alt_mask},
/* #define K_Alt_F3               0x16a */	{F3, alt_mask},
/* #define K_Alt_F4               0x16b */	{F4, alt_mask},
/* #define K_Alt_F5               0x16c */	{F5, alt_mask},
/* #define K_Alt_F6               0x16d */	{F6, alt_mask},
/* #define K_Alt_F7               0x16e */	{F7, alt_mask},
/* #define K_Alt_F8               0x16f */	{F8, alt_mask},
/* #define K_Alt_F9               0x170 */	{F9, alt_mask},
/* #define K_Alt_F10              0x171 */	{F10, alt_mask},
/* #define K_Control_Print        0x172 */	{I},
/* #define K_Control_Left         0x173 */	{LFkey, ctrl_mask},
/* #define K_Control_Right        0x174 */	{RTkey, ctrl_mask},
/* #define K_Control_End          0x175 */	{ENDkey, ctrl_mask},
/* #define K_Control_PageDown     0x176 */	{PGDNkey, ctrl_mask},
/* #define K_Control_Home         0x177 */	{HOMEkey, ctrl_mask},
/* : #define K_Alt_1                0x178 */	{key_1, alt_mask},
/* : #define K_Alt_2                0x179 */	{key_2, alt_mask},
/* : #define K_Alt_3                0x17a */	{key_3, alt_mask},
/* : #define K_Alt_4                0x17b */	{key_4, alt_mask},
/* : #define K_Alt_5                0x17c */	{key_5, alt_mask},
/* : #define K_Alt_6                0x17d */	{key_6, alt_mask},
/* : #define K_Alt_7                0x17e */	{key_7, alt_mask},
/* : #define K_Alt_8                0x17f */	{key_8, alt_mask},
/* : #define K_Alt_9                0x180 */	{key_9, alt_mask},
/* : #define K_Alt_0                0x181 */	{key_0, alt_mask},
/* #define K_Alt_Dash             0x182 */	{I},
/* #define K_Alt_Equals           0x183 */	{I},
/* #define K_Control_PageUp       0x184 */	{PGUPkey, ctrl_mask},
/* #define K_F11                  0x185 */	{F11},
/* #define K_F12                  0x186 */	{F12},
/* #define K_Shift_F11            0x187 */	{F11, shift_mask},
/* #define K_Shift_F12            0x188 */	{F12, shift_mask},
/* #define K_Control_F11          0x189 */	{F11, ctrl_mask},
/* #define K_Control_F12          0x18a */	{F12, ctrl_mask},
/* #define K_Alt_F11              0x18b */	{F11, alt_mask},
/* #define K_Alt_F12              0x18c */	{F12, alt_mask},
/* #define K_Control_Up           0x18d */	{UPkey, ctrl_mask},
/* #define K_Control_KPDash       0x18e */	{screensmaller, ctrl_mask},
/* #define K_Control_Center       0x18f */	{HOP, ctrl_mask},
/* #define K_Control_KPPlus       0x190 */	{screenbigger, ctrl_mask},
/* #define K_Control_Down         0x191 */	{DNkey, ctrl_mask},
/* #define K_Control_Insert       0x192 */	{INSkey, ctrl_mask},
/* #define K_Control_Delete       0x193 */	{DELkey, ctrl_mask},
					{I},
/* : #define K_Control_KPSlash      0x195 */	{FIND},
/* : #define K_Control_KPStar       0x196 */	{GOHOP},
/* #define K_Alt_EHome            0x197 */	{smallHOMEkey, alt_mask},
/* #define K_Alt_EUp              0x198 */	{UPkey, alt_mask},
/* #define K_Alt_EPageUp          0x199 */	{PGUPkey, alt_mask},
					{I},
/* #define K_Alt_ELeft            0x19b */	{LFkey, alt_mask},
					{I},
/* #define K_Alt_ERight           0x19d */	{RTkey, alt_mask},
					{I},
/* #define K_Alt_EEnd             0x19f */	{smallENDkey, alt_mask},
/* #define K_Alt_EDown            0x1a0 */	{DNkey, alt_mask},
/* #define K_Alt_EPageDown        0x1a1 */	{PGDNkey, alt_mask},
/* #define K_Alt_EInsert          0x1a2 */	{smallINSkey, alt_mask},
/* #define K_Alt_EDelete          0x1a3 */	{smallDELkey, alt_mask},
/* #define K_Alt_KPSlash          0x1a4 */	{FIND, alt_mask},
/* #define K_Alt_Tab              0x1a5 */	{GOHOP},
/* #define K_Alt_Enter            0x1a6 */	{SNL, alt_mask},
					{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},

	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
					{I}, {I}, {I}, {I}, {I}, {I}, {I},
/* #define K_EHome                0x247 */	{smallHOMEkey},
/* #define K_EUp                  0x248 */	{UPkey},
/* #define K_EPageUp              0x249 */	{PGUPkey},
					{I},
/* #define K_ELeft                0x24b */	{LFkey},
					{I},
/* #define K_ERight               0x24d */	{RTkey},
					{I},
/* #define K_EEnd                 0x24f */	{smallENDkey},
/* #define K_EDown                0x250 */	{DNkey},
/* #define K_EPageDown            0x251 */	{PGDNkey},
/* #define K_EInsert              0x252 */	{smallINSkey},
/* #define K_EDelete              0x253 */	{smallDELkey},
					{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
					{I}, {I}, {I},
/* #define K_Control_ELeft        0x273 */	{LFkey, ctrl_mask},
/* #define K_Control_ERight       0x274 */	{RTkey, ctrl_mask},
/* #define K_Control_EEnd         0x275 */	{smallENDkey, ctrl_mask},
/* #define K_Control_EPageDown    0x276 */	{PGDNkey, ctrl_mask},
/* #define K_Control_EHome        0x277 */	{smallHOMEkey, ctrl_mask},
					{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
					{I}, {I}, {I}, {I},
/* #define K_Control_EPageUp      0x284 */	{PGUPkey, ctrl_mask},
					{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
/* #define K_Control_EUp          0x28d */	{UPkey, ctrl_mask},
					{I}, {I}, {I},
/* #define K_Control_EDown        0x291 */	{DNkey, ctrl_mask},
/* #define K_Control_EInsert      0x292 */	{smallINSkey, ctrl_mask},
/* #define K_Control_EDelete      0x293 */	{smallDELkey, ctrl_mask},
					{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
	{I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I}, {I},
};


#ifndef msdos

/* mapping table: function key escape sequences -> mined functions */
struct fkeyentry fkeymap [] = {
	/* Alt-Letter sequences for manual menu activation */
/*
	{"\033f",	FILEMENU},
	{"\033e",	EDITMENU},
	{"\033s",	SEARCHMENU},
	{"\033p",	PARAMENU},
	{"\033o",	OPTIONSMENU},
*/

	{"\033]",	OSC},

	/* Alt-Letter sequences for keyboard switching functions 
	   to be recognised on prompt line */
	{"\033k",	toggleKEYMAP},
	{"\033K",	setupKEYMAP},
	{"\033I",	setupKEYMAP},
	{"\033x",	AltX},

	/* Alt-Digit sequences for diacritic combination prefixes */
	{"\0330", key_0, alt_mask},
	{"\0331", key_1, alt_mask},
	{"\0332", key_2, alt_mask},
	{"\0333", key_3, alt_mask},
	{"\0334", key_4, alt_mask},
	{"\0335", key_5, alt_mask},
	{"\0336", key_6, alt_mask},
	{"\0337", key_7, alt_mask},
	{"\0338", key_8, alt_mask},
	{"\0339", key_9, alt_mask},

	/* VT100 and Sun */
	{"\033[A",	UPkey},	/* move cursor up	*/
	{"\033[B",	DNkey},	/* more cursor down	*/
	{"\033[C",	RTkey},	/* more cursor right	*/
	{"\033[D",	LFkey},	/* more cursor left	*/

	/* Sun raw function key codes */
	{"\033[215z",	UPkey},	/* move cursor up	*/
	{"\033[221z",	DNkey},	/* more cursor down	*/
	{"\033[219z",	RTkey},	/* more cursor right	*/
	{"\033[217z",	LFkey},	/* more cursor left	*/

	/* Linux console, Cygwin console */
	{"\033[1~",	smallHOMEkey},	/* Pos1, VT220: FIND (@0)	*/
	{"\033[4~",	smallENDkey},	/* End, VT220: SELECT (*6)	*/
	{"\033[[A",	F1},	/* F1	*/
	{"\033[[B",	F2},	/* F2	*/
	{"\033[[C",	F3},	/* F3	*/
	{"\033[[D",	F4},	/* F4	*/
	{"\033[[E",	F5},	/* F5	*/
	{"\033[G",	HOP},	/* 5	*/

	/* found somewhere? */
	{"\033[1G",	HOP},	/* pattern for shifted 5? */

	/* PuTTY */
	{"\033OG",	HOP},	/* 5	*/

	/* Linux X / xterm */
	{"\033OP",	F1},	/* F1, VT100 PF1	*/
	{"\033OQ",	F2},	/* F2, VT100 PF2	*/
	{"\033OR",	F3},	/* F3, VT100 PF3	*/
	{"\033OS",	F4},	/* F4, VT100 PF4	*/
	{"\033OH",	HOMEkey},	/* Pos1	*/
	{"\033OF",	ENDkey},	/* End	*/
	{"\033OE",	HOP},	/* 5	*/

	/* non-application mode xterm */
	{"\033[H",	smallHOMEkey},	/* Pos1, esp on left keypad	*/
	{"\033[F",	smallENDkey},	/* End, esp on left keypad	*/
	{"\033[E",	HOP},	/* 5, Begin, hanterm keypad 5	*/

	/* xterm generic patterns for modified keypad keys */
	{"\033[1H",	HOMEkey},	/* Pos1	*/
	{"\033[1F",	ENDkey},	/* End	*/
	{"\033[1E",	HOP},	/* 5	*/
	{"\033[1A",	UPkey},	/* Up	*/
	{"\033[1D",	LFkey},	/* Left	*/
	{"\033[1B",	DNkey},	/* Down	*/
	{"\033[1C",	RTkey},	/* Right	*/

	/* xterm generic patterns for modified function keys */
	{"\033[1P",	F1},	/* F1	*/
	{"\033[1Q",	F2},	/* F2	*/
	{"\033[1R",	F3},	/* F3	*/
	{"\033[1S",	F4},	/* F4	*/

	/* xterm specials */
	{"\033[200~",	I},	/* begin bracketed paste */
	{"\033[201~",	I},	/* end bracketed paste */

	/* Deprecated; mined keyboard add-ons: shifted digits etc*/
	{"\033[00~",	key_0},
	{"\033[01~",	key_1},
	{"\033[02~",	key_2},
	{"\033[03~",	key_3},
	{"\033[04~",	key_4},
	{"\033[05~",	key_5},
	{"\033[06~",	key_6},
	{"\033[07~",	key_7},
	{"\033[08~",	key_8},
	{"\033[09~",	key_9},
	{"\033[~",	DPC},	/* modified BackSpace (ctrl/shift) */

	/* various modified keypad keys (non-application mode),
	   e.g. with xterm *VT100*modifyCursorKeys: 1 or 0;
	   not complete (wrt modifiers);
	   would be detected generically with TERM=xterm-sco
	 */
	{"\033[2H",	smallHOMEkey, shift_mask},	/* shift-Pos1	*/
	{"\033[2F",	smallENDkey, shift_mask},	/* shift-Ende	*/
	{"\033[5H",	smallHOMEkey, ctrl_mask},	/* control-Pos1	*/
	{"\033[5F",	smallENDkey, ctrl_mask},	/* control-Ende	*/

	{"\0332A",	UPkey, shift_mask},	/* shift-Up	*/
	{"\0332B",	DNkey, shift_mask},	/* shift-Down	*/
	{"\0332C",	RTkey, shift_mask},	/* shift-Right	*/
	{"\0332D",	LFkey, shift_mask},	/* shift-Left	*/
	{"\0335A",	UPkey, ctrl_mask},	/* control-Up	*/
	{"\0335B",	DNkey, ctrl_mask},	/* control-Down	*/
	{"\0335C",	RTkey, ctrl_mask},	/* control-Right	*/
	{"\0335D",	LFkey, ctrl_mask},	/* control-Left	*/
	{"\0336A",	UPkey, ctrlshift_mask},	/* control-shift-Up	*/
	{"\0336B",	DNkey, ctrlshift_mask},	/* control-shift-Down	*/
	{"\0336C",	RTkey, ctrlshift_mask},	/* control-shift-Right	*/
	{"\0336D",	LFkey, ctrlshift_mask},	/* control-shift-Left	*/

	/* mined shortcuts */
	{"\033\r",	Popmark},	/* alt-Enter	*/
	{"\033\n",	Popmark},	/* alt-Enter	*/

	/* vim mouse wheel mappings for xterm */
	{"\033[64~",	PGUPkey},	/* shift-wheel-up */
	{"\033[65~",	PGDNkey},	/* shift-wheel-down */
	{"\033[62~",	SU},	/* wheel-up */
	{"\033[63~",	SD},	/* wheel-down */

	/* various VT100-style Fn function keys */
	{"\033[11~",	F1},	/* F1	*/
	{"\033[12~",	F2},	/* F2	*/
	{"\033[13~",	F3},	/* F3	*/
	{"\033[14~",	F4},	/* F4	*/
	{"\033[15~",	F5},	/* F5	*/
	{"\033[17~",	F6},	/* F6	*/
	{"\033[18~",	F7},	/* F7	*/
	{"\033[19~",	F8},	/* F8	*/
	{"\033[20~",	F9},	/* F9	*/
	{"\033[21~",	F10},	/* F10	*/

	/* VT220-style Fn function keys (control-Fn with xterm VT220 mode) */
	{"\033[23~",	F11},	/* F11 (also shift-F1)	*/
	{"\033[24~",	F12},	/* F12 (also shift-F2)	*/
	/* the following 10 (F3...F12) are also attached to Control-Fn 
	   by xterm 216 with the "old function keys" option -
	   it should better not be used, especially as the DO (vt220) 
	   sequence is ambiguously emitted by both Shift-F6 / Control-F6 
	   and Menu keys
	 */
	{"\033[25~",	F3, shift_mask},	/* F13	*/
	{"\033[26~",	F4, shift_mask},	/* F14	*/
	{"\033[28~",	F5, shift_mask},	/* VT220: HELP (%1) */
	{"\033[29~",	HOP},		/* VT220: DO (%0 redo), xterm: Menu */
	{"\033[31~",	F7, shift_mask},	/* F17	*/
	{"\033[32~",	F8, shift_mask},	/* F18	*/
	{"\033[33~",	F9, shift_mask},	/* F19	*/
	{"\033[34~",	F10, shift_mask},	/* F20	*/
	/* xterm VT220 mode Fn function keys (control-Fn, actually),
	   since xterm 216, also with oldFunctionKeys mode,
	   also extended Fn function keys with modifyFunctionKeys disabled
	 */
	{"\033[42~",	F11, shift_mask}, /* F11 (also control-F1) */
	{"\033[43~",	F12, shift_mask}, /* F12 (also control-F2) */

	/* xterm extended Fn function keys with modifyFunctionKeys disabled,
	   since xterm 216
	 */
	{"\033[44~",	F3, ctrl_mask}, /* control-F3 */
	{"\033[45~",	F4, ctrl_mask}, /* control-F4 */
	{"\033[46~",	F5, ctrl_mask}, /* control-F5 */
	{"\033[47~",	F6, ctrl_mask}, /* control-F6 */
	{"\033[48~",	F7, ctrl_mask}, /* control-F7 */
	{"\033[49~",	F8, ctrl_mask}, /* control-F8 */
	{"\033[50~",	F9, ctrl_mask}, /* control-F9 */
	{"\033[51~",	F10, ctrl_mask}, /* control-F10 */
	{"\033[52~",	F11, ctrl_mask}, /* control-F11 (also ctrl-shft-F1) */
	{"\033[53~",	F12, ctrl_mask}, /* control-F12 (also ctrl-shft-F2) */

	{"\033[54~",	F3, ctrlshift_mask}, /* control-shift-F3 */
	{"\033[55~",	F4, ctrlshift_mask}, /* control-shift-F4 */
	{"\033[56~",	F5, ctrlshift_mask}, /* control-shift-F5 */
	{"\033[57~",	F6, ctrlshift_mask}, /* control-shift-F6 */
	{"\033[58~",	F7, ctrlshift_mask}, /* control-shift-F7 */
	{"\033[59~",	F8, ctrlshift_mask}, /* control-shift-F8 */
	{"\033[60~",	F9, ctrlshift_mask}, /* control-shift-F9 */
	{"\033[61~",	F10, ctrlshift_mask}, /* control-shift-F10 */
	{"\033[62~",	F11, ctrlshift_mask}, /* control-shift-F11 */
	{"\033[63~",	F12, ctrlshift_mask}, /* control-shift-F12 */

	/* further VT100 keys - some have been disabled by linux sequences */
	{"\033[6~",	PGDNkey},	/* Next Screen, xterm PgDn	*/
	{"\033[5~",	PGUPkey},	/* Prev Screen, xterm PgUp	*/
	{"\033[3~",	DELkey},	/* Remove, xterm Del	*/
	{"\033[2~",	INSkey},	/* Insert Here, xterm Ins	*/
	{"\033[@",	HOMEkey},	/* Pos1, PC-xterm	*/
	{"\033[e",	ENDkey},	/* End, PC-xterm	*/

	/* application cursor keys */
	{"\033OA",	UPkey},	/* up	*/
	{"\033OB",	DNkey},	/* dn	*/
	{"\033OC",	RTkey},	/* rt	*/
	{"\033OD",	LFkey},	/* lf	*/
	{"\033O@",	HOMEkey},	/* Pos1, PC-xterm	*/
	{"\033Oe",	ENDkey},	/* End, PC-xterm	*/

	/* application keypad */
	{"\033O ",	HOP},	/* Space, also old hanterm keypad 5 ?	*/
	{"\033OI",	HOP},	/* shift-Tab */
	{"\033OM",	SNL},	/* Enter (@8) */
	{"\033Oo",	FIND},	/* PF1, keypad /	*/
	{"\033Oj",	GOHOP},	/* PF2, keypad *	*/
	{"\033Om",	SU},	/* PF3, keypad -	*/
	{"\033Ok",	SD},	/* PF4, keypad +	*/

	{"\033Op",	INSkey},	/* Ins (kI), keypad 0	*/
	{"\033Oq",	ENDkey},	/* End (K4), keypad 1	*/
	{"\033Or",	DNkey},		/* dn (kd), keypad 2	*/
	{"\033Os",	PGDNkey},	/* PgDn (K5), keypad 3	*/
	{"\033Ot",	LFkey},		/* lf (kl), keypad 4	*/
	{"\033Ou",	HOP},		/* mid (K2), keypad 5	*/
	{"\033Ov",	RTkey},		/* rt (kr), keypad 6	*/
	{"\033Ow",	HOMEkey},	/* Pos1 (K1), keypad 7	*/
	{"\033Ox",	UPkey},		/* up (ku), keypad 8	*/
	{"\033Oy",	PGUPkey},	/* PgUp (K3), keypad 9	*/
	{"\033On",	DELkey},	/* Del (kD), keypad .	*/
	{"\033Ol",	DELkey},	/* Del, keypad ,	*/

	/* mintty generic patterns for modified application mode keys */
	{"\033[1p",	INSkey},	/* Ins (kI), keypad 0	*/
	{"\033[1q",	ENDkey},	/* End (K4), keypad 1	*/
	{"\033[1r",	DNkey},		/* dn (kd), keypad 2	*/
	{"\033[1s",	PGDNkey},	/* PgDn (K5), keypad 3	*/
	{"\033[1t",	LFkey},		/* lf (kl), keypad 4	*/
	{"\033[1u",	HOP},		/* mid (K2), keypad 5	*/
	{"\033[1v",	RTkey},		/* rt (kr), keypad 6	*/
	{"\033[1w",	HOMEkey},	/* Pos1 (K1), keypad 7	*/
	{"\033[1x",	UPkey},		/* up (ku), keypad 8	*/
	{"\033[1y",	PGUPkey},	/* PgUp (K3), keypad 9	*/
	{"\033[1n",	DELkey},	/* Del (kD), keypad .	*/
	{"\033[1l",	DELkey},	/* Del, keypad ,	*/
	{"\033[1M",	SNL},		/* Enter (@8) */
	{"\033[1o",	FIND},		/* keypad /	*/
	{"\033[1j",	GOHOP},		/* keypad *	*/
	{"\033[1m",	KP_minus},	/* keypad -	*/
	{"\033[1k",	KP_plus},	/* keypad +	*/

	/* mintty generic patterns for modified TAB */
	{"\033[1Z",	STAB},	/* TAB	*/

	/* mlterm generic patterns for modified application mode keys */
	{"\033O0p",	INSkey},	/* Ins (kI), keypad 0	*/
	{"\033O0q",	ENDkey},	/* End (K4), keypad 1	*/
	{"\033O0r",	DNkey},		/* dn (kd), keypad 2	*/
	{"\033O0s",	PGDNkey},	/* PgDn (K5), keypad 3	*/
	{"\033O0t",	LFkey},		/* lf (kl), keypad 4	*/
	{"\033O0u",	HOP},		/* mid (K2), keypad 5	*/
	{"\033O0v",	RTkey},		/* rt (kr), keypad 6	*/
	{"\033O0w",	HOMEkey},	/* Pos1 (K1), keypad 7	*/
	{"\033O0x",	UPkey},		/* up (ku), keypad 8	*/
	{"\033O0y",	PGUPkey},	/* PgUp (K3), keypad 9	*/
	{"\033O0n",	DELkey},	/* Del (kD), keypad .	*/
	{"\033O0o",	FIND},		/* keypad /	*/
	{"\033O0j",	GOHOP},		/* keypad *	*/
	{"\033O0m",	KP_minus},	/* keypad -	*/
	{"\033O0k",	KP_plus},	/* keypad +	*/

	/* Sun */
	{"\033[208z",	F6, shift_mask},	/* R1 -> grave */
	{"\033[209z",	F6, ctrl_mask},		/* R2 -> circumflex */
	{"\033[210z",	F6},			/* R3 -> acute */
	{"\033[211z",	F5},			/* R4 -> diaeresis */
	{"\033[212z",	F5, shift_mask},	/* R5 -> tilde */
	{"\033[213z",	F5, ctrl_mask},		/* R6 -> angstrom */
	{"\033[214z",	SU},	/* R7	*/
	{"\033[216z",	PGUPkey},	/* R9	*/
	{"\033[218z",	HOP},	/* R11	*/
	{"\033[220z",	SD},	/* R13	*/
	{"\033[222z",	PGDNkey},	/* R15	*/
	{"\033[254z",	GOMA},	/* - */
	{"\033[253z",	MARK},	/* + */
	{"\033[250z",	COPY},	/* Enter */
	{"\033[249z",	DELkey},	/* Del */
	{"\033[247z",	INSkey, ctrl_mask},	/* Ins */
	{"\033[1z",	HOMEkey},	/* Home/Pos1, non-appl */
	{"\033[4z",	ENDkey},	/* End, non-appl */
	{"\033[2z",	INSkey},	/* Ins on VT100 block */
	{"\033[3z",	DELkey},	/* Del (xterm/Sun) */
	{"\033[5z",	PGUPkey},	/* PgUp on VT100 block */
	{"\033[6z",	PGDNkey},	/* PgDn on VT100 block */
	{"\033[192z",	MARK},	/* Stop (F11 with xterm "Sun Function-Keys") */
	{"\033[193z",	AGAIN},	/* Again (F12 with xterm "Sun Function-Keys") */
	{"\033[194z",	FS},	/* Props */
	{"\033[195z",	I},	/* Undo */
	{"\033[197z",	COPY},	/* Copy */
	{"\033[199z",	INSkey},	/* Paste */
	{"\033[200z",	FIND},	/* Find */
	{"\033[201z",	CUT},	/* Cut */
	{"\033[196z",	HELP},	/* Help */

	{"\033[224z",	F1},	/* F1, xterm "Sun Function-Keys" */
	{"\033[225z",	F2},	/* F2 */
	{"\033[226z",	F3},	/* F3 */
	{"\033[227z",	F4},	/* F4 */
	{"\033[228z",	F5},	/* F5 */
	{"\033[229z",	F6},	/* F6 */
	{"\033[230z",	F7},	/* F7 */
	{"\033[231z",	F8},	/* F8 */
	{"\033[232z",	F9},	/* F9 */
	{"\033[233z",	F10},	/* F10 */
	{"\033[234z",	F11},	/* F11 */
	{"\033[235z",	F12},	/* F12 */

	/* Iris */
	{"\033[209q",	HOMEkey},	/* Print Screen */
	{"\033[213q",	ENDkey},	/* Scroll Lock */
/*	{"\033[217q",	I},	*//* Pause */
	{"\033[139q",	INSkey},	/* Insert, Ins */
	{"\033[P",	DELkey},	/* Del */
/*	{"\033[H",	smallHOMEkey},	*//* Home */
	{"\033[150q",	PGUPkey},	/* PgUp */
	{"\033[146q",	smallENDkey},	/* End */
	{"\033[154q",	PGDNkey},	/* PgDn */
	{"\033[000q",	HOP},	/* 5 */
	{"\033[001q",	F1},	/* F1 */
	{"\033[002q",	F2},	/* F2 */
	{"\033[003q",	F3},	/* F3 */
	{"\033[004q",	F4},	/* F4 */
	{"\033[005q",	F5},	/* F5 */
	{"\033[006q",	F6},	/* F6 */
	{"\033[007q",	F7},	/* F7 */
	{"\033[008q",	F8},	/* F8 */
	{"\033[009q",	F9},	/* F9 */
	{"\033[010q",	F10},	/* F10 */
	{"\033[011q",	F11},	/* F11 */
	{"\033[012q",	F12},	/* F12 */
	{"\033[013q",	F1, shift_mask},	/* shift-F1 ? */
	{"\033[014q",	F2, shift_mask},	/* shift-F2 ? */
	{"\033[015q",	F3, shift_mask},	/* shift-F3 ? */
	{"\033[016q",	F4, shift_mask},	/* shift-F4 ? */
	{"\033[017q",	F5, shift_mask},	/* shift-F5 ? */
	{"\033[018q",	F6, shift_mask},	/* shift-F6 ? */
	{"\033[019q",	F7, shift_mask},	/* shift-F7 ? */
	{"\033[020q",	F8, shift_mask},	/* shift-F8 ? */
	{"\033[021q",	F9, shift_mask},	/* shift-F9 ? */
	{"\033[022q",	F10, shift_mask},	/* shift-F10 */
	{"\033[023q",	F11, shift_mask},	/* shift-F11 */
	{"\033[024q",	F12, shift_mask},	/* shift-F12 */

	/* VT100+, older xterm, Mach console, AT&T, QNX ansi */
	{"\033OT",	F5},	/* F5	*/
	{"\033OU",	F6},	/* F6	*/
	{"\033OV",	F7},	/* F7	*/
	{"\033OW",	F8},	/* F8	*/
	{"\033OX",	F9},	/* F9, conflict with VT102 appl. mode =	*/
	{"\033OY",	F10},	/* F10	*/
	{"\033OZ",	F11},	/* F11	*/

	/* rxvt */
	{"\033[7~",	smallHOMEkey},	/* 	*/
	{"\033[8~",	smallENDkey},	/* 	*/
	{"\033[Z",	HOP},	/* shift-TAB, also mintty */

	/* SCO ansi */
	{"\033[L",	INSkey},	/* (termcap: insert character) */

	/* xterm mouse */
	{"\033[M",	DIRECTxterm},	/* direct cursor address */
	{"\033[t",	TRACKxterm},	/* xterm mouse highlight tracking */
	{"\033[T",	TRACKxtermT},	/* xterm mouse highlight tracking */
	{"\033[O",	FOCUSout},	/* window loses focus */
	{"\033[I",	FOCUSin},	/* window gains focus */

	/* DEC locator */
	{"\033[&w",	DIRECTvtlocator},	/* DECterm locator */

	/* mintty ambiguous width change report */
	{"\033[1W",	AMBIGnarrow},	/* ambig. width chars are narrow */
	{"\033[2W",	AMBIGwide},	/* ambig. width chars are wide */

	/* mintty application escape key mode (enabled with ^[[?7727h) */
	{"\033O[",	ESCAPE},	/* ESC */

	/* crttool */
	{"\033m",	DIRECTcrttool},	/* direct cursor address */

	{NIL_PTR}
};

struct fkeyentry fkeymap_vt52 [] = {
	/* VT52 and HP */
	{"\033A",	UPkey},	/* up	*/
	{"\033B",	DNkey},	/* dn	*/
	{"\033C",	RTkey},	/* rt	*/
	{"\033D",	LFkey},	/* lf	*/
	{"\033P",	F1},	/* PF1 */
	{"\033Q",	F2},	/* PF2 */
	{"\033R",	F3},	/* PF3 */
	{"\033S",	F4},	/* PF4 */

	/* keypad application mode */
	{"\033?p",	INSkey},	/* keypad 0 */
	{"\033?q",	ENDkey},	/* keypad 1 */
	{"\033?r",	DNkey},	/* keypad 2 */
	{"\033?s",	PGDNkey},	/* keypad 3 */
	{"\033?t",	LFkey},	/* keypad 4 */
	{"\033?u",	HOP},	/* keypad 5 */
	{"\033?v",	RTkey},	/* keypad 6 */
	{"\033?w",	HOMEkey},	/* keypad 7 */
	{"\033?x",	UPkey},	/* keypad 8 */
	{"\033?y",	PGUPkey},	/* keypad 9 */
	{"\033?l",	DELkey},	/* keypad , */
	{"\033?m",	SU},	/* keypad - */
	{"\033?n",	DELkey},	/* keypad . */
	{"\033?M",	SNL},	/* keypad ENTER */

	/* xterm/VT52 application mode */
	{"\033?j",	GOHOP},	/* keypad * */
	{"\033?k",	SD},	/* keypad + */
	{"\033?o",	FIND},	/* keypad / */
	{"\033?X",	HOP},	/* keypad = */
	{"\033?I",	STAB},	/* Tab */
	{"\033? ",	SSPACE},	/* Space ? */

	/* xterm/VT52 */
	{"\033H",	HOMEkey},	/* Pos1	*/
	{"\033F",	ENDkey},	/* End	*/
	{"\033E",	HOP},		/* keypad 5	*/
	{"\0331H",	HOMEkey},	/* modified Pos1	*/
	{"\0331F",	ENDkey},	/* modified End	*/
	{"\0331E",	HOP},		/* modified keypad 5	*/
	{"\0331A",	UPkey},	/* modified up	*/
	{"\0331B",	DNkey},	/* modified dn	*/
	{"\0331C",	RTkey},	/* modified rt	*/
	{"\0331D",	LFkey},	/* modified lf	*/
	{"\0331P",	F1},	/* modified PF1 (could be) */
	{"\0331Q",	F2},	/* modified PF2 (could be) */
	{"\0331R",	F3},	/* modified PF3 (could be) */
	{"\0331S",	F4},	/* modified PF4 (could be) */

	{NIL_PTR}
};

struct fkeyentry fkeymap_hp [] = {
	/* VT52 and HP */
	{"\033A",	UPkey},	/* up	*/
	{"\033B",	DNkey},	/* dn	*/
	{"\033C",	RTkey},	/* rt	*/
	{"\033D",	LFkey},	/* lf	*/
	{"\033P",	DELkey}, /* VT52 PF1 Del */
	{"\033Q",	INSkey}, /* VT52 PF2 Ins (termcap: insert character) */
	{"\033R",	HOP},	/* VT52 PF3 */
	{"\033S",	PGDNkey}, /* VT52 PF4
				   termcap xterm-hp: PgDn
				   termcap hp*: scroll-forward/backward key
				  */
	/* HP */
	{"\033T",	PGUPkey}, /* termcap xterm-hp: PgUp
				   termcap hp*: scroll-backward/forward key
				  */
	{"\033h",	HOMEkey},	/* Pos1	*/
	{"\033F",	ENDkey},	/* End	*/
	{"\033V",	PGUPkey},
	{"\033U",	PGDNkey},
	{"\033L",	APPNL},	/* insert line	*/
	{"\033J",	DELchar},	/* erase (clear to (end of) screen)	*/
	{"\033K",	DLN},	/* clear to end of line	*/
	{"\033M",	DLINE},	/* delete line	*/
	{"\033i",	MPW},	/* back-tab	*/
	{"\033p",	F1},	/* F1 */
	{"\033q",	F2},	/* F2 */
	{"\033r",	F3},	/* F3 */
	{"\033s",	F4},	/* F4 */
	{"\033t",	F5},	/* F5 */
	{"\033u",	F6},	/* F6 */
	{"\033v",	F7},	/* F7 */
	{"\033w",	F8},	/* F8 */

	/* xterm HP mode generic patterns for modified function keys */
	{"\033[1p",	F1},	/* F1 */
	{"\033[1q",	F2},	/* F2 */
	{"\033[1r",	F3},	/* F3 */
	{"\033[1s",	F4},	/* F4 */
	{"\033[1t",	F5},	/* F5 */
	{"\033[1u",	F6},	/* F6 */
	{"\033[1v",	F7},	/* F7 */
	{"\033[1w",	F8},	/* F8 */

	/* xterm HP mode generic patterns for modified keypad keys */
	{"\033[1h",	HOMEkey},	/* Pos1	*/
	{"\033[1T",	PGUPkey},	/* PgUp */
	{"\033[1S",	PGDNkey},	/* PgDn */
	{"\033[1Q",	INSkey},	/* Ins */
	{"\033[1P",	DELkey},	/* Del */

	/* crttool */
/*	{"\033m",	DIRECTcrttool},	direct cursor address */

	/* ?? */
	{"\033&",	SNL},	/* keypad Enter */

	{NIL_PTR}
};

#ifdef hp_console_dont_know_how_to_trigger_this
	/* HP workstation keyboard without NumLock */
	/* conflicts with VT100+ etc, see above */
	{"\033OU",	MARK},	/* Pos1	*/
	{"\033OV",	LFkey},
	{"\033OW",	UPkey},
	{"\033OX",	RTkey},
	{"\033OY",	DNkey},
	{"\033OZ",	PGUPkey},	/* PgUp	*/
	{"\033O?",	PGDNkey},	/* PgDn; End, Ins, Del?	*/
#endif

struct fkeyentry fkeymap_rxvt [] = {
	/* rxvt */
	{"\033[2~",	INSkey, ctrl_mask},	/* small keypad Ins key	*/
	{"\033[3~",	DELkey, ctrl_mask},	/* small keypad Del key	*/
	/* xterm */
	{"\033[M",	DIRECTxterm},	/* direct cursor address */
	/* Shift-function keys: shifted by 2 against older conventions */
	{"\033[23~",	F11},	/* F11, shift-F1 */
	{"\033[24~",	F12},	/* F12, shift-F2 */
	{"\033[25~",	F3, shift_mask},
	{"\033[26~",	F4, shift_mask},
	{"\033[28~",	F5, shift_mask},
	{"\033[29~",	F6, shift_mask},
	{"\033[31~",	F7, shift_mask},
	{"\033[32~",	F8, shift_mask},
	{"\033[33~",	F9, shift_mask},
	{"\033[34~",	F10, shift_mask},
	{"\033[23$",	F11, shift_mask},
	{"\033[24$",	F12, shift_mask},

	/* rxvt-unicode application keypad */
	{"\033OU",	HOMEkey},	/* Pos1	*/
	{"\033OV",	LFkey},	/* lf	*/
	{"\033OW",	UPkey},	/* up	*/
	{"\033OX",	RTkey},	/* rt	*/
	{"\033OY",	DNkey},	/* dn	*/
	{"\033OZ",	PGUPkey},	/* PgUp	*/
	{"\033O]",	HOP},	/* 5	*/
	{"\033O\\",	ENDkey},	/* End	*/
	{"\033O[",	PGDNkey},	/* PgDn	*/
	{"\033O^",	INSkey},	/* Ins	*/
	{"\033O_",	DELkey},	/* Del	*/

	{NIL_PTR}
};

struct fkeyentry fkeymap_linux [] = {
	/* Linux console F-keys */
	{"\033[23~",	F11},
	{"\033[24~",	F12},
	{"\033[25~",	F1, shift_mask},
	{"\033[26~",	F2, shift_mask},
	{"\033[28~",	F3, shift_mask},
	{"\033[29~",	F4, shift_mask},
	{"\033[31~",	F5, shift_mask},
	{"\033[32~",	F6, shift_mask},
	{"\033[33~",	F7, shift_mask},
	{"\033[34~",	F8, shift_mask},

	/* override terminfo-derived settings */
	{"\033[1~",	smallHOMEkey},	/* small Pos1	*/
	{"\033[4~",	smallENDkey},	/* small End	*/
	{"\033[3~",	smallDELkey},	/* small Del	*/

	{NIL_PTR}
};

struct fkeyentry fkeymap_xterm [] = {
	/* xterm */
	{"\033[M",	DIRECTxterm},	/* direct cursor address */
	{"\033[t",	TRACKxterm},	/* xterm mouse highlight tracking */
	{"\033[T",	TRACKxtermT},	/* xterm mouse highlight tracking */
	{"\033[O",	FOCUSout},	/* window loses focus */
	{"\033[I",	FOCUSin},	/* window gains focus */

	/* override wrong terminfo-derived settings */
	{"\033[1~",	smallHOMEkey},	/* small Pos1	*/
	{"\033[4~",	smallENDkey},	/* small End	*/

	{NIL_PTR}
};

struct fkeyentry fkeymap_vt100 [] = {
	{"\033OX",	HOP},	/* appl. mode =, conflict with older xterm F9 */

	/* override wrong terminfo-derived settings */
	{"\033Op",	INSkey},	/* Ins (kI), keypad 0	*/
	{"\033Oq",	ENDkey},	/* End (K4), keypad 1	*/
	{"\033Or",	DNkey},		/* dn (kd), keypad 2	*/
	{"\033Os",	PGDNkey},	/* PgDn (K5), keypad 3	*/
	{"\033Ot",	LFkey},		/* lf (kl), keypad 4	*/
	{"\033Ou",	HOP},		/* mid (K2), keypad 5	*/
	{"\033Ov",	RTkey},		/* rt (kr), keypad 6	*/
	{"\033Ow",	HOMEkey},	/* Pos1 (K1), keypad 7	*/
	{"\033Ox",	UPkey},		/* up (ku), keypad 8	*/
	{"\033Oy",	PGUPkey},	/* PgUp (K3), keypad 9	*/
	{"\033On",	DELkey},	/* Del (kD), keypad .	*/
	{"\033Ol",	DELkey},	/* Del, keypad ,	*/

	/* BSD console */
	{"\033[7~",	HOMEkey},	/* 	*/
	{"\033[8~",	ENDkey},	/* 	*/

	{NIL_PTR}
};

struct fkeyentry fkeymap_scoansi [] = {
	{"\033[M",	F1},	/* F1 */
	{"\033[N",	F2},	/* F2 */
	{"\033[O",	F3},	/* F3 */
	{"\033[P",	F4},	/* F4 */
	{"\033[Q",	F5},	/* F5 */
	{"\033[R",	F6},	/* F6 */
	{"\033[S",	F7},	/* F7 */
	{"\033[T",	F8},	/* F8 */
	{"\033[U",	F9},	/* F9 */
	{"\033[V",	F10},	/* F10 */
	{"\033[W",	F11},	/* F11, PuTTY SCO */
	{"\033[X",	F12},	/* F12, PuTTY SCO */
	{"\033[Y",	F1, shift_mask},	/* shift-F1, PuTTY SCO */
	{"\033[Z",	F2, shift_mask},	/* shift-F2, PuTTY SCO */
	{"\033[a",	F3, shift_mask},	/* shift-F3, PuTTY SCO */
	{"\033[b",	F4, shift_mask},	/* shift-F4, PuTTY SCO */
	{"\033[c",	F5, shift_mask},	/* shift-F5, PuTTY SCO */
	{"\033[d",	F6, shift_mask},	/* shift-F6, PuTTY SCO */
	{"\033[e",	F7, shift_mask},	/* shift-F7, PuTTY SCO */
	{"\033[f",	F8, shift_mask},	/* shift-F8, PuTTY SCO */
	{"\033[g",	F9, shift_mask},	/* shift-F9, PuTTY SCO */
	{"\033[h",	F10, shift_mask},	/* shift-F10, PuTTY SCO */
	{"\033[i",	F11, shift_mask},	/* shift-F11, PuTTY SCO */
	{"\033[j",	F12, shift_mask},	/* shift-F12, PuTTY SCO */
	{"\033[k",	F1, ctrl_mask},	/* control-F1, PuTTY SCO */
	{"\033[l",	F2, ctrl_mask},	/* control-F2, PuTTY SCO */
	{"\033[m",	F3, ctrl_mask},	/* control-F3, PuTTY SCO */
	{"\033[n",	F4, ctrl_mask},	/* control-F4, PuTTY SCO */
	{"\033[o",	F5, ctrl_mask},	/* control-F5, PuTTY SCO */
	{"\033[p",	F6, ctrl_mask},	/* control-F6, PuTTY SCO */
	{"\033[q",	F7, ctrl_mask},	/* control-F7, PuTTY SCO */
	{"\033[r",	F8, ctrl_mask},	/* control-F8, PuTTY SCO */
	{"\033[s",	F9, ctrl_mask},	/* control-F9, PuTTY SCO */
	{"\033[t",	F10, ctrl_mask},	/* control-F10, PuTTY SCO */
	{"\033[u",	F11, ctrl_mask},	/* control-F11, PuTTY SCO */
	{"\033[v",	F12, ctrl_mask},	/* control-F12, PuTTY SCO */
	{"\033OA",	UPkey, ctrl_mask},	/* control-up, PuTTY SCO */
	{"\033OB",	DNkey, ctrl_mask},	/* control-dn, PuTTY SCO */
	{"\033OC",	RTkey, ctrl_mask},	/* control-rt, PuTTY SCO */
	{"\033OD",	LFkey, ctrl_mask},	/* control-lf, PuTTY SCO */
	{"\033[L",	INSkey},	/* (termcap: insert character) */
	{"\033[H",	HOMEkey},	/* 	*/
	{"\033[F",	ENDkey},	/* (xterm-sco)	*/
	{"\033[G",	PGDNkey},	/* 	*/
	{"\033[I",	PGUPkey},	/* 	*/

	{NIL_PTR}
};

struct fkeyentry fkeymap_interix [] = {
	/* Interix console ("Windows Services for UNIX") */
	{"\033[H",	HOMEkey},
	{"\033[S",	PGUPkey},
	{"\033[T",	PGDNkey},
	{"\033[U",	ENDkey},
	{"\033[F-",	UPkey, shift_mask},
	{"\033[F+",	DNkey, shift_mask},
	{"\033[F$",	RTkey, shift_mask},
	{"\033[F^",	LFkey, shift_mask},
	{"\033[L",	INSkey},
/*	{"\037",	DELkey, ctrl_mask},	ctrl-DEL sends ^_ */

	{"\033F1",	F1},
	{"\033F2",	F2},
	{"\033F3",	F3},
	{"\033F4",	F4},
	{"\033F5",	F5},
	{"\033F6",	F6},
	{"\033F7",	F7},
	{"\033F8",	F8},
	{"\033F9",	F9},
	{"\033FA",	F10},
	{"\033FB",	F11},
	{"\033FC",	F12},
	{"\033FD",	F1, shift_mask},
	{"\033FE",	F2, shift_mask},
	{"\033FF",	F3, shift_mask},
	{"\033FG",	F4, shift_mask},
	{"\033FH",	F5, shift_mask},
	{"\033FI",	F6, shift_mask},
	{"\033FJ",	F7, shift_mask},
	{"\033FK",	F8, shift_mask},
	{"\033FL",	F9, shift_mask},
	{"\033FM",	F10, shift_mask},
	{"\033FN",	F11, shift_mask},
	{"\033FO",	F12, shift_mask},
	{"\033FP",	F1, alt_mask},
	{"\033FQ",	F2, alt_mask},
	{"\033FR",	F3, alt_mask},
	{"\033FS",	F4, alt_mask},
	{"\033FT",	F5, alt_mask},
	{"\033FU",	F6, alt_mask},
	{"\033FV",	F7, alt_mask},
	{"\033FW",	F8, alt_mask},
	{"\033FX",	F9, alt_mask},
	{"\033FY",	F10, alt_mask},
	{"\033FZ",	F11, alt_mask},
	{"\033Fa",	F12, alt_mask},
	{"\033Fb",	F1, ctrl_mask},
	{"\033Fc",	F2, ctrl_mask},
	{"\033Fd",	F3, ctrl_mask},
	{"\033Fe",	F4, ctrl_mask},
	{"\033Ff",	F5, ctrl_mask},
	{"\033Fg",	F6, ctrl_mask},
	{"\033Fh",	F7, ctrl_mask},
	{"\033Fi",	F8, ctrl_mask},
	{"\033Fj",	F9, ctrl_mask},
	{"\033Fk",	F10, ctrl_mask},
	{"\033Fm",	F11, ctrl_mask},
	{"\033Fn",	F12, ctrl_mask},
	{"\033Fo",	F1, ctrlshift_mask},
	{"\033Fp",	F2, ctrlshift_mask},
	{"\033Fq",	F3, ctrlshift_mask},
	{"\033Fr",	F4, ctrlshift_mask},
	{"\033Fs",	F5, ctrlshift_mask},
	{"\033Ft",	F6, ctrlshift_mask},
	{"\033Fu",	F7, ctrlshift_mask},
	{"\033Fv",	F8, ctrlshift_mask},
	{"\033Fw",	F9, ctrlshift_mask},
	{"\033Fx",	F10, ctrlshift_mask},
	{"\033Fy",	F11, ctrlshift_mask},
	{"\033Fz",	F12, ctrlshift_mask},

	{NIL_PTR}
};

struct fkeyentry fkeymap_siemens [] = {
	/* Siemens 97801/97808 */
	{"\033[Z",	MPW},		/* |<- */
	{"\0339",	MPW},		/* <~- */
	{"\033:",	MNW},		/* -~> */
	{"\0336",	HOP},		/* key next to END, above cursor block */
	{"\033[@",	INSkey},	/* Insert */
	{"\033[S",	PGUPkey},
	{"\033[T",	PGDNkey},
	{"\033^",	HOP},

	{"\033@",	F1},	/* F1 */
	{"\033A",	F2},	/* F2 */
	{"\033B",	F3},	/* F3 */
	{"\033C",	F4},	/* F4 */
	{"\033D",	F5},	/* F5 */
	{"\033F",	F6},	/* F6 */
	{"\033G",	F7},	/* F7 */
	{"\033H",	F8},	/* F8 */
	{"\033I",	F9},	/* F9 */
	{"\033J",	F10},	/* F10 */
	{"\033K",	F11},	/* F11 */
	{"\033L",	F12},	/* F12 */
	{"\033M",	F3, ctrl_mask},	/* F13 */
	{"\033N",	F4, ctrl_mask},	/* F14 */
	{"\033O",	F5, ctrl_mask},	/* F15 */
	{"\033P",	F6, ctrl_mask},	/* F16 */
	{"\0330",	F7, ctrl_mask},	/* F17 */
	{"\033_",	F8, ctrl_mask},	/* F18 */
	{"\033d",	F9, ctrl_mask},	/* F19 */
	{"\033T",	F10, ctrl_mask},	/* F20 */
	{"\033V",	F11, ctrl_mask},	/* F21 */
	{"\033X",	F12, ctrl_mask},	/* F22 */
	{"\033 ",	F1, shift_mask},	/* shift-F1 */
	{"\033;",	F2, shift_mask},	/* shift-F2 */
	{"\033\"",	F3, shift_mask},	/* shift-F3 */
	{"\033#",	F4, shift_mask},	/* shift-F4 */
	{"\033$",	F5, shift_mask},	/* shift-F5 */
	{"\033%",	F6, shift_mask},	/* shift-F6 */
	{"\033&",	F7, shift_mask},	/* shift-F7 */
	{"\033'",	F8, shift_mask},	/* shift-F8 */
	{"\033<",	F9, shift_mask},	/* shift-F9 */
	{"\033=",	F10, shift_mask},	/* shift-F10 */
	{"\033*",	F11, shift_mask},	/* shift-F11 */
	{"\033+",	F12, shift_mask},	/* shift-F12 */
	{"\033,",	F3, ctrlshift_mask},	/* control-shift-F13 */
	{"\033-",	F4, ctrlshift_mask},	/* control-shift-F14 */
	{"\033.",	F5, ctrlshift_mask},	/* control-shift-F15 */
	{"\033/",	F6, ctrlshift_mask},	/* control-shift-F16 */
	{"\0331",	F7, ctrlshift_mask},	/* control-shift-F17 */
	{"\0332",	F8, ctrlshift_mask},	/* control-shift-F18 */
	{"\0333",	F9, ctrlshift_mask},	/* control-shift-F19 */
	{"\033U",	F10, ctrlshift_mask},	/* control-shift-F20 */
	{"\033W",	F11, ctrlshift_mask},	/* control-shift-F21 */
	{"\033Y",	F12, ctrlshift_mask},	/* control-shift-F22 */

	{NIL_PTR}
};

#endif	/* #ifndef msdos */


/*======================================================================*\
|*				End					*|
\*======================================================================*/
