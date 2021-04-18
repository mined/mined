/*======================================================================*\
|*		Editor mined						*|
|*		Function key dispatcher					*|
\*======================================================================*/

#include "mined.h"
#include "charprop.h"
#include "io.h"


/*======================================================================*\
|*	Debug function dispatching					*|
\*======================================================================*/

#define dont_debug_dispatch

#ifdef debug_dispatch
#define trace_dispatch(func)	do_debug_dispatch (func)
#else
#define trace_dispatch(func)	
#endif

#ifdef debug_dispatch
static
void
do_debug_dispatch (func)
  char * func;
{
  if (keyshift & applkeypad_mask) {
	printf ("appl-keypad ");
  }
  if (keyshift & ctrl_mask) {
	printf ("ctrl-");
  }
  if (keyshift & alt_mask) {
	printf ("alt-");
  }
  if (keyshift & shift_mask) {
	printf ("shift-");
  }
  printf ("%s\n", func);
}
#endif


/*======================================================================*\
|*	Dispatch function keys and some keypad keys to mined functions	*|
\*======================================================================*/

/**
   HOMEkey () is invoked by a Home key on the right (application) keypad;
   function configurable
 */
void
HOMEkey ()
{
  trace_dispatch ("Home");

  if (! (keyshift & altctrlshift_mask) && home_selection) {
	MARK ();
	return;
  }

  if (shift_selection) {
	if (keyshift & alt_mask) {
		keyshift &= ~ alt_mask;
	} else if (apply_shift_selection) {
		trigger_highlight_selection ();

		if (keyshift & ctrl_mask) {
			keyshift = 0;
			BFILE ();
		} else {
			BLINEORUP ();
		}
		return;
	}
  }

#ifdef weird_stuff_disabling_alt_in_xterm
  if ((keyshift & (alt_mask | applkeypad_mask)) == alt_mask) {
	smallHOMEkey ();
	return;
  }
#endif

  if (keyshift & shift_mask) {
	keyshift = 0;
	MARK ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	BLINEORUP ();
  } else {
	if (mined_keypad == ! (keyshift & alt_mask)) {
		MARK ();
	} else {
		BLINEORUP ();
	}
  }
}

/**
   smallHOMEkey () is invoked by a Home key on the small keypad;
   function configurable
 */
void
smallHOMEkey ()
{
  trace_dispatch ("smallHome");

  if (! (keyshift & altctrlshift_mask) &&
      (small_home_selection || (shift_selection && ! mined_keypad))) {
	MARK ();
	return;
  }

  if (shift_selection) {
	if (keyshift & alt_mask) {
		keyshift &= ~ alt_mask;
	} else if (apply_shift_selection) {
		trigger_highlight_selection ();

		if (keyshift & ctrl_mask) {
			keyshift = 0;
			BFILE ();
		} else {
			BLINEORUP ();
		}
		return;
	}
  }

  if (keyshift & shift_mask) {
	keyshift = 0;
	MARK ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	MOVLBEG ();
  } else {
	if (mined_keypad == ! (keyshift & alt_mask)) {
		BLINEORUP ();
	} else {
		MARK ();
	}
  }
}

/**
   ENDkey () is invoked by an End key on the right (application) keypad;
   function configurable
 */
void
ENDkey ()
{
  trace_dispatch ("End");

  if (! (keyshift & altctrlshift_mask) && home_selection) {
	COPY ();
	return;
  }

  if (shift_selection) {
	if (keyshift & alt_mask) {
		keyshift &= ~ alt_mask;
	} else if (apply_shift_selection) {
		trigger_highlight_selection ();

		if (keyshift & ctrl_mask) {
			keyshift = 0;
			EFILE ();
		} else {
			ELINEORDN ();
		}
		return;
	}
  }

#ifdef weird_stuff_disabling_alt_in_xterm
  if ((keyshift & (alt_mask | applkeypad_mask)) == alt_mask) {
	smallENDkey ();
	return;
  }
#endif

  if (keyshift & shift_mask) {
	keyshift = 0;
	COPY ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	ELINEORDN ();
  } else {
	if (mined_keypad == ! (keyshift & alt_mask)) {
		COPY ();
	} else {
		ELINEORDN ();
	}
  }
}

/**
   smallENDkey () is invoked by an End key on the small keypad;
   function configurable
 */
void
smallENDkey ()
{
  trace_dispatch ("smallEnd");

  if (! (keyshift & altctrlshift_mask) &&
      (small_home_selection || (shift_selection && ! mined_keypad))) {
	COPY ();
	return;
  }

  if (shift_selection) {
	if (keyshift & alt_mask) {
		keyshift &= ~ alt_mask;
	} else if (apply_shift_selection) {
		trigger_highlight_selection ();

		if (keyshift & ctrl_mask) {
			keyshift = 0;
			EFILE ();
		} else {
			ELINEORDN ();
		}
		return;
	}
  }

  if (keyshift & shift_mask) {
	keyshift = 0;
	COPY ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	MOVLEND ();
  } else {
	if (mined_keypad == ! (keyshift & alt_mask)) {
		ELINEORDN ();
	} else {
		COPY ();
	}
  }
}


