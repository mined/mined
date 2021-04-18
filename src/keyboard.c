/*======================================================================*\
|*		Editor mined						*|
|*		keyboard input						*|
\*======================================================================*/

#include "mined.h"
#include "charprop.h"
#include "io.h"
#include "termprop.h"

#define dont_debug_mouse


#if ! defined (__TURBOC__) && ! defined (VAXC)
#include <sys/time.h>
#endif

#ifdef vms
#undef CURSES
#endif

#ifdef CURSES
#include <curses.h>
#include <strings.h>
#endif


voidfunc keyproc = I;	/* function addressed by entered function key */
unsigned char keyshift = 0;	/* shift state of entered function key */
static unsigned char fkeyshift;
#define max_ansi_params 10
int ansi_params;
int ansi_param [max_ansi_params];
char ansi_ini;
char ansi_fini;

static
int keymap_delay = 0;		/* wait to detect mapped key sequence */
static
int default_keymap_delay = 900;	/* overridden by $MAPDELAY */

static
int escape_delay = 0;		/* wait to detect escape sequence */
static
int default_escape_delay = 450;	/* overridden by $ESCDELAY */

static
int dont_suppress_keymap = 1;

static
int report_winchg = 0;

/* The flag queue_mode_mapped indicates when the byte input queue 
   has been stuffed with keyboard mapped characters, always being 
   encoded in UTF-8, while otherwise the queue contents are in 
   terminal native encoding (which may also be UTF-8, or CJK, or Latin-1).
 */
static
FLAG queue_mode_mapped = False;	/* queue contains mapped character(s) */

/* In hp_shift_mode key shift indications like ESC 2p or ESC 5h 
   are considered to determine the keyshift state.
 */
static
FLAG hp_shift_mode = False;

/* In sco_shift_mode key shift indications like ESC [2A or ESC [2M 
   are considered to determine the keyshift state.
 */
static
FLAG sco_shift_mode = False;

/* mouse handling */
mousebutton_type mouse_button;
mousebutton_type mouse_prevbutton = movebutton;
int mouse_xpos, mouse_ypos, mouse_prevxpos, mouse_prevypos;
int mouse_shift = 0;		/* modifier status */
FLAG report_release = False;	/* selectively suppress release event */
FLAG window_focus = True;	/* does the window have mouse/keyboard focus? */
int mouse_buttons_pressed = 0;	/* count pressed buttons */


/*======================================================================*\
|*		Local function declaration				*|
\*======================================================================*/

static unsigned long _readchar _((void));


/*======================================================================*\
|*		Function key tables					*|
\*======================================================================*/

#define dont_debug_findkey


#ifndef msdos

#ifdef unix
#define TERMINFO
#endif

static
void
dummyfunc ()
{
}

extern struct fkeyentry fkeymap [];
extern struct fkeyentry fkeymap_xterm [];
extern struct fkeyentry fkeymap_rxvt [];
extern struct fkeyentry fkeymap_linux [];
extern struct fkeyentry fkeymap_vt100 [];
extern struct fkeyentry fkeymap_vt52 [];
extern struct fkeyentry fkeymap_hp [];
extern struct fkeyentry fkeymap_siemens [];
extern struct fkeyentry fkeymap_scoansi [];
extern struct fkeyentry fkeymap_interix [];

static struct fkeyentry * fkeymap_spec = fkeymap_xterm;
#ifdef TERMINFO
extern struct fkeyentry fkeymap_terminfo [];
#endif

void
set_fkeymap (term)
  char * term;
{
#ifdef debug_findkey
  printf ("set_fkeymap %s\n", term);
#endif

  if (term == NIL_PTR) {
	fkeymap_spec = fkeymap;
  } else if (* term == 'x') {
	fkeymap_spec = fkeymap_xterm;
  } else if (* term == 'r') {
	fkeymap_spec = fkeymap_rxvt;
  } else if (* term == 'l') {
	fkeymap_spec = fkeymap_linux;
  } else if (* term == 'h') {
	fkeymap_spec = fkeymap_hp;
	hp_shift_mode = True;
  } else if (* term == 'v') {
	fkeymap_spec = fkeymap_vt100;
  } else if (* term == '5') {
	fkeymap_spec = fkeymap_vt52;
  } else if (* term == 'i') {
	fkeymap_spec = fkeymap_interix;
  } else if (* term == 's') {
	fkeymap_spec = fkeymap_siemens;
	use_mouse = False;
  } else if (* term == 'o') {
	fkeymap_spec = fkeymap_scoansi;
	sco_shift_mode = True;
	use_mouse = False;
  }
}

#else

void
set_fkeymap (term)
  char * term;
{
}

#endif


/*======================================================================*\
|*		String input functions					*|
\*======================================================================*/

/**
   get_digits () reads in a number.
   In contrast to get_number, it does no echoing, no messaging, reads 
   only decimal, and accepts zero length input.
   The last character typed in is returned.
   The resulting number is put into the integer the arguments points to.
   It is for use within I/O routines (e.g. reading mouse escape sequences 
   or cursor position report) and thus calls read1byte / __readchar.
 */
int
get_digits (result)
  int * result;
{
  register int byte1;
  register int value;

  byte1 = read1byte ();
  * result = -1;
/* Convert input to a decimal number */
  value = 0;
  while (byte1 >= '0' && byte1 <= '9') {
	value *= 10;
	value += byte1 - '0';
	* result = value;
	byte1 = read1byte ();
  }

  return byte1;
}


/**
   get_string_nokeymap prompts a string like get_string
   but doesn't apply international keyboard mapping
 */
int
get_string_nokeymap (prompt, inbuf, statfl, term_input)
  char * prompt;
  char * inbuf;
  FLAG statfl;
  char * term_input;
{
  int ret;

  dont_suppress_keymap = 0;
  ret = get_string (prompt, inbuf, statfl, term_input);
  dont_suppress_keymap = 1;

  return ret;
}


/*======================================================================*\
|*			Terminal input					*|
\*======================================================================*/

#ifdef debug_keytrack

void
do_trace_keytrack (tag, c)
  char * tag;
  unsigned long c;
{
  printf ("[%s] ", tag);
  if (command (c) == COMPOSE) {
	printf ("COMPOSE");
  } else if (c == FUNcmd) {
	printf ("FUNcmd");
  } else {
	printf ("%02lX", c);
  }
  printf (" [keyshift %X]\n", keyshift);
}

#endif

#define dont_debug_mapped_keyboard

#define max_stamps 10
static long last_sec = 0;
static long last_msec = 0;
static int n_stamps = 0;
static int i_stamp = 0;
static long stamps [max_stamps];
static long total_deltas = 0;
long average_delta_readchar = 0;
long last_delta_readchar = 0;

static FLAG skip_1click = False;
static FLAG skip_1release = False;


static
void
timestamp_readchar ()
{
#if ! defined (__TURBOC__) && ! defined (VAXC)
  struct timeval now;
  long sec;
  long msec;

  gettimeofday (& now, 0);
  sec = now.tv_sec;
  msec = now.tv_usec / 1000;
  if (last_sec == 0) {
	last_delta_readchar = 1000;
  } else {
	last_delta_readchar = (sec - last_sec) * 1000 + msec - last_msec;
  }
  last_sec = sec;
  last_msec = msec;

  if (n_stamps < max_stamps) {
	n_stamps ++;
  } else {
	/* remove oldest stamp from statistics */
	total_deltas -= stamps [i_stamp];
  }
  stamps [i_stamp] = last_delta_readchar;
  /* add stamp to statistics */
  total_deltas += last_delta_readchar;
  i_stamp ++;
  if (i_stamp == max_stamps) {
	i_stamp = 0;
  }

  average_delta_readchar = total_deltas / n_stamps;
#endif
}

/*
 * Readchar () reads one character from the terminal.
 * There are problems due to interruption of the read operation by signals
 * (QUIT, WINCH). The waitingforinput flag is only a partial solution.
 * Unix doesn't provide sufficient facilities to handle these situations
 * neatly and properly. Moreover, different Unix versions yield different
 * surprising effects. However, the use of select () could still be
 * an improvement.
 */
static
unsigned long
readchar ()
{
  unsigned long c;

#ifdef msdos
#define winchhere
#endif

#ifdef winchhere
  FLAG waiting;

  if (winchg && waitingforinput == False) {
	/* In the Unix version, this is now done in __readchar () */
	RDwinchg ();
  }
  /* must save waitingforinput flag since in the MSDOS version, 
     readchar can be called recursively */
  waiting = waitingforinput;
#endif
  waitingforinput = True;

  c = _readchar ();
  trace_keytrack ("readchar:_readchar", c);
  /* check report_release to suppress subsequent release actions 
     (esp after menu item selection) 
     when multiple buttons are pressed simultaneously */
  while (report_release == False &&
	 command (c) == MOUSEfunction && mouse_button == releasebutton)
  {
	last_mouse_event = releasebutton;
#ifdef debug_mouse
	printf ("readchar: ignoring mouse release (report_release == False)\n");
#endif
	c = _readchar ();
  }
#ifdef debug_mouse
  printf ("readchar: report_release = False\n");
#endif
  report_release = False;

#ifdef winchhere
  waitingforinput = waiting;
#else
  waitingforinput = False;
#endif

  /* the modification
	if (quit) {
		c = quit_char;
	}
     (now in __readchar) must not be placed after resetting the flag
	waitingforinput = False;
     Otherwise a QUIT signal coming in just between these two would
     discard the last valid character just taken up.
  */

  timestamp_readchar ();

  return c;
}

/*
   readchar_nokeymap reads like readchar
   but doesn't apply international keyboard mapping
 */
static
unsigned long
readchar_nokeymap ()
{
  unsigned long c;
  int prev_dont_suppress_keymap = dont_suppress_keymap;

  dont_suppress_keymap = 0;
  c = readchar ();
  dont_suppress_keymap = prev_dont_suppress_keymap;

  return c;
}

/*
   readcharacter_mapping () reads one character
   either with or without keyboard mapping
 */
static
unsigned long
readcharacter_mapping (map_keyboard, return_unicode)
  FLAG map_keyboard;
  FLAG return_unicode;
{
  int utfcount = 1;
  unsigned long unichar;

  if (map_keyboard) {
	unichar = readchar ();
  } else {
	unichar = readchar_nokeymap ();
	trace_keytrack ("readcharacter:readchar_nokeymap", unichar);
	/* double check needed here because command ('\n') == SNL */
	if (unichar == FUNcmd && command (unichar) == SNL) {
		return '\r';
	}
  }

  if (unichar == FUNcmd) {
	/** housekeeping of some events to cover the cases:
		focus-out mouse-click focus-in mouse-release (xterm 224)
		focus-out mouse-click mouse-release focus-in
		focus-out focus-in mouse-click mouse-release
	    and ignore the first mouse click/release in any case
	 */
	FLAG skipmouse = False;

	if (command (unichar) == FOCUSout) {
#ifdef debug_mouse
		printf ("FOCUS out\n");
#endif
		FOCUSout ();
		skip_1click = True;
		skip_1release = True;
		skipmouse = True;
	} else if (command (unichar) == FOCUSin) {
#ifdef debug_mouse
		printf ("FOCUS in\n");
#endif
		FOCUSin ();
		skipmouse = True;
		return FUNcmd;
	} else if (command (unichar) == MOUSEfunction) {
		if (mouse_button != releasebutton 
		    && last_delta_readchar > 100 && skip_1click) {
			/* skip click only immediately after focus in */
			skip_1click = False;
			skip_1release = False;
		}

		if (mouse_button == releasebutton && skip_1release) {
#ifdef debug_mouse
			printf ("skipping 1 mouse release event\n");
#endif
			skipmouse = True;
			skip_1release = False;
		} else if (mouse_button != releasebutton && skip_1click) {
#ifdef debug_mouse
			printf ("skipping 1 mouse event\n");
#endif
			skipmouse = True;
			skip_1click = False;
			skip_1release = False;
		} else if (! window_focus) {
			/* should not be needed anymore */
#ifdef debug_mouse
			printf ("skipping mouse event while focus out\n");
#endif
			skipmouse = True;
		}
	}

	if (skipmouse) {
		return readcharacter_mapping (map_keyboard, return_unicode);
	} else {
		/* WINCH exception isn't passing by here... */
#ifdef debug_mouse
		if (command (unichar) == MOUSEfunction) {
			if (mouse_button == releasebutton) {
				printf ("passing mouse release event\n");
			} else {
				printf ("passing mouse event\n");
			}
		}
#endif
		return FUNcmd;
	}
  }

  if (utf8_input || queue_mode_mapped) {
	if ((unichar & 0x80) == 0x00) {
		utfcount = 1;
	} else if ((unichar & 0xE0) == 0xC0) {
		utfcount = 2;
		unichar = unichar & 0x1F;
	} else if ((unichar & 0xF0) == 0xE0) {
		utfcount = 3;
		unichar = unichar & 0x0F;
	} else if ((unichar & 0xF8) == 0xF0) {
		utfcount = 4;
		unichar = unichar & 0x07;
	} else if ((unichar & 0xFC) == 0xF8) {
		utfcount = 5;
		unichar = unichar & 0x03;
	} else if ((unichar & 0xFE) == 0xFC) {
		utfcount = 6;
		unichar = unichar & 0x01;
	} else /* illegal UTF-8 code */ {
		return unichar;
	}
	while (utfcount > 1) {
		unichar = (unichar << 6) | (readchar_nokeymap () & 0x3F);
		utfcount --;
	}
	if (! return_unicode && (cjk_text || mapped_text)) {
		return encodedchar (unichar);
	} else {
		return unichar;
	}
  } else if (cjk_term && multichar (unichar)) {
	if (unichar == 0x8E && text_encoding_tag == 'C') {
		unichar = (unichar << 24)
			| (readchar_nokeymap () << 16)
			| (readchar_nokeymap () << 8)
			| readchar_nokeymap ();
	} else if (unichar == 0x8F && (text_encoding_tag == 'J' || text_encoding_tag == 'X')) {
		unichar = (unichar << 16)
			| (readchar_nokeymap () << 8)
			| readchar_nokeymap ();
	} else {
		int byte2 = readchar_nokeymap ();
		if (text_encoding_tag == 'G' && '0' <= byte2 && byte2 <= '9') {
			unichar = (unichar << 24)
				| (byte2 << 16)
				| (readchar_nokeymap () << 8)
				| readchar_nokeymap ();
		} else {
			unichar = (unichar << 8) | byte2;
		}
	}
	if (return_unicode || utf8_text) {
		unichar = lookup_encodedchar (unichar);
	}
	return unichar;
  } else {
	if (mapped_term) {
		if (! return_unicode && (cjk_text || mapped_text) && ! remapping_chars ()) {
#ifdef debug_mapped_keyboard
			printf ("rc_m mapped (ret_u %d) -> %04X\n", return_unicode, unichar);
#endif
			return unichar;
		} else if (ascii_screen) {
			/* skip validity check to allow Latin-1 input 
			   if ASCII screen is set (reason may be just 
			   missing display capability)
			 */
		} else {
#ifdef debug_mapped_keyboard
			printf ("rc_m mapped (ret_u %d) -> %04X -> %04X ...\n", return_unicode, unichar, lookup_mappedtermchar (unichar));
#endif
			unichar = lookup_mappedtermchar (unichar);
			if (no_unichar (unichar)) {
#ifdef debug_mapped_keyboard
				printf ("rc_m mapped -> CHAR_INVALID\n");
#endif
				return CHAR_INVALID;
			}
		}
	}

	if (! return_unicode && (cjk_text || mapped_text)) {
#ifdef debug_mapped_keyboard
		printf ("rc_m -> %04X -> %04X\n", unichar, encodedchar (unichar));
#endif
		return encodedchar (unichar);
	} else {
#ifdef debug_mapped_keyboard
		printf ("rc_m -> %04X\n", unichar);
#endif
		return unichar;
	}
  }
}

/*
   readcharacter () reads one character
   keyboard mapping is suppressed
 */
unsigned long
readcharacter ()
{
  return readcharacter_mapping (False, False);
}

/*
   readcharacter_mapped () reads one character
   keyboard mapping is enabled
 */
unsigned long
readcharacter_mapped ()
{
  return readcharacter_mapping (True, False);
}

/*
   readcharacter_unicode () reads one Unicode character
   keyboard mapping is suppressed
 */
unsigned long
readcharacter_unicode ()
{
  return readcharacter_mapping (False, True);
}

/*
   readcharacter_unicode_mapped () reads one Unicode character
   keyboard mapping is enabled
 */
unsigned long
readcharacter_unicode_mapped ()
{
  return readcharacter_mapping (True, True);
}

/*
   readcharacter_allbuttons always reports mouse button release
   and also reports window size change as function RDwin
 */
unsigned long
readcharacter_allbuttons (map_keyboard)
  FLAG map_keyboard;
{
  unsigned long c;
  int prev_report_winchg = report_winchg;

  report_winchg = 1;

#ifdef debug_mouse
  printf ("readchar_allbuttons: report_release = True\n");
#endif
  report_release = True;

  c = readcharacter_mapping (map_keyboard, True);

  report_winchg = prev_report_winchg;

  return c;
}


/*======================================================================*\
|*			Mouse input					*|
\*======================================================================*/

#ifdef debug_mouse

extern void do_trace_mouse _((char * tag));

void
do_trace_mouse (tag)
  char * tag;
{
  static char * button_name [] = {
	"releasebutton",
	"leftbutton",
	"middlebutton",
	"rightbutton",
	"movebutton",
	"wheelup",
	"wheeldown"};

  printf ("[%s] ", tag);
  printf ("%X ", mouse_shift);
  if (mouse_button != releasebutton) {
	printf ("%s", button_name [mouse_button]);
  } else {
	printf ("release (%s)", button_name [mouse_prevbutton]);
  }
  printf (" @ %d:%d (%d:%d)\n", mouse_ypos, mouse_xpos, mouse_prevypos, mouse_prevxpos);
}

#endif

static
void
notice_previous_click ()
{
  /* remember previous mouse click */
  if (mouse_button == leftbutton || mouse_button == rightbutton || mouse_button == middlebutton) {
	mouse_prevbutton = mouse_button;
	mouse_prevxpos = mouse_xpos;
	mouse_prevypos = mouse_ypos;
	mouse_buttons_pressed ++;
  }
}

static
void
fix_mouse_release_event ()
{
  /* workaround for incorrect mouse wheel escape sequences */
  if (mouse_button == releasebutton) {
	if (mouse_buttons_pressed > 0) {
		mouse_buttons_pressed --;
	} else {
		/* fix the event report */
		mouse_button = movebutton;
	}
  }
}