#define not_alt_del_always_deletes_char
#define alt_alt_may_delete_char

#ifdef alt_del_always_deletes_char
#define cut_cond (mined_keypad && ! (keyshift & (alt_mask | applkeypad_mask)))
#else
#ifdef alt_alt_may_delete_char
#define cut_cond (mined_keypad == ! (keyshift & alt_mask))
#else
#define cut_cond (mined_keypad == ! ((keyshift & (alt_mask | applkeypad_mask)) == (alt_mask | applkeypad_mask)))
#endif
#endif

/**
   DELkey () is invoked by a Delete function key, 
   often KP_Delete on right keypad;
   function configurable
 */
void
DELkey ()
{
  trace_dispatch ("Delete");

  if ((keyshift & ctrl_mask) && ! (keyshift & alt_mask)) {
	/*keyshift = 0;*/
	DCC ();
  } else if ((keyshift & shift_mask) && ! (keyshift & alt_mask)) {
	keyshift = 0;
	CUT ();
  } else {
#ifdef oldstyle_DELkey
	/*
	mined_keypad alt_mask applkeypad_mask	alt-KP	alt-DEL	alt-ALT
		1	0	0		cut !	cut	cut
		1	0	1		*	*	*
		1	1	0		cut	del	del
		1	1	1		del	del	del
		0	0	0		del !	del	del
		0	0	1		*	*	*
		0	1	0		del	del	cut
		0	1	1		cut	del	cut
	*/
	if (cut_cond) {
		CUT ();
	} else {
		DCC ();
	}
#else
	if (has_active_selection ()) {
		if (keyshift & alt_mask) {
			DCC ();
		} else {
			CUT ();
		}
	} else {
		if (keyshift & alt_mask) {
			CUT ();
		} else {
			DCC ();
		}
	}
#endif
  }
}

/**
   smallDELkey () is invoked by the small keypad ("extended") Delete key
 */
void
smallDELkey ()
{
  trace_dispatch ("smallDelete");

  keyshift |= ctrl_mask;
  DELkey ();
}

/**
   DELchar () is invoked by a DEL character unless reconfigured to 
   map to DPC;
   the key may be a Delete function key (on small keypad, if so configured),
   or the Backarrow key (unless configured to emit the BackSpace character)
 */
void
DELchar ()
{
  trace_dispatch ("DEL");

  if (keyshift & shift_mask) {
	keyshift = 0;
	CUT ();
  } else if (keyshift & ctrl_mask) {
	DCC ();
  } else {
	if (hop_flag > 0 || keyshift & alt_mask) {
		hop_flag = 0;
		CUT ();
	} else {
		DCC ();
	}
  }
}

void
INSkey ()
{
  trace_dispatch ("Insert");

  if (! hop_flag && (keyshift & alt_mask)) {
	keyshift = 0;
	YANKRING ();
  } else if (keyshift & ctrl_mask) {
	PASTEstay ();
  } else {
	PASTE ();
  }
}

/**
   smallINSkey () is invoked by the small keypad ("extended") Insert key
 */
void
smallINSkey ()
{
  trace_dispatch ("smallInsert");

  keyshift |= ctrl_mask;
  INSkey ();
}


/**
   Keypad keys with special functions
 */
void
KP_plus ()
{
  if (keyshift & (ctrl_mask | shift_mask)) {
	if ((keyshift & altctrlshift_mask) == altctrlshift_mask) {
		screenmorecols ();
	} else if ((keyshift & altctrlshift_mask) == ctrlshift_mask) {
		screenmorelines ();
	} else {
		fontbigger ();
	}
  } else {
	SD ();
  }
}

void
KP_minus ()
{
  if (keyshift & (ctrl_mask | shift_mask)) {
	if ((keyshift & altctrlshift_mask) == altctrlshift_mask) {
		screenlesscols ();
	} else if ((keyshift & altctrlshift_mask) == ctrlshift_mask) {
		screenlesslines ();
	} else {
		fontsmaller ();
	}
  } else {
	SU ();
  }
}

/**
   Display function key help lines
 */
static voidfunc fhelp_func = F1;
static char fhelp_keyshift = 0;

void
display_FHELP ()
{
/* use the following screen attributes:
			unicode indicator (cyan background)
			highlight (red background)
			reverse
			plain (normal)
*/
  if (fhelp_func == COMPOSE) {
	status_uni ("ctrl- , ¸/ͺ . ˙/῾ :/\" ¨ ; ˛ ' ´tonos ´ oxia - ¯ <  ̌/᾿ ( ˘ )  ̑ & hook ^ ~ ` / °");
  } else if (fhelp_func == key_1) {
    if (fhelp_keyshift == altctrl_mask) {
	status_uni ("alt-ctrl- 1 ˘/ ̛ + ´ 2 ˘/ ̛ + ` 3 ˘/ ̛ +  ̉ 4 ˘/ ̛ + ~ 5 ˘/ ̛ +  ̣ 6 ῞ 7 ῝ 8 ῟ 9  0 ");
    } else if (fhelp_keyshift == ctrl_mask) {
	status_uni ("ctrl- 1 ´ 2 ` 3  ̉ 4 ~ 5  ̣ 6 ^ 7 ˘ 8  ̛ 9 / 0 °");
    } else {
	status_uni ("alt- 1 ^ + ´ 2 ^ + ` 3 ^ +  ̉ 4 ^ + ~ 5 ^ +  ̣ 6 ῎ 7 ῍ 8 ῏ 9  0 ");
    }
  } else {
    if ((fhelp_keyshift & altctrlshift_mask) == altshift_mask) {
	status_uni ("alt-shift-               F4 Kill window! F5 ˘ F6 ˙");
    } else if ((fhelp_keyshift & altctrlshift_mask) == ctrlshift_mask) {
	status_uni ("ctrl-shift-                         F5 ˛ F6 ¯    F8 LineRepl F9 FindChar");
    } else if (fhelp_keyshift & alt_mask) {
	status_uni ("alt-   F2 SaveAs    F4 Kill window!       F8 SearchBack F9 NextBack F10 FlagMenu");
    } else if (fhelp_keyshift & ctrl_mask) {
	status_uni ("ctrl- F2 SaveTo  F4 PastePrev F5 °/¸ F6 ^  F8 Subst/Confirm  F10 InfoMenu");
    } else if (fhelp_keyshift & shift_mask) {
	status_uni ("shift- F2 Save! F3 CaseTog F4 WrtBuf F5 ~ F6 `  F8 Subst F9 FindIdf F10 Menu");
    } else {
	status_uni ("       F2 Save F3 Edit F4 InsFile F5 \" F6 ´  F8 Search F9 Next F10 Filemenu");
    }
  }
}

void
toggle_FHELP ()
{
  always_disp_help = ! always_disp_help;
}

void
FHELP (func)
  voidfunc func;
{
  fhelp_func = func;
  fhelp_keyshift = keyshift;
  if (hop_flag > 0) {
	hop_flag = 0;
	always_disp_help = ! always_disp_help;
  } else {
	display_FHELP ();
  }
}


void
F1 ()
{
  trace_dispatch ("F1");

  if (keyshift & altctrlshift_mask) {
	FHELP (F1);
  } else {
	HELP ();
  }
}

void
F2 ()
{
  trace_dispatch ("F2");

  if (keyshift & ctrl_mask) {
	/* enforce creation of file info file */
	hop_flag = 1;
  }

  if (keyshift & alt_mask) {
	keyshift = 0;
	SAVEAS ();
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	WTU ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	EXED ();
  } else {
	WT ();
  }
}

void
F3 ()
{
  trace_dispatch ("F3");

  if (keyshift & alt_mask) {
	keyshift = 0;
	SELECTFILE ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	VIEW ();
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	LOWCAPWORD ();
  } else {
	EDIT ();
  }
}

void
F4 ()
{
  trace_dispatch ("F4");

  if (keyshift & shift_mask) {
	keyshift = 0;
	WB ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	YANKRING ();
  } else {
	INSFILE ();
  }
}

#ifdef dispatch_accents
/* accent prefix keys are now handled generically in edit.c:
  insert_prefix (F5);
  insert_prefix (F6);
 */
void
F5 ()
{
  trace_dispatch ("F5");

  /*	standard function key assignment:
		F5	F6
		"	´
	shift	~	`
	ctrl	°	^
  */
  if (keyshift & ctrl_mask) {
	keyshift = 0;
	insert_angstrom ();
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	insert_tilde ();
  } else {
	insert_diaeresis ();
  }
}

void
F6 ()
{
  trace_dispatch ("F6");

  /*	standard function key assignment:
		F5	F6
		"	´
	shift	~	`
	ctrl	°	^
  */
  if (keyshift & ctrl_mask) {
	keyshift = 0;
	insert_circumflex ();
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	insert_grave ();
  } else {
	insert_acute ();
  }
}
#endif

void
FIND ()
{
  if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
	keyshift = 0;
	if (hop_flag > 0) {
		Stag ();
	} else {
		LR ();
	}
  } else if ((keyshift & altctrl_mask) == altctrl_mask) {
	keyshift = 0;
	if (hop_flag > 0) {
		hop_flag = 0;
		SCURCHAR (REVERSE);
	} else {
		REPL ();
	}
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	if (hop_flag > 0) {
		hop_flag = 0;
		SCURCHAR (FORWARD);
	} else {
		REPL ();
	}
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	if (hop_flag > 0) {
		hop_flag = 0;
		Stag ();
	} else {
		GR ();
	}
  } else if (keyshift & alt_mask) {
	keyshift = 0;
	SRV ();
  } else {
	SFW ();
  }
}