/*
 * DIRECT...getxy () reads in the information part of a cursor escape sequence
 */
static
int
DIRECTxtermgetbut ()
{
  int but = _readchar_nokeymap ();
  if (but == quit_char) {
	quit = True;
	return 0;
  }
  if (use_mouse_extended) {
	if ((but & 0xE0) == 0xC0) {
		but &= 0x1F;
		but = (but << 6) | (_readchar_nokeymap () & 0x3F);
	}
  }
  return but;
}

static
int
DIRECTxtermgetpos ()
{
  int pos = _readchar_nokeymap ();
  if (pos == quit_char) {
	quit = True;
	return 0;
  }
  if (use_mouse_extended) {
	if ((pos & 0xE0) == 0xC0) {
		pos &= 0x1F;
		pos = (pos << 6) | (_readchar_nokeymap () & 0x3F);
	}
	if (pos == 0) {
		pos = 0x800;	/* out of range */
	}
  } else if (pos == 0) {
	pos = 256;	/* out of range */
  }
  return pos - 33;
}

static
void
get_mouse_button (button)
  unsigned int button;
{
  mouse_shift = button & 0x1C;
  if (button & 32) {
	mouse_button = movebutton;
  } else if (button & 64) {
	button &= 0x03;
	if (button == 0x00) {
		mouse_button = wheelup;
	} else {
		mouse_button = wheeldown;
	}
  } else {
	button &= 0x03;
	if (button == 0x00) {
		mouse_button = leftbutton;
	} else if (button == 0x01) {
		mouse_button = middlebutton;
	} else if (button == 0x02) {
		mouse_button = rightbutton;
	} else {
		mouse_button = releasebutton;
	}
  }
}

static
void
DIRECTxtermgetxy (code)
  char code;	/* 'M' for normal mouse report, 't'/'T' for tracking */
{
  character button;

  notice_previous_click ();

  if (code == 't') {
	mouse_button = releasebutton;
  } else if (code == 'T') {
	/* could be Siemens 97801/97808 page down ... !?! */
	button = _readchar_nokeymap ();
	button = _readchar_nokeymap ();
	button = _readchar_nokeymap ();
	button = _readchar_nokeymap ();
	mouse_button = releasebutton;
  } else {	/* code == 'M' */
	if (xterm_version >= 268) {
		button = DIRECTxtermgetbut ();
	} else {
		button = _readchar_nokeymap ();
		if (button == quit_char) {
			quit = True;
		}
	}

	if (bidi_screen && mintty_version == 0 && button == '$') {
		/* fix mlterm quirk for mouse wheel scroll down */
		button = 'a';
		button = '#';
	}
	get_mouse_button (button - 32);
  }

  fix_mouse_release_event ();

  mouse_xpos = DIRECTxtermgetpos ();
  mouse_ypos = DIRECTxtermgetpos () - MENU;

#ifdef debug_mouse
  printf ("mouse %c: ", code);
  trace_mouse ("DIRECTxtermgetxy");
#endif

#if defined (unix) || defined (vms)
  if (use_mouse_button_event_tracking && mouse_button == releasebutton) {
	mouse_button_event_mode (False);
  }
#endif
}

static
void
DIRECTsgrgetxy ()
{
  notice_previous_click ();

  if (ansi_fini == 'm') {
	ansi_param [0] |= 0x3;	/* release button */
  }
  get_mouse_button (ansi_param [0]);

  fix_mouse_release_event ();

  mouse_ypos = ansi_param [2] - 1 - MENU;
  mouse_xpos = ansi_param [1] - 1;
}

static
void
DIRECTurxvtgetxy ()
{
  notice_previous_click ();

  get_mouse_button (ansi_param [0] - 32);

  fix_mouse_release_event ();

  mouse_ypos = ansi_param [2] - 1 - MENU;
  mouse_xpos = ansi_param [1] - 1;
}

static
void
DIRECTcrttoolgetxy ()
{
  character c;
  int xpos;
  int ypos;

  /* fix crttool settings */
  use_ascii_graphics = True;
  menu_border_style = 'r';
  use_mouse_release = False;

  notice_previous_click ();

  c = get_digits (& ypos); /* c should be ';' */
  c = get_digits (& xpos);
  if (c == 'l') {
	mouse_button = leftbutton;
  } else if (c == 'm') {
	mouse_button = middlebutton;
  } else if (c == 'r') {
	mouse_button = rightbutton;
  } else {
	mouse_button = releasebutton;
  }

  fix_mouse_release_event ();

  mouse_ypos = ypos - 1 - MENU;
  mouse_xpos = xpos - 1;
}

static
void
DIRECTvtlocatorgetxy ()
{
  notice_previous_click ();

  if (ansi_param [0] == 2) {
	mouse_button = leftbutton;
  } else if (ansi_param [0] == 4) {
	mouse_button = middlebutton;
  } else if (ansi_param [0] == 6) {
	mouse_button = rightbutton;
  } else if (ansi_param [0] == 9) {
	mouse_button = wheelup;
  } else if (ansi_param [0] == 11) {
	mouse_button = wheeldown;
  } else {
	mouse_button = releasebutton;
  }

  fix_mouse_release_event ();

  mouse_ypos = ansi_param [2] - 1 - MENU;
  mouse_xpos = ansi_param [3] - 1;
}

/*
 * DIRECT... () reads in a direct cursor movement input sequence.
 * Then it either performs a direct function (move/copy/paste) or 
 * invokes the menu functions.
 * Is no longer called as this is already handled in _readchar ().
 */
void
DIRECTxterm ()
{
#ifdef debug_mouse
  printf ("DIRECTxterm\n");
#endif
  DIRECTxtermgetxy ('M');
  MOUSEfunction ();
}

void
TRACKxterm ()
{
#ifdef debug_mouse
  printf ("TRACKxterm\n");
#endif
  DIRECTxtermgetxy ('t');
  MOUSEfunction ();
}

void
TRACKxtermT ()
{
#ifdef debug_mouse
  printf ("TRACKxtermT\n");
#endif
  DIRECTxtermgetxy ('T');
  MOUSEfunction ();
}

void
DIRECTcrttool ()
{
  DIRECTcrttoolgetxy ();
  MOUSEfunction ();
}

void
DIRECTvtlocator ()
{
  DIRECTvtlocatorgetxy ();
  MOUSEfunction ();
}


#ifdef CURSES
#ifdef KEY_MOUSE

#ifdef __PDCURSES__

/*
 * CURSgetxy () reads in the information of a mouse event
 */
static
void
CURSgetxy ()
{
# define mouse_state Mouse_status.changes
# define mouse_x MOUSE_X_POS
# define mouse_y MOUSE_Y_POS

  int button = 0;

  notice_previous_click ();

  request_mouse_pos ();

  if (BUTTON_CHANGED (1)) {
	button = 1;
  } else if (BUTTON_CHANGED (2)) {
	button = 2;
  } else if (BUTTON_CHANGED (3)) {
	button = 3;
  }

#ifdef BUTTON_SHIFT
  mouse_shift = (BUTTON_STATUS (button) & BUTTON_MODIFIER_MASK) == BUTTON_SHIFT;
#else
  mouse_shift = 0;
#endif

  if ((BUTTON_STATUS (button) & BUTTON_ACTION_MASK) == BUTTON_PRESSED) {
	if (button == 1) {
		mouse_button = leftbutton;
	} else if (button == 2) {
		mouse_button = middlebutton;
	} else if (button == 3) {
		mouse_button = rightbutton;
	} else {
		mouse_button = releasebutton;
	}
  } else {
	mouse_button = releasebutton;
  }

  fix_mouse_release_event ();

  mouse_xpos = mouse_x;
  mouse_ypos = mouse_y - MENU;
}

# else

static
void
CURSgetxy ()
{
#ifdef NCURSES_VERSION
  MEVENT mouse_info;
# define mouse_state mouse_info.bstate
# define mouse_x mouse_info.x
# define mouse_y mouse_info.y
#else
  unsigned long mouse_state;
# define mouse_x Mouse_status.x
# define mouse_y Mouse_status.y
#endif

  notice_previous_click ();

#ifdef NCURSES_VERSION
  getmouse (& mouse_info);
#else
  mouse_state = getmouse ();
#endif

#ifdef BUTTON_SHIFT
  mouse_shift = mouse_state & BUTTON_SHIFT;
#else
  mouse_shift = 0;
#endif

  if (mouse_state & BUTTON1_CLICKED) {
	mouse_button = leftbutton;
  } else if (mouse_state & BUTTON2_CLICKED) {
	mouse_button = middlebutton;
  } else if (mouse_state & BUTTON3_CLICKED) {
	mouse_button = rightbutton;
  } else {
	mouse_button = releasebutton;
  }

  fix_mouse_release_event ();

  mouse_xpos = mouse_x - 1;
  mouse_ypos = mouse_y - 1 - MENU;
}

#endif

#endif
#endif


/*======================================================================*\
|*			Character input					*|
\*======================================================================*/

/* max. length of function key sequence to be detected
   (depending on the fkeymap table, approx. 7),
   or of keyboard mapping sequence plus potential mapping rest
*/
#define MAXCODELEN 33