void
AGAIN ()
{
  if ((keyshift & altctrlshift_mask) == altctrlshift_mask) {
	keyshift = 0;
	SCURCHAR (REVERSE);
  } else if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
	keyshift = 0;
	SCURCHAR (FORWARD);
  } else if ((keyshift & altshift_mask) == altshift_mask) {
	keyshift = 0;
	SIDF (REVERSE);
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	SIDFW ();
  } else if (keyshift & alt_mask) {
	RESreverse ();
  } else {
	RES ();
  }
}

void
F7 ()
{
  trace_dispatch ("F7");

  FIND ();
}

void
F8 ()
{
  trace_dispatch ("F8");

  FIND ();
}

void
F9 ()
{
  trace_dispatch ("F9");

  AGAIN ();
}

void
F10 ()
{
  trace_dispatch ("F10");

  if (keyshift & alt_mask) {
	keyshift = 0;
	handleFlagmenus ();
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	QUICKMENU ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	handleFlagmenus ();
  } else {
	FILEMENU ();
  }
}

void
F11 ()
{
  trace_dispatch ("F11");

  if (keyshift & alt_mask) {
	if (keyshift & shift_mask) {
		hop_flag = 1;
	}
	search_wrong_enc ();
  } else if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
	hop_flag = 1;
	changeuni ();
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	hop_flag = 1;
	LOWCAP ();
  } else if (keyshift & ctrl_mask) {
	UML (' ');
  } else {
	LOWCAP ();
  }
}

void
F12 ()
{
  trace_dispatch ("F12");

  if (keyshift & alt_mask) {
	if (keyshift & ctrl_mask) {
		toggleKEYMAP ();
	} else {
		switchAltScreen ();
	}
  } else if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
	SAVPOS ();
  } else if (keyshift & ctrl_mask) {
	setupKEYMAP ();
  } else if (keyshift & shift_mask) {
	KEYREC ();
  } else {
	KEYREC ();
  }
}


/*======================================================================*\
|*			Generic command processing functions		*|
\*======================================================================*/

/*
 * return the mined command associated with the key value
 */
voidfunc
command (c)
  unsigned long c;
{
  if (c == FUNcmd) {
	return keyproc;
  } else if (/* c >= 0 && */ c < arrlen (key_map)) {
	return key_map [c];
  } else {
	return Scharacter;
  }
}


/**
   Invoke function associated with the key.
 */
void
invoke_key_function (key)
  unsigned long key;
{
  (command (key)) (key);
}


/*
 * BAD complains about unknown command characters.
 */
static
void
BAD (c, tag)
  unsigned long c;
  char * tag;
{
  char cmdbuf [34];
  char * cbuf = cmdbuf;

  strcpy (cmdbuf, "Unknown command: ");
  if (tag) {
	strcat (cmdbuf, tag);
  }
  while (* cbuf) {
	cbuf ++;
  }

  if (no_char (c)) {
	strcpy (cbuf, "<unknown character>");
  } else if (c < ' ') {
	cbuf [0] = '^';
	cbuf [1] = c + '@';
	cbuf [2] = '\0';
  } else {
	(void) utfencode (c, cbuf);
  }

  ring_bell ();
  status_uni (cmdbuf);
}

void
BADch (c)
  unsigned long c;
{
  BAD (c, "");
}

/*
 * Ignore this keystroke.
 */
void
I ()
{
}

/*
 * 'HOP' key function expander.
 */
void
HOP ()
{
  if (shift_selection && (keyshift & shift_mask)) {
	keyshift &= ~ shift_mask;
	COPY ();
	return;
  }

  hop_flag = 2;
  if (MENU) {
	displayflags ();
	set_cursor_xy ();
	flush ();
  }
  if (! char_ready_within (500, NIL_PTR)) {
	status_uni ("HOP: type command (to amplify/expand) ...");
  }
}

/*
 * Cancel prefix function.
 */
void
CANCEL ()
{
  hop_flag = 0;
  clear_status ();
}


/**
   Read ANSI escape sequence that has been started with ESC [
   (but too slowly to be recognised on keyboard input level);
   swallow [, ? or > (for reports), 0-9, and ; characters 
   plus final letter, @ or ~
 */
static
void
CSI ()
{
  character c;
  FLAG msg_shown = False;

  if (stat_visible) {
	ring_bell ();
  } else {
	status_uni ("... absorbing delayed terminal escape sequence ... - Press Enter to abort");
	msg_shown = True;
  }
  flush ();

  while ((c = read1byte ()) == '[' || c == '?' || c == '>' || c == ';'
	|| (c >= '0' && c <= '9')) {
  }

  if (msg_shown) {
	error ("(Discarded slow escape sequence) - Re-enter function key");
  }
}

/**
   Read escape sequence that has been started with ESC O
   (but too slowly to be recognised on keyboard input level);
   swallow 0-9, and ; characters plus final character
 */
static
void
ESCO ()
{
  character c;
  FLAG msg_shown = False;

  if (stat_visible) {
	ring_bell ();
  } else {
	status_uni ("... Absorbing delayed terminal escape sequence ... - Press Enter to abort");
	msg_shown = True;
  }
  flush ();

  while ((c = read1byte ()) == ';' || (c >= '0' && c <= '9')) {
  }

  if (msg_shown) {
	error ("(Discarded slow escape sequence) - Re-enter function key");
  }
}