/*
 * queue collects the keys of an Escape sequence typed in until the 
 * sequence can be detected or rejected.
 * If the queue is not empty, queue [0] contains the character next 
 * to be delivered by _readchar () (it's not a ring buffer).
 * The queue contents are always terminated by a '\0', so queue can also 
 * be taken as a character string.
 */
static character queue [MAXCODELEN + 1];
static character * endp = queue;
static character ctrl_queue [MAXCODELEN + 1];
static character * endcp = ctrl_queue;
static character raw_queue [MAXCODELEN + 1];
static int rqi = 0;

static
int
get1byte ()
{
  int c = read1byte ();
  if (rqi < MAXCODELEN) {
	raw_queue [rqi ++] = c;
	raw_queue [rqi] = '\0';
  }
  return c;
}

#ifdef not_used
static
int
q_empty ()
{
  return endp == queue ? 1 : 0;
}
#endif

static
int
q_notfull ()
{
  return endp - queue == MAXCODELEN ? 0 : 1;
}

static
void
raw_q_clear ()
{
  rqi = 0;
}

static
void
q_clear ()
{
  endp = queue;
  endcp = ctrl_queue;
  rqi = 0;
}

static
int
q_len ()
{
  return endp - queue;
}

static
void
q_put (c)
  character c;
/* queue must not be full prior to this call! */
{
  * endp = c;
  * ++ endp = '\0';
  if (c == '\0') {
	* endcp = '@';
  } else {
	* endcp = c;
  }
  * ++ endcp = '\0';
}

/*
static
character
q_unput ()
{
  character c;
  if (q_len () == 0) {
	return '\0';
  } else {
	endp --;
	endcp --;
	c = * endp;
	* endp = '\0';
	* endcp = '\0';
	return c;
  }
}
*/

static
character
q_get ()
{
  character c;
  register character * pd;
  register character * ps;

  c = * queue;
  pd = queue;
  ps = pd + 1;
  while (ps <= endp) {
	* pd ++ = * ps ++;
  }
  if (endp > queue) {
	endp --;
	endcp --;
  }

#define dont_debug_queue_delivery
#ifdef debug_queue_delivery
  printf ("%02X\n", c);
#endif

  return c;
}


#ifndef msdos

/*
 * Look up key sequence in fkeymap table.
 * if matchmode == 0:
 * findkey (str) >=  0: escape sequence found
 *				(str == fkeymap [findkey (str)].fk)
 *			keyproc = fkeymap [i].fp (side effect)
 *		 == -1: escape sequence prefix recognised
 *				(str is prefix of some entry in fkeymap)
 *		 == -2: escape sequence not found
 *				(str is not contained in fkeymap)
 * * found == index of exact match if found
 * if matchmode == 1 (not used):
 * return -1 if any prefix detected (even if other exact match found)
 * if matchmode == 2 (not used):
 * don't return -1, only report exact or no match
 */
static
int
findkeyin (str, fkeymap, matchmode, found)
  char * str;
  struct fkeyentry * fkeymap;
  int matchmode;
  int * found;
{
  int lastmatch = 0;	/* last index with string matching prefix */
  register int i;
  int ret = -2;

  * found = -1;

  if (fkeymap [0].fk == NIL_PTR) {
	return ret;
  }

  i = lastmatch;
  do {
	char * strpoi = str;
	char * mappoi = fkeymap [i].fk;
	do {
		/* compare str with map entry */
		if (* strpoi != * mappoi) {
			break;
		}
		if (* strpoi) {
			strpoi ++;
			mappoi ++;
		}
	} while (* strpoi);

/*	if (strncmp (str, fkeymap [i].fk, strlen (str)) == 0) {*/
	if (* strpoi == '\0') {
		/* str is prefix of current entry */
		lastmatch = i;
/*		if (strlen (str) == strlen (fkeymap [i].fk)) {*/
		if (* mappoi == '\0') {
			/* str is equal to current entry */
			* found = i;
			keyproc = fkeymap [i].fp;
			if (! keyproc) {
				keyproc = I;
			}
			fkeyshift = fkeymap [i].fkeyshift;
			if (matchmode == 1) {
				/* return index unless other prefix 
				   was or will be found */
				if (ret == -2) {
					ret = i;
				}
			} else {
				return i;
			}
		} else {
			/* str is partial prefix of current entry */
			if (matchmode == 1) {
				/* return -1 but continue to search 
				   for exact match */
				ret = -1;
			} else if (matchmode != 2) {
				return -1;
			} else {
			}
		}
	}
	++ i;
	if (fkeymap [i].fk == NIL_PTR) {
		i = 0;
		return ret;
	}
  } while (i != lastmatch);

  return ret;
}

static
int
findkey (str)
  char * str;
{
  int dummy;
  int res;

  res = findkeyin (str, fkeymap_spec, 0, & dummy);
#ifdef debug_findkey
  printf ("<^[%s> _specific [%d]\n", str + 1, res);
#endif

  if (res != -2) {
	return res;
  }

  res = findkeyin (str, fkeymap, 0, & dummy);
#ifdef debug_findkey
  printf ("<^[%s> _deftable [%d]\n", str + 1, res);
#endif

  if (res != -2) {
	return res;
  }

#ifdef TERMINFO
  res = findkeyin (str, fkeymap_terminfo, 0, & dummy);
# ifdef debug_findkey
  printf ("<^[%s> _terminfo [%d]\n", str + 1, res);
# endif
#endif

  return res;
}

#endif	/* #ifndef msdos */


#ifdef CURSES
# ifdef vms
typedef unsigned int chtype;
# endif

chtype key;

extern int curs_readchar _((void));

static
void
showfkey ()
{
  build_string (text_buffer, "Curses function key entered: %03X / %04o", (int) key, (int) key);
  status_msg (text_buffer);
}


/*
 * Read a character, ignoring uninterpreted key strokes (SHIFT etc.)
 * also handles resize events
 */
int
curs_readchar ()
{
  unsigned long keycode;
  keycode = __readchar ();
#ifdef vms
  if (keycode >= 0x100) {
	printf ("readchar: vms curses key %03X\n", keycode); sleep (1);
  }
#else
  while (keycode >= KEY_MIN) {
	switch (keycode) {
#ifdef KEY_RESIZE
	case KEY_RESIZE:
		printf ("curses resize key received \n"); sleep (1);
		keyproc = showfkey;
		return FUNcmd;
#endif
#ifdef KEY_SHIFT_L
	case KEY_SHIFT_L:
	case KEY_SHIFT_R:
	case KEY_CONTROL_L:
	case KEY_CONTROL_R:
	case KEY_ALT_L:
	case KEY_ALT_R:
		break;
#endif
	default:
		return keycode;
	}
	keycode = __readchar ();
  }
#endif

  return keycode;
}

/*
 * Mapping KEY_ code (curses) -> mined function
 */
struct {
	chtype keycode;
	voidfunc func;
	unsigned char fkeyshift;
} cursfunc [] = {
#ifndef vms
	{KEY_F(1), F1},
	{KEY_F(2), F2},
	{KEY_F(3), F3},
	{KEY_F(4), F4},
	{KEY_F(5), F5},
	{KEY_F(6), F6},
	{KEY_F(7), F7},
	{KEY_F(8), F8},
	{KEY_F(9), F9},
	{KEY_F(10), F10},
	{KEY_F(11), F11},
	{KEY_F(12), F12},
	{KEY_IC, INSkey},
	{KEY_DC, DELkey},
	{KEY_HOME, HOMEkey},
	{KEY_END, ENDkey},
	{KEY_PPAGE, PGUPkey},
	{KEY_NPAGE, PGDNkey},
	{KEY_UP, UPkey},
	{KEY_DOWN, DNkey},
	{KEY_LEFT, LFkey},
	{KEY_RIGHT, RTkey},
	{KEY_B2, HOP},
	{KEY_A1, HOMEkey},
	{KEY_A3, PGUPkey},
	{KEY_C1, ENDkey},
	{KEY_C3, PGDNkey},
#endif
#ifdef KEY_A2
	{KEY_A2, UPkey},
#endif
#ifdef KEY_C2
	{KEY_C2, DNkey},
#endif
#ifdef KEY_B1
	{KEY_B1, LFkey},
#endif
#ifdef KEY_B3
	{KEY_B3, RTkey},
#endif
	{0x1BF, BLINEORUP},
	{0x1C0, ELINEORDN},

#ifdef PAD0
	{PAD0, PASTE},
#endif
#ifdef __PDCURSES__
	{PADSLASH, I},
	{PADSTAR, I},
	{PADMINUS, I},
	{PADPLUS, I},
	{PADSTOP, CUT},
	{PADENTER, SNL},
	{ALT_F, FILEMENU},
	{ALT_E, EDITMENU},
	{ALT_S, SEARCHMENU},
	{ALT_P, PARAMENU},
	{ALT_O, OPTIONSMENU},
#endif

	{0}
};

/*
 * Look up mined function by curses key code
 * lookup_curskey (str) >=  0: keycode == cursfunc [lookup_curskey (str)].fk
 *			== -1: keycode is not contained in mapping table
 */
static
voidfunc
lookup_curskey (keycode)
  chtype keycode;
{
  register int i;

  i = 0;
  while (cursfunc [i].keycode != 0 && cursfunc [i].keycode != keycode) {
	i ++;
  }
  if (cursfunc [i].keycode == 0) {
	return showfkey;
  } else {
	keyshift |= cursfunc [i].fkeyshift;
	return cursfunc [i].func;
  }
}

#endif


/*
 * Functions to read a character from terminal.
   If _readchar () detects a function key sequence (according to the 
   table fkeymap), FUNcmd is returned, and the associated function is 
   stored in the variable keyproc.
   Keyboard mapping is applied.
 */

unsigned long
_readchar_nokeymap ()
{
  unsigned long c;
  int prev_dont_suppress_keymap = dont_suppress_keymap;

  dont_suppress_keymap = 0;
  c = _readchar ();
  dont_suppress_keymap = prev_dont_suppress_keymap;

  return c;
}


#define dont_debug_readbyte

#define dont_debug_terminal_response
#ifdef debug_terminal_response
#define trace_terminal_response(what)	if (debug_tag) printf ("[%s (%d)] %s\n", debug_tag, msec, what);
#else
#define trace_terminal_response(what)
#endif


static int choose_char _((char *));

static character rest_queue [MAXCODELEN + 1];
static FLAG have_rest_queue = False;

static
void
putback_rest (rest)
  char * rest;
{
  char old_rest_queue [MAXCODELEN + 1];
#ifdef debug_readbyte
  printf ("putback_rest <%s>\n", rest);
#endif

  if (! streq (rest, " ")) {
	if (have_rest_queue) {
		/**
		   overdraft characters to be mapped are 
		   put back into rest_queue 
		   to be considered for subsequent mapping
			-> Hiragana nihongo
			-> Hiragana nyi
			-> test abcde
		 */
		strcpy (old_rest_queue, (char *) rest_queue);
	} else {
		old_rest_queue [0] = '\0';
	}
	strcpy ((char *) rest_queue, rest);
	strcat ((char *) rest_queue, old_rest_queue);
	have_rest_queue = True;
  }
}

/* read 1 byte from queue; for curses, however, function 
   key indications must be passed on which are longer than 8 bits
 */
int
read1byte ()
{
  if (have_rest_queue) {
	character * cr = rest_queue;
	character ch = * rest_queue;
	while (* (cr + 1) != '\0') {
		* cr = * (cr + 1);
		cr ++;
	}
	* cr = '\0';
	if (* rest_queue == '\0') {
		have_rest_queue = False;
	}
#ifdef debug_readbyte
	printf ("read byte %02X from rest queue\n", ch);
#endif
	return ch;
  } else {
	int ch;
#ifdef CURSES
	ch = curs_readchar ();
#else
#ifdef debug_readbyte
	printf ("reading byte from io (report_winchg %d)\n", report_winchg);
#endif
	if (report_winchg) {
		ch = __readchar_report_winchg ();
	} else {
		ch = __readchar ();
	}
#endif
#ifdef debug_readbyte
	printf ("read byte %02X from io\n", ch);
#endif
	return ch;
  }
}


static
int
bytereadyafter (msec, debug_tag)
  int msec;
  char * debug_tag;
{
  if (have_rest_queue) {
	trace_terminal_response ("from rest queue");
	return 1;
  } else {
	if (inputreadyafter (msec) > 0) {
		trace_terminal_response ("response");
		return 1;
	} else {
		trace_terminal_response ("timeout");
		return 0;
	}
  }
}

/*
 * Is a character available within a specified number of milliseconds ?
 */
int
char_ready_within (msec, debug_tag)
  int msec;
  char * debug_tag;
{
  if (q_len () > 0) {
	trace_terminal_response ("from queue");
	return 1;
  } else {
	return bytereadyafter (msec, debug_tag);
  }
}


#ifndef msdos
#define radical_stroke
#endif


#ifdef radical_stroke
static menuitemtype radicalsmenu [] = {
	/* 1 stroke radicals */
	{"①.1:一 2:丨 3:丶 4:丿乀乁 5:乙乚乛 6:亅", dummyfunc, ""},
	/* 2 strokes radicals */
	{"②.7:二 8:亠 9:人亻 10:儿 11:入 12:八 13:冂 14:冖", dummyfunc, ""},
	{"②.15:冫 16:几 17:凵 18:刀刁刂 19:力 20:勹 21:匕", dummyfunc, ""},
	{"②.22:匚 23:匸 24:十 25:卜 26:卩 27:厂 28:厶 29:又", dummyfunc, ""},
	/* 3 strokes radicals */
	{"③.30:口 31:囗 32:土 33:士 34:夂 35:夊 36:夕 37:大夨 38:女 39:子孑孒孓", dummyfunc, ""},
	{"③.40:宀 41:寸 42:小 43:尢尣 44:尸 45:屮 46:山 47:巛巜川 48:工 49:己已巳", dummyfunc, ""},
	{"③.50:巾 51:干 52:乡幺 53:广 54:廴 55:廾 56:弋 57:弓 58:彐彑 59:彡 60:彳", dummyfunc, ""},
	/* 4 strokes radicals */
	{"④.61:心忄 62:戈 63:戶户戸 64:手扌才 65:支 66:攴攵 67:文 68:斗", dummyfunc, ""},
	{"④.69:斤 70:方 71:无 72:日 73:曰 74:月 75:木朩 76:欠 77:止 78:歹 79:殳", dummyfunc, ""},
	{"④.80:毋毌 81:比 82:毛 83:氏 84:气 85:水氵 86:火灬 87:爪爫 88:父 89:爻", dummyfunc, ""},
	{"④.90:丬爿 91:片 92:牙㸦 93:牛牜 94:犬犭", dummyfunc, ""},
	/* 5 strokes radicals */
	{"⑤.95:玄 96:玉王 97:瓜 98:瓦 99:甘 100:生 101:用甩 102:田由甲申甴电", dummyfunc, ""},
	{"⑤.103:疋 104:疒 105:癶 106:白 107:皮 108:皿 109:目", dummyfunc, ""},
	{"⑤.110:矛 111:矢 112:石 113:示礻 114:禸 115:禾 116:穴 117:立", dummyfunc, ""},
	/* 6 strokes radicals */
	{"⑥.118:竹 119:米 120:糸糹纟 121:缶 122:网罒罓䍏 123:羊 124:羽 125:老耂考", dummyfunc, ""},
	{"⑥.126:而 127:耒 128:耳 129:聿肀 130:肉 131:臣 132:自", dummyfunc, ""},
	{"⑥.133:至 134:臼 135:舌 136:舛 137:舟 138:艮 139:色 140:艸艹䒑", dummyfunc, ""},
	{"⑥.141:虍 142:虫 143:血 144:行 145:衣衤 146:襾西覀", dummyfunc, ""},
	/* 7 strokes radicals */
	{"⑦.147:見见 148:角 149:言訁讠 150:谷 151:豆 152:豕 153:豸", dummyfunc, ""},
	{"⑦.154:貝贝 155:赤 156:走赱 157:足 158:身 159:車车 160:辛", dummyfunc, ""},
	{"⑦.161:辰 162:辵辶 163:邑 164:酉 165:釆 166:里", dummyfunc, ""},
	/* 8 strokes radicals */
	{"⑧.167:金釒钅 168:長镸长 169:門门 170:阜阝", dummyfunc, ""},
	{"⑧.171:隶 172:隹 173:雨 174:靑青 175:非", dummyfunc, ""},
	/* 9 strokes radicals */
	{"⑨.176:面靣 177:革 178:韋韦 179:韭 180:音 181:頁页", dummyfunc, ""},
	{"⑨.182:風风 183:飛飞 184:食飠饣 185:首 186:香", dummyfunc, ""},
	/* 10 strokes radicals */
	{"⑩.187:馬马 188:骨 189:高髙 190:髟 191:鬥 192:鬯 193:鬲 194:鬼", dummyfunc, ""},
	/* 11 strokes radicals */
	{"⑪.195:魚鱼 196:鳥鸟 197:鹵 198:鹿 199:麥麦 200:麻", dummyfunc, ""},
	/* 12 strokes radicals */
	{"⑫.201:黃黄 202:黍 203:黑黒 204:黹", dummyfunc, ""},
	/* 13 strokes radicals */
	{"⑬.205:黽黾 206:鼎 207:鼓鼔 208:鼠鼡", dummyfunc, ""},
	/* 14 strokes radicals */
	{"⑭.209:鼻 210:齊齐", dummyfunc, ""},
	/* 15 strokes radicals */
	{"⑮.211:齒齿", dummyfunc, ""},
	/* 16 strokes radicals */
	{"⑯.212:龍龙 213:龜龟", dummyfunc, ""},
	/* 17 strokes radical */
	{"⑰.214:龠", dummyfunc, ""},
};
#endif


#define dont_debug_kbesc
#define dont_debug_kbansi
#define dont_debug_kbmap