/**
   Read terminal report string which has been started with ESC ]
 */
void
OSC ()
{
  character c;
  FLAG msg_shown = False;

  if (stat_visible) {
	ring_bell ();
  } else {
	status_uni ("... Absorbing delayed terminal report string ... - Press Enter to abort");
	msg_shown = True;
  }
  flush ();

  while ((c = read1byte ()) >= ' ') {
  }
  if (c == '\033') {
	read1byte ();	/* '\\' */
  }

  if (msg_shown) {
	clear_status ();
  }
}


#define cmd_char(c)	(c < '\040' ? c + '\100' : (c >= '\140' ? c - '\040' : c))

/*
 * Interpret control-Q commands. Most can be implemented with the Hop function.
 */
void
ctrlQ ()
{
  unsigned long c;
  voidfunc func;

  if (! char_ready_within (500, NIL_PTR)) {
	status_uni ("^Q: blockBegin Find replAce goto<n>mark HOP...");
  }
  if (quit) {
	return;
  }

  c = readcharacter_unicode ();
  if (quit) {
	return;
  }

  clear_status ();
  if ('0' <= c && c <= '9') {
	GOMAn ((int) c - (int) '0');
	return;
  }
  if (c == '\033' || c == quit_char) {
	CANCEL ();
	return;
  }
  switch (cmd_char (c)) {
	case 'B' : {GOMA () ; return;}
	case 'K' : { ; return;}		/* not exactly WS function */
	case 'P' : { ; return;}		/* not exactly WS function */
	case 'V' : { ; return;}		/* not exactly WS function */
	case 'W' :			/* not exactly WS function */
	case 'Z' :			/* not exactly WS function */
	case 'Y' :
	case '\177' : {
			func = command (c);
			hop_flag = 1;
			(* func) (c);
			return;
		      }
	case 'F' : {if (hop_flag > 0) {
			SRV ();
		    } else {
			SFW ();
		    }
		    return;
		   }
	case 'A' : {if (hop_flag > 0) {
			REPL ();
		    } else {
			GR ();
		    }
		    return;
		   }
	case 'Q' : {REPT (' '); return;}	/* not exactly WS function */
	case 'L' :			/* not exactly WS function */
/*
^Q: B/K top/bottom block
    P last position
    W/Z continuous scroll
    V last find or block
    Y/DEL delete line right/left
    0-9 marker
    F find
    A replace
    Q repeat next key/command
    L find misspelling
*/
	default : {
		func = command (c);
		if (func != Scharacter) {
			hop_flag = 1;
			keyshift |= alt_mask;
			(* func) (c);
		} else {
			BAD (c, "^Q ");
		}
		return;
	}
  }
}

/*
 * Interpret control-K commands.
 */
void
ctrlK ()
{
  unsigned long c;

  if (! char_ready_within (500, NIL_PTR)) {
	status_msg ("^K: Save Done eXit Quit Read Log <n>mark / block: B/K mark Cop Ydel moV Wr...");
  }
  if (quit) {
	return;
  }

  c = readcharacter_unicode ();
  if (quit) {
	return;
  }

  clear_status ();
  if ('0' <= c && c <= '9') {
	MARKn ((int) c - (int) '0');
	return;
  }
  if (c == '\033' || c == quit_char) {
	CANCEL ();
	return;
  }
  switch (cmd_char (c)) {
	case 'S' : {WTU (); return;}
	case 'D' : {EXFILE (); return;}
	case 'X' : {EXMINED (); return;}
	case 'Q' : {QUED (); return;}
	case 'B' : {setMARK (True) ; return;}
	case 'K' : {COPY () ; return;}	/* not exactly WS function */
	case 'H' : { ; return;}		/* not exactly WS function */
	case 'C' : {PASTE () ; return;}	/* not exactly WS function */
	case 'Y' : {CUT () ; return;}	/* not exactly WS function */
	case 'V' : {PASTE (); return;}	/* not exactly WS function */
	case 'W' : {WB (); return;}	/* not exactly WS function */
	case 'N' : { ; return;}		/* not exactly WS function */
	case 'R' : {INSFILE (); return;}
	case 'L' : {CHDI (); return;}
/*
^K  0-9 set/hide marker
    B/K block begin/end
    H block hide
    C/Y/V/W block copy/delete/move/write
    N column block
*/
	default : {
		BAD (c, "^K ");
		return;
	}
  }
}

/*
 * Interpret control-O commands.
 */
void
ctrlO ()
{
  unsigned long c;

  if (! char_ready_within (500, NIL_PTR)) {
	status_msg ("^O: L/R left/right margins...");
  }
  if (quit) {
	return;
  }

  c = readcharacter_unicode ();
  if (quit) {
	return;
  }

  clear_status ();
  if ('0' <= c && c <= '9') {
	return;
  }
  if (c == '\033' || c == quit_char) {
	CANCEL ();
	return;
  }
  switch (cmd_char (c)) {
	case 'L' : {ADJLM (); return;}
	case 'R' : {ADJRM (); return;}
	case 'G' : {ADJFLM () /* actually paragraph tab */; return;}
/*
^O  L/R/M set left/right margin /release
    I/N set/clear tab
    F ruler from line
    C center line
    S set line spacing
    W toggle word wrap
    T toggle ruler line
    J toggle justify
    V     vari-tabs
    H     hyph-help
    E     soft hyph
    D     print display
    P     page break
*/
	default : {
		BAD (c, "^O ");
		return;
	}
  }
}

/*
 * Set marker / go to marker.
 */
void
MARKER ()
{
  unsigned long c;

  status_msg ("0..9: set marker / , or blank: default marker");
  c = readcharacter_unicode ();
  if (quit) {
	return;
  }

  clear_status ();
  if (c == '\033' || c == quit_char) {
	CANCEL ();
  } else if ('0' <= c && c <= '9') {
	MARKn ((int) c - (int) '0');
  } else if ('a' <= c && c <= 'f') {
	MARKn ((int) c - (int) 'a' + 10);
  } else if (c == ',' || c == '\'' || c == ' ' || c == ']' || c == '\035') {
	setMARK (True);
  } else {
	BAD (c, "mark ");
  }
}

void
GOMARKER ()
{
  unsigned long c;

  status_msg ("0..9: go marker / blank: default marker");
  c = readcharacter_unicode ();
  if (quit) {
	return;
  }

  clear_status ();
  if (c == '\033' || c == quit_char) {
	CANCEL ();
  } else if ('0' <= c && c <= '9') {
	GOMAn ((int) c - (int) '0');
  } else if ('a' <= c && c <= 'f') {
	GOMAn ((int) c - (int) 'a' + 10);
  } else if (c == ',' || c == '.' || c == 'g' || c == 'G' || c == '\'' || 
      c == ' ' || c == ']' || c == '\035') {
	GOMA ();
  } else {
	BAD (c, "go mark ");
  }
}


/*
   Toggle TAB width.
 */
void
toggle_tabsize ()
{
  static int prev_tabsize = 2;

  if (hop_flag > 0) {
	toggle_tab_expansion ();
	return;
  }

  if (tabsize == 2 || tabsize == 8) {
	prev_tabsize = tabsize;
	tabsize = 4;
  } else if (prev_tabsize == 8) {
	tabsize = 2;
  } else {
	tabsize = 8;
  }
  RDwin ();
}

/*
 * Interpret Escape commands.
 */