static
unsigned long
_readchar ()
{
  unsigned long ch;
  char * choice;
  int choice_index;
  /* queue handling */
  int res;
  char * found;
  char * mapped;
  char * prev_found = NIL_PTR;


  if (escape_delay == 0 || keymap_delay == 0) {
	char * env = envvar ("ESCDELAY");
	if (env) {
		(void) scan_int (env, & escape_delay);
	}
	if (escape_delay == 0) {
		escape_delay = default_escape_delay;
	}
	env = envvar ("MAPDELAY");
	if (env) {
		(void) scan_int (env, & keymap_delay);
	}
	if (keymap_delay == 0) {
		keymap_delay = default_keymap_delay;
	}
  }


  if (q_len () > 0) {
	return q_get ();
  }
  queue_mode_mapped = False;
  raw_q_clear ();

  keyshift = 0;
#ifdef msdos
  mouse_shift = 0;
#endif


#ifdef radical_stroke
  /* plug-in specific input methods */
  if (allow_keymap && dont_suppress_keymap
	&& streq (keyboard_mapping, "rs")
  ) {
	int choice_col = x;
	int choice_line = y + 1;
	char radicalnumber [4];

	choice_index = -1;

	/* adjust menu position if too far down */
	if (choice_line + arrlen (radicalsmenu) + 2 > YMAX) {
		choice_line = y - arrlen (radicalsmenu) - 2;
		if (choice_line < 0) {
			choice_line = 0;
		}
	}

	/* adjust menu position if on status line */
	if (input_active) {
		choice_line = YMAX - arrlen (radicalsmenu) - 2;
		if (choice_line < 0) {
			choice_line = 0;
		}
		choice_col = lpos;
	}

	/* try to get radical, then character, from user */
	while (choice_index < 0) {
		int radical = popup_menu (radicalsmenu, arrlen (radicalsmenu), 
					choice_col, choice_line, 
					"Select radical", False, False, "1234567890x");
		if (radical < 0) {
			/* revert to previous mapping (or none) */
			toggleKEYMAP ();
			set_cursor_xy ();
			flush ();
			choice_index = 0;	/* continue */
		} else {
			int radical_row = radical / 11;
			int radical_col = radical % 11;
			mapped = radicalsmenu [radical_row].itemname;
			while (* mapped != '\0' &&
			       (* mapped < '0' || * mapped > '9'
			        || radical_col > 0))
			{
				if (* mapped == ':') {
					radical_col --;
				}
				mapped ++;
			}
			found = radicalnumber;
			while (* mapped >= '0' && * mapped <= '9') {
				* found ++ = * mapped ++;
			}
			* found = '\0';
			res = map_key (radicalnumber, 2, & found, & mapped);
			if (res >= 0) {
				/* first goto in my life and then 
				   such an exceptionally ugly one -
				   but it works well just this way */
				goto handle_mapped;
			} else {
				ring_bell ();
			}
		}
	}
  }
#endif


#ifdef CURSES

  key = read1byte ();
#ifdef vms
  if (key >= 0x100) {
	printf ("_readchar: vms curses key %03X\n", key); sleep (1);
  }
#else
  if (key >= KEY_MIN) {
#ifdef KEY_MOUSE
	if (key == KEY_MOUSE) {
		CURSgetxy ();
		keyproc = MOUSEfunction;
		return FUNcmd;
	}
#endif
	keyproc = lookup_curskey (key);
	return FUNcmd;
  }
#endif
  ch = key;

#else	/* #ifdef CURSES */

#ifdef dosmouse
  {
	int ich = __readchar ();
	if (ich == FUNcmd) {
		DIRECTdosmousegetxy ();
		keyproc = MOUSEfunction;
		return FUNcmd;
	} else {
		ch = ich;
	}
  }
#else
  ch = read1byte ();
  if (ch == FUNcmd) {
	/* pass back WINCH exception */
	return FUNcmd;
  }
#endif

#endif	/* #else CURSES */


#ifdef msdos

  if (ch > 0xFF) {
	keyproc = pc_xkey_map [ch - 0x100].fp;
	if (! keyproc) {
		keyproc = I;
	}
	keyshift |= pc_xkey_map [ch - 0x100].fkeyshift;
	return FUNcmd;
  }

  if (ch == '\000') {
# ifdef dosmouse
	ch = getch ();
# else
	ch = read1byte ();
# endif
	keyproc = pc_xkey_map [ch].fp;
	if (! keyproc) {
		keyproc = I;
	}
	keyshift |= pc_xkey_map [ch].fkeyshift;
# ifdef debug_whatever
	/* VAXC needs the identity cast (voidfunc) */
	if ((voidfunc) keyproc == (voidfunc) I) {
		return ch;
	}
# endif
	return FUNcmd;
  } else if (ch == ' ' && (keyshift & ctrl_mask)) {
	return 0;	/* ctrl-Space -> NUL */
  } else {
	q_put (ch);
  }

#else	/* #ifdef msdos */

  trace_keytrack ("_readchar<033", ch);
  if (ch == '\033') {
	char second_esc_char = '\0';
	int esc_wait = 2 * escape_delay;

	q_put (ch);

	if (! bytereadyafter (esc_wait, NIL_PTR)) {
		return q_get ();
	}

	while ((res = findkey (ctrl_queue)) == -1 /* prefix of table entry */
		&& q_notfull ()
#ifdef simple_escape_delay_handling
		&& bytereadyafter (escape_delay, NIL_PTR)
#endif
	      )
	{
		char prev_ch = ch;

		if (prev_ch != '\033' && ! input_active && ! bytereadyafter (escape_delay, NIL_PTR)) {
			status_uni ("...awaiting slow key code sequence...");
		}

		ch = get1byte ();
#ifdef debug_kbesc
		printf ("<^[%s> + [%dms] %02lX\n", ctrl_queue + 1, escape_delay, ch);
#endif

		/* accept ESC prefix as Alt modifier */
		if (detect_esc_alt && ch == '\033' && prev_ch == '\033') {
			if (bytereadyafter (escape_delay, NIL_PTR)) {
				ch = get1byte ();
				keyshift |= alt_mask;
#ifdef debug_kbesc
				printf ("-> <^[%s> + ^[/%02lX\n", ctrl_queue + 1, ch);
#endif
			}
		}

		/* accept XTerm*modifyCursorKeys: 3 as well as 
		   device attributes reports (primary/secondary DA) */
		if (second_esc_char == '[' && prev_ch == '[' &&
			(ch == '>' || ch == '?')
		   ) {
			if (ch == '?') {
				keyshift |= report_mask;
			}
			ch = get1byte ();
#ifdef debug_kbesc
			printf ("-> <^[%s> + >?/%02lX\n", ctrl_queue + 1, ch);
#endif
		}

check_ctrl_byte:
		/* handle generic shift state of ANSI sequences: */
		/* syntax of modifier indication varies with 
		   terminal type and mode; indications are as follows:
					ANSI 1	ANSI 2	keyshift
		   plain		-	9?	0x00
		   Shift		2	10	0x01
		   Alt			3	11	0x02
		   Alt-Shift		4	12	0x03
		   Control		5	13	0x04
		   Control-Shift	6	14	0x05
		   Alt-Control		7	15	0x06
		   Alt-Control-Shift	8	16	0x07
		   applkeypad_mask			0x08
		 */
		if (ch == ';' && second_esc_char == '[') {
			ch = get1byte ();
			if (ch >= '2' && ch <= '9') {
				/* ESC [1;2A → ESC [1A + shift etc */
				/* special: map '9' to applkeypad_mask */
				keyshift |= (ch & ~'0') - 1;
			} else if (ch == '1') {
				ch = get1byte ();
				if (ch >= '0' && ch <= '6') {
					/* xterm with alwaysUseMods: */
					/* ESC [1;11A → ESC [1A + alt etc */
					keyshift |= applkeypad_mask | ((ch & ~'0') + 1);
				} else {
					q_put (';');
					q_put ('1');
					q_put (ch);
				}
			} else {
				q_put (';');
				q_put (ch);
			}
			ch = get1byte ();
#ifdef debug_kbesc
			printf ("-> <^[%s> + 1-8/%02lX\n", ctrl_queue + 1, ch);
#endif
		} else if (ch == ';' && second_esc_char == '1') {
			ch = get1byte ();
			if (ch >= '2' && ch <= '8') {
				/* xterm/VT52 mode: ESC 1;2A → ESC 1A + shift etc */
				keyshift |= (ch & ~'0') - 1;
			} else {
				q_put (';');
				q_put (ch);
			}
			ch = get1byte ();
#ifdef debug_kbesc
			printf ("-> <^[%s> + ;/%02lX\n", ctrl_queue + 1, ch);
#endif
		} else if (ch >= '0' && ch <= '9'
			   && (second_esc_char == 'O' || 
			       (hp_shift_mode && ! second_esc_char))) {
			if (ch >= '2' && ch <= '9') {
				/* ESC O2A → ESC OA + shift etc */
				/* HP: ESC 2p → ESC p + shift etc */
				/* special: map '9' to applkeypad_mask */
				keyshift |= (ch & ~'0') - 1;
			} else if (ch == '1' || (ch == '0' && mlterm_version)) {
				ch = get1byte ();
				if (ch >= '0' && ch <= '6') {
					/* ESC O11P → ESC OP + alt etc */
					/* HP: ESC 11p → ESC p + alt etc */
					keyshift |= applkeypad_mask | ((ch & ~'0') + 1);
				} else if (ch == ';') {
					/* mlterm: ESC O1;2A → ESC OA + shift etc */
					/* mlterm: ESC O0;3w → ESC O0w + alt etc */
					ch = get1byte ();
					keyshift |= ((ch & ~'0') - 1);
				} else {
					q_put ('1');
					q_put (ch);
				}
			} else {
				q_put (ch);
			}
			ch = get1byte ();
#ifdef debug_kbesc
			printf ("-> <^[%s> + 0-9/%02lX\n", ctrl_queue + 1, ch);
#endif
		} else if (sco_shift_mode && second_esc_char == '['
			   && ch >= '0' && ch <= '9') {
			char newkeyshift;
			char newch = '\0';
			if (ch >= '2' && ch <= '9') {
				/* SCO: ESC [2M → ESC [M + shift etc */
				/* special: map '9' to applkeypad_mask */
				newkeyshift = (ch & ~'0') - 1;
				newch = get1byte ();
#ifdef debug_kbesc
				printf ("-> <^[%s> + 2-8/%02X\n", ctrl_queue + 1, newch);
#endif
				if (newch >= 'A' && newch <= 'Z') {
					keyshift |= newkeyshift;
				} else {
					q_put (ch);
				}
			} else if (ch == '1') {
				ch = get1byte ();
#ifdef debug_kbesc
				printf ("-> <^[%s> + 1/%02lX\n", ctrl_queue + 1, ch);
#endif
				if (ch >= '0' && ch <= '6') {
					/* SCO: ESC [11M → ESC [M + alt etc */
					newkeyshift = applkeypad_mask | ((ch & ~'0') + 1);
					newch = get1byte ();
					if (newch >= 'A' && newch <= 'Z') {
						keyshift |= newkeyshift;
					} else {
						q_put ('1');
						q_put (ch);
					}
				} else if (ch == ';') {
					/* xterm SCO: ESC [1;5R → ESC [R + alt etc */
					ch = get1byte ();
					keyshift |= (ch & ~'0') - 1;
					newch = get1byte ();
				} else {
					q_put ('1');
					q_put (ch);
					newch = get1byte ();
				}
			} else {
#ifdef debug_kbesc
				printf ("-> <^[%s>\n", ctrl_queue + 1);
#endif
				q_put (ch);
				newch = get1byte ();
			}
			ch = newch;
			if (ch == ';') {
				/* OK, so here is my second goto; 
				   this is really a very peculiar case, 
				   based on misconfiguration 
				   (mined assumes sco, terminal sends 
				   modified xterm sequences), 
				   which does not deserve any 
				   goto avoidance effort
				*/
#ifdef debug_kbesc
				printf ("goto check_ctrl_byte\n");
#endif
				goto check_ctrl_byte;
			}
		} else if (prev_ch >= '0' && prev_ch <= '9') {
			if (ch == '^') {
				/* ESC [5^ → ESC [5~ + control (rxvt) */
				keyshift |= ctrl_mask;
				ch = '~';
			} else if (ch == '$') {
				/* ESC [5$ → ESC [5~ + shift (rxvt) */
				keyshift |= shift_mask;
				ch = '~';
			} else if (ch == '@') {
				/* ESC [5@ → ESC [5~ + control+shift (rxvt) */
				keyshift |= ctrlshift_mask;
				ch = '~';
			}
		} else if (ch >= 'a' && ch <= 'd') {
			if (prev_ch == 'O') {
				/* ESC Oa → ESC OA + control (rxvt) */
				keyshift |= ctrl_mask;
				ch -= 'a' - 'A';
			} else if (prev_ch == '[') {
				/* ESC [a → ESC [A + shift (rxvt) */
				keyshift |= shift_mask;
				ch -= 'a' - 'A';
			}
		}

		q_put (ch);
		if (second_esc_char == '\0') {
			second_esc_char = ch;
			if (second_esc_char == 'O') {
				/* appl. cursor keys, appl. keypad */
				keyshift |= applkeypad_mask;
			}
		}
	}
#ifdef debug_kbesc
	printf ("=> [%d] <^[%s> [keyshift %X]\n", res, ctrl_queue + 1, keyshift);
#endif
#ifdef debug_kbansi
	printf ("[%d] [keyshift %X]\n", res, keyshift);
#endif

	if (quit) {
		keyproc = I;
		return FUNcmd;
	} else if (res < 0) {	/* key pattern not found in fkeymap table */
		if (second_esc_char == '[') {
			/* generic scanning for unknown ANSI sequence */
#ifdef debug_kbansi
			character q1, q2;
			q1 = q_get (); /* swallow ESC */
			q2 = q_get (); /* swallow [ */
			printf ("swallowing %X %X\n", q1, q2);
#else
			(void) q_get (); /* swallow ESC */
			(void) q_get (); /* swallow [ */
#endif
			/* (Was putback_rest (ctrl_queue + 2);) */
			/* Use raw_queue to ignore previous modifier 
			   transformations like ESC [1;11A → ESC [1A + alt */
			if ((keyshift & alt_mask) && raw_queue [1] == '[') {
				/* consider double-ESC alt (rxvt) */
				putback_rest (raw_queue + 2);
			} else {
				putback_rest (raw_queue + 1);
			}

			ansi_params = 0;
			ansi_ini = '\0';
			do {
				int i = -1;
				ansi_fini = get_digits (& i);
				if (ansi_params == 0 && i == -1 &&
				   (ansi_fini == '>' || ansi_fini == '?' || ansi_fini == '<')
				   ) {
					ansi_ini = ansi_fini;
					ansi_fini = ';';
				} else if (ansi_params < max_ansi_params) {
					ansi_param [ansi_params] = i;
					ansi_params ++;
				}
			} while (ansi_fini == ';');
			q_clear ();
#ifdef debug_kbansi
			printf ("ansi %c (%d)", ansi_fini, ansi_params);
			{int i;
			 for (i = 0; i < ansi_params; i ++) {
				printf (" %d", ansi_param [i]);
			 }
			}
			printf ("\n");
#endif

			if ((ansi_fini == '~'
			     && ansi_param [0] == 27
			     && ansi_params >= 3
			    )
			 || (ansi_fini == 'u'
			     && (ansi_params == 2
			      || (ansi_params == 1 && keyshift > 0)
			        )
			    )
			   ) {
				/* map xterm 235 mode formatOtherKeys: 1
					^[[77;2u
				   to xterm mode formatOtherKeys: 0
					^[[27;2;77~
				 */
				if (ansi_fini == 'u') {
					ansi_param [2] = ansi_param [0];
				}

				if (ansi_params > 1) {
					/* handle generic escape sequence 
					   for modified key 
					   (xterm 214 resource modifyOtherKeys)
					 */
					if (ansi_param [1] > 8) {
						ansi_param [1] -= 8;
					}
					keyshift |= ansi_param [1] - 1;
				}

				/* enforce UTF-8 interpretation of input */
				queue_mode_mapped = True;

				if (ansi_param [2] == ' ' && (keyshift & ctrlshift_mask) == ctrl_mask) {
					/* map ctrl-space to MARK,
					   alt-ctrl-space to Hop MARK */
					q_put ('\0');
				} else if (ansi_param [2] == 0) {
					q_put ('\0');
				} else {
					char cbuf [7];
					char * cp = cbuf;

					/* Shift-a/A -> A */
					if ((keyshift & altctrlshift_mask) == shift_mask
					   && isLetter (ansi_param [2])
					   ) {
						keyshift = 0;
						ansi_param [2] = case_convert (ansi_param [2], 1);
					}

					/* Control-/ etc as accent prefix */
					if (keyshift & ctrl_mask) {
						struct prefixspec * ps;
						ps = lookup_prefix (COMPOSE, ansi_param [2]);
						if (ps && * ps->accentname) {
							keyshift = ansi_param [2];
							keyproc = COMPOSE;
							return FUNcmd;
						} else if (ps) {
							unsigned long unichar;
							int len;
							utf8_info (ps->pat1, & len, & unichar);
							ansi_param [2] = unichar;
						} else if ('?' <= ansi_param [2] && ansi_param [2] < '') {
							keyshift &= ~ ctrl_mask;
							return ansi_param [2] & 0x1F;
						}
					}

					(void) utfencode (ansi_param [2], cbuf);
					while (* cp) {
						q_put (* cp ++);
					}
				}

				/* Control/Alt/Control-Alt-0 -> ...key_0 */
				if (ansi_param [2] >= '0' && ansi_param [2] <= '9') {
					switch (ansi_param [2]) {
					case '0': keyproc = key_0; break;
					case '1': keyproc = key_1; break;
					case '2': keyproc = key_2; break;
					case '3': keyproc = key_3; break;
					case '4': keyproc = key_4; break;
					case '5': keyproc = key_5; break;
					case '6': keyproc = key_6; break;
					case '7': keyproc = key_7; break;
					case '8': keyproc = key_8; break;
					case '9': keyproc = key_9; break;
					}
					q_clear ();
					return FUNcmd;
				}

				if (keyshift & alt_mask) {
					/* 2000.13 workaround for prompt */
					if ((keyshift & altctrl_mask) == alt_mask) {
					    if (ansi_param [2] == 'k') {
						q_clear ();
						keyproc = toggleKEYMAP;
						return FUNcmd;
					    }
					    if (ansi_param [2] == 'K'
					     || ansi_param [2] == 'I') {
						q_clear ();
						keyproc = setupKEYMAP;
						return FUNcmd;
					    }
					}

					return '\033';
				} else if (keyshift) {
					return q_get ();
				} else {
					/* fall through to keyboard mapping */
				}
			} else {	/* not CSI 27 sequence */
				if (ansi_fini == '&') {	/* DEC locator report */
					(void) read1byte ();	/* swallow 'w' */
					if (ansi_params < 4) {
						ansi_param [3] = 1;
						if (ansi_params < 3) {
							ansi_param [2] = 1;
						}
					}
					DIRECTvtlocatorgetxy ();
					keyproc = MOUSEfunction;
				} else if (ansi_ini == '<') {
					DIRECTsgrgetxy ();
					keyproc = MOUSEfunction;
				} else if (! ansi_ini && ansi_fini == 'M') {
					DIRECTurxvtgetxy ();
					keyproc = MOUSEfunction;
				} else {
					keyproc = ANSIseq;
				}
				return FUNcmd;
			}
		} else if (keyshift & alt_mask) {
			/* restore the previously filtered ESCAPE */
			return '\033';
		} else {
			/* just deliver the typed characters */
			return q_get ();
		}
	} else {	/* key pattern found in fkeymap table */
		q_clear ();

		keyshift |= fkeyshift;

		if (keyproc == ESCAPE) {
			/* mintty application escape key mode */
			return '\033';
		} else if (keyproc == DIRECTxterm) {
#ifdef debug_mouse
			printf ("scanned xterm mouse escape\n");
#endif
			DIRECTxtermgetxy ('M');
			keyproc = MOUSEfunction;
		} else if (keyproc == TRACKxterm) {
#ifdef debug_mouse
			printf ("scanned xterm mouse tracking escape\n");
#endif
			DIRECTxtermgetxy ('t');
			keyproc = MOUSEfunction;
		} else if (keyproc == TRACKxtermT) {
#ifdef debug_mouse
			printf ("scanned xterm mouse tracking escape\n");
#endif
			DIRECTxtermgetxy ('T');
			keyproc = MOUSEfunction;
		} else if (keyproc == DIRECTcrttool) {
			DIRECTcrttoolgetxy ();
			keyproc = MOUSEfunction;
		}

		return FUNcmd;
	}
  } else {
	q_put (ch);
  }

#endif	/* #else msdos */

  if (allow_keymap && dont_suppress_keymap && keyboard_mapping_active ()) {
	disable_modify_keys (True);
	while  ((res = map_key (ctrl_queue, 1, & found, & mapped)) == -1
		 /* prefix of some table entry */
		&& q_notfull ()
		&& bytereadyafter (keymap_delay, NIL_PTR)
	       )
	{
#ifdef fix_modified_input
		/* avoid xterm modifyOtherKeys mode to interfere with 
		   input method handling: will have to use separate queue
		 */
		q_put (_readchar_nokeymap ());
		plainly like this, we get an endless loop...
		now using disable_modify_keys workaround
#else
		q_put (read1byte ());
#endif
		/* exact match found? note for later consideration */
		if (found != NIL_PTR) {
			prev_found = found;
		}
	}
	disable_modify_keys (False);
	/* possible results:
		res >= 0: match found, no further prefix match
		res == -1, prev_found != NIL_PTR: prefix with previous match
		res == -1: prefix only, no further key typed
		res == -2: no match with previous match
	*/

#ifdef debug_kbmap
	printf ("mapping %s, res %d, found %s -> ", ctrl_queue, res, found);
#endif
	if (res < 0 && prev_found != NIL_PTR) {
		res = 1;
		if (found == NIL_PTR) {
#ifdef debug_kbmap
			printf ("rest %s, ", ctrl_queue + strlen (prev_found));
#endif
			putback_rest (ctrl_queue + strlen (prev_found));
		}
	} else if (res == -1) {
		res = map_key (ctrl_queue, 2, & found, & mapped);
#ifdef debug_kbmap
		printf ("find again %s, found %s, ", ctrl_queue, found);
#endif
	}

#ifdef debug_kbmap
	printf ("res %d\n", res);
#endif

	if (res >= 0) { /* key pattern detected in key mapping table */

handle_mapped:
		q_clear ();

		/* check if mapping defines multiple choices */
		while (* mapped == ' ') {
			mapped ++;
		}
		choice = strchr (mapped, ' ');
		while (choice != NIL_PTR && * choice == ' ') {
			choice ++;
		}
		if (choice != NIL_PTR && * choice != '\0') {
			choice_index = choose_char (mapped);
#ifdef debug_kbmap
			printf (" choose_char (%s) -> %d\n", mapped, choice_index);
#endif
			if (choice_index < 0) {
				mapped = NIL_PTR;
				/* mapping cancelled */
			} else {
				while (choice_index > 0
					&& mapped != NIL_PTR
					&& * mapped != '\0')
				{
					mapped = strchr (mapped, ' ');
					if (mapped != NIL_PTR) {
						while (* mapped == ' ') {
							mapped ++;
						}
					}
					choice_index --;
				}
			}
		}

		if (mapped != NIL_PTR) {
			queue_mode_mapped = True;
			while (* mapped != '\0' && * mapped != ' ') {
				q_put (* mapped);
				mapped ++;
			}
		}
#ifdef debug_kbmap
		printf (" queued %s\n", ctrl_queue);
#endif
	}

	if (q_len () > 0) {
		return q_get () /* deliver the first mapped byte */;
	} else {
		ring_bell ();
		flush ();	/* clear key mapping menu */
#define return_after_esc
#ifdef return_after_esc
		return CHAR_UNKNOWN;	/* actually "CHAR_NONE" */
#else
		if (is_menu_open () /* doesn't seem to work */) {
			return CHAR_UNKNOWN;	/* actually "CHAR_NONE" */
		} else {
			/* read another character */
			/* prompt should have been redrawn;
			   menu (file chooser) would stay blank...
			 */
			return _readchar ();
		}
#endif
	}
  } else {	/* not keyboard mapped */
	return q_get ();
  }

}