void
ESCAPE ()
{
  unsigned long c;
  voidfunc func;
  FLAG quick_alt = False;

  if (char_ready_within (50, NIL_PTR)) {
	/* try to distinguish Alt- from explicit ESC */
	quick_alt = True;
  }
  if (! char_ready_within (450, NIL_PTR)) {
	status_uni ("ESCexit SPACEmenu quit /search \\backw (match replace goto justify =rept help ...");
  }
  if (quit) {
	return;
  }

  c = readcharacter_unicode ();
  if (quit) {
	return;
  }

  clear_status ();

  if ('0' <= c && c <= '9') {
	REPT (c);
	return;
  }

  trace_keytrack ("ESCAPE", c);
  switch (c) {
	case '[' : {CSI (); return;}
	case 'O' : {ESCO (); return;}
	case ']' : {OSC (); return;}
	case '\'' :
		{
			if (quick_alt) { /* detect Alt-' vs. ESC ' */
				break;
			}
			if (keyshift & alt_mask) {
				break;
			} else {
				GOMARKER (); return;
			}
		}
	case '\033' : {EXED (); return;}
	case '\r' :
	case '\n' :
		if (keyshift & ctrlshift_mask) {
			break;
		} else {
			Popmark ();
			return;
		}
	case '.' :
		{
			show_vt100_graph = False;
			if (hop_flag > 0) {
				RDcenter ();
			} else {
				RDwin ();
			}
			return;
		}
	case 'q' : {QUED (); return;}
	case '/' : {SFW (); return;}
	case '\\' : {SRV (); return;}
	case 'R' : {LR (); return;}
	case 'r' : {REPL (); return;}
	case 'w' : {WT (); return;}
	case 'W' : {WTU (); return;}
	case 'v' : {VIEW (); return;}
	case 'V' : {toggle_VIEWmode (); return;}
	case 'g' : {GOTO (); return;}
	case 'h' : {HELP (); return;}
	case '?' : {FS (); return;}
	case 'm' : {MARKER (); return;}
	case 'i' : {INSFILE (); return;}
	case 'b' : {WB (); return;}
	case '=' : {REPT (' '); return;}
	case 'z' : {SUSP (); return;}
	case 'd' : {CHDI (); return;}
	case '!' : {SH (); return;}
	case '@' :
	case '^' : {setMARK (True); return;}
	case 'n' : {NN (); return;}
	case 'c' : {CMD (); return;}
	case 'u' : {display_code (); return;}
	case 'U' : {changeuni (); return;}
	case 'X' : {changehex (); return;}
	case 'A' : {changeoct (); return;}
	case 'D' : {changedec (); return;}
	case '+' : {NXTFILE (); return;}
	case '-' : {PRVFILE (); return;}
	case '#' : {SELECTFILE (); return;}
	case '%' : {screensmaller (); return;}
	case '&' : {screenbigger (); return;}
	case 'l' : {screenlesslines (); return;}
	case 'L' : {screenmorelines (); return;}
	case 'J' : {JUS (); return;}
	case 'j' : {JUSclever (); return;}
	case '<' : {ADJLM (); return;}
	case ';' : {ADJFLM (); return;}
	case ':' : {ADJNLM (); return;}
	case '>' : {ADJRM (); return;}
	case 'P' : {ADJPAGELEN (); return;}
	case 'T' : {toggle_tabsize (); return;}
	case 'H' : {HTML (); return;}
	case 'x' : {AltX (); return;}
	case (character) '�' :	/* ä German */
	case (character) '�' :	/* ö German */
	case (character) '�' :	/* ü German */
	case (character) '�' :	/* ß German */
			{UML ('g'); return;}
	case (character) '�' :	/* é French */
	case (character) '�' :	/* è French */
	case (character) '�' :	/* à French */
	case (character) '�' :	/* ù French */
	case (character) '�' :	/* ç French */
	case (character) '�' :	/* ² French */
			{UML ('f'); return;}
	case (character) '�' :	/* æ Scandinavian */
	case (character) '�' :	/* å Scandinavian */
	case (character) '�' :	/* ø Scandinavian */
			{UML ('d'); return;}
	case (character) '�' :	/* ì Italian */
	case (character) '�' :	/* ò Italian */
			{UML ('i'); return;}
	case 0x0142 :	/* ł Polish/Baltic */
	case 0x0144 :	/* ń Polish/Baltic */
	case 0x017A :	/* ź Polish/Baltic */
	case 0x0107 :	/* ć Polish/Baltic/Croatian/Slovenian */
			{UML ('e'); return;}
	case 0x016F :	/* ů Slovak */
	case 0x0151 :	/* ő Hungarian */
	case 0x0171 :	/* ű Hungarian */
	case 0x010D :	/* č Croatian/Slovenian */
	case 0x017E :	/* ž Croatian/Slovenian */
	case 0x0161 :	/* š Croatian/Slovenian */
	case 0x0111 :	/* đ Croatian/Slovenian/Vietnamese */
	case 0x01A1 :	/* ơ Vietnamese */
	case 0x01B0 :	/* ư Vietnamese */
			{UML ('e'); return;}
	case '_' :		/* generic */
	case (character) '�' :	/* ° Norwegian */
	case (character) '�' :	/* ª Portuguese */
	case (character) '�' :	/* º Portuguese */
	case (character) '�' :	/* « Portuguese */
	case (character) '�' :	/* ñ Spanish */
	case 0x011F :		/* ğ Turkish */
	case 0x015F :		/* ş Turkish */
	case 0x0131 :		/* ı Turkish */
			{UML (language_tag); return;}
	case 'C' : {LOWCAP (); return;}
	case '(' :
	case '{' : {SCORR (REVERSE); return;}
	case ')' :
	case '}' : {SCORR (FORWARD); return;}
	case 't' : {Stag (); return;}
	case 'a' : {toggle_append (); return;}
	case 'k' : {toggleKEYMAP (); return;}
	case 'I' :
	case 'K' : {setupKEYMAP (); return;}
	case 'Q' : if (smart_quotes) {
			handleQuotemenu ();
			displayflags ();
		   } else {
			error ("Smart quotes not enabled");
		   }
		   return;
	case 'E' : if (hop_flag > 0) {
			toggle_encoding ();
		   } else {
			handleEncodingmenu ();
		   }
		   return;
	case ' ' : {QUICKMENU (); return;}
	case 'f' : {FILEMENU (); return;}
	case 'e' : {EDITMENU (); return;}
	case 's' : {SEARCHMENU (); return;}
	case 'p' : {PARAMENU (); return;}
	case 'o' : {OPTIONSMENU (); return;}
	case ',' : {GR (); return;}
/*
	case 'e' : {EDIT (); return;}
	case 's' : {GR (); return;}
	case 'p' : {PRINT (); return;}
*/
  }

  /* fallback */
  if (c == quit_char) {
	CANCEL ();
	return;
  }
  func = command (c);
  if (func != Scharacter) {
	hop_flag = 1;
	keyshift |= alt_mask;
	(* func) (c);
  } else {
	BAD (c, "ESC/Alt-");
  }
  return;
}

/*
 * Interpret emacs meta commands.
 */