#define dont_debug_choose_char

static
int
choose_char (choices)
  char * choices;
{
  int choice_count = 0;
  char * choicepoi = choices;
  int choice_col = x;
  int choice_line = y + 1;

  char * thischoiceline;
  char * choicelines;
  menuitemtype * choicemenu;

  int li, ci, lastcol, lastrow;
  char * choicelinepoi;

  int selected;

#ifdef cut_picklist_to_screen
  FLAG reduced_menu = False;
#endif

	/* count choices */
	do {
		choice_count ++;
		choicepoi = strchr (choicepoi, ' ');
		while (choicepoi != NIL_PTR && * choicepoi == ' ') {
			choicepoi ++;
		}
	} while (choicepoi != NIL_PTR && * choicepoi != '\0');
#ifdef debug_choose_char
	printf ("choices <%s>\n", choices);
	printf ("choice_count %d\n", choice_count);
#endif

	lastrow = (choice_count - 1) / 10;
	choicelines = alloc (strlen (choices) + 1 + choice_count * 2);
	choicemenu = alloc ((lastrow + 1) * sizeof (menuitemtype));
	if (choicelines == NIL_PTR || choicemenu == (menuitemtype *) NIL_PTR) {
		if (choicelines != NIL_PTR) {
			free_space (choicelines);
		}
		ring_bell ();
		flush ();
		return -1;
	}

	/* adjust menu position if too far down */
	if (choice_line + lastrow + 3 > YMAX) {
		choice_line = y - lastrow - 3;
		if (choice_line < 0) {
			choice_line = 0;
		}
	}

	/* adjust menu position if on status line */
	if (input_active) {
		choice_line = YMAX - lastrow - 3;
		if (choice_line < 0) {
			choice_line = 0;
		}
		choice_col = lpos;
	}

	/* construct menu items */
	choicepoi = choices;
	while (* choicepoi == ' ') {
		choicepoi ++;
	}
	thischoiceline = choicelines;
	for (li = 0; li <= lastrow; li ++) {
		/* construct menu line */
		choicelinepoi = thischoiceline;
		if (li == lastrow) {
			lastcol = (choice_count - 1) % 10;
			if (lastcol < 0) {
				/* should not occur as choice_count > 0 */
				lastcol = 0;
			}
		} else {
			lastcol = 9;
		}
		for (ci = 0; ci <= lastcol; ci ++) {
			* choicelinepoi ++ = (ci + 1) % 10 + '0';
			* choicelinepoi ++ = ':';
			while (* choicepoi != ' ' && * choicepoi != '\0') {
				* choicelinepoi ++ = * choicepoi ++;
			}
			while (* choicepoi == ' ') {
				choicepoi ++;
			}
			if (ci < lastcol) {
				* choicelinepoi ++ = ' ';
			}
		}
		* choicelinepoi ++ = '\0';
		/* add menu line to menu */
#ifdef debug_choose_char
		printf ("choiceline <%s>\n", thischoiceline);
#endif
		/* fill menu item with collected data */
		fill_menuitem (& choicemenu [li], thischoiceline, NIL_PTR);

		thischoiceline = choicelinepoi;
	}
#ifdef debug_choose_char
	printf ("choicelines alloc %d length %d\n",
		strlen (choices) + 1 + choice_count * 2 + lastrow + 1,
		thischoiceline - choicelines);
#endif

#ifdef cut_picklist_to_screen
	if (lastrow >= YMAX - 2) {
		lastrow = YMAX - 3;
		reduced_menu = True;
		if (! input_active) {
			status_msg ("Warning: full menu too large to display");
		}
	}
#endif
	selected = popup_menu (choicemenu, lastrow + 1, 
				choice_col, choice_line, 
				"Choose character", False, False, "1234567890");
	if (input_active) {
		/* redraw_prompt () now checked and called in input () */
#ifdef cut_picklist_to_screen
	} else if (reduced_menu) {
		clear_status ();
#endif
	}

#ifdef debug_choose_char
	printf ("selected %d\n", selected);
#endif

	free_space (choicemenu);
	free_space (choicelines);

	if (selected < choice_count) {
		return selected;
	} else {
		return -1;
	}
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