void
META ()
{
  unsigned long c;
  voidfunc func;

  if (! char_ready_within (500, NIL_PTR)) {
	status_uni ("Meta ESC(exit) TAB,space(menu) /,\\(search) (match ...");
  }
  if (quit) {
	return;
  }

  c = readcharacter_unicode ();
  if (quit) {
	return;
  }

  clear_status ();

  if ('0' <= c && c <= '9') {
	REPT (c);
	return;
  }

  switch (c) {
	/* emacs meta commands */
	case 'v' : {MOVPU (); return;}
	case 'f' : {MNW (); return;}
	case 'b' : {MPW (); return;}
	case 'a' : {BSEN (); return;}
	case 'e' : {ESEN (); return;}
	case '<' : {BFILE (); return;}
	case '>' : {EFILE (); return;}
	case 'd' : {DNW (); return;}
	case 'k' : {setMARK (True); ESEN (); CUT (); return;}
	case 'w' : {COPY (); return;}
	case 'y' : {YANKRING (); return;}
	case 'z' : {SUSP (); return;}
	case '%' : {REPL (); return;}
	case 'u' : {hop_flag = 1; UPPER (); return;}
	case 'l' : {hop_flag = 1; LOWER (); return;}
	case 'c' : {CAPWORD (); return;}
	case '.' : {Stag (); return;}

	case 'x' : {ESCAPE (); return;}
	case '\033' : {ESCAPE (); return;}

	default : {
		if (c == quit_char) {
			CANCEL ();
			return;
		}
		func = command (c);
		if (func != Scharacter) {
			hop_flag = 1;
			keyshift |= alt_mask;
			(* func) (c);
		} else {
			BAD (c, "Meta-");
		}
		return;
	}
  }
}

/*
 * Interpret emacs ^X commands.
 */
void
EMAX ()
{
  unsigned long c;

  if (! char_ready_within (500, NIL_PTR)) {
	status_msg ("^X ...");
  }
  if (quit) {
	return;
  }

  c = readcharacter_unicode ();
  if (quit) {
	return;
  }

  clear_status ();

  if (command (c) == MARK) {
	Popmark ();
	return;
  }
  switch (c) {
	case 'u' : {UNDO (); return;}
	case '' : {QUED (); return;}
	case '' : {WT (); return;}
	case '' : {SAVEAS (); return;}
	case '' : {PRVFILE (); return;}
	case '' : {EDIT (); return;}
	case '\032' : {SUSP (); return;}
	case '\033' : {REPT (' '); return;}
	case 'i' : {INSFILE (); return;}
	case 's' : {WT (); return;}
	case 'k' : {EDIT (); return;}
	case '=' : {FS (); return;}
	case '[' : {MOVPU (); return;}
	case ']' : {MOVPD (); return;}
	default : {
		if (c == quit_char) {
			CANCEL ();
			return;
		}
		BAD (c, "^X ");
	}
  }
}


/*
 * REPT () prompts for a count and wants a command after that. It repeats the
 * command count times. If a ^\ is given during repeating, stop looping and
 * return to main loop.
 */
void
REPT (firstdigit)
  char firstdigit;
{
  int count;
  voidfunc func;
  long val;
  unsigned long cmd;
  int number;

  hop_flag = 0;
  if (firstdigit >= '0' && firstdigit <= '9') {
     val = get_number ("Please continue repeat count...", firstdigit, & number);
     if (firstdigit != '0' && number < 10) {
	error ("Invalid repeat count after ESC <digit>");
	return;
     }
  } else {
     val = get_number ("Please enter repeat count...", '\0', & number);
  }
  if (val == ERRORS) {
	return;
  }
  cmd = val;

  func = command (cmd);
  if (func == I) {	/* no function assigned? */
	clear_status ();
	return;
  } else if (func == CTRLINS
	  || func == COMPOSE
	  || func == F5
	  || func == F6
	  || func == key_1
	  || func == key_2
	  || func == key_3
	  || func == key_4
	  || func == key_5
	  || func == key_6
	  || func == key_7
	  || func == key_8
	  || func == key_9
	  || func == key_0
	) {
	/* handle ^V/compose input here to not only repeat the prefix */
	if (func == CTRLINS) {
		cmd = CTRLGET (False);
	} else {
		cmd = CTRLGET (True);
	}
	for (count = 0; count < number; count ++) {
		Scharacter (cmd);
	}
	return;
  }

  count = number;
  while (count -- > 0 && quit == False) {
	char save_keyshift = keyshift;
	if (stat_visible) {
		clear_status ();
	}
	reset_smart_replacement ();
	(* func) (cmd);
	keyshift = save_keyshift;
	display_flush ();
	flush ();
  }
  reset_smart_replacement ();

  if (quit) {		/* Abort has been given */
	error ("Repeat aborted");
  } else {
	clear_status ();
  }
}


/*
   Toggle TAB expansion.
 */
void
toggle_tab_expansion ()
{
  expand_tabs = ! expand_tabs;
}

/*
 * Toggle insert/overwrite mode.
 */
void
TOGINS ()
{
  if (insert_mode) {
	insert_mode = False;
  } else {
	insert_mode = True;
  }
}

void
UNDO ()
{
  error ("Undo not implemented");
}

void
SPELL ()
{
  error ("Spell checking not implemented");
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
