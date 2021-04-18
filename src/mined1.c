/*======================================================================*\
|*		Editor mined						*|
|*		part 1							*|
\*======================================================================*/

#include "mined.h"
#include "version.h"

#ifndef VAXC
#include <sys/stat.h>	/* umask () */
#endif

#include "textfile.h"
#include "charprop.h"
#include "encoding.h"
#include "io.h"
#include "termprop.h"


#if defined (vms) || defined (__TURBOC__)
#ifndef VAXC
#define wildcard_expansion
#endif
#endif

#define dont_debug_wildcard_expansion
#ifdef debug_wildcard_expansion
#define wildcard_expansion
#endif

#ifdef wildcard_expansion
#include <dirent.h>
#endif


#include <errno.h>	/* EEXIST */

#ifdef __CYGWIN__
#include <cygwin/version.h>	/* check CYGWIN_VERSION_DLL_MAJOR >= 1007 ? */
#include <locale.h>
#if CYGWIN_VERSION_DLL_MAJOR >= 1007
#include <langinfo.h>	/* available #if CYGWIN_VERSION_DLL_MAJOR >= 1005 */
#endif
#include <sys/utsname.h>
#endif


/**
   How to deal with non-matching tty and terminal sizes.
   Method 1: adjust_terminal_height
   	accept the tty size assumption and try to resize the terminal to it
   	- not needed anymore (see method 3)
   Method 2: adjust_to_actual_termsize
   	request the terminal size, process the reponse asynchronously
   	- what a weird idea, never fully implemented ...
   Method 3: checkwinsize ()
   	detect the terminal size: set the cursor way out the area, 
   	request cursor position
   	(or use specific size request as in method 1 but synchronously)
 */
#define dont_adjust_terminal_height
#define dont_adjust_to_actual_termsize	/* not fully implemented */


/*======================================================================*\
|*		Local function declarations and FLAGs			*|
\*======================================================================*/

static void flush_keyboard _((void));
static char * get_terminal_report_string _((char * s));

static void check_cjk_width _((void));
static void config_markers _((void));


/*======================================================================*\
|*		Data section						*|
\*======================================================================*/

char * debug_mined = NIL_PTR;

LINE * header;			/* Head of line list */
LINE * tail;			/* Last line in line list */
LINE * cur_line;		/* Current line in use */
LINE * top_line;		/* First line of screen */
LINE * bot_line;		/* Last line of screen */
char * cur_text;		/* Current char on current line in use */
int last_y;			/* Last y of screen. Usually SCREENMAX */
int x = 0;			/* screen column of current text position */
int y = 0;			/* screen row of current text position */
int line_number;		/* current line # determined by file_status */
int total_lines = 0;		/* Number of lines in file */
long total_chars = 0L;		/* Number of characters in file */

int lines_per_page = 00;	/* assumption for file_status */

FLAG loading = True;		/* Loading a file? Init True for error handling */
static FLAG utf8_auto_detected = False;

int hop_flag = 0;		/* Counter flag for the HOP function */
int hop_flag_displayed = 0;

FLAG quit = False;		/* Set on SIGQUIT or quit character typed */
FLAG winchg = False;		/* Set when window size has changed */
FLAG interrupted = False;	/* Set when a signal interrupts */
FLAG isscreenmode = False;	/* Set when screen mode is on */
FLAG stat_visible;		/* Set if status line is visible */
FLAG top_line_dirty = False;	/* Was menu line cleared? */
FLAG text_screen_dirty = False;	/* Was text display position affected? */

FLAG waitingforinput = False;	/* Set while waiting for the next command key */
FLAG reading_pipe = False;	/* Set if file should be read from stdin */
FLAG writing_pipe = False;	/* Set if file should be written to stdout */

char * minedprog;		/* store argv [0] for help invocation */
character quit_char = '\034';	/* ^\/^G character to cancel command */

int YMAX, XMAX;

long chars_saved;		/* # of chars in paste buffer */
long bytes_saved;		/* # of bytes in paste buffer */
int lines_saved;		/* # of lines in paste buffer */

char text_buffer [maxLINElen];	/* for get_line, modifications, get_tagline, build_string */

/**
   Yank variables
 */
char yank_file [maxFILENAMElen];
char yankie_file [maxFILENAMElen];
char spool_file [maxFILENAMElen];
char html_file [maxFILENAMElen];	/* temp. buffer for HTML embedding */
char panic_file [maxFILENAMElen];

/**
   Terminal properties
 */
static char * TERM;
static int terminal_type = -1;
static int terminal_version = 0;
static char * glyphs = NIL_PTR;
static FLAG combining_screen_selected = False;	/* explicitly selected ? */
static FLAG term_encoding_selected = False;	/* explicitly selected ? */
static FLAG text_encoding_selected = False;	/* explicitly selected ? */
static FLAG limited_marker_font = False;	/* -F to limit font usage? */
static FLAG very_limited_marker_font = False;	/* -FF to limit font usage? */
static FLAG explicit_selection_style = False;	/* -QQ or -Qq? */
FLAG suppress_unknown_cjk = True;	/* on CJK terminal if no Unicode mapping */
FLAG suppress_invalid_cjk = True;	/* on CJK terminal if invalid CJK code */
FLAG suppress_extended_cjk = False;	/* on CJK terminal if in extended code range */
FLAG suppress_surrogates = True;	/* suppress display of single Unicode surrogates */
FLAG suppress_non_BMP = False;		/* suppress display of non-BMP range */
FLAG suppress_EF = True;		/* suppress display of 0x*FFFE/F codes */
FLAG suppress_non_Unicode = True;	/* suppress display of non-Unicode codes */
FLAG utf_cjk_wide_padding = False; /* always display CJK on UTF double-width ? */
#ifdef pc_term
FLAG dark_term = True;		/* PC terminal */
#else
FLAG dark_term = False;		/* dark colour terminal ? */
#endif
FLAG darkness_detected = False;	/* could background colour be queried ? */
FLAG fg_yellowish = False;	/* foreground colour near yellow ? */
FLAG bright_term = False;	/* need to improved contrast ? */
FLAG bw_term = False;		/* black/white terminal ? */
FLAG suppress_colour = False;	/* don't use ANSI color settings */
static FLAG cygwin_console = False;
static FLAG can_report_props = True;

FLAG configure_xterm_keyboard = False;	/* deleteIsDEL, metaSendsEscape */

FLAG combining_mode = False;	/* UTF-8 combining character display support ? */
FLAG separate_isolated_combinings = True;	/* separated display of comb. at line beg./after TAB ? */

static unsigned long dec_features = 1 << 4;	/* assume SIXEL for mlterm */

#if defined (unix) || defined (ANSI)
/* window headline and icon text setting */
char * title_string_pattern = "";
char * mined_modf = " (*)";
#endif

FLAG apply_joining = True;	/* apply LAM/ALEF joining ? */


/*======================================================================*\
|*		File-related properties					*|
\*======================================================================*/

lineend_type default_lineend = lineend_LF;
#ifdef msdos
lineend_type newfile_lineend = lineend_CRLF;
#else
lineend_type newfile_lineend = lineend_LF;
#endif
FLAG writable;			/* Set if file cannot be written */
FLAG file_is_dir;		/* Tried to open a directory as file? */
FLAG file_is_dev;		/* Tried to open a char/block device file? */
FLAG file_is_fifo;		/* Read from a named pipe? */

FLAG cjk_text = False;		/* text in CJK encoding ? */
FLAG utf8_text = False;		/* text in UTF-8 representation ? */
FLAG utf16_file = False;	/* file encoded in UTF-16 ? */
FLAG utf16_little_endian = False;	/* UTF-16 file encoded little endian ? */
FLAG mapped_text = False;	/* text in 8 bit, non-Latin-1 representation ? */
FLAG ebcdic_text = False;
FLAG ebcdic_file = False;
FLAG utf8_lineends = True;	/* detect UTF-8 LS and PS line ends ? */
FLAG poormansbidi = True;	/* poor man's bidirectional support ? */

char file_name [maxFILENAMElen];	/* Name of file in use */

PROT fprot0 = 0644;	/* default prot. mode for new files */
PROT fprot1 = 0;	/* prot. mode for new file being edited */
PROT bufprot = 0600;	/* prot. mode for paste buffer file */
static PROT exeprot = 0111;	/* default prot. mask for executables */
PROT xprot = 0;		/* actual prot. mask representing +x option */


/*======================================================================*\
|*		Modes and options					*|
\*======================================================================*/

static FLAG cmdline_selected = False;

short MENU = 1;
FLAG use_file_tabs = True;
static int splash_level = 2;	/* Show no/text/SIXEL splash_logo? */

FLAG flags_changed = False;	/* Should flag menu area be redrawn? */
static FLAG quickmenu = True;	/* Right mouse button pops up menu */
static int wheel_scroll = 3;	/* Number of lines scrolled by mouse wheel */
FLAG modified = False;		/* Set when file is modified */
FLAG viewonly_mode = False;	/* Set when view only mode is selected */
FLAG viewonly_locked = False;	/* Enforced view only status */
FLAG viewonly_err = False;	/* Error view only status */
FLAG restricted = False;	/* Set when edited file shall not be switched */

/* emulation and keyboard assignment flags */
char emulation = 'm';		/* 'e'macs, 'p'ico, 'w'indows, word's'tar */
char keypad_mode = 'm';		/* 'm'ined, 'S'hift-select, 'w'indows */
FLAG shift_selection = False;	/* selection highlighting on shift-cursor */
FLAG mined_keypad = True;	/* Apply mined keypad assignments */
FLAG home_selection = False;	/* numeric keypad Home/End forced to Mark/Copy */
FLAG small_home_selection = False;	/* small keypad Home/End: Mark/Copy */
FLAG emacs_buffer = False;	/* enable emacs buffer fct for ^K/^T */
FLAG paste_stay_left = False;	/* cursor stays before pasted region */
FLAG tab_left = False;	/* Set if moving up/down on TAB should go left */
FLAG tab_right = False;	/* Set if moving up/down on TAB should go left */
FLAG plain_BS = False;	/* prefer BS to perform plain rather than smart? */

FLAG append_flag = False;	/* Set when buffer should be appended to */
FLAG pastebuf_utf8 = False;	/* Paste buffer always treated as UTF-8? */
FLAG rectangular_paste_flag = False;	/* Copy/Paste rectangular area? */
FLAG visselect_key = False;	/* Visual selection on unmodified cursor key? */
FLAG visselect_anymouse = False;	/* Visual selection on any mouse move? */
FLAG visselect_keeponsearch = False;	/* Keep visual selection on search ? */
FLAG visselect_setonfind = False;	/* Select search result? */
FLAG visselect_autocopy = True;		/* Auto-copy selection? */
FLAG visselect_copydeselect = True;	/* Deselect on copy? */
FLAG visselect_autodelete = False;	/* Delete selection on insert? */

FLAG insert_mode = True;	/* insert or overwrite */

int JUSlevel = 0;		/* Keep justified while typing? */
int JUSmode = 0;		/* 1: paragraphs end at empty line */
FLAG autoindent = True;		/* Auto indent on input of Enter? */
FLAG autonumber = True;		/* Auto numbering on input of Enter? */
FLAG backnumber = True;		/* Auto-undent numbering on input of BS? */
FLAG backincrem = False;	/* Auto-increment numbering on input of BS? */
FLAG lowcapstrop = False;	/* capitalize letter symbols on input? */
FLAG dispunstrop = False;	/* display stropped letters in bold? */
FLAG strop_selected = False;	/* was stropping explictly selected? */
char strop_style = ' ';		/* bold/underline? */
FLAG mark_HTML = UNSURE;	/* Display HTML marked ? */
FLAG mark_JSP = True;		/* Display JSP marked ? */

char backup_mode = 'a';		/* none ('\0'), simple, e/v/numbered, auto */
char * backup_directory = NIL_PTR;	/* directory name for backup files */
char * recover_directory = NIL_PTR;	/* directory name for recovery files */
static FLAG homedir_selected = False;	/* option -~ given? */
static char * inisearch = NIL_PTR;	/* Optional startup search string */

FLAG only_detect_text_encoding = False;
static FLAG only_detect_terminal_encoding = False;

FLAG wordnonblank = False;	/* Handle all non-blank sequences as words */
FLAG proportional = False;	/* Enable support for proportional fonts? */
FLAG hide_passwords = UNSURE;	/* Hide passwords in display */
FLAG filtering_read = False;	/* Use filter for reading file ? */
char * filter_read = NIL_PTR;	/* Filter for reading file */
FLAG filtering_write = False;	/* Use filter for writing file ? */
char * filter_write = NIL_PTR;	/* Filter for writing file */
FLAG show_vt100_graph = False;	/* Display letters as VT100 block graphics ? */
FLAG auto_detect = True;	/* Auto detect character encoding from file ? */
char * detect_encodings;	/* List of encodings to detect */
char language_tag = 0;
FLAG translate_output = False;	/* Transform output diacritics to strings */
int translen;			/* length of " */
char * transout;		/* Output transformation table */
int tabsize = 8;		/* Width of tab positions, 2 or 4 or 8 */
FLAG tabsize_selected = False;	/* Tab width selected by explicit option? */
FLAG expand_tabs = False;	/* Expand TABs to Spaces? */
FLAG controlQS = False;		/* must respect ^Q/^S handshake ? */
character erase_char = '\010';	/* effective (configured) char for erase left */

FLAG prefer_comspec = False;	/* for ESC ! command (cygwin) */
FLAG smart_quotes = True;	/* replace " with typographic quote ? */
static char * ext_status = NIL_PTR;	/* status line per option -_ */

FLAG disp_scrollbar = True;	/* shall scrollbar be displayed ? */
FLAG fine_scrollbar = True;	/* fine-grained UTF-8 scrollbar ? */
FLAG old_scrollbar_clickmode = False;	/* old left/right click semantics ? */
int scrollbar_width = 1;
FLAG update_scrollbar_lazy = True;	/* partial scrollbar refresh as needed ? */
static FLAG use_window_title = True;	/* filename display in window title? */
static FLAG use_window_title_query = True;	/* query old title / detect title encoding? */

static FLAG combining_mode_disabled = False;	/* combining mode explicitly disabled ? */
static FLAG U_mode_set = False;

FLAG detect_esc_alt = True;	/* Enable detection of Alt key by ESC prefix? */

char selection_space = SPACE_NEXT;	/* space behaviour in keyboard mapping menu */
FLAG enforce_keymap = True;	/* enable keyboard mapping even on non-suitable terminal */

FLAG page_scroll = False;	/* use scroll for page up/down */
FLAG page_stay = False;		/* stay at edge of screen after page up/down */
int display_delay = 3;		/* delay between display lines */

FLAG paradisp = False;		/* Shall paragraph end be distinguished? */

/**
   Line indicators
 */
static char TABdefault = '�';		/* default TAB indicator */
static char RETdefault = '�';		/* indicates line end */
static char PARAdefault = '�';		/* indicates end of paragraph */
char UNI_marker = '�';		/* Char to be shown in place of Unicode char */
char TAB_marker = ' ';		/* Char to be shown in place of tab chars */
char TAB0_marker = '\0';	/* Char to be shown at start of tab chars */
char TAB2_marker = '\0';	/* Char to be shown at end of tab chars */
char TABmid_marker = '\0';	/* Char to be shown in middle of tab chars */
unsigned long CJK_TAB_marker = 0x2026;	/* to be shown in place of tab */
char RET_marker = '\0';		/* Char indicating end of line (LF) */
char DOSRET_marker = '\0';	/* Char indicating DOS end of line (CRLF) */
char MACRET_marker = '\0';	/* Char indicating Mac end of line (CR) */
char PARA_marker = '\0';	/* Char indicating end of paragraph */
char RETfill_marker = '\0';	/* Char to fill the end of line with */
char RETfini_marker = '\0';	/* Char to fill last position of line with */
char SHIFT_marker = '�';	/* Char indicating that line continues */
char SHIFT_BEG_marker = '�';	/* Char indicating that line continues left */
char MENU_marker = '�';		/* Char to mark selected item */
char * UTF_TAB_marker = NIL_PTR;	/* Char to be shown in place of tab chars */
char * UTF_TAB0_marker = NIL_PTR;	/* Char to be shown at start of tab chars */
char * UTF_TAB2_marker = NIL_PTR;	/* Char to be shown at end of tab chars */
char * UTF_TABmid_marker = NIL_PTR;	/* Char to be shown in middle of tab chars */
char * UTF_RET_marker = NIL_PTR;	/* Char indicating end of line */
char * UTF_DOSRET_marker = NIL_PTR;	/* Char indicating DOS end of line */
char * UTF_MACRET_marker = NIL_PTR;	/* Char indicating Mac end of line */
char * UTF_PARA_marker = NIL_PTR;	/* Char indicating end of paragraph */
char * UTF_RETfill_marker = NIL_PTR;	/* Char to fill the end of line with */
char * UTF_RETfini_marker = NIL_PTR;	/* Char to fill last position of line with */
char * UTF_SHIFT_marker = NIL_PTR;	/* Char indicating that line continues */
char * UTF_SHIFT_BEG_marker = NIL_PTR;	/* Char indicating that line continues left */
char * UTF_MENU_marker = "✓";		/* Char to mark selected item */
static char * UTF_MENU_marker_fancy = "☛";
static char * UTF_MENU_marker_alt = "►";
char * submenu_marker = "▶";
static char * submenu_marker_alt = "►";
char * menu_cont_marker = "┆";
char * menu_cont_fatmarker = "┇";

/**
   Information display preferences
 */
FLAG always_disp_fstat = True;	/* Permanent file status display on status line? */
FLAG always_disp_help = False;	/* Permanent F2... help display on status line? */
FLAG always_disp_code = False;	/* Permanent char code display on status line? */
FLAG disp_scriptname = True;	/* display Unicode script range? */
FLAG disp_charname = True;	/* display Unicode character name? */
FLAG disp_charseqname = True;	/* display Unicode named sequences? */
FLAG disp_decomposition = False;	/* display Unicode character decomposition? */
FLAG disp_mnemos = False;	/* display mined input mnemos? */

FLAG always_disp_Han = False;	/* Permanent Han character description display on status line? */
FLAG disp_Han_Mandarin = False;	/* display this Han pronunciation ? */
FLAG disp_Han_Cantonese = False;	/* display this Han pronunciation ? */
FLAG disp_Han_Japanese = False;	/* display this Han pronunciation ? */
FLAG disp_Han_Sino_Japanese = False;	/* display this Han pronunciation ? */
FLAG disp_Han_Hangul = False;	/* display this Han pronunciation ? */
FLAG disp_Han_Korean = False;	/* display this Han pronunciation ? */
FLAG disp_Han_Vietnamese = False;	/* display this Han pronunciation ? */
FLAG disp_Han_HanyuPinlu = False;	/* display this Han pronunciation ? */
FLAG disp_Han_HanyuPinyin = False;	/* display this Han pronunciation ? */
FLAG disp_Han_XHCHanyuPinyin = False;	/* display this Han pronunciation ? */
FLAG disp_Han_Tang = False;	/* display this Han pronunciation ? */
FLAG disp_Han_description = True;	/* display Han description ? */
FLAG disp_Han_full = True;	/* display full popup Han description ? */


/*======================================================================*\
|*		Mouse and other terminal interaction			*|
\*======================================================================*/

static FLAG in_selection_mouse_mode = False;

static
void
selection_mouse_mode (selecting)
  FLAG selecting;
{
  if (use_mouse_release) {
	in_selection_mouse_mode = selecting;
	menu_mouse_mode (selecting);
  }
}

static
void
mouse_scroll (S, P)
  voidfunc S;
  voidfunc P;
{
  if (in_selection_mouse_mode == True) {
	in_selection_mouse_mode = VALID;
	setMARK (True);
  }

  if (mouse_shift & shift_button) {
	(* P) ();
  } else if (mouse_shift & control_button) {
	(* S) ();
  } else if (disp_scrollbar && mouse_xpos == XMAX) {
	hop_flag = 1;
	(* S) ();
  } else {
	int l;
	for (l = 0; quit == False && l < wheel_scroll && l < YMAX; l ++) {
		if (l > 0 && disp_scrollbar) {
			(void) display_scrollbar (True);
		}
		(* S) ();
	}
  }

  if (in_selection_mouse_mode) {
	move_to (mouse_xpos, mouse_ypos);
  }
}

mousebutton_type last_mouse_event = movebutton;
static mousebutton_type prev_mouse_event = movebutton;
static int prev_x = -1;
static int prev_y = -1;

#define dont_debug_mouse

/*
 * MOUSEfunction () is invoked after a mouse escape sequence with 
   coordinates and button info already stored in 
   mouse_xpos, mouse_ypos, and mouse_button.
   Then it performs a menu or other mouse controlled function.
 * FOCUSout and FOCUSin are special cases of mouse report for 
   the window losing or gaining focus.
 * AMBIGnarrow and AMBIGwide are mintty reports of changed ambiguous 
   character width (corresponding to xterm option -cjk_width).
 */
/*
   MOUSEfunction is most often called after checking parameter from 
   _readchar, as follows:
	(_readchar:)
	DIRECTxtermgetxy ('M');
	keyproc = MOUSEfunction;

   MOUSEfunction may also be called after reading and arranging the 
   parameters, as follows:
   DIRECTxterm ()
   {
     DIRECTxtermgetxy ('M');
     MOUSEfunction ();
   }
 */
void
MOUSEfunction ()
{
  if (! window_focus) {
	return;
  }
  prev_mouse_event = last_mouse_event;
  last_mouse_event = mouse_button;

  trace_mouse ("MOUSEfunction");

  if (mouse_ypos < -1) {
	/* click in filename tab label area */
	if (use_file_tabs) {
		char * fn = filelist_search (mouse_ypos + MENU, mouse_xpos);
		if (fn) {
			if (! streq (fn, file_name)) {
				save_text_load_file (fn);
			}
		} else {
			/* "No specific file selected" - clicked on gap */
		}
	}
  } else if (mouse_ypos == -1
	&& ! in_selection_mouse_mode
	/* double-check whether movebutton enabled for buggy Haiku Terminal */
	&& (mouse_button != movebutton || use_mouse_anymove_always)
	) {
	openmenuat (mouse_xpos);
  } else if (mouse_button == wheelup) {
	mouse_scroll (SU, MOVPU);
  } else if (mouse_button == wheeldown) {
	mouse_scroll (SD, MOVPD);
  } else if (disp_scrollbar && mouse_xpos == XMAX) {
	static int last_dir = 1;
	if (mouse_button == leftbutton) {
		if (old_scrollbar_clickmode) {
			MOVPD ();
		} else {
			int cmp = check_scrollbar_pos ();
			if (cmp < 0) {
				MOVPU ();
				last_dir = -1;
			} else if (cmp > 0) {
				MOVPD ();
				last_dir = 1;
			}
		}
	} else if (mouse_button == rightbutton) {
		if (old_scrollbar_clickmode) {
			MOVPU ();
		} else {
			int cmp = check_scrollbar_pos ();
			if (cmp < 0) {
				MOVPD ();
			} else if (cmp > 0) {
				MOVPU ();
			} else if (last_dir > 0) {
				MOVPU ();
			} else {
				MOVPD ();
			}
		}
	} else if (mouse_button == middlebutton
		   || (mouse_button == releasebutton
		       && mouse_prevbutton == middlebutton)
		   || (mouse_button == movebutton
		       /* double-check for the sake of buggy Haiku Terminal */
		       && mouse_buttons_pressed > 0
		      )
		  ) {
		int proz = (mouse_ypos + 1) * 100 / YMAX;
		if (proz > 100) {
			goproz (100);
		} else {
			goproz ((mouse_ypos + 1) * 100 / YMAX);
		}
	}
#if defined (unix) || defined (vms)
	/* enable scrollbar dragging */
	if (mouse_button != releasebutton) {
		mouse_button_event_mode (True);
	}
#endif
  } else if (mouse_button == movebutton) {
	/* keep drag-and-copy enabled */
	report_release = True;
#ifdef debug_mouse
	printf ("MOUSEfunction: report_release = True\n");
#endif

	/* workaround for rxvt-unicode quirk:
		urxvt focus-in sends 3 mouse event codes: click, move, release
	 */
	if (mouse_prevbutton == leftbutton
	    && mouse_xpos == mouse_prevxpos && mouse_ypos == mouse_prevypos
	   ) {
		if (rxvt_version > 0) {
			FOCUSin ();
			last_mouse_event = mouse_button;
		}
	} else if (in_selection_mouse_mode) {
		/* initiate visual selection */
		if (in_selection_mouse_mode == True) {
			in_selection_mouse_mode = VALID;
			setMARK (True);
		}
		/* extend visual selection */
		if (mouse_ypos == -1) {
			mouse_scroll (SU, MOVPU);
			move_to (mouse_xpos, 0);
		} else if (mouse_ypos == YMAX) {
			mouse_scroll (SD, MOVPD);
			move_to (mouse_xpos, YMAX - 1);
		} else {
			FLAG alt_mouse = mouse_shift & alt_button
					? True
					: False;
			adjust_rectangular_mode (alt_mouse);
			move_to (mouse_xpos, mouse_ypos);
		}
	} else if (use_mouse_anymove_always) {
		/* click-free mouse interface (alpha, experimental) */
		if (! char_ready_within (350, "mouse")) {
			if (mouse_ypos == -1) {	/* menu stuff */
				openmenuat (mouse_xpos);
			}
		}
	}
  } else if (mouse_ypos == YMAX
	     /* ensure that release on bottom line terminates selection */
	     && mouse_button != releasebutton) {
	if (mouse_button == leftbutton) {
		MOVPD ();
	} else if (mouse_button == middlebutton) {
		if (always_disp_fstat) {
			display_code ();
		} else {
			FS ();
		}
	} else if (mouse_button == rightbutton) {
		MOVPU ();
	}
  } else if (quickmenu) {
	if (mouse_ypos > last_y) {
		mouse_ypos = last_y;
	}

	if (mouse_button == leftbutton) {
		char * prev_text = cur_text;
		prev_x = x;
		prev_y = y;
		move_to (mouse_xpos, mouse_ypos);

		if (x == prev_x && y == prev_y
		    && cur_text == prev_text	/* unless moved or shifted */
		    && last_delta_readchar < 500	/* unless too slow */
		   ) {
			/* word selection on double click */
			MNW ();
			setMARK (True);
			MPW ();
		} else {
			if (! visselect_autocopy && ! mouse_shift) {
				clear_highlight_selection ();
				/* setMARK (True); */
			}
			if (shift_selection && (mouse_shift & shift_button)) {
				continue_highlight_selection (mouse_xpos);
			}
		}

		/* enable drag-and-copy */
		report_release = True;
#ifdef debug_mouse
		printf ("MOUSEfunction: report_release = True\n");
#endif

		/* enable visual selection */
		selection_mouse_mode (True);
	} else if (mouse_button == middlebutton) {
		if (always_disp_fstat) {
			display_code ();
		} else {
			FS ();
		}
	} else if (mouse_button == rightbutton) {
		if (! shift_selection) {
			move_to (mouse_xpos, mouse_ypos);
		}
		display_flush ();
		QUICKMENU ();
	} else if (mouse_button == releasebutton 
			&& mouse_prevbutton == leftbutton 
			&& (mouse_xpos != mouse_prevxpos 
			    || mouse_ypos != mouse_prevypos)) {
		/* mouse-drag selection in terminal without move reports */
		if (in_selection_mouse_mode != VALID) {
			setMARK (True);
		}
		move_to (mouse_xpos, mouse_ypos);
		if (visselect_autocopy) {
			COPY ();
		}
		if (in_selection_mouse_mode) {
			selection_mouse_mode (False);
		}
	} else if (mouse_button == releasebutton && in_selection_mouse_mode
			&& (mouse_xpos != mouse_prevxpos 
			    || mouse_ypos != mouse_prevypos)) {
		/* mouse-drag selection in terminal with mouse move reports */
		move_to (mouse_xpos, mouse_ypos);
		if (visselect_autocopy) {
			COPY ();
		}
		selection_mouse_mode (False);
	} else if (mouse_button == releasebutton
		   && prev_mouse_event == focusin
		   && rxvt_version > 0
		  ) {
			/* try to revert previous click-positioning? */
			move_to (prev_x, prev_y);
	} else {
		/* catch short click/release to not keep up selection */
		if (in_selection_mouse_mode) {
			selection_mouse_mode (False);
		}
	}
  } else {
	prev_x = x;
	prev_y = y;

	if (mouse_ypos > last_y) {
		mouse_ypos = last_y;
	}
	move_to (mouse_xpos, mouse_ypos);

	if (mouse_button == leftbutton) {
		if (x == prev_x && y == prev_y) {
			MNW ();
			setMARK (True);
			MPW ();
		} else {
			setMARK (True);
		}
	} else if (mouse_button == middlebutton) {
		PASTE ();
	} else if (mouse_button == rightbutton) {
		COPY ();
	}
  }
}

void
FOCUSout ()
{
#ifdef debug_mouse
  printf ("mouse focus out\n");
#endif
  window_focus = False;
  mouse_button = focusout;
}

void
FOCUSin ()
{
#ifdef debug_mouse
  printf ("mouse focus in\n");
#endif
  window_focus = True;
  mouse_button = focusin;
}

void
AMBIGnarrow ()
{
  /*check_cjk_width ();*/

  width_data_version = cjk_width_data_version;
  cjk_width_data_version = 0;
  config_markers ();

  RDwin ();
}

void
AMBIGwide ()
{
  /*check_cjk_width ();*/
  cjk_width_data_version = width_data_version;

  fine_scrollbar = False;
  limited_marker_font = True;
  if (! explicit_border_style) {
	use_stylish_menu_selection = False;
  }
  config_markers ();

  RDwin ();
}


/*======================================================================*\
|*		Screen size handling					*|
\*======================================================================*/

#ifdef msdos
#ifdef __TURBOC__
#define msdos_screenfunctions
#endif
#endif


#define dont_debug_checkwinsize

#ifdef debug_checkwinsize
#define trace_checkwinsize(params)	printf params
#else
#define trace_checkwinsize(params)	
#endif

static
void
change_screen_size (sb, keep_columns)
  FLAG sb;
  FLAG keep_columns;
{
  trace_checkwinsize (("change_screen_size (HOP %d) SML/BIG %d keep_col %d\n", hop_flag, sb, keep_columns));
/* Experimental area: */
/*	set_screen_mode (mode1);	any available mode number */
/*	set_video_lines (mode1);	0/1/2: 200/350/400 lines */
	/* does not seem to have any effect */
/*	set_textmode_height (mode1);	0/1/2: font height 8/14/16 */
/*	set_grafmode_height (mode1, mode2);
		0/1/2: font height 8/14/16 1/2/3/n: 14/25/43/n lines */
/*	set_fontbank (f);		0..7 */
/**/
  if (hop_flag > 0) {
    int index;
    int mode1;

#ifdef msdos_screenfunctions
    int mode2;

    if (keep_columns) {
      if (sb == BIGGER) {
	index = get_number ("Switch to font bank (0..7) ", '\0', & mode1);
	if (index == ERRORS) {
		return;
	}
	set_fontbank (mode1);
      } else {
	index = get_number ("Set character height (<= 32 pixels) ", '\0', & mode1);
	if (index == ERRORS) {
		return;
	}
	set_font_height (mode1);
      }
    } else {
      if (sb == BIGGER) {
#endif
	index = get_number ("Select video mode ", '\0', & mode1);
	if (index == ERRORS) {
		return;
	}
	set_screen_mode (mode1);
#ifdef msdos_screenfunctions
      } else {
	index = get_number ("Select graf font (0/1/2: font height 8/14/16) ", '\0', & mode1);
	if (index == ERRORS) {
		return;
	}
	index = get_number ("Select line number (1/2/3/n: 14/25/43/n) ", '\0', & mode2);
	if (index == ERRORS) {
		return;
	}
	set_grafmode_height (mode1, mode2);	/* 0/1/2: font height 8/14/16 */
					/* 1/2/3/n: 14/25/43/n lines */
      }
    }
#endif
  } else {
	resize_the_screen (sb, keep_columns);
  }
  RDwin ();
}

static
void
change_font_size (inc)
  int inc;
{
  resize_font (inc);
  RDwin ();
}

void
screenmorelines ()
{
  change_screen_size (BIGGER, True);
}

void
screenlesslines ()
{
  change_screen_size (SMALLER, True);
}

void
screenmorecols ()
{
  change_screen_size (BIGGER, NOT_VALID);
}

void
screenlesscols ()
{
  change_screen_size (SMALLER, NOT_VALID);
}

void
screenbigger ()
{
  change_screen_size (BIGGER, False);
}

void
screensmaller ()
{
  change_screen_size (SMALLER, False);
}

void
fontbigger ()
{
  change_font_size (1);
}

void
fontsmaller ()
{
  change_font_size (-1);
}

void
fontdefault ()
{
  change_font_size (0);
}

void
LNCI ()
{
  switch_textmode_height (True);
  RDwin ();
}

void
LNSW ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	LNCI ();
  } else {
	switch_textmode_height (False);
	RDwin ();
  }
}


/*======================================================================*\
|*		Restriction handling					*|
\*======================================================================*/

/*
 * viewonlyerr () outputs an error message with a beep
 */
void
viewonlyerr ()
{
  ring_bell ();
  error ("View only mode");
}

/*
 * restrictederr () outputs an error message with a beep
 */
void
restrictederr ()
{
  ring_bell ();
  error2 ("Restricted mode", " - function not allowed");
}


/*
 * Set the modified flag
 */
void
set_modified ()
{
  if (modified == False) {
	modified = True;
#ifdef unix
	RD_window_title ();
#endif
  }
  clear_highlight_selection ();
}


/*======================================================================*\
|*		Keyboard macros						*|
\*======================================================================*/

#define dont_enable_key_macros

#ifdef enable_key_macros
# define debug_key_macros
#endif

#ifdef debug_key_macros
#define trace_key_macros(params)	printf params
#else
#define trace_key_macros(params)	
#endif

static FLAG key_recording = False;
static FLAG key_replaying = False;

#ifndef enable_key_macros
void
KEYREC ()
{
}
static
void
KEYrecord (key)
  unsigned long key;
{
}
static
unsigned long
KEYreplay ()
{
  return 0;
}
#else

/**
   first experimental attempt: one static limited buffer
   plan: multiple unlimited buffers, assignable to keys
 */
static struct {
	unsigned long key;
	unsigned char shift;
	voidfunc proc;
} keyrec [] = {
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
	{0, 0, I},
};
#define maxkeyreclen arrlen (keyrec)

static int keyreci = 0;
static int keyreclen = 0;

void
KEYREC ()
{
  if (keyshift & shift_mask) {
	/* Record keyboard macro */
	if (key_replaying) {
		/* display notice that key replaying is aborted? */
		key_replaying = False;
	}

	if (key_recording) {
		/* Stop keyboard macro recording */
		key_recording = False;
		keyreclen --;	/* fix KEYREC already inserted */
	} else {
		/* Start keyboard macro recording */
		key_recording = True;
		keyreci = 0;
	}
  } else {
	/* Replay keyboard macro */
	if (key_recording) {
		/* display notice that key recording is aborted? */
		key_recording = False;
	}

	if (key_replaying) {
		/* abort invalid recursive macro replaying */
		key_replaying = False;
		ring_bell ();
	} else if (keyreclen > 0) {
		/* Start keyboard macro replaying */
		keyreci = 0;
		key_replaying = True;
	} else {
		/* display notice that key replaying is not available? */
		ring_bell ();
	}
  }
}

static
void
KEYrecord (key)
  unsigned long key;
{
  if (keyreci >= maxkeyreclen) {
	key_recording = False;
	trace_key_macros (("KEYrecord buf\n"));
	ring_bell ();
  } else {
	keyrec [keyreci].key = key;
	keyrec [keyreci].shift = keyshift;
	keyrec [keyreci].proc = keyproc;
	trace_key_macros (("KEYrecord [%d] %02lX\n", keyreci, key));
	keyreci ++;
	keyreclen = keyreci;
  }
}

static
unsigned long
KEYreplay ()
{
  unsigned long key = keyrec [keyreci].key;
  keyshift = keyrec [keyreci].shift;
  keyproc = keyrec [keyreci].proc;
  trace_key_macros (("KEYreplay [%d (%d)] %02lX\n", keyreci, keyreclen, key));
  keyreci ++;
  if (keyreci >= keyreclen) {
	key_replaying = False;
  }
  return key;
}

#endif


/*======================================================================*\
|*		Setup display preferences				*|
\*======================================================================*/

/**
   Configure line and display markers.
   Width data detection must be settled for the case of UTF-8.
 */
static
void
config_markers ()
{
  char * Mark;

  Mark = envvar ("MINEDSHIFT");
  if (Mark != NIL_PTR) {
	SHIFT_BEG_marker = Mark [0];
	if (SHIFT_BEG_marker == ' ') {
		SHIFT_BEG_marker = '\0';
	}
	if (Mark [0] && Mark [1]) {
		SHIFT_marker = Mark [1];
	}
  }

  Mark = envvar ("MINEDTAB");
  if (Mark != NIL_PTR) {
	if (Mark [0] == '\0') {
		TAB_marker = TABdefault;
	} else {
		TAB_marker = Mark [0];
		if (Mark [1]) {
			if (Mark [2]) {
				TAB0_marker = Mark [0];
				TAB_marker = Mark [1];
				TAB2_marker = Mark [2];
			} else {
				TABmid_marker = Mark [1];
			}
		}
		if (TAB_marker >= ' ' && TAB_marker != '\\' && TAB_marker < '~') {
			CJK_TAB_marker = TAB_marker;
		}
	}
  } else {
	TAB_marker = TABdefault;
  }

  Mark = envvar ("MINEDRET");
  if (Mark != NIL_PTR) {
	RET_marker = Mark [0];
	if (RET_marker != '\0') {
		RETfill_marker = Mark [1];
	}
	if (RETfill_marker != '\0') {
		RETfini_marker = Mark [2];
	}
  } else {
	RET_marker = RETdefault;
  }
  Mark = envvar ("MINEDDOSRET");
  if (Mark && * Mark) {
	DOSRET_marker = Mark [0];
  } else {
	if (bw_term) {
		DOSRET_marker = '�';
	} else {
		DOSRET_marker = RET_marker;
	}
  }
  Mark = envvar ("MINEDMACRET");
  if (Mark && * Mark) {
	MACRET_marker = Mark [0];
  } else {
	if (bw_term) {
		MACRET_marker = '@';
	} else {
		MACRET_marker = RET_marker;
	}
  }
  Mark = envvar ("MINEDPARA");
  if (Mark && * Mark) {
	PARA_marker = Mark [0];
  } else {
	PARA_marker = PARAdefault;
  }
  Mark = envvar ("MINEDMENUMARKER");
  if (Mark) {
	if (* Mark) {
		MENU_marker = Mark [0];
	} else {
		/*MENU_marker = '`';*/
		MENU_marker = '*';
	}
  }

  if (cjk_width_data_version) {
	submenu_marker = submenu_marker_alt;
  }

  if (! limited_marker_font) {
	/* unlimited font settings */
	UTF_SHIFT_BEG_marker = envvar ("MINEDUTFSHIFT");
	if (UTF_SHIFT_BEG_marker && * UTF_SHIFT_BEG_marker) {
		UTF_SHIFT_marker = UTF_SHIFT_BEG_marker;
		advance_utf8 (& UTF_SHIFT_marker);
		if (* UTF_SHIFT_BEG_marker == ' ') {
			UTF_SHIFT_BEG_marker = "";
		}
	}
	UTF_TAB_marker = envvar ("MINEDUTFTAB");
	if (UTF_TAB_marker != NIL_PTR) {
		char * markpoi = UTF_TAB_marker;
		if (* markpoi) {
			advance_utf8 (& markpoi);
			if (* markpoi) {
				UTF_TAB0_marker = UTF_TAB_marker;
				UTF_TAB_marker = markpoi;
				advance_utf8 (& markpoi);
				if (* markpoi) {
					UTF_TAB2_marker = markpoi;
				} else {
					UTF_TABmid_marker = UTF_TAB_marker;
					UTF_TAB_marker = UTF_TAB0_marker;
					UTF_TAB0_marker = NIL_PTR;
				}
			}
		}
	}
	UTF_RET_marker = envvar ("MINEDUTFRET");
	if (UTF_RET_marker != NIL_PTR) {
		UTF_RETfill_marker = UTF_RET_marker;
		if (* UTF_RETfill_marker != '\0') {
			advance_utf8 (& UTF_RETfill_marker);
		}
		UTF_RETfini_marker = UTF_RETfill_marker;
		if (* UTF_RETfini_marker != '\0') {
			advance_utf8 (& UTF_RETfini_marker);
		}
	}
	UTF_DOSRET_marker = envvar ("MINEDUTFDOSRET");
	if (UTF_DOSRET_marker == NIL_PTR) {
		if (bw_term) {
			UTF_DOSRET_marker = "µ";
		} else {
			UTF_DOSRET_marker = UTF_RET_marker;
		}
	}
	UTF_MACRET_marker = envvar ("MINEDUTFMACRET");
	if (UTF_MACRET_marker == NIL_PTR) {
		if (bw_term) {
			UTF_MACRET_marker = "@";
		} else {
			UTF_MACRET_marker = UTF_RET_marker;
		}
	}
	UTF_PARA_marker = envvar ("MINEDUTFPARA");
	Mark = envvar ("MINEDUTFMENUMARKER");
	if (Mark) {
		if (* Mark) {
			int len;
			unsigned long unichar;
			utf8_info (Mark, & len, & unichar);
			if (len > 1 && ! iswide (unichar) && ! iscombining (unichar)) {
				UTF_MENU_marker = Mark;
			}
		} else {
			UTF_MENU_marker = UTF_MENU_marker_fancy;
		}
	}
  } else if (! very_limited_marker_font) {
	/* limited font settings */
	UTF_MENU_marker = UTF_MENU_marker_alt;
	submenu_marker = submenu_marker_alt;
  } else {
	/* very limited font settings */
	UTF_MENU_marker = "»";
	submenu_marker = "»";
  }
}

#ifdef unix

static float dimfactor = 0.6;	/* MUST be >= 0 AND <= 1 ! */
static float bgdimfactor = 0.15;	/* MUST be >= 0 AND <= 1 ! */

static
char *
get_terminal_rgb (s)
  char * s;
{
  char * report = get_terminal_report_string (s);
  if (report) {
	return strstr (report, "rgb:");
  } else {
	return NIL_PTR;
  }
}

static
int
sscanrgb (s, __r, __g, __b)
  char * s;
  unsigned int * __r;
  unsigned int * __g;
  unsigned int * __b;
{
  char * rgb = strstr (s, "rgb:");
  if (rgb) {
	if (sscanf (rgb, "rgb:%04X/%04X/%04X", __r, __g, __b) == 3) {
		return 3;
	}
  }
  return 0;
}

/**
   Determine a suitable dim mode for marker display 
   	(TAB, line end indicators).
   Also check whether terminal background is dark (to adjust highlighting).
   * Retrieve color values of text and background colors.
   * Calculate a value between them.
   * Retrieve color value of ANSI color 7 (for later restoring).
   * Redefine ANSI color 7 to calculated color for use as dim attribute.
   Works in xterm, should work in rxvt (but doesn't...).
   The feature is activated if the environment variable MINEDDIM is 
   set to an empty value.
   The feature is not applied if the terminal provides a native dim mode.
 */
static
FLAG
determine_dim_mode (darkcheck_only)
  FLAG darkcheck_only;
{
  char * color_report;
  int ret;
  int r, g, b, _r, _g, _b, r_, g_, b_, r3, g3, b3;	/* signed! */

#define dont_debug_dim_mode

  if (tmux_version > 0) {
	return False;
  }

  /* check whether terminal can report ANSI colours */
  if (! (xterm_version > 2 || mintty_version >= 404 || rxvt_version >= 300 || gnome_terminal_version)) {
	return False;
  }

  /* query current ANSI color 3 to fix misconfigured yellow */
  color_report = get_terminal_rgb ("\033]4;3;?\033\\");
  if (! color_report) {
	return False;
  }
  /* only redefine yellow if it's grossly misconfigured */
  ret = sscanrgb (color_report, & r3, & g3, & b3);
#ifdef debug_dim_mode
  printf ("ANSI color 3 (yellow) %s -> %d: %04X %04X %04X\n", color_report, ret, r3, g3, b3);
#endif
  if (ret < 3) {
	return False;
  }
  /* build escape string to restore ANSI color 3 */
  build_string (restore_ansi3, "\033]4;3;%s\033\\", color_report);
  /* check whether yellow is too dark (e.g. actually more brown) */
  r = (r3 - 0xC000) >> 8;
  g = (g3 - 0xC000) >> 8;
  b = b3 >> 8;
  if (r * r + g * g + b * b > 12000) {
	redefined_ansi3 = True;
	/* build escape string to define ANSI color 3 as yellow */
	build_string (set_ansi3, "\033]4;3;rgb:C000/C000/0000\033\\");
#ifdef debug_dim_mode
	printf (" redefining yellow %s\n", set_ansi3 + 6);
#endif
	/* redefine ANSI color 3 as yellow */
	putescape (set_ansi3);
  }

  /* check whether terminal can report background colours */
  if (! (xterm_version > 0 || mintty_version >= 404 || rxvt_version >= 300)) {
	return False;
  }

  /* query terminal background color */
  color_report = get_terminal_rgb ("\033]11;?\033\\");
  if (! color_report) {
	return False;
  }
  /* analyse query result */
  ret = sscanrgb (color_report, & _r, & _g, & _b);
#ifdef debug_dim_mode
  printf ("background color %s -> %d: %04X %04X %04X\n", color_report, ret, _r, _g, _b);
#endif
  if (ret < 3) {
	return False;
  }

  darkness_detected = True;

  /* check dark background */
  if (_r + _g + _b < 99000) {	/* range is 0...65535 * 3 */
	dark_term = True;
#ifdef debug_dim_mode
	printf (" -> dark_term\n");
#endif
  }

  /* query color 48, the one with the largest difference between
     256 color mode and 88 color mode
   */
  color_report = get_terminal_rgb ("\033]4;48;?\033\\");
	if (color_report) {
	/* analyse query result */
	ret = sscanrgb (color_report, & r, & g, & b);
#ifdef debug_dim_mode
	printf ("color 48 %s -> %d: %04X %04X %04X\n", color_report, ret, r, g, b);
#endif
	/* check whether it's nearer to color 48 of
	   256 color mode or 88 color mode
	 */
	if (ret == 3) {
		/* difference to 88 color mode color 48 */
		int _r = r - 170;
		int _g = g - 0;
		int _b = b - 0;
		/* difference to 256 color mode color 48 */
		int r_ = r - 0;
		int g_ = g - 255;
		int b_ = b - 102;
		if (_r * _r + _g * _g + _b * _b < r_ * r_ + g_ * g_ + b_ * b_) {
			colours_88 = True;
			colours_256 = False;
		} else {
			colours_256 = True;
			colours_88 = False;
		}
#ifdef debug_dim_mode
		printf (" detected %d color mode\n", colours_256 ? 256 : 88);
#endif
	}
  }

#ifdef dont_use_dimmed_menu_bg
  if (darkcheck_only) {
	return dark_term;
  }
  /* check whether we need to dim ourselves */
  if (can_dim) {
#ifdef debug_dim_mode
	printf ("terminal has native dim attribute, not redefining colours\n");
#endif
	return False;
  }
#endif

  /* query terminal text color */
  color_report = get_terminal_rgb ("\033]10;?\033\\");
  if (! color_report) {
	return darkcheck_only ? dark_term : False;
  }
  /* analyse query result */
  ret = sscanrgb (color_report, & r, & g, & b);
#ifdef debug_dim_mode
  printf ("foreground color %s -> %d: %04X %04X %04X\n", color_report, ret, r, g, b);
#endif
  if (ret < 3) {
	return darkcheck_only ? dark_term : False;
  }

  if (! mlterm_version) {
    /* query terminal cursor color */
    color_report = get_terminal_rgb ("\033]12;?\033\\");
    if (color_report) {
	/* analyse query result */
	ret = sscanrgb (color_report, & r_, & g_, & b_);
#ifdef debug_dim_mode
	printf ("cursor color %s -> %d: %04X %04X %04X\n", color_report, ret, r_, g_, b_);
#endif
	if (ret == 3) {
	    if ((r_ == r && g_ == g && b_ == b) || (r_ == _r && g_ == _g && b_ == _b)) {
		redefined_curscolr = True;
		/* build escape string to restore cursor color */
		build_string (restore_curscolr, "\033]12;%s\033\\", color_report);
		/* ensure cursor contrast */
		/* calculate cursor color, between foreground and background */
		r_ = _r + (r - _r) / 2;
		g_ = _g + (g - _g) / 2;
		b_ = _b + (b - _b) / 2;
		/* build escape string to define cursor color */
		build_string (set_curscolr, "\033]12;rgb:%04X/%04X/%04X\033\\", r_, g_, b_);
#ifdef debug_dim_mode
		printf (" using cursor color %s\n", set_curscolr + 6);
#endif
		/* redefine cursor color */
		putescape (set_curscolr);
	    }
	}
    }
  }

  /* check whether text color is near yellow */
  r3 = (0xC000 - r) >> 8;
  g3 = (0xC000 - g) >> 8;
  b3 = (0x0000 - b) >> 8;
  /* use blue (for flags) rather than yellow */
  if (r3 * r3 + g3 * g3 + b3 * b3 < 12000) {
	fg_yellowish = True;
  }

  /* query current ANSI color 2 */
  color_report = get_terminal_rgb ("\033]4;2;?\033\\");
  if (! color_report) {
	return darkcheck_only ? dark_term : False;
  }
  /* build escape string to restore ANSI color 2 */
  build_string (restore_ansi2, "\033]4;2;%s\033\\", color_report);
  redefined_ansi2 = True;
  /* calculate menu bg color, dimmed from background */
  if (r + g + b > _r + _g + _b) {
	r_ = _r + (65535 - _r) * bgdimfactor;
	g_ = _g + (65535 - _g) * bgdimfactor;
	b_ = _b + (65535 - _b) * bgdimfactor;
#ifdef debug_dim_mode
	printf (" %d,%d,%d > %d,%d,%d -> %d,%d,%d\n", r, g, b, _r, _g, _b, r_, g_, b_);
#endif
  } else {
	r_ = _r + (0 - _r) * bgdimfactor;
	g_ = _g + (0 - _g) * bgdimfactor;
	b_ = _b + (0 - _b) * bgdimfactor;
#ifdef debug_dim_mode
	printf (" %d,%d,%d < %d,%d,%d -> %d,%d,%d\n", r, g, b, _r, _g, _b, r_, g_, b_);
#endif
  }
  /* build escape string to define ANSI color 2 as menu bg color */
  build_string (set_ansi2, "\033]4;2;rgb:%04X/%04X/%04X\033\\", r_, g_, b_);
#ifdef debug_dim_mode
  printf (" using menu bg color %s\n", set_ansi2 + 6);
#endif
  /* redefine ANSI color 2 as menu bg color */
  putescape (set_ansi2);

  if (darkcheck_only) {
#ifdef debug_dim_mode
	printf ("darkcheck only\n");
#endif
	return dark_term;
  }
  /* check whether we need to dim ourselves */
  if (can_dim) {
#ifdef debug_dim_mode
	printf ("terminal has native dim attribute, not redefining colours\n");
#endif
	return False;
  }

  /* query current ANSI color 7 */
  color_report = get_terminal_rgb ("\033]4;7;?\033\\");
  if (! color_report) {
	return False;
  }
  /* build escape string to restore ANSI color 7 */
  build_string (restore_ansi7, "\033]4;7;%s\033\\", color_report);
  redefined_ansi7 = True;
  /* calculate dim color, between foreground and background */
  r_ = _r + (r - _r) * dimfactor;
  g_ = _g + (g - _g) * dimfactor;
  b_ = _b + (b - _b) * dimfactor;
  /* mix in some red */
  r_ += (65535 - r_) / 2;
  /* build escape string to define ANSI color 7 as dim color */
  build_string (set_ansi7, "\033]4;7;rgb:%04X/%04X/%04X\033\\", r_, g_, b_);
#ifdef debug_dim_mode
  printf (" using dim color %s\n", set_ansi7 + 6);
#endif
  /* redefine ANSI color 7 as dim color */
  putescape (set_ansi7);

  return True;
}

#else

static
FLAG
determine_dim_mode (darkcheck_only)
  FLAG darkcheck_only;
{
  return False;
}

#endif

/**
   Setup configured display attributes for certain items
 */
static
void
get_ansi_modes ()
{
  markansi = envvar ("MINEDDIM");
  if (markansi != NIL_PTR) {
#ifdef unix
	/* check if MINEDDIM is a percentage value */
	int dim_percent;
	char c;
	int res = sscanf (markansi, "%d%c", & dim_percent, & c);
	if (res == 2 && c == '%' && dim_percent > 0 && dim_percent < 100) {
		dimfactor = dim_percent / 100.0;
		markansi = "";	/* trigger procedure below */
	}
#endif
  }
  if (markansi == NIL_PTR || * markansi == '\0') {
	if (determine_dim_mode (False)) {
		markansi = "37";	/* use redefined ANSI color 7 */
	} else {
		markansi = "31";	/* use red */
	}
  } else {
	(void) determine_dim_mode (True);
  }

  emphansi = envvar ("MINEDEMPH");
  if (emphansi == NIL_PTR) {
	emphansi = "31";	/* use red */
  }

  borderansi = envvar ("MINEDBORDER");
  if (borderansi == NIL_PTR) {
	borderansi = "31";
  }

  selansi = envvar ("MINEDSEL");
  selfgansi = envvar ("MINEDSELFG");
  if (selfgansi == NIL_PTR) {
	selfgansi = "43";
  }
  if (selansi == NIL_PTR) {
	if (dark_term) {
		selansi = "34;1";
	} else {
		selansi = "34";
	}
  }

  uniansi = envvar ("MINEDUNI");
  if (uniansi == NIL_PTR) {
	if (cjk_term) {
		/* needed for hanterm */
		uniansi = "36;7;40";
	} else {
		uniansi = "40;36;7";
	}
  } else if ((character) * uniansi > '9') {
	UNI_marker = * uniansi;
	do {
		uniansi ++;
	} while (* uniansi == ' ');
  }
  specialansi = envvar ("MINEDSPECIAL");
  if (specialansi == NIL_PTR) {
	specialansi = "36;1";
  }
  combiningansi = envvar ("MINEDCOMBINING");
  if (combiningansi == NIL_PTR) {
	combiningansi = "46;30";
  }

  ctrlansi = envvar ("MINEDCTRL");
  if (ctrlansi == NIL_PTR) {
	ctrlansi = "";
  }
  menuansi = envvar ("MINEDMENU");
  if (menuansi == NIL_PTR) {
	menuansi = "";
  }
  HTMLansi = envvar ("MINEDHTML");
  if (HTMLansi == NIL_PTR) {
	if (dark_term) {
		HTMLansi = "36";	/* conflicts with JSP */
		HTMLansi = "1;34";	/* not sufficient with dark blue */
		HTMLansi = "34;42";
		HTMLansi = "34;46";
#ifdef special_case_for_very_dark
		if (streq ("cygwin", TERM)) {
			HTMLansi = "1;36";
		}
#endif
	} else {
		HTMLansi = "34";
	}
  }
  XMLattribansi = envvar ("MINEDXMLATTRIB");
  if (XMLattribansi == NIL_PTR) {
	if (dark_term) {
		XMLattribansi = "31;46";
	} else {
		XMLattribansi = "31";
	}
  }
  XMLvalueansi = envvar ("MINEDXMLVALUE");
  if (XMLvalueansi == NIL_PTR) {
	if (dark_term) {
		XMLvalueansi = "35;1;46";
	} else {
		XMLvalueansi = "35;1";
	}
  }
  diagansi = envstr ("MINEDDIAG");

  scrollbgansi = envvar ("MINEDSCROLLBG");
  if (scrollbgansi == NIL_PTR) {
	if (colours_256 || colours_88) {
		/*scrollbgansi = "34;48;5;45";*/
		scrollbgansi = "46;34;48;5;45";
	} else {
		scrollbgansi = "46;34";
	}
  }
  scrollfgansi = envvar ("MINEDSCROLLFG");
  if (scrollfgansi == NIL_PTR) {
	scrollfgansi = "";
	if (colours_256 || colours_88) {
		/*scrollfgansi = "44;38;5;45";*/
		/*scrollfgansi = "44;36;38;5;45";*/
	} else if (cjk_term && (text_encoding_tag == 'K' || text_encoding_tag == 'H')
		  && strisprefix ("xterm", TERM)
	    ) {
		/* probably hanterm; attributes will all be reverse 
		   and could not be distinguished to build the scrollbar
		 */
		/*scrollfgansi = "0";*/
		scrollfgansi = "44;36";
	} else {
		/*scrollfgansi = "44;36";*/
	}
  }
}


/*======================================================================*\
|*		Terminal report, window resize handling			*|
\*======================================================================*/

#define dont_debug_queries

#define dont_debug_ansiseq
#define dont_debug_expect

#ifdef debug_expect
#warning extra debug read will provoke hanging
# define debug_queries
#define trace_expect(params)	printf ("[%lu] ", gettime ()); printf params
static
unsigned long
gettime ()
{
  struct timeval now;
  gettimeofday (& now, 0);
  return ((long) now.tv_sec) * 1000000 + now.tv_usec;
}
#else
#define trace_expect(params)	
#endif

#ifdef debug_queries
# define debug_ansiseq
#define trace_query(params)	printf params
#else
#define trace_query(params)	
#endif


/* For the initial terminal report request, balance the delay time 
   (to accept a response) so that reponses via slow remote 
   terminal lines can be acquired but the user delay on a 
   terminal that doesn't respond at all remains acceptable.
 */
static int escape_delay = 0;		/* wait to detect escape sequence */
#ifdef vms
static int default_escape_delay = 2000;	/* overridden by $ESCDELAY */
#else
static int default_escape_delay = 450;	/* overridden by $ESCDELAY */
#endif

static
void
adjust_escdelay ()
{
  if (escape_delay == 0) {
	char * env = envvar ("ESCDELAY");
	if (env) {
		(void) scan_int (env, & escape_delay);
	}

	/**
	   The default escape waiting delay (or minimum, see below)
	   is a trade-off between terminals that do not respond to 
	   certain requests and supporting slow remote connections 
	   on which extended delay is needed before timeout.
	 */
	if (strisprefix ("rxvt", TERM)) {
		/* accept slow rxvt DA report */
		default_escape_delay = 25555;
	} else if (strisprefix ("xterm", TERM)
		|| strisprefix ("screen", TERM)
		|| (strisprefix ("vt", TERM) && TERM [2] >= '2' && TERM [2] < '5')
		) {
		/* support slower remote connections */
		default_escape_delay = 3333;
	}

	/* if unset (== 0) or too small (e.g. ESCDELAY=200), set to default */
	if (escape_delay < default_escape_delay) {
		escape_delay = default_escape_delay;
	}
  }
}

static
character
expect1byte (timeout, debug_tag)
  FLAG timeout;
  char * debug_tag;
{
  character c;
  FLAG awaiting = True;
  int delay = escape_delay;
  if (timeout == UNSURE) {
	/* timeout, but not too long, in case response is disabled */
	delay = 555;
  }

  if (xterm_version > 0 || mintty_version > 0 || rxvt_version > 0) {
	/* if we know the terminal should respond, don't timeout 
	   in order to avoid garbage from late responses
	 */
  }

  trace_expect (("expect1byte [%s] (%d) escdel %d/%d\n", unnull (debug_tag), timeout, escape_delay, delay));
  if (char_ready_within (delay, "expect")) {
	awaiting = False;
  }

  if (timeout && awaiting) {
	trace_expect (("expect1byte [%s] timeout\n", unnull (debug_tag)));
#ifdef debug_expect
	c = read1byte ();
	trace_expect (("expect1byte [%s] read after timeout %02X\n", unnull (debug_tag), c));
#endif
	return 0;
  }

  if (awaiting) {
	status_uni ("... awaiting slow terminal response ...");
  }

  if (timeout && streq (debug_tag, "acquire") && strisprefix ("rxvt", TERM)) {
	status_uni ("... waiting for rxvt to report device attributes ...");
  }

  c = read1byte ();
  trace_expect (("expect1byte [%s] read %02X\n", unnull (debug_tag), c));

  if (awaiting) {
	clear_status ();
  }
  return c;
}


static
FLAG
receiving_response (c, debug_tag)
  character c;
  char * debug_tag;
{
  if (c == '\033') {
	return True;
  }
#ifdef debug_queries
  printf ("not receiving response [%s] (esc_delay %d) (%02X)\n", unnull (debug_tag), escape_delay, c);
#endif
  flush_keyboard ();
  return False;
}

/**
   Check ANSI escape sequence (which has already been read)
 */
void
ANSIseq ()
{
#ifdef debug_ansiseq
  printf ("ANSIseq %d:", ansi_params);
  {	int i;
	for (i = 0; i < ansi_params; i ++) {
		printf (" %d", ansi_param [i]);
	}
	printf (" %c\n", ansi_fini);
  }
#endif

  if (ansi_fini == 'R') {
	/* could also be modified F3, though, not to arrive here */
	status_line ("Late screen mode response ",
		"- set ESCDELAY=2000 or higher for proper detection");
  } else if (ansi_fini == 't') {
	if (ansi_params == 3 && ansi_param [0] == 8) {
# ifdef debug_terminal_resize
		printf ("received terminal size report %dx%d\n", ansi_param [1], ansi_param [2]);
# endif
#ifdef adjust_to_actual_termsize
		/* adjust to actual screen size reported by terminal */
		if (YMAX != ansi_param [1] - 1 - MENU || XMAX != ansi_param [2] - 1) {
			YMAX = ansi_param [1] - 1 - MENU;
			XMAX = ansi_param [2] - 1;
			RD ();
			... see RDwin
			flush ();
		}
#endif
	} else {
		error ("Unknown terminal status report");
	}
  } else if (ansi_fini == 'c') {
	if (strisprefix ("rxvt", TERM)) {
		error ("Late device attribute report - restart mined for proper screen detection");
	} else {
		error ("Unexpected (delayed) device attribute report");
	}
  } else {
	error ("Unknown keyboard control sequence");
  }
}


static
FLAG
get_CPR (rowpoi, colpoi)
  int * rowpoi;
  int * colpoi;
{
  character c;
  /* apply timeout to cursor position report? */
  static FLAG timeout_CPR = True;
  trace_checkwinsize (("get_CPR\n"));

  if (cygwin_console) {
	trace_query (("-> skipped on cygwin console\n"));
	return False;
  }

  adjust_escdelay ();
  trace_query (("query CPR\n"));

  c = expect1byte (timeout_CPR, "CPR");	/* ESC */
  if (receiving_response (c, "CPR")) {
	int row, col;
	timeout_CPR = False;
	c = expect1byte (False, "CPR.");	/* '[' */
	c = get_digits (& row);
	if (c == ';') {
		c = get_digits (& col);
		* rowpoi = row;
		* colpoi = col;

		trace_checkwinsize (("get_CPR %d %d\n", row, col));
		trace_query (("-> %d %d\n", row, col));
		return True;
	}
  }

  trace_checkwinsize (("get_CPR failed\n"));
  trace_query (("-> failed\n"));
  return False;
}

static
FLAG
get_TSZ (rowpoi, colpoi)
  int * rowpoi;
  int * colpoi;
{
  character c;
  /* apply timeout to cursor position report? */
  static FLAG timeout_CPR = True;
  trace_query (("query TSZ\n"));

  if (! can_report_props || xterm_version == 2) {
	trace_query (("-> skipped\n"));
	return False;
  }

  adjust_escdelay ();

  c = expect1byte (timeout_CPR, "TSZ");	/* ESC */
  if (receiving_response (c, "TSZ")) {
	int dum, row, col;
	timeout_CPR = False;
	c = expect1byte (False, "TSZ.");	/* '[' */
	c = get_digits (& dum);			/* '8' */
	c = get_digits (& row);
	if (c == ';') {
		c = get_digits (& col);
		* rowpoi = row;
		* colpoi = col;

		trace_query (("-> %d %d\n", row, col));
		return True;
	}
  }

  trace_query (("-> failed\n"));
  return False;
}

static FLAG can_get_winsize = True;

static
void
checkwinsize ()
{
  static int checking_winsize = 0;
  trace_checkwinsize (("checkwinsize\n"));
  if (can_get_winsize) {
	/* try to get screen size from terminal device driver (ioctl/BIOS) */
	getwinsize ();
	trace_checkwinsize (("using getwinsize: %d %d\n", YMAX + 1 + MENU, XMAX + 1));
  }
#ifndef msdos
  if (checking_winsize) {
	/* this doesn't need to be atomic with checking_winsize = 1 below
	   because there is no parallel or asynchronous processing here;
	   RDwinchg is not called from catchwinch anymore;
	   the recursive invocation can only occur from further down 
	   in the execution tree below 
	   (calling RDwinchg from readchar/__readchar)
	 */
	trace_checkwinsize (("skip checking winsize\n"));
	return;
  }
  if (ansi_esc) {
	int row, col;
	checking_winsize = 1;
	/* scrolling region is cleared during screen initialization */
	set_cursor (2222, 2222);
	trace_checkwinsize (("trying ^[[6n\n"));
	flush_keyboard ();
	putescape ("\033[6n");
	/* could probably use putescape ("\033[18t") rightaway (see below);
	   however, let's prefer the method used by the 'resize' tool
	 */
	flush ();
	trace_checkwinsize (("... waiting ...\n"));
	if (get_CPR (& row, & col) && row > 1 && col > 1) {
		row = row - 1 - MENU;
		col = col - 1;
		if (YMAX != row || XMAX != col) {
			YMAX = row;
			XMAX = col;
			can_get_winsize = False;
		}
		trace_checkwinsize (("using ^[[6n: %d %d\n", row + 1 + MENU, col + 1));
		checking_winsize = 0;
		return;
	}

	/* the cursor-beyond-limits method failed (maybe with curses...) */
	trace_checkwinsize (("trying ^[[18t\n"));
	flush_keyboard ();
	putescape ("\033[18t");	/* long timeout if response disabled? */
	flush ();
	trace_checkwinsize (("... waiting ...\n"));
	if (get_TSZ (& row, & col)) {
		row = row - 1 - MENU;
		col = col - 1;
		if (YMAX != row || XMAX != col) {
			YMAX = row;
			XMAX = col;
			can_get_winsize = False;
		}
		trace_checkwinsize (("using ^[[18t: %d %d\n", row + 1 + MENU, col + 1));
		checking_winsize = 0;
		return;
	}
	trace_checkwinsize (("terminal size detection failed\n"));
  }
  checking_winsize = 0;
#endif
}


/*======================================================================*\
|*		Screen redraw and Window title handling			*|
\*======================================================================*/

#ifdef unix

#define dont_debug_window_title

/*
 * Set window headline and icon text
 */
static
void
build_window_title (ws, title, icon)
  char * ws;
  char * title;
  char * icon;
{
  if (strcontains (title_string_pattern, "%d")) {
	/* hpterm */
	int lent = strlen (title);
	int leni = strlen (icon);
	build_string (ws, title_string_pattern, lent, title, leni, icon);
  } else {
	build_string (ws, title_string_pattern, title, icon);
  }
}

static char old_window_title [maxXMAX + 9];
static char old_icon_title [maxXMAX + 9];
static FLAG saved_old_window_title = False;

static
void
save_old_window_title ()
{
  if (xterm_version >= 251 || mintty_version >= 10003) {
	putescape ("\033[22t");
	saved_old_window_title = True;
  } else if (use_window_title_query && ansi_esc && xterm_version > 2 && ! mlterm_version) {
	/* title query can be disabled since xterm 174 (allowWindowOps)
	   the default is false since xterm 249 (security concerns);
	   to mitigate this, get_terminal_report_string enforces a 
	   shorter timeout (since 2014.24)
	 */
	char * t = get_terminal_report_string ("\033[21t");
	if (t && * t) {
		t ++;
		strcpy (old_window_title, t);
		saved_old_window_title = True;
		t = get_terminal_report_string ("\033[20t");
		if (t && * t) {
			t ++;
			strcpy (old_icon_title, t);
		}
	} else {
		use_window_title_query = False;
	}
  }
}

static
void
restore_old_window_title ()
{
  if (saved_old_window_title) {
	if (xterm_version >= 251 || mintty_version >= 10003) {
		putescape ("\033[23t");
	} else if (* old_window_title) {
		char window_string [2 * maxXMAX + 18];
		build_window_title (window_string, old_window_title, old_icon_title);
		putescape (window_string);
	}
  }
}

void
clear_window_title ()
{
  char window_string [2 * maxXMAX + 18];

  if (! use_window_title) {
	return;
  }

  if (! saved_old_window_title) {
	save_old_window_title ();
  }

  build_window_title (window_string, " ", " ");
  putescape (window_string);

  restore_old_window_title ();
}

static char title_encoding = ' ';
static FLAG title_encoding_xterm_to_be_checked = False;

static
void
check_title_encoding ()
{
  if (title_encoding == ' ') {
	title_encoding_xterm_to_be_checked = False;
	if (konsole_version > 0) {
		/* locale dependent */
		title_encoding = 'T';	/* terminal encoding */
	} else if (gnome_terminal_version > 0) {
		/* probably gnome-terminal */
		/* locale dependent */
		title_encoding = 'T';	/* terminal encoding */
	} else if (mintty_version > 136) {
		/* mintty: locale dependent since 0.3.9 */
		title_encoding = 'T';	/* terminal encoding */
	} else if (mintty_version == 136) {
		/* PuTTY: Windows Western */
		title_encoding = 'W';
	} else if (xterm_version >= 210) {
		/* title string controlled by utf8Title since xterm 210 */
		if (utf8_screen) {
			if (use_window_title_query) {
				title_encoding_xterm_to_be_checked = True;
				/* fall-back assumption */
				title_encoding = 'U';	/* UTF-8 */
			} else {
				title_encoding = 'A';	/* ASCII */
			}
		} else {
			title_encoding = 'L';	/* ISO Latin-1 */
		}
	} else if (xterm_version >= 201) {
		title_encoding = 'L';	/* ISO Latin-1 */
	} else if (xterm_version > 0 && utf8_screen) {
		title_encoding = 'A';	/* ASCII */
	} else if (rxvt_version > 0) {
#ifdef incapable_X_windows
		/* how to detect this? */
		if (utf8_screen) {
			title_encoding = 'A';	/* workaround rxvt bug? */
		} else {
			title_encoding = 'L';	/* ISO Latin-1 */
		}
#else
		title_encoding = 'T';	/* terminal encoding */
#endif
	} else if (streq (TERM, "cygwin")) {
		if (term_encoding_tag == 'W') {
			/* assume cygwin 1.5 */
# ifdef __CYGWIN__
			/* drop this assumption unless verified locally */
			if (CYGWIN_VERSION_DLL_MAJOR < 1007) {
				title_encoding = 'P';	/* PC-Latin-1 (CP850) */
			} else {
				title_encoding = 'T';	/* terminal encoding */
			}
# endif
		} else {
			/* cygwin 1.5 codepage:oem or cygwin 1.7.0-49 */
			title_encoding = 'T';	/* terminal encoding */
		}
	} else {
#ifdef msdos
		title_encoding = 'P';	/* PC-Latin-1 (CP850) */
#endif
	}
	if (title_encoding == ' ') {
		if (utf8_screen) {
			title_encoding = 'U';	/* UTF-8 */
		} else {
			title_encoding = 'L';	/* ISO Latin-1 */
		}
	}
  }

  if (title_encoding_xterm_to_be_checked) {	/* => xterm_version >= 210 */
	/* first check if we can skip the check (ASCII-only file name) */
	char * filename_poi = file_name;
	FLAG check_xterm = False;
	while (* filename_poi != '\0') {
		if (* filename_poi ++ & 0x80) {
			check_xterm = True;
			break;
		}
	}

	if (check_xterm) {
	    if (xterm_version >= 252 && utf8_screen) {
		/* assume that it's reasonable to just set utf8Title mode */
#ifdef debug_window_title
		printf ("setting xterm window title encoding to UTF-8");
		putescape ("\033[>2t");
		title_encoding = 'U';	/* UTF-8 */
#endif
	    } else {
#ifdef debug_window_title
		printf ("checking xterm window title encoding");
#endif
		/* title string controlled by utf8Title since xterm 210 */
		char * r = get_terminal_report_string ("\033]2;x�x\033[21t");
		title_encoding_xterm_to_be_checked = False;
		if (! r || ! r [0]) {
			/* no response, resource 'allowWindowOps' disabled
			   or non-xterm */
			use_window_title_query = False;
			title_encoding = 'A';	/* ASCII */
		} else if (r [2] == '�') {
			/* only with utf8Title: false */
			title_encoding = 'L';	/* ISO Latin-1 */
		} else {
			/* only with utf8Title: true */
			title_encoding = 'U';	/* UTF-8 */
		}
#ifdef debug_window_title
		printf ("window title:");
		while (* r) {
			printf (" %02X%c", (character) * r, * r);
			r ++;
		}
		printf ("\n");
#endif
	    }
	}
  }
}

void
RD_window_title ()
{
  char window_string [2 * (maxFILENAMElen + maxXMAX) + 18];
  char filename_ok [maxFILENAMElen];
  char * filename_dispoi;
  char * filename_poi;
  char * save_term_encoding = NIL_PTR;

  if (! use_window_title) {
	return;
  }

  if (! saved_old_window_title) {
	save_old_window_title ();
  }

  check_title_encoding ();

  if (title_encoding == 'P' || title_encoding == 'W') {
	save_term_encoding = get_term_encoding ();
	if (title_encoding == 'P') {
		(void) set_term_encoding ("CP850", 'P');
	} else {
		(void) set_term_encoding ("CP1252", 'W');
	}
  }

  filename_poi = file_name;
  filename_dispoi = filename_ok;
  while (* filename_poi != '\0') {
	unsigned long c = unicodevalue (filename_poi);
	if (no_unichar (c) || c < (character) ' ' || (c >= 0x80 && c < 0xA0)) {
		* filename_dispoi ++ = '?';
	} else if (title_encoding == 'L') {
		if (c >= 0x100) {
			* filename_dispoi ++ = '?';
		} else {
			* filename_dispoi ++ = (character) c;
		}
	} else if (title_encoding == 'A' && c >= 0x80) {
		* filename_dispoi ++ = '?';
	} else if (title_encoding == 'U') {
#ifdef incapable_X_windows
		/* how to detect this? */
		if (c >= 0x100) {
			/* only a few selected non-Latin-1 chars are displayed */
			* filename_dispoi ++ = '?';
		} else {
			filename_dispoi += utfencode (c, filename_dispoi);
		}
#else
		filename_dispoi += utfencode (c, filename_dispoi);
#endif
	} else { /* assume title encoding is same as screen encoding */
		if (utf8_screen) {
			filename_dispoi += utfencode (c, filename_dispoi);
		} else if (cjk_term || mapped_term) {
			unsigned long tc = mappedtermchar (c);
			if (no_char (tc)) {
				* filename_dispoi ++ = '?';
			} else if (cjk_term) {
				filename_dispoi += cjkencode_char (True, mappedtermchar (c), filename_dispoi);
			} else {
				* filename_dispoi ++ = tc;
			}
		} else { /* also fall-back for ASCII chars in all cases */
			* filename_dispoi ++ = (character) c;
		}
	}
	advance_char (& filename_poi);
  }
  * filename_dispoi = '\0';

  if (save_term_encoding != NIL_PTR) {
	(void) set_term_encoding (save_term_encoding, ' ');
  }

#ifdef __CYGWIN__
#define mined_name " - MinEd"
#else
#define mined_name ""
#endif

  if (loading == False) {
	char title_text [maxFILENAMElen + maxXMAX];
	char icon_text [maxFILENAMElen + maxXMAX];
	build_string (title_text, "%s%s%s",
			file_name [0] == '\0' ? "[no file]" : filename_ok,
			modified ? mined_modf: "",
			mined_name);
	build_string (icon_text, "%s%s",
			modified ? mined_modf: "",
			file_name [0] == '\0' ? "[no file]" : getbasename (filename_ok));
	build_window_title (window_string, title_text, icon_text);
	putescape (window_string);
  }
}

#endif


/*
 * Redraw the screen
 */
static
void
RD_nobot ()
{
  reverse_off ();
  clearscreen ();

#ifdef use_logo
  if (xterm_version > 0 || mintty_version > 0 || decterm_version > 0) {
	/* double line logo - would waste 2 lines */
	/* show accent conditionally, see splash_logo () */
	set_cursor (XMAX - 10, 1);
	putescape ("\033#3ḿ   MinEd");
	set_cursor (XMAX - 10, 2);
	putescape ("\033#4ḿ   MinEd");
  }
#endif

/* display page */
  display (0, top_line, last_y, y);

/* redraw scroll bar */
  if (disp_scrollbar && ! winchg) {
	(void) display_scrollbar (False);
  }

/* clear/redraw last line */
  set_cursor (0, YMAX);
  clear_lastline ();
  move_address (cur_text, find_y_w_o_RD (cur_line));

#ifdef unix
  RD_window_title ();
#endif
}

void
RD ()
{
  RD_nobot ();
  if (stat_visible) {
	rd_bottom_line ();
  }
}

void
RD_y (y_pos)
  int y_pos;
{
  reverse_off ();
  clearscreen ();

/* display page */
  display (0, top_line, last_y, y_pos);

/* clear/redraw last line */
  set_cursor (0, YMAX);
  clear_lastline ();
  if (stat_visible) {
	rd_bottom_line ();
  }
}

/*
 * Adjust current window size after WINCH signal
 */
static
void
RDwin_menu (rd_menu, after_winchg)
  FLAG rd_menu;
  FLAG after_winchg;
{
  screen_buffer (True);

#ifdef debug_terminal_resize
  printf ("RDwin %dx%d", YMAX + 1 + MENU, XMAX + 1);
#endif
  winchg = False;
  checkwinsize ();
#ifdef debug_terminal_resize
  printf (" -> %dx%d\n", YMAX + 1 + MENU, XMAX + 1);
#endif

#ifdef adjust_to_actual_termsize	/* incomplete & obsolete */
  flush_keyboard ();
  putescape ("\033[18t");	/* long timeout if response disabled? */
  flush ();
  if (char_ready_within (...)) { /*  c = expect1byte (True, NIL_PTR); if (receiving_response (c, NIL_PTR)) { */
	c = readcharacter ();...
	if (command (c) == ANSIseq) {
		ANSIseq ();
		... but without RD which is actually done here ...
		in both cases, reset the variables below, however ...
	}
  }
#endif

  if (loading == False) {
	LINE * current_line = cur_line;
	reset (top_line, y);
	/* move_y (find_y_w_o_RD (current_line)); */
	move_address (cur_text, find_y_w_o_RD (current_line));

	RD_nobot ();
	if (MENU && ! winchg) {
		displaymenuline (True);
		if (rd_menu) {
			redrawmenu ();
		}
	}
  }

  display_flush ();

  if (after_winchg && ! winchg) {
	(void) display_scrollbar (True);
  }

  set_cursor_xy ();

  if (stat_visible && ! winchg) {
	rd_bottom_line ();
	/* could call redraw_prompt () instead, but:
	   - menu header missing
	   - with empty text: line marker not restored?
	*/
  }

  if (winchg) {
	trace_checkwinsize (("recursive RDwin_menu after winchg\n"));
	RDwin_menu (rd_menu, after_winchg);
  }

  flush ();
}

void
RDwin ()
{
  RDwin_menu (True, False);
}

void
RDcenter ()
{
  LINE * l = cur_line;
  int targy = YMAX / 2 - 1;
  int li = 0;
  while (l->prev != header && li < targy) {
	l = l->prev;
	li ++;
  }
  reset (l, li);

  RDwin ();
}

void
RDwinchg ()
{
  trace_checkwinsize (("RDwinchg\n"));
  if (loading) {
#ifdef debug_terminal_resize
	printf ("RDwin (skip/loading) %dx%d\n", YMAX + 1 + MENU, XMAX + 1);
#endif
	/* cannot use text buffer for refresh */
	return;
  }
  RDwin_menu (True, True);
}

void
RDwin_nomenu ()
{
  RDwin_menu (False, False);
}


/*======================================================================*\
|*		Terminal mode debugging					*|
\*======================================================================*/

#define dont_debug_encoding


#define dont_debug_test_screen_width

#define dont_debug_screenmode
#define dont_debug_width_data_version

#define dont_debug_graphics


#ifdef debug_test_screen_width
# define debug_encoding
#endif


#ifdef debug_encoding

static
FLAG
do_set_term_encoding (function, line, charmap, tag)
  char * function;
  unsigned int line;
  char * charmap;
  char tag;
{
  FLAG ret = set_term_encoding (charmap, tag);
  printf ("set_term_encoding [%s:%d] %s '%c' -> %d %s\n", function, line, charmap, tag, ret, get_term_encoding ());
  return ret;
}

#define set_term_encoding(charmap, tag)	do_set_term_encoding (__FUNCTION__, __LINE__, charmap, tag)

#define trace_encoding(tag)	\
 printf ("[%s]  	TERM <%s> '%c' utf8 %d cjk %d map %d comb/bidi %d/%d\n		TEXT <%s> '%c' (auto %d) utf8 %d utf16 %d cjk %d map %d\n", \
 tag, \
 get_term_encoding (), term_encoding_tag, utf8_screen, cjk_term, mapped_term, combining_screen, bidi_screen, \
 get_text_encoding (), text_encoding_tag, auto_detect, utf8_text, utf16_file, cjk_text, mapped_text)

#else

#define trace_encoding(tag)	

#endif


#ifdef debug_width_data_version

#define trace_width_data_version(tag)	\
 printf ("[%s] width_data_version %d (CJK %d) nonbmp %X combining_data_version %d (Jamo %d)\n", \
 tag, \
 width_data_version, cjk_width_data_version, nonbmp_width_data, \
 combining_data_version, hangul_jamo_extended)

#else

#define trace_width_data_version(tag)	

#endif


/*======================================================================*\
|*		TTY setup						*|
\*======================================================================*/

/* workaround for cygwin console delaying some escape sequences */
#ifdef __CYGWIN__
static
void
install_console_pipe ()
{
  if ((streq ("cygwin", TERM) && ! screen_version && ! tmux_version)) {
	struct utsname uts;
	int pfds [2];
	int pid;
	int cygwin_version_major = CYGWIN_VERSION_DLL_MAJOR;
	int cygwin_version_minor = CYGWIN_VERSION_DLL_MINOR;

	if (uname (& uts) == 0) {
		int mil, maj, min;
		int ret = sscanf (uts.release, "%d.%d.%d", & mil, & maj, & min);
		if (ret == 3) {
			cygwin_version_major = mil * 1000 + maj;
			cygwin_version_minor = min;
		}
	}

	if (cygwin_version_major < 1007 || cygwin_version_minor < 10) {
		/* no bug yet */
	} else if (pipe (pfds) < 0) {
		/* don't fork, accept deficiencies */
	} else {
		pid = fork ();
		if (pid < 0) {	/* fork error */
			/* ignore, accept deficiencies */
		} else if (pid == 0) {	/* child */
			int n;
			(void) close (pfds [0]);
			sleep (1);
			/* copy input_fd to pipe */
			do {
				character c;
				n = read (input_fd, & c, 1);
				if (n > 0) {
					n = write (pfds [1], & c, 1);
				}
			} while (n >= 0);
			_exit (127);
		} else {	/* parent */
			(void) close (pfds [1]);
			/* redirect input from pipe */
			input_fd = pfds [0];
			/* proceed with editing */
		}
	}
  }
}
#else
#define install_console_pipe()	
#endif


/*======================================================================*\
|*		Terminal setup						*|
\*======================================================================*/

/**
   Request the terminal (VT100-like, xterm and derivatives) to send 
   Device Attributes report containing terminal type and version.
 */
static
void
acquire_device_attributes (again)
  FLAG again;
{
#ifndef msdos
  character c;

  trace_query (("query ^[[>c\n"));

  terminal_type = -1;
  terminal_version = 0;

  flush_keyboard ();
  putescape ("\033[>c");
  flush ();

  adjust_escdelay ();

  c = expect1byte (True, "acquire");	/* ESC */
  if (receiving_response (c, "acquire")) {
	c = expect1byte (False, "acquire.");	/* '[' */
	c = expect1byte (False, "acquire..");
		/* '>' (secondary DA) or '?' (CJK terminals) */
	c = get_digits (& terminal_type);
	if (c == ';') {
		c = get_digits (& terminal_version);
	}
	while (c == '.') {
		int dummy;
		/* mrxvt sends sub-version 0.4.1 */
		c = get_digits (& dummy);
		terminal_version = terminal_version * 100 + dummy;
	}
	while (c == ';') {
		int dummy;
		c = get_digits (& dummy);
	}
  }

  trace_query (("-> %d %d\n", terminal_type, terminal_version));
#endif
}

static
void
acquire_primary_device_attributes ()
{
#ifndef msdos
  character c;

  trace_query (("query ^[[c\n"));

  flush_keyboard ();
  putescape ("\033[c");
  flush ();

  adjust_escdelay ();

  c = expect1byte (True, "acquirep");	/* ESC */
  if (receiving_response (c, "acquirep")) {
	c = expect1byte (False, "acquirep.");	/* '[' */
	c = expect1byte (False, "acquirep..");	/* '?' */
	dec_features = 0;
	do {
		int feature;
		c = get_digits (& feature);
		trace_query (("terminal feature %d\n", feature));
		dec_features |= 1 << feature;
	} while (c == ';');
	trace_query (("terminal features %08lX\n", dec_features));
  }
#endif
}


/**
   Swallow keyboard input.
   For use before enquiring all kinds of terminal reports.
 */
static
void
flush_keyboard ()
{
  /* swallow either
	- key typed in by user while requesting terminal
	- delayed response to a previous request 
	  which could not be handled in time
  */
#ifndef vms_slow_lookahead
  while (char_ready_within (30, "swallow")) {
#ifdef debug_expect
	unsigned long swallowed = _readchar_nokeymap ();
	printf ("swallowed %04lX\n", swallowed);
	(void) swallowed;
#else
	(void) _readchar_nokeymap ();
#endif
  }
#endif
}

/**
   Retrieve terminal response to various enquiry escape sequences.
   Used for:
	color report
	window title report (not with newer xterm/mintty)
	glyph availability report (with newer mintty)
 */
static
char *
get_terminal_report_string (s)
  char * s;
{
#ifndef msdos
  character c;
  static char sbuf [maxMSGlen];
  char * spoi = sbuf;

  trace_query (("query [%d] ^[%s\n", escape_delay, s + 1));

  flush_keyboard ();
  putescape (s);
  flush ();

  /* UNSURE: enforce shorter timeout to mitigate delay 
     if xterm resource allowWindowOps is false
   */
  c = expect1byte (UNSURE, "report");	/* ESC */
  if (receiving_response (c, s + 1)) {
	c = expect1byte (False, "report.");	/* ']' */
	while ((c = expect1byte (False, "report..")) != '\033' && c != '\007' && c != 0x9C) {
		if (spoi < & sbuf [maxMSGlen - 1]) {
			* spoi ++ = c;
		}
	}
	if (c == '\033' && rxvt_version < 300) {
		/* rxvt-unicode fails to send the final \ */
		c = expect1byte (False, "report...");	/* '\\' */
	}
  }
  * spoi = '\0';

  if (debug_mined) {
	debuglog ("report", s + 1, sbuf);
  }
  trace_query (("-> ^[%s\n", sbuf));
  return sbuf;
#else
  return "";
#endif
}

static
void
get_width_report (s, wpoi)
  char * s;
  int * wpoi;
{
  int row, col;
  if (get_CPR (& row, & col)) {
#ifdef debug_test_screen_width
	printf ("width <%s> -> %d\n", s, col - 1);
#endif
	* wpoi = col - 1;
  }
}

static
int
test_screen_width (s)
  char * s;
{
  int col = -1;

#ifndef msdos

  if (! ansi_esc) {
	return -1;
  }

  /* if (xterm_version >= 201) {
	suppress visible effect by setting invisible character mode
	- this would need intensive regression testing, so leave it
     }
  */
  putescape ("\r");
  flush_keyboard ();
  debuglog ("6n", "", screen_version ? "s" : "");
  putstring (s);	/* don't use putescape here for 'screen' 4.0 */
  putescape ("\033[6n");	/* maybe termcap u7 but not really defined */
  putescape ("\r");
  clear_eol ();	/* reduce visible effect */
  flush ();
  debuglog ("6n", "", "flush");

  get_width_report (s, & col);
  debuglog ("CPR", "", "");
#endif

  return col;
}

typedef struct {
	char * test;
	int width;
	} screen_width;

static screen_width utf8_widths [] = {
	/* detecting combining mode: */
	{"a̡"},

	/* detecting width data version: */
	{"《》〚〛｟｠"},
	{"︐"},
	{"ꥠ"},
	{"𛀁𓐀"},	/* non-BMP characters cannot reliably be used for detection */

	/* detecting combining_data_version: */
	{"a܏"},
	{"aͣ"},
	{".͐.឴.᠎"},
	{".͘.͙"},
	{".᷄.᷅"},
	{".҇.᷌"},	/* U510 */
	{".ࠖ.ऀ"},	/* U520 */
	{".ٟ.ऺ"},	/* U600 */
	{".؄.ꙴ"},	/* U620 */
	{".؜.؜"},	/* U630 */
	{".᪰.ᷮ"},	/* U700 */
	{"ᄀힰᄀퟋ"},	/* Hangul Jamo Extended-B */

	/* detecting xterm -cjk_width: */
	{"‘’“”…―­"},
#ifdef single_width_check
	{"…―…"},
#endif

	/* detecting non-BMP width properties: */
	{"𠀁𠀁𠀁𠀁a𝆪a𝆪a󠀠"},
	{"𐌀𐌰𐐀"},
	/* detecting unassigned character width properties: */
	{"㄀ㄯ㄰㆏꒎꓏﫿﹯＀"},

	/* detecting Yijing Hexagram widths and printable bidi markers: */
	{"䷀䷀‎"},
#ifdef poderosa_detect_string
	{"¡×"},
#endif

#ifdef oldmlterm_wideboldborderbug_workaround
	{"╭"},
#endif
};

static screen_width cjk_widths [] = {
	{"�0�2"},
	{"����ꥦ�ޡ"},
	{"����"},
	{"���x"},
	{"��"},
	{"��"},
};

static
int
get_screen_width (s, sw, len)
  char * s;
  screen_width * sw;
  int len;
{
  int i;
  for (i = 0; i < len; i ++) {
	/* if screen width has been acquired for this test string, return */
	if (streq (s, sw [i].test) && sw [i].width) {
#ifdef debug_test_screen_width
		printf ("get <%s> -> %d\n", s, sw [i].width);
#endif
		return sw [i].width;
	}
  }
  return test_screen_width (s);
}

static
void
acquire_screen_widths (sw, len)
  screen_width * sw;
  int len;
{
#ifdef vms
  /* VMS cannot buffer more than 80 characters, 
     so it would hang on trying to read the responses...
     Even the response read-ahead trick below doesn't seem to work.
   */
#else
#ifndef msdos
  int i;
  int k = 0;

  /*
    if (xterm_version >= 201) {
	suppress visible effect by setting invisible character mode
	(invisible mode double width bug fixed in xterm 201)
	- this would need intensive regression testing, so leave it
    }
  */
#ifdef debug_test_screen_width
  printf ("acquire_screen_widths\n");
#endif
  debuglog ("6n", "**", screen_version ? "s" : "");
  flush_keyboard ();
  putescape ("\r");
  for (i = 0; i < len; i ++) {
	/*debuglog ("6n", "*", screen_version ? "s" : "");*/
	putstring (sw [i].test);	/* don't use putescape here for 'screen' 4.0 */
	putescape ("\033[6n");	/* maybe termcap u7 but not really defined */
	putescape ("\r");
#ifdef vms
	clear_eol ();
	flush ();
	while (k <= i && char_ready_within (35, NIL_PTR)) {
		get_width_report (sw [k].test, & sw [k].width);
		/*debuglog ("CPR", "!", "");*/
		k ++;
	}
#endif
  }
#ifndef vms
  clear_eol ();	/* reduce visible effect */
  flush ();
#endif
  debuglog ("6n", "**", "flush");

  for (; k < len; k ++) {
	get_width_report (sw [k].test, & sw [k].width);
	/*debuglog ("CPR", "*", "");*/
  }
  debuglog ("CPR", "**", "");
#endif
#endif
}

static
void
check_cjk_width ()
{
  static FLAG init = True;

  cjk_width_data_version = 0;

  if (utf8_screen) {
	if (width_data_version >= U320) {
		int swidth;
		if (init) {
			swidth = get_screen_width ("‘’“”…―­", utf8_widths, arrlen (utf8_widths));
			init = False;
		} else {
			swidth = test_screen_width ("‘’“”…―­");
		}
		if (swidth > 8) {
			/* xterm -cjk_width (since xterm 168) */
#ifdef consider_wide_block_slices
			/* not useful since block chars used for 
			   fine scrollbar have ambiguous width
			 */
			if (! explicit_scrollbar_style) {
				fine_scrollbar = False;
			}
#else
			fine_scrollbar = False;
#endif
			if (! explicit_border_style) {
				use_stylish_menu_selection = False;
			}
			if (swidth & 1) {
				/* soft hyphen is narrow, Unicode 4.0 */
				cjk_width_data_version = U400;
				/* ?
				if (width_data_version < U400) {
					width_data_version = U400;
				}
				*/
			} else {
				/* soft hyphen is wide (ambiguous), Unicode 3.0/3.2 */
				cjk_width_data_version = U320beta;
			}
			trace_width_data_version ("cjk");
		}
#ifdef single_width_check
		swidth = get_screen_width ("…―…", utf8_widths, arrlen (utf8_widths));
		if ((swidth & 1) == 0) {
			/* ― is wide */
		}
		if (swidth >= 5) {
			/* … is wide */
		}
#endif
	}
  }
}

static
int
isglyph_code (glyph)
  char * glyph;
{
  char * match;
  if (! glyphs || ! * glyphs) {
	return 0;	/* no info -> reject */
  }
  match = strcontains (glyphs, glyph);
  if (match) {
	char * post = match + strlen (glyph);
	if (* (match - 1) == ';'
	    && (* post < '0' || * post > '9')) {
		return 1;
	}
  }
  return 0;
}

int
isglyph_uni (u)
  unsigned long u;
{
  if (u == 0) {
	/* detect whether glyph info available */
	return glyphs != NIL_PTR;
  }

  if (u < 255) {
	return 1;
  } else {
	char glyphcode [20];
	build_string (glyphcode, "%ld", u);
	return isglyph_code (glyphcode);
  }
}

static
int
isglyph_utf (c)
  char * c;
{
  if (! c || ! * c) {
	return 0;
  } else {
	return isglyph_uni (utf8value (c));
  }
}

/**
   Request terminal type and version from terminal.
 */
static
void
detect_terminal_type ()
{
  trace_query (("detect_terminal_type @ %s\n", TERM));
  if ((strisprefix ("xterm", TERM)
	|| strisprefix ("rxvt", TERM)
	|| strisprefix ("gnome", TERM)
	|| strisprefix ("konsole", TERM)
	|| strisprefix ("screen", TERM)
	|| (strisprefix ("vt", TERM)
	    && ! streq ("vt52", TERM)
	    && ! strisprefix ("vt50", TERM)
	   )
      )
#ifdef indulge_mlterm
	/* mlterm does not respond to secondary device attribute request;
	   to avoid the timeout, we could suppress the request but only 
	   if we detected mlterm by its joining property first;
	   also, be sure about possible other joining terminals...
	   (note: mintty is only "half-joining" - it joins the glyphs 
	   but not the two character cells, and leaves a dummy space)
	   - OBSOLETE in newer mlterm, 3.1.2 at least
	 */
	&& ! joining_screen
#endif
     ) {
	acquire_device_attributes (False);
	if (terminal_type == 'S') {	/* screen */
		screen_version = terminal_version / 100;
	} else if (strisprefix ("screen", TERM)) {
		screen_version = 1;
	}
	if (screen_version > 0) {
#ifdef __CYGWIN__
	    if (strcontains (envstr ("STY"), ".cons")) {
		/* suppress host terminal attribute enquiry 
		   (pass-through) in screen in cygwin console; 
		   workaround for not capturing the response then 
		   (which would later be inserted as garbage)
		 */
		terminal_type = 'C';
		terminal_version = CYGWIN_VERSION_DLL_MAJOR * 100 + CYGWIN_VERSION_DLL_MINOR;
	    } else {
		acquire_device_attributes (True);
	    }
#else
		/* check host terminal;
		   with screen_version set, terminal requests will 
		   apply the pass-through escape sequence of 'screen'
		   (for host terminal detection here 
		   and width auto-detection below)
		 */
		acquire_device_attributes (True);
#endif
	}

#ifdef test_DEC_features
	if (strisprefix ("vt", TERM)) {
		decterm_version = terminal_version;
	}
#endif
	if (terminal_type == 0 && terminal_version == 95 && getenv ("TMUX")) {
		tmux_version = 1;
	} else if (terminal_type == 'T') {	/* tmux 2.0 */
		tmux_version = 2;
	} else if (terminal_type == 0 && terminal_version == 115) {
		konsole_version = terminal_version;
		set_fkeymap ("xterm");
	} else if ((terminal_type | 1) == 1 && konsole_version <= 0) {
		if (terminal_version >= 1115) {
			/* probably gnome-terminal */
			gnome_terminal_version = terminal_version;
		} else if (terminal_version == 136) {
			/* MinTTY 0.3, PuTTY */
			mintty_version = terminal_version;
		} else if (terminal_version == 115) {
			/* KDE konsole */
			konsole_version = terminal_version;
		} else if (terminal_version == 10) {	/* VT220 */
			decterm_version = 220;
		} else if (terminal_version == 2) {	/* Openwin xterm */
			xterm_version = terminal_version;
		} else if (terminal_version <= 20) {	/* ? */
			decterm_version = terminal_version;
#ifdef test_DEC_locator_on_xterm
		} else if (strisprefix ("vt", TERM)) {
			decterm_version = terminal_version;
#endif
		} else {
			xterm_version = terminal_version;
		}
		set_fkeymap ("xterm");
	} else if (terminal_type == 'M') {	/* mintty */
		mintty_version = terminal_version;
		set_fkeymap ("xterm");
	} else if (terminal_type == 'R') {	/* old rxvt */
		rxvt_version = terminal_version / 100;
		TERM = "rxvt";
	} else if (terminal_type == 'U') {	/* rxvt-unicode */
		rxvt_version = terminal_version * 10;
		TERM = "rxvt";
	} else if (terminal_type == 6) {	/* Haiku Terminal */
		fine_scrollbar = False;
		use_stylish_menu_selection = False;
	} else if (terminal_type == 2) {	/* VT240 */
		decterm_version = 240;
	} else if (terminal_type == 18) {	/* VT330 */
		decterm_version = 330;
	} else if (terminal_type == 19) {	/* VT340 */
		decterm_version = 340;
	} else if (terminal_type == 24) {	/* VT320 */
		decterm_version = 320;
	} else if (terminal_type == 28) {	/* DECterm window */
		decterm_version = 1;
	} else if (terminal_type == 41) {	/* VT420 */
		decterm_version = 420;
	} else if (terminal_type == 61) {	/* VT510 */
		decterm_version = 510;
	} else if (terminal_type == 64) {	/* VT520 */
		decterm_version = 520;
	} else if (terminal_type == 65) {	/* VT525 */
		decterm_version = 525;
	} else if (terminal_type < 64) {	/* ? */
		decterm_version = terminal_type;
	}
	if (decterm_version > 1 && terminal_version >= 280) {
		xterm_version = terminal_version;
	}
  }

/* try to detect KDE konsole */
  if (! konsole_version &&
	(strisprefix ("konsole", TERM)
#ifdef heuristic_konsole_detection
	|| envvar ("KONSOLE_DCOP") || envvar ("KONSOLE_DBUS_SESSION")
#endif
	)
     ) {
	konsole_version = 1;
	/*xterm_version = 0;*/
  }

/* check whether xterm supports SIXEL graphics */
  if (xterm_version >= 298) {
	acquire_primary_device_attributes ();
  }

/* try to detect gnome-terminal */
  if (strisprefix ("gnome-terminal", envstr ("COLORTERM")) && gnome_terminal_version <= 0) {
	gnome_terminal_version = 1;
  }

/* try to identify mlterm */
  if (bidi_screen && ! mintty_version) { /* probably mlterm */
	mlterm_version = 1;
  } else if (strisprefix ("mlterm", TERM)) {
	mlterm_version = 1;
  } else if (streq ("xterm", TERM) && envvar ("MLTERM")) {
	mlterm_version = 1;
  }
  if (mlterm_version) {
	char * mlterm= envvar ("MLTERM");
	int v1, v2, v3;
	int ret = sscanf (mlterm, "%d.%d.%d", & v1, & v2, & v3);
	if (ret == 3) {
		mlterm_version = 100 * v1 + 10 * v2 + v3;
	}
  }
}

static
void
check_glyphs ()
{
  if (mintty_version >= 909) {
#ifdef vms
	/* real VMS limits the glyph detection response 
	   by its 80 characters input buffer,
	   so check only the most essential markers:
	   ╭9581;▁9601;▶9654;►9658;⏎9166;‣8227;⇾8702;→8594
	 */
	glyphs = get_terminal_report_string ("\033]7771;?;9581;9601;9654;9658;9166;8227;8702;8594");
#else
	glyphs = get_terminal_report_string ("\033]7771;?;9472;9581;9484;9601;9613;9654;9658;10003;8231;8702;8594;8227;9166;8629;8626;10007;8623;8226;9755;9758;8228;8229;8230;9655;9656;9657;9659;9664;9665;9666;9667;9668;9669;9670;9830;9672;11032;11033;11030;11031;9478;9479;8364;699;769;8472");
#endif
	if (glyphs) {
		glyphs = strchr (glyphs, '!');
		if (glyphs) {
			glyphs = dupstr (glyphs);
			/* update ḿ */
			/* used to call splash_logo () here */
		}
	}
  }
}


#define use_splash_logo
#define use_sixel_splash

#ifdef use_sixel_splash
static char * sixelsplash = "P0;0;8q\"1;1\
#0;2;0;0;0#1;2;0;18;0#2;2;0;32;0#3;2;0;45;0#4;2;0;57;0#5;2;0;69;0#6;2;0;80;0\
#0!105@$\
#0!105A$\
#0!105C$\
#0!105G$\
#0!105O$\
#0!105_$\
-\
#0!105@$\
#0!105A$\
#0!105C$\
#0!105G$\
#0!105O$\
#0!105_$\
-\
#0!105@$\
#0!105A$\
#0!105C$\
#0!105G$\
#0!105O$\
#0!105_$\
-\
#0!59@#2@#6!9@#0!36@$\
#0!58A#5A#6!11A#0!35A$\
#0!57C#3C#6!11C#3C#0!35C$\
#0!57G#6!11G#4G#0!36G$\
#0!56O#4O#6!10O#5O#0!37O$\
#0!55_#2_#6!11_#0!38_$\
-\
#0!55@#6!11@#1@#0!38@$\
#0!54A#3A#6!10A#2A#0!39A$\
#0!53C#1C#6!10C#3C#0!40C$\
#0!53G#5G#6!9G#4G#0!41G$\
#0!52O#3O#6!9O#5O#0!42O$\
#0!52_#6!10_#0!43_$\
-\
#0!51@#4@#6!9@#1@#0!43@$\
#0!50A#1A#6!9A#2A#0!44A$\
#0!51C#5C#6!7C#0!46C$\
#0!105G$\
#0!105O$\
#0!105_$\
-\
#0!105@$\
#0!105A$\
#0!40C#4C#6!7C#3C#0!18C#2C#6!8C#0!29C$\
#0!20G#1G#6!8G#1G#0!7G#2G#6!13G#2G#0!13G#6!13G#4G#0!26G$\
#0!20O#6!10O#0!5O#1O#6!17O#0!10O#5O#6!16O#2O#0!24O$\
#0!20_#6!10_#0!4_#3_#6!19_#1_#0!6_#1_#6!19_#4_#0!23_$\
-\
#0!20@#6!10@#0!3@#5@#6!21@#1@#0!4@#3@#6!21@#3@#0!22@$\
#0!20A#6!10A#0!2A#5A#6!22A#5A#0!3A#4A#6!23A#2A#0!21A$\
#0!20C#6!10C#0C#6!25C#3C#0C#4C#6!25C#0!21C$\
#0!20G#6!10G#5G#6!26G#4G#6!26G#3G#0!20G$\
#0!20O#6!64O#5O#0!20O$\
#0!20_#6!65_#2_#0!19_$\
-\
#0!20@#6!18@#2@#0!4@#4@#6!21@#4@#0!4@#3@#6!14@#3@#0!19@$\
#0!20A#6!16A#3A#0!7A#1A#6!18A#5A#0!8A#6!13A#5A#0!19A$\
#0!20C#6!15C#2C#0!9C#2C#6!16C#4C#0!10C#6!13C#0!19C$\
#0!20G#6!14G#2G#0!11G#5G#6!14G#4G#0!11G#3G#6!12G#1G#0!18G$\
#0!20O#6!13O#2O#0!12O#3O#6!13O#4O#0!12O#1O#6!12O#1O#0!18O$\
#0!20_#6!12_#3_#0!13_#1_#6!12_#5_#0!14_#5_#6!11_#2_#0!18_$\
-\
#0!20@#6!11@#4@#0!15@#6!12@#0!15@#4@#6!11@#2@#0!18@$\
#0!20A#6!11A#4A#0!15A#5A#6!11A#0!15A#3A#6!11A#2A#0!18A$\
#0!20C#6!11C#4C#0!15C#5C#6!11C#0!15C#2C#6!11C#2C#0!18C$\
#0!20G#6!11G#4G#0!15G#4G#6!11G#0!15G#2G#6!11G#2G#0!18G$\
#0!20O#6!11O#4O#0!15O#4O#6!11O#0!15O#2O#6!11O#2O#0!18O$\
#0!20_#6!11_#4_#0!15_#4_#6!11_#0!15_#2_#6!11_#2_#0!18_$\
-\
#0!20@#6!11@#4@#0!15@#4@#6!11@#0!15@#2@#6!11@#2@#0!18@$\
#0!20A#6!11A#4A#0!15A#4A#6!11A#0!15A#2A#6!11A#2A#0!18A$\
#0!20C#6!11C#4C#0!15C#4C#6!11C#0!15C#2C#6!11C#2C#0!18C$\
#0!20G#6!11G#4G#0!15G#4G#6!11G#0!15G#2G#6!11G#2G#0!18G$\
#0!20O#6!11O#4O#0!15O#4O#6!11O#0!15O#2O#6!11O#2O#0!18O$\
#0!20_#6!11_#4_#0!15_#4_#6!11_#0!15_#2_#6!11_#2_#0!18_$\
-\
#0!20@#6!11@#4@#0!15@#4@#6!11@#0!15@#2@#6!11@#2@#0!18@$\
#0!20A#6!11A#4A#0!15A#4A#6!11A#0!15A#2A#6!11A#2A#0!18A$\
#0!20C#6!11C#4C#0!15C#4C#6!11C#0!15C#2C#6!11C#2C#0!18C$\
#0!20G#6!11G#4G#0!15G#4G#6!11G#0!15G#2G#6!11G#2G#0!18G$\
#0!20O#6!11O#4O#0!15O#4O#6!11O#0!15O#2O#6!11O#2O#0!18O$\
#0!20_#6!11_#4_#0!15_#4_#6!11_#0!15_#2_#6!11_#2_#0!18_$\
-\
#0!20@#6!11@#4@#0!15@#4@#6!11@#0!15@#2@#6!11@#2@#0!18@$\
#0!20A#6!11A#4A#0!15A#4A#6!11A#0!15A#2A#6!11A#2A#0!18A$\
#0!20C#6!11C#4C#0!15C#4C#6!11C#0!15C#2C#6!11C#2C#0!18C$\
#0!20G#6!11G#4G#0!15G#4G#6!11G#0!15G#2G#6!11G#2G#0!18G$\
#0!20O#6!11O#4O#0!15O#4O#6!11O#0!15O#2O#6!11O#2O#0!18O$\
#0!20_#6!11_#4_#0!15_#4_#6!11_#0!15_#2_#6!11_#2_#0!18_$\
-\
#0!20@#6!11@#4@#0!15@#4@#6!11@#0!15@#2@#6!11@#2@#0!18@$\
#0!20A#6!11A#4A#0!15A#4A#6!11A#0!15A#2A#6!11A#2A#0!18A$\
#0!20C#6!11C#4C#0!15C#4C#6!11C#0!15C#2C#6!11C#2C#0!18C$\
#0!20G#6!11G#4G#0!15G#4G#6!11G#0!15G#2G#6!11G#2G#0!18G$\
#0!20O#6!11O#4O#0!15O#4O#6!11O#0!15O#2O#6!11O#2O#0!18O$\
#0!21_#6!9_#3_#0!17_#3_#6!8_#5_#0!17_#2_#6!9_#1_#0!19_$\
-\
#0!105@$\
#0!105A$\
#0!105C$\
#0!105G$\
#0!105O$\
#0!105_$\
-\
#0!105@$\
#0!105A$\
#0!105C$\
#0!105G$\
#0!105O$\
#0!105_$\
-\
#0!105@$\
#0!105A$\
#0!105C$\
#0!105G$\
#0!105O$\
#0!105_$\
-\
#0!105@$\
#0!105A$\
#0!105C$\
#0!105G$\
\\";
#endif

void
splash_logo ()
{
#ifdef use_splash_logo
  static FLAG splash_init_done = False;
  int splashpos = YMAX / 3;

  if (splash_level == 0) {
	return;
  }

  clear_screen ();
  if ((xterm_version > 2 && ! mlterm_version)
      || mintty_version > 0 || decterm_version > 0) {
	/* cjk_width_data_version not yet determined @ first invocation! */
	char * logo1 = (mapped_term || cjk_term 
			|| cjk_width_data_version || ! mintty_version)
			? "`   MinEd " VERSION
			: utf8_screen
				? "´   MinEd " VERSION
				: "�   MinEd " VERSION;
	char * logo2 = "m   MinEd " VERSION;
	int xpos;

	if (utf8_screen &&
	    (combining_screen || mintty_version > 0 || xterm_version > 141)) {
		if (! screen_version && mintty_version > 0 && isglyph_code ("769")) {
			/* double-height combining works in mintty */
			logo1 = "ḿ   MinEd " VERSION;
			logo2 = logo1;
		}
	}
	xpos = XMAX / 4 - strlen ("m   MinEd " VERSION) / 2 + 1;
	if (! mlterm_version) {
		set_cursor (xpos, YMAX / 3);
		putescape ("\033#3"); putescape (logo1);
		set_cursor (xpos, YMAX / 3 + 1);
		putescape ("\033#4"); putescape (logo2);
		putstring ("\n");
		splashpos = YMAX / 3 + 3;
	}
  }
#ifdef use_sixel_splash
  if (splash_level > 1 &&
      ((xterm_version >= 298 && dec_features & (1 << 4))
       || mlterm_version >= 319 /* actually since 3.1.9 */
      )
     ) {
	set_cursor (XMAX / 2 - 5, splashpos);
	putescape (sixelsplash);
	putstring ("\n");
	flush ();

	if (splash_init_done || filelist_count () == 0) {
		/* don't sleep (1); */
	}
	splash_init_done = True;
  }
#endif
  flush ();
#endif
}

void
switchAltScreen ()
{
  status_msg ("Trying to switch to command line view (normal screen)");
  screen_buffer (False);
  flush ();
  (void) readcharacter ();
  RDwin ();
  status_msg ("Returned to editing view (alternate screen)");
}

static
void
print_terminal_info ()
{
	printf ("Terminal size %d x %d\n", YMAX + 1 + MENU, XMAX + 1);
	printf ("Terminal encoding %s\n", get_term_encoding ());
	printf ("- combining %d, bidi %d, joining %d, halfjoining %d\n", 
			combining_screen, bidi_screen, 
			joining_screen, halfjoining_screen);
	if (utf8_auto_detected) {
		printf ("- UTF-8 auto-detected\n");
	}
	printf ("- width data version %d - %s\n", 
			width_data_version, 
			term_Unicode_version_name (width_data_version));
	if (cjk_term) {
		printf ("- CJK terminal%s%s\n",
			cjk_uni_term ? " - Unicode based" : "",
			gb18030_term ? " - GB18030" : "");
	}
	if (cjk_width_data_version) {
		printf ("- CJK width data version %d - %s\n", 
			cjk_width_data_version, 
			term_Unicode_version_name (cjk_width_data_version));
	}
	if (combining_screen) {
		printf ("- combining data version %d - %s (Hangul Jamo extended %d)\n", 
			combining_data_version, 
			term_Unicode_version_name (combining_data_version), 
			hangul_jamo_extended);
	}
	printf ("- non-BMP width data mode %02X: plane_2_double %d plane_1_comb %d plane_14_comb %d\n", 
			nonbmp_width_data, plane_2_double_width, 
			plane_1_combining, plane_14_combining);

	if (terminal_type > ' ') {
		printf ("- terminal type %d ('%c') version %d\n", 
			terminal_type, terminal_type, terminal_version);
	} else if (terminal_type >= 0) {
		printf ("- terminal type %d version %d\n", 
			terminal_type, terminal_version);
	}
	if (screen_version > 0) {
		printf ("- 'screen' version %d\n", screen_version);
	}
	if (tmux_version > 0) {
		printf ("- 'tmux' version %d\n", tmux_version);
	}
	if (xterm_version > 0) {
		printf ("- 'xterm' version %d\n", xterm_version);
	}
	if (decterm_version > 0) {
		printf ("- 'DEC terminal' version %d\n", decterm_version);
	}
	if (rxvt_version > 0) {
		if (terminal_type == 'U') {
			printf ("- 'rxvt-unicode' version %d\n", rxvt_version);
		} else {
			printf ("- 'rxvt' version %d\n", rxvt_version);
		}
	}
	if (gnome_terminal_version > 0) {
		printf ("- 'gnome terminal' version %d\n", gnome_terminal_version);
	}
	if (konsole_version > 0) {
		printf ("- 'KDE konsole' version %d\n", konsole_version);
	}
	if (mintty_version > 0) {
		printf ("- 'mintty' version %d\n", mintty_version);
	}
	if (mlterm_version > 1) {
		printf ("- 'mlterm' version %d\n", mlterm_version);
	} else if (mlterm_version > 0) {
		printf ("- 'mlterm'\n");
	}
	if (colours_256) {
		printf ("- assuming 256%s color mode\n", colours_88 ? " (maybe 88)" : "");
	} else if (colours_88) {
		printf ("- assuming 88 color mode\n");
	}
	/* check #ifdef debug_screenmode for further information */

	/*printf ("- window title encoding <%c>\n", title_encoding);*/

	printf ("Menu border characters: ~ %s\n",
		use_ascii_graphics ? "ASCII" :
			use_vga_block_graphics ? "VGA" :
				use_pc_block_graphics ? "PC-compatible":
					use_vt100_block_graphics ? "VT100 block graphics":
						utf8_screen ? "Unicode":
							"VT100 block graphics"
		);

	if (dark_term) {
		printf ("- dark terminal background\n");
	}
	printf ("- dim mode: ");
	if (can_dim) {
		printf ("native\n");
#ifdef unix
	} else if (redefined_ansi7) {
		printf ("redefining ansi mode\n");
#endif
	} else {
		printf ("none\n");
	}
}

/**
   Perform terminal detection, encoding auto-detection, determine features, 
   setup terminal.
 */
static
void
terminal_configure_init ()
{
  int swidth;

/* terminal mode initialisation */
  /* Note: this must be called before any screen interaction, 
	e.g. test_screen_width, acquire_screen_widths, get_screen_width
     but after pipe handling (I/O redirection)
   */
  get_term (TERM);

  /* Set tty to appropriate mode; don't rely on checkwinsize() yet! */
  raw_mode (True);
  if (erase_char != '\0') {
	key_map [erase_char] = DPC;
  }
#ifdef vms
  /* detection of terminal setting fails, so in case DEL is on Backarrow: */
  key_map ['\177'] = DPC;
#endif


  checkwinsize ();
  if (XMAX < 39 || YMAX < 1) {
	panic ("Min. 3x40 size needed for terminal", "too small");
  }


/* ensure feedback after start-up when terminal is slow on queries */
  if (strisprefix ("rxvt", TERM)) {
	get_ansi_modes ();
	clearscreen ();
	status_msg ("Auto-detecting terminal properties - waiting for rxvt to report");
	set_cursor (0, 0);
	flush ();

	/* Make sure output is flushed in rxvt (was apparently needed): */
	(void) char_ready_within (30, NIL_PTR);
  }


/* request terminal type and version from terminal */
  detect_terminal_type ();

#ifdef debug_graphics
  printf ("(ty) ascii %d, vga %d, vt100 %d\n", use_ascii_graphics, use_vga_block_graphics, use_vt100_block_graphics);
#endif


/* detect screen encoding and calibrate screen properties */

/* detect kterm encodings */
#define sjis_3bytes	"x�a"
/*#define sjis_3bytes	"���"*/
  if (streq ("kterm", TERM)) {
	if (test_screen_width (sjis_3bytes) == 3) {
		(void) set_term_encoding ("Shift-JIS", 'S');
		if (! text_encoding_selected) {
			(void) set_text_encoding ("Shift-JIS", 'S', "TERM=kterm");
		}
	} else {
		(void) set_term_encoding ("EUC-JP", 'J');
		if (! text_encoding_selected) {
			(void) set_text_encoding ("EUC-JP", 'J', "TERM=kterm");
		}
	}
	auto_detect = False;
	trace_encoding ("kterm");
  }


/* detect UTF-8 and CJK screen encodings */
  trace_encoding ("init");

  swidth = test_screen_width ("åلاษษ刈墢");
#ifdef debug_screenmode
  printf ("test_screen_width -> %d\n", swidth);
#endif

  if (screen_version > 0 && swidth == 3) {
	/* workaround for problem to get LAM through 'screen' */
	swidth = test_screen_width ("ålษษ刈墢");
#ifdef debug_screenmode
	printf ("test_screen_width -> %d\n", swidth);
#endif
  }

  if (swidth > 0) {
	/**
	 check cursor column after test string, determine screen mode
	  6	-> UTF-8, no double-width, with LAM/ALEF ligature joining
	  7	-> UTF-8, no double-width, no LAM/ALEF ligature joining
	  8	-> UTF-8, double-width, with LAM/ALEF ligature joining
	  9	-> UTF-8, double-width, no LAM/ALEF ligature joining
	 11,16	-> CJK terminal (with luit)
	 10,15	-> 8 bit terminal or CJK terminal
	 13	-> Poderosa terminal emulator, UTF-8, or TIS620 terminal
	 14,17	-> CJK terminal
	 16	-> Poderosa, ISO Latin-1
	 (17)	-> Poderosa, (JIS)
	 18	-> CJK terminal (or 8 bit terminal, e.g. Linux console)
	*/

	if (swidth == 13) {
		/* TIS620 terminal or Poderosa UTF-8 mode */
		if (test_screen_width ("å") == 1) {
			poderosa_version = 1;
			swidth = 9;	/* continue with UTF-8 checks */
		} else {
			set_term_encoding ("TIS", 'T');
		}
	} else if (swidth == 16) {
		poderosa_version = 1;
	}

	if (swidth > 0 && swidth <= 9) {
	    int xwidth;
	    FLAG do_width_checks = True;

	    utf8_screen = True;
	    utf8_auto_detected = True;
	    utf8_input = True;
	    cjk_term = False;
	    mapped_term = False;

	    check_glyphs ();
	    splash_logo ();

#ifdef vms
	    if (rxvt_version > 0
		|| mintty_version > 0
	       ) {
		/* checks are too slow... */
		do_width_checks = False;
	    }
#endif

	    if (do_width_checks) {
		acquire_screen_widths (utf8_widths, arrlen (utf8_widths));
	    }

	    if (get_screen_width ("a̡", utf8_widths, arrlen (utf8_widths)) == 1) {
			combining_screen = True;
			if (! combining_mode_disabled) {
				combining_mode = True;
			}
	    } else {
			combining_screen = False;
			combining_mode = False;
	    }
	    if ((swidth & 1) == 0) {
			/* ligature joining detected, must also be bidi */
			joining_screen = True;
			bidi_screen = True;
			poormansbidi = False;
			/* disable scrollbar to prevent interference */
			if (! explicit_scrollbar_style) {
				disp_scrollbar = False;
				scrollbar_width = 0;
			}
	    }

	    /* used to call splash_logo () here */

	    if (swidth < 8) {
			/* no wide character support */
			width_data_version = 0;
			trace_width_data_version ("0");
	    } else if (do_width_checks) {
		if (get_screen_width ("𛀁𓐀", utf8_widths, arrlen (utf8_widths)) == 3) {
			width_data_version = U600;
			trace_width_data_version ("600");
		} else if (get_screen_width ("ꥠ", utf8_widths, arrlen (utf8_widths)) >= 2) {
			width_data_version = U520;
			trace_width_data_version ("520");
		} else if (get_screen_width ("︐", utf8_widths, arrlen (utf8_widths)) >= 2) {
			width_data_version = U410;
			trace_width_data_version ("410");
		} else if (get_screen_width ("《》〚〛｟｠", utf8_widths, arrlen (utf8_widths)) < 9) {
			/* older width data (before xterm 167) */
			width_data_version = U300;
			trace_width_data_version ("300");
		}
	    }

	    if (do_width_checks) {
		/* determine combining data version */

		/* Add new test strings to array utf8_widths
		   (section detecting combining_data_version) */
		if (get_screen_width (".᪰.ᷮ", utf8_widths, arrlen (utf8_widths)) == 2) {
			combining_data_version = U700;
		} else if (get_screen_width (".؜.؜", utf8_widths, arrlen (utf8_widths)) == 2) {
			combining_data_version = U630;
		} else if (get_screen_width (".؄.ꙴ", utf8_widths, arrlen (utf8_widths)) == 2) {
			combining_data_version = U620;
		} else if (get_screen_width (".ٟ.ऺ", utf8_widths, arrlen (utf8_widths)) == 2) {
			combining_data_version = U600;
		} else if (get_screen_width (".ࠖ.ऀ", utf8_widths, arrlen (utf8_widths)) == 2) {
			combining_data_version = U520;
		} else if (get_screen_width (".҇.᷌", utf8_widths, arrlen (utf8_widths)) == 2) {
			combining_data_version = U510;
		} else if (get_screen_width (".᷄.᷅", utf8_widths, arrlen (utf8_widths)) == 2) {
			combining_data_version = U500;
		} else if (get_screen_width (".͘.͙", utf8_widths, arrlen (utf8_widths)) == 2) {
			combining_data_version = U410;
		} else if (get_screen_width (".͐.឴.᠎", utf8_widths, arrlen (utf8_widths)) == 4) {
			combining_data_version = U400;
		} else if (get_screen_width ("aͣ", utf8_widths, arrlen (utf8_widths)) == 1) {
			combining_data_version = U320;
		} else if (get_screen_width ("a܏", utf8_widths, arrlen (utf8_widths)) == 1) {
			combining_data_version = U300;
		} else {
			combining_data_version = U300beta;
		}
		if (combining_data_version >= U520) {
			if (get_screen_width ("ᄀힰᄀퟋ", utf8_widths, arrlen (utf8_widths)) == 4) {
				hangul_jamo_extended = True;
			}
		}
		trace_width_data_version ("comb");

		check_cjk_width ();

		/* check non-BMP width properties */
		nonbmp_width_data = get_screen_width ("𠀁𠀁𠀁𠀁a𝆪a𝆪a󠀠", utf8_widths, arrlen (utf8_widths)) - 7;
		if (nonbmp_width_data > 7) {
			/* non-BMP combining characters are wide;
			   xterm -cjk_width ?
			 */
			nonbmp_width_data -= 3;
		}
		if (get_screen_width ("𐌀𐌰𐐀", utf8_widths, arrlen (utf8_widths)) > 3 && suppress_non_BMP == False) {
			/* e.g. KDE konsole */
			suppress_non_BMP = True;
		}
		/* check unassigned character width properties;
		   they are sometimes displayed single-width by rxvt
		   or xterm +mk_width (the default)
		 */
		xwidth = get_screen_width ("㄀ㄯ㄰㆏꒎꓏﫿﹯＀", utf8_widths, arrlen (utf8_widths));
		if (xwidth < 18) {
			unassigned_single_width = True;
		}

		xwidth = get_screen_width ("䷀䷀‎", utf8_widths, arrlen (utf8_widths));
		/* check Yijing Hexagram width (wcwidth glitch) */
		if (xwidth < 4) {
			wide_Yijing_hexagrams = False;
		}
		/* check bidi markers printable width (since xterm 230) */
		if (xwidth & 1) {
			printable_bidi_controls = True;
		}
	    }
	} else {
		utf8_screen = False;
		utf8_input = False;

		splash_logo ();

		if (! combining_screen_selected) {
			combining_screen = False;
			combining_mode = False;
		}
		acquire_screen_widths (cjk_widths, arrlen (cjk_widths));
		if (swidth > 13) {
			int cwidth = get_screen_width ("�0�2", cjk_widths, arrlen (cjk_widths));
			if (cwidth > 2) {
				/* quite safe detection */
				gb18030_term = False;
				suppress_extended_cjk = True;
			} else if (cwidth >= 0) {
				cjk_term = True;
				if (cwidth == 1) {
					cjk_uni_term = True;
				}
			}
			/* if any of the following characters is smaller 
			   than 2 character cells, 
			   it's not a native CJK terminal
			 */
			cwidth = get_screen_width ("����ꥦ�ޡ���", cjk_widths, arrlen (cjk_widths));
			if (cwidth < 14) {
				cjk_uni_term = True;
			}
			cwidth = get_screen_width ("����", cjk_widths, arrlen (cjk_widths));
			if (cwidth > 2) {
				/* quite safe detection */
				euc4_term = False;
				suppress_extended_cjk = True;
			} else if (cwidth >= 0 && swidth != 15 /* mlterm */) {
				if (! mapped_term) {
					cjk_term = True;
				}
			}
			if (euc3_term && get_screen_width ("���x", cjk_widths, arrlen (cjk_widths)) > 3) {
				/* unsafe detection */
				euc3_term = False;
				suppress_extended_cjk = True;
			}
			if (get_screen_width ("��", cjk_widths, arrlen (cjk_widths)) < 2) {
				cjklow_term = False;
			}
		}
		if ((swidth > 10 && swidth != 13 && swidth != 15 && swidth < 18) || cjk_term) {
			/* if the test string width did not comply 
			   with 8 bit behaviour, or a 
			   GB18030 or EUC 4 byte (EUC-TW) terminal 
			   was asserted, assume CJK terminal;
			   a EUC 3 byte (EUC-JP) terminal cannot 
			   be asserted but can just be assumed as 
			   an 8 bit terminal might respond with 
			   the same width behaviour
			 */
			utf8_screen = False;
			utf8_input = False;
			if (! combining_screen_selected) {
				combining_screen = False;
				combining_mode = False;
			}
			/* assume CJK terminal with unspecific encoding */
			if (! mapped_term) {
				cjk_term = True;
			}

		}
	}
	trace_encoding ("detected");
  } else {
#ifdef __ANDROID__
	swidth = 0;	/* no CPR */
#endif
	if (swidth == 0) {	/* e.g. Better Terminal on Android */
		(void) set_term_encoding ("ASCII", ' ');
		use_ascii_graphics = True;
		menu_border_style = 'r';
	}

	splash_logo ();
  }

/* detect and adjust Poderosa terminal emulator (narrow ¡ and wide ×) */
  if (poderosa_version > 0) {
	if (utf8_screen
#ifdef poderosa_detect_string
	 && get_screen_width ("¡×", utf8_widths, arrlen (utf8_widths)) == 3
#endif
	   ) {
	/* Poderosa utf-8 (or undetected euc-jp/shift-jis) */
		limited_marker_font = True;
	} else if (! cjk_term
#ifdef poderosa_detect_string
	 && ! utf8_screen && get_screen_width ("��", cjk_widths, arrlen (cjk_widths)) == 3
#endif
	   ) {
	/* Poderosa iso-8859-1 */
		(void) set_term_encoding ("ISO-8859-1", ' ');
	} else {
		poderosa_version = 0;
	}
	if (poderosa_version > 0) {
		cjk_width_data_version = U300beta;
		use_vt100_block_graphics = False;
		use_ascii_graphics = True;
		menu_border_style = 'r';
		use_vga_block_graphics = False;
		use_mouse = False;
		use_appl_keypad = False;
		colours_256 = False;
		colours_88 = False;
	}
  }

#ifdef debug_graphics
  printf ("(tw) ascii %d, vga %d, vt100 %d\n", use_ascii_graphics, use_vga_block_graphics, use_vt100_block_graphics);
#endif


/* determine combining character support on non-Unicode terminal */
  if (cjk_term) {
	unsigned long cjk_combining = mappedtermchar (0x0300);
	if (no_char (cjk_combining)) {
		combining_screen = False;
	} else {
		char cjk_check [9];
		char * check = cjk_check;
		* check ++ = 'a';
		check += cjkencode_char (True, cjk_combining, check);
		* check = '\0';
		if (test_screen_width (cjk_check) == 1) {
			combining_screen = True;
			if (! combining_mode_disabled) {
				combining_mode = True;
			}
#ifdef debug_screenmode
			printf ("cjk combining screen %d (disabled %d) mode %d\n", combining_screen, combining_mode_disabled, combining_mode);
#endif
		}
	}
  } else if (mapped_term) {
	character c;
	combining_screen = False;
	for (c = 0x80; ; c ++) {
		unsigned long u = lookup_mappedtermchar (c);
		if (! no_unichar (u) && iscombining (u)) {
			char comb_check [3];
			comb_check [0] = 'a';
			comb_check [1] = c;
			comb_check [2] = '\0';
			if (test_screen_width (comb_check) == 1) {
				combining_screen = True;
				if (! combining_mode_disabled) {
					combining_mode = True;
				}
			}
			break;
		}
		if (c == 0xFF) {
			break;
		}
	}
  }

/* determine joining character support on non-Unicode terminal */
  if (cjk_term || mapped_term) {
	unsigned long lam = mappedtermchar (0x0644);
	if (! no_char (lam)) {
		unsigned long alef = mappedtermchar (0x0627);
		char join_check [9];
		char * check = join_check;
		* check ++ = 'a';
		if (cjk_term) {
			check += cjkencode_char (True, lam, check);
			check += cjkencode_char (True, alef, check);
		} else {
			* check ++ = lam;
			* check ++ = alef;
		}
		* check = '\0';
		if (test_screen_width (join_check) == 2) {
			/* ligature joining detected, must also be bidi */
			joining_screen = True;
			bidi_screen = True;
			poormansbidi = False;
			/* disable scrollbar to prevent interference */
			if (! explicit_scrollbar_style) {
				disp_scrollbar = False;
				scrollbar_width = 0;
			}
			/* bold does not work */
			use_bold = False;
#ifdef debug_screenmode
			printf ("bidi screen\n");
#endif
		}
	}
  }

/* determine terminal restrictions relevant for marker configuration */

  if (strisprefix ("terminator", TERM)) {
	limited_marker_font = True;
  }


/* adjust CJK width properties */
/* adjust CJK line markers */
  if (cjk_term) {
	/* determine size of line markers in CJK terminal encoding */
	int cjk_ellipsis_width;
	unsigned long cjk_degree = mappedtermchar (0x00B0);	/* ° */
	unsigned long cjk_lineend = mappedtermchar (0x300A);	/* 《 */
	unsigned long cjk_ellipsis = mappedtermchar (0x2026);	/* … */
	unsigned long cjk_uni = mappedtermchar (0x00A2);	/* ¢ */
	unsigned long cjk_dot = mappedtermchar (0x00B7);	/* · */
	unsigned long cjk_lat = mappedtermchar (0x00F8);	/* ø */
	unsigned long cjk_shy = mappedtermchar (0x00AD);	/* ­ */
	unsigned long cjk_ten = mappedtermchar (0x3248);	/* ㉈ */

	char cjk_check [29];
	char * check = cjk_check;
	int cjkwidth;
	cjk_currency_width = 1;
	check += cjkencode_char (True, cjk_ellipsis, check);
	check += cjkencode_char (True, cjk_lineend, check);
	check += cjkencode_char (True, cjk_lineend, check);
	if (no_char (cjk_uni)) {
		cjk_currency_width = 0;
		cjk_uni = cjk_shy;
		if (no_char (cjk_uni)) {
			cjk_uni = mappedtermchar (0x00AF);	/* ¯ */
		}
	}
	if (! no_char (cjk_uni)) {
		check += cjkencode_char (True, cjk_uni, check);
		check += cjkencode_char (True, cjk_uni, check);
		check += cjkencode_char (True, cjk_uni, check);
		check += cjkencode_char (True, cjk_uni, check);
	}
	* check = '\0';
	cjkwidth = test_screen_width (cjk_check);
	cjkwidth --;
	cjk_ellipsis_width = 1 + (cjkwidth & 1);
	cjkwidth = (cjkwidth >> 1) - 1;
	cjk_lineend_width = 1 + (cjkwidth & 1);
	cjkwidth = (cjkwidth >> 1) - 1;
	if (cjk_currency_width && cjkwidth == 1) {
		cjk_currency_width = 2;
	}
	if (! no_char (cjk_uni) && (cjkwidth & 1) == 0) {
		cjk_uni_term = True;
	}

	if (cjk_uni_term) {
		if (! no_char (cjk_degree)) {
			check = cjk_check;
			check += cjkencode_char (True, cjk_degree, check);
			* check = '\0';
			if (test_screen_width (cjk_check) == 2) {
				cjk_width_data_version = U320beta;
			}
		}

		if (cjk_lineend_width == 1) {
			if (width_data_version > 0) {
				width_data_version = U300;
			}
		} else if (cjk_ellipsis_width == 2) {
			cjk_width_data_version = U400;
			/* ?
			if (width_data_version < U400) {
				if (width_data_version > 0) {
					width_data_version = U400;
				}
			}
			*/

			if (! no_char (cjk_ten)) {
				check = cjk_check;
				check += cjkencode_char (True, cjk_ten, check);
				* check = '\0';
				if (test_screen_width (cjk_check) == 2) {
					cjk_width_data_version = U520;
				}
			}

			if (cjk_width_data_version < U520 && ! no_char (cjk_shy)) {
				check = cjk_check;
				check += cjkencode_char (True, cjk_shy, check);
				* check = '\0';
				if (test_screen_width (cjk_check) == 2) {
					cjk_width_data_version = U320beta;
				}
			}

			if (! no_char (cjk_lat)) {
				check = cjk_check;
				check += cjkencode_char (True, cjk_lat, check);
				* check = '\0';
				if (test_screen_width (cjk_check) == 2) {
					cjk_wide_latin1 = True;
				} else {
					cjk_wide_latin1 = False;
				}
			}
		} else if (rxvt_version > 0 && ! no_char (cjk_dot)) {
			check = cjk_check;
			check += cjkencode_char (True, cjk_dot, check);
			* check = '\0';
			if (test_screen_width (cjk_check) == 1) {
				/* fix this as a workaround for buggy rxvt */
				if (width_data_version > 0) {
					width_data_version = U320;
				}
			}
		}
		trace_width_data_version ("cjk_uni_term");
#ifdef debug_screenmode
		printf ("cjk_uni_term (width_data_version %d CJK %d) ", width_data_version, cjk_width_data_version);
#endif
	}
#ifdef debug_screenmode
	printf ("CJK %c\n", text_encoding_tag);
	printf ("euc3 %d, euc4 %d, low cjk %d\n", euc3_term, euc4_term, cjklow_term);
#endif

	if (CJK_TAB_marker >= 0x80) {
#ifdef CJKTAB_MIDDLE_DOT
		/* not available in all CJK encodings and terminals */
		CJK_TAB_marker = 0x00B7;
		cjk_tab_width = cjk_dot_width;
#else
		cjk_tab_width = cjk_ellipsis_width;
#endif
		if (term_encoding_tag == 'H') {
			/* workaround for buggy hanterm */
			CJK_TAB_marker = '.';
		}
		if (limited_marker_font) {
			CJK_TAB_marker = '.';
		}
	}
	if (CJK_TAB_marker < 0x80) {
		cjk_tab_width = 1;
	}

	/* ensure further markers work for CJK */
	SHIFT_marker = ' ';
	MENU_marker = '>';
  }


#ifdef debug_screenmode
	printf ("width_data_version %d (CJK %d), combining_data_version %d\n", width_data_version, cjk_width_data_version, combining_data_version);
#else
	trace_width_data_version ("final");
#endif


/* request terminal type and version from terminal */
/*  detect_terminal_type ();	moved above */


/* 'screen' tweaks */
  if (screen_version > 0) {
	if (! explicit_scrollbar_style) {
		fine_scrollbar = False;
	}
	if (! explicit_border_style) {
		use_stylish_menu_selection = False;
		use_vt100_block_graphics = True;
	}
	if (limited_marker_font) {
		very_limited_marker_font = True;
	} else {
		limited_marker_font = True;
	}
	use_mouse_anymove_inmenu = True;
  }

/* 'tmux' tweaks */
  if (tmux_version > 0) {
	if (limited_marker_font) {
		very_limited_marker_font = True;
	} else {
		limited_marker_font = True;
	}
	if (! explicit_border_style) {
		use_stylish_menu_selection = False;
	}
	bold_border = False;	/* could be in mintty using DejaVu fonts */
  }

/* mlterm tweaks */
  /* since mlterm does not identify itself, try to reveal it: */
  if (! mlterm_version) {
	if (streq ("mlterm", TERM)) {
		mlterm_version = 1;
	} else if (bidi_screen && ! mintty_version) { /* probably mlterm */
		mlterm_version = 1;
	}
  }
  if (mlterm_version > 0) {
	use_appl_keypad = True;
	apply_joining = False;	/* use native LAM/ALEF joining of mlterm */
	unassigned_single_width = True;
	spacing_combining = True;
	/* bold would use weird font */
	use_bold = False;
	if (xterm_version >= 96) {	/* mlterm 3.1.0 */
		use_mouse_1015 = True;	/* mlterm 3.1.2 supports 1015 and 1006 */
	}
	/* enforce some observed font limitations of cygwin mlterm */
	if (limited_marker_font) {
		very_limited_marker_font = True;
	} else {
		limited_marker_font = True;
	}
	if (! explicit_border_style) {
		use_stylish_menu_selection = False;
	}
	if (! explicit_scrollbar_style) {
		disp_scrollbar = False;
		scrollbar_width = 0;
	}
	fine_scrollbar = False;
  }

/* workaround for buggy mlterm bold/width chaos */
#ifdef oldmlterm_wideboldborderbug_workaround
  if (! explicit_border_style && ! cjk_width_data_version) {
	bold_on ();
	/* check width of upper left rounded corner */
	if (get_screen_width ("╭", utf8_widths, arrlen (utf8_widths)) > 1) {
		use_vt100_block_graphics = True;
	}
	bold_off ();
  }
#endif

/* tweaks for KDE konsole; disable use of some features */
  if (konsole_version > 0) {
	colours_256 = False;
	if (! utf8_screen) {
		/* VT100 block graphics work now with workaround for 
		   missing eA capability (see io.c) */
		/* disable use of VT100 block graphics for 
		   konsole instances that do not support them */
		if (strcontains (TERM, "linux")) {
			use_ascii_graphics = True;
			menu_border_style = 'r';
			use_vga_block_graphics = False;
		}
	}
	/* disable use of Unicode fine-grained block graphics */
	if (! explicit_border_style) {
		use_stylish_menu_selection = False;
		if (! use_ascii_graphics) {
			menu_border_style = 's';
		}
	}
	/* make menu selections more visible ? */
	/*dark_term = True;*/	/* fatal if not actually dark */

	/* workaround for buggy behaviour on systems that report 
	   wrong window size (SCO Unixware Caldera Linux) */
	if (! can_get_winsize) {
		can_delete_line = False;
	}
  }

/* handle mintty/PuTTY */
  if (mintty_version > 0) {
	display_delay = 0;

	if (utf8_screen && mintty_version >= 909) {
		/* used to check_glyphs () here */
		if (! glyphs) {
			limited_marker_font = True;
		}
	} else {
		if (limited_marker_font) {
			very_limited_marker_font = True;
		} else {
			limited_marker_font = True;
		}
	}

	spacing_combining = False;
	if (mintty_version < 500 && suppress_non_BMP == False) {
		suppress_non_BMP = True;
	}
	bidi_screen = True;
	halfjoining_screen = True;
	apply_joining = True;
	poormansbidi = False;
	/* disable scrollbar to prevent interference */
	if (! explicit_scrollbar_style) {
		/*disp_scrollbar = False;*/
		/*scrollbar_width = 0;*/
	}

	/* mintty display adjustment */
	if (! explicit_scrollbar_style) {
		if (! isglyph_code ("9601")) { /* LOWER ONE EIGHTH BLOCK */
			fine_scrollbar = False;
		}
	}
	if (! explicit_selection_style) {
#ifdef assume_proper_vertical_blocks
		if (! isglyph_code ("9613")) { /* LEFT THREE EIGHTHS BLOCK */
			use_stylish_menu_selection = False;
		}
#else
		/* defensively disable use of crap vertical blocks */
		use_stylish_menu_selection = False;
#endif
	}
	if (! explicit_border_style) {
		if (mintty_version >= 400) {
			if (isglyph_code ("9581")) { /* BOX DRAWINGS ARC */
				menu_border_style = 'r';
			} else if (isglyph_code ("9484")) { /* BOX DRAW. CORNER */
				menu_border_style = 's';
			} else {
				use_vt100_block_graphics = True;
				/*use_ascii_graphics = True;*/
				menu_border_style = 'r';
			}
		} else {
			menu_border_style = 'd';
		}
	}
	/* for the sake of DejaVu fonts */
	bold_border = False;

	can_dim = True;		/* ? some shading problems... */
#ifdef avoid_light_pseudo_bold
	dark_term = False;
#endif

	if (mintty_version >= 400) {
		use_modifyOtherKeys = True;
		use_appl_cursor = False;
		use_appl_keypad = True;
	}
	if (mintty_version >= 10004) {
		/* numeric encoding of mouse coordinates */
		use_mouse_1015 = True;
	} else {
		use_mouse_1015 = False;	/* buggy in 1.0.3 */
		if (mintty_version >= 900) {
			/* UTF-8 encoding of mouse coordinates */
			use_mouse_extended = True;
		}
	}
	/* enable mouse move reporting without button pressed (for menus) */
	use_mouse_anymove_inmenu = True;
	/* limit terminal to ASCII in POSIX locale */
	(void) locale_terminal_encoding ();
  }

  if (strisprefix ("xterm", TERM) && xterm_version != 2) {
	colours_256 = True;
  }

/* disable VT100 graphics for openwin xterm (but not for cxterm etc) */
  if (xterm_version == 2 && ! cjk_term) {
	use_ascii_graphics = True;
	use_vt100_block_graphics = False;
	menu_border_style = 'r';
  }

/* disable erase characters for older xterm */
  if (xterm_version < 154 && mlterm_version == 0) {
	can_erase_chars = False;
  }

/* indicate enabling of special terminal modes */
  if (xterm_version >= 216) {
	use_modifyOtherKeys = True;
	/*use_appl_keypad = True;*/
  }
  if (xterm_version > 0) {
	/* enable mouse move reporting without button pressed (for menus) */
	use_mouse_anymove_inmenu = True;
  }
  if (xterm_version >= 277) {
	/* numeric encoding of mouse coordinates */
	use_mouse_1015 = True;
  } else if (xterm_version >= 262) {
	/* UTF-8 encoding of mouse coordinates */
	use_mouse_extended = True;
  }
  if (xterm_version > 305) {
	can_dim = True;
  }
  if (xterm_version > 0 /* && xterm_version < ?? */) {
	suppress_non_BMP = True;
  }

#ifdef use_mouse_hilite_tracking
/* suppress mouse highlight tracking when it would lock xterm */
  if (xterm_version == 224) {
	/* xterm bug broke hilite mode abort sequence */
	use_mouse_hilite_tracking = False;
  }
#endif

/* suppress fancy graphic characters in gnome-terminal, fix keyboard mapping */
  if (gnome_terminal_version > 0) {
	/* give built-in keydefs precedence over buggy termcap */
	set_fkeymap (NIL_PTR);
	/* workaround for unusual keypad assignments */
	if (mined_keypad) {
		mined_keypad = False;
	} else {	/* in case option -k reversed it already */
		mined_keypad = True;
	}

	/* probably gnome-terminal */
	if (! use_ascii_graphics) {
		menu_border_style = 's';
	}
	use_stylish_menu_selection = False;
	fine_scrollbar = False;

	/* make menu selections more visible ? */
	/*dark_term = True;*/	/* fatal if not actually dark */

	/* suppress attempts to display invalid Unicode characters */
	suppress_EF = True;

	/* enable mouse move reporting without button pressed (for menus) */
	use_mouse_anymove_inmenu = True;

	if (gnome_terminal_version >= 2000 && gnome_terminal_version < 2800) {
		/* for the sake of sakura */
		use_vt100_block_graphics = True;
		limited_marker_font = True;
		very_limited_marker_font = True;
	}

	cursor_style = 0;
   }

/* special cases of terminal capabilities handling (esp. block graphics) */

  if (strcontains (TERM, "vt220")
     || strcontains (TERM, "vt320")
     || strcontains (TERM, "vt340")
     || strcontains (TERM, "vt400")
     || strcontains (TERM, "vt420")
     || strcontains (TERM, "vt510")
     || strcontains (TERM, "vt520")
     || strcontains (TERM, "vt525")
     || strisprefix ("pcvt", TERM)
     || strisprefix ("ncsa", TERM)
     || strisprefix ("ti916", TERM)
     || strisprefix ("bq300", TERM)
     || strisprefix ("z340", TERM)
     || strisprefix ("ncr160vt300", TERM)
     || strisprefix ("ncr260vt300", TERM)
     || streq ("emu-220", TERM)
     || streq ("crt", TERM)
     ) {
	use_appl_keypad = True;
  }
  if (strcontains (TERM, "linux")) {
	set_fkeymap ("linux");
	fine_scrollbar = False;
	use_stylish_menu_selection = False;
	UTF_MENU_marker = "»";
	submenu_marker = "»";
	dark_term = True;
	bold_border = False;	/* needed e.g. for Debian VGA console */
	use_appl_keypad = True;
	if (! explicit_border_style) {
		if (utf8_screen) {
			/*use_ascii_graphics = True;*/
			if (! use_ascii_graphics) {
				/* 's' or 'd' work */
				menu_border_style = 's';
			}
		} else if (use_vt100_block_graphics == False) {
			use_vga_block_graphics = True;
		}
	}
  }
  if (! can_alt_cset && ! use_pc_block_graphics && ! use_vga_block_graphics && ! utf8_screen ) {
	use_ascii_graphics = True;
	menu_border_style = 'r';
  }
  if (cjk_term) {
	/* make sure lineend marker will always fit: */
	scrollbar_width = 1;

	/*use_bgcolor = False;*/

	if (strisprefix ("rxvt", TERM)) {
		/* seems to work now */
	} else if (xterm_version == 2) {
		/* cxterm would blink instead of setting 256 color mode */
		colours_256 = False;
		colours_88 = False;
	}
  }
#ifdef debug_graphics
  printf ("(tt) ascii %d, vga %d, vt100 %d\n", use_ascii_graphics, use_vga_block_graphics, use_vt100_block_graphics);
#endif

/* derive further terminal restrictions */
  if (standout_glitch) {
	use_bold = False;
  }


/* some terminal-specific properties */
  if (strisprefix ("vt1", TERM)) {
	use_appl_keypad = True;
  }
  if (rxvt_version > 0 || strisprefix ("rxvt", TERM)) {
	use_appl_keypad = True;

	if (rxvt_version < 909 && rxvt_version >= 380) {
		colours_256 = False;
		colours_88 = True;
		/* ?? colour mode doesn't work with older buggy rxvt after configure --enable-text-blink --disable-256-color */
	}

#ifdef check_unassigned_single_width_here
	/* rxvt-unicode sometimes displays all unassigned Unicode chars 
	   in single width;
	   According to change log, this could have been fixed in:
		   4.4: rewrote handling of default-char width
		   8.0: fixed urxvt::strwidth to calculate width in the same way as screen.C
		   8.1: rewrote handling of default-char width
	   It does not happen with cygwin rxvt-unicode 
	   (which is not locale-driven).
	   Check should not be limited to certain versions (e.g. < 810):
	   - inconsistent behaviour (e.g. cygwin, see above)
	   - unreliable version detection, e.g. rxvt 7.7 reports to be 9.4
	 */
	/* issue also sometimes occurs with xterm +mk_width, 
	   so the check was moved out (see above)
	 */
	if (rxvt_version > 0) {
		if (test_screen_width ("㄀ㄯ㄰㆏꒎꓏﫿﹯＀") < 18) {
			unassigned_single_width = True;
		}
	}
#endif

	/* rxvt sends ESC prefix for Alt-modified escape sequences */
	detect_esc_alt = True;
	/* disable use of VT100 block graphics on rxvt which does not support them */
	if (! utf8_screen) {
		/* VT100 block graphics work now with workaround for 
		   missing eA capability (see io.c) */
		if (rxvt_version < 300) {	/* old (non-Unicode) rxvt */
			/* VT100 block graphics depend on font capabilities */
			use_ascii_graphics = True;
			menu_border_style = 'r';
		}
	}

	if (rxvt_version >= 840) {
		use_mouse_anymove_inmenu = True;
	}
	/* numeric encoding of mouse coordinates since rxvt-unicode 9.10 */
	use_mouse_1015 = True;
  }

#ifdef debug_graphics
  printf ("(tx) ascii %d, vga %d, vt100 %d\n", use_ascii_graphics, use_vga_block_graphics, use_vt100_block_graphics);
#endif


/* disable graphics on some CJK terminals */
  if (cjk_term && ! use_vt100_block_graphics && ! can_alt_cset) {
	use_ascii_graphics = True;
	menu_border_style = 'r';
  }
#ifdef debug_graphics
  printf ("(ct) ascii %d, vga %d, vt100 %d\n", use_ascii_graphics, use_vga_block_graphics, use_vt100_block_graphics);
#endif


/* configure line markers */
  config_markers ();

/* adjust wide line markers */
  if (cjk_width_data_version) {
	if (UTF_TAB_marker && iswide (utf8value (UTF_TAB_marker))) {
		UTF_TAB_marker = NIL_PTR;
	}
	if (UTF_TAB0_marker && iswide (utf8value (UTF_TAB0_marker))) {
		UTF_TAB0_marker = NIL_PTR;
	}
	if (UTF_TAB2_marker && iswide (utf8value (UTF_TAB2_marker))) {
		UTF_TAB2_marker = NIL_PTR;
	}
	if (UTF_TABmid_marker && iswide (utf8value (UTF_TABmid_marker))) {
		UTF_TABmid_marker = NIL_PTR;
	}
  }

/* adjust to glyph detection */
  if (glyphs && * glyphs) {	/* utf8_screen && mintty_version >= 909 */
    /*
    the check covers misc. configurable alternatives and char. substitutes:
    border styles:
	border	┌ ┆/╎/┊
	- rnd	╭
	- fat	┏ ╏/┇/┋
	- dbl	╔
	- cont	◆ ♦ ◈ ⬘ ⬙
	- yet unchecked: 9487┏;9556╔;9550╎;9482┊;9551╏;9483┋
    markers (additional):
	menu	• ☛ ☞
	tab	․ ‥ … ▷ ▸ ▹ ▻
	ret	◀ ◁ ◂ ◃ ◄ ◅
	shift	tab/ret ⬖ ⬗
    special characters:
	char	✓ ✗ ↯
    */
	/* Usage of TAB markers:
		TAB0 TAB TAB TAB TAB TAB TAB (TAB2)
		TAB TAB TAB TAB TABmid TAB TAB TAB
	*/
	if (! isglyph_utf (UTF_TAB_marker)) {
		if (isglyph_code ("8231")) {
			UTF_TAB_marker = "‧";
		} else {
			UTF_TAB_marker = "·";
		}
	}
	if (! isglyph_utf (UTF_TABmid_marker)) {
		if (isglyph_code ("8227")) {
			UTF_TABmid_marker = "‣";
		} else if (isglyph_code ("8702")) {
			UTF_TABmid_marker = "⇾";
		} else if (isglyph_code ("8594")) {
			UTF_TABmid_marker = "→";
		} else {
			UTF_TABmid_marker = UTF_TAB_marker;
		}
	}
	if (! isglyph_utf (UTF_TAB0_marker)) {
		if (UTF_TAB2_marker != NIL_PTR) {
			UTF_TAB0_marker = UTF_TAB_marker;
		} else {
			UTF_TAB0_marker = NIL_PTR;
		}
	}
	if (! isglyph_utf (UTF_TAB2_marker)) {
		UTF_TAB2_marker = NIL_PTR;
	}

	if (! isglyph_utf (UTF_RET_marker)) {
		if (isglyph_code ("9166")) {
			UTF_RET_marker = "⏎";
		} else if (isglyph_code ("8629")) {
			UTF_RET_marker = "↵";
		} else if (isglyph_code ("8626")) {
			UTF_RET_marker = "↲";
		} else {
			UTF_RET_marker = "«";
		}
	}
	if (! isglyph_utf (UTF_RETfill_marker)) {
		UTF_RETfill_marker = "";
	}
	if (! isglyph_utf (UTF_RETfini_marker)) {
		UTF_RETfini_marker = "";
	}
	if (! isglyph_utf (UTF_DOSRET_marker)) {
		UTF_DOSRET_marker = UTF_RET_marker;
	}
	if (! isglyph_utf (UTF_MACRET_marker)) {
		UTF_MACRET_marker = UTF_RET_marker;
	}
	if (UTF_PARA_marker && ! isglyph_utf (UTF_PARA_marker)) {
		UTF_PARA_marker = NIL_PTR;
	}

	if (! isglyph_utf (UTF_SHIFT_marker)) {
		UTF_SHIFT_marker = "»";
	}
	if (! isglyph_utf (UTF_SHIFT_BEG_marker)) {
		UTF_SHIFT_BEG_marker = "«";
	}

	if (! isglyph_utf (UTF_MENU_marker)) {
		if (isglyph_code ("10003")) {
			UTF_MENU_marker = "✓";
		} else if (isglyph_utf (UTF_MENU_marker_alt)) {
			UTF_MENU_marker = UTF_MENU_marker_alt;
		} else {
			UTF_MENU_marker = "»";
		}
	}
	if (! isglyph_utf (submenu_marker)) {
		submenu_marker = submenu_marker_alt;
		if (! isglyph_utf (submenu_marker)) {
			submenu_marker = "»";
		}
	}

	/* menu scrolling indications */
	if (! isglyph_utf (menu_cont_marker)) {
		if (isglyph_code ("9830")) {
			menu_cont_marker = "♦";
		} else {
			menu_cont_marker = "│";
		}
	}
	if (! isglyph_utf (menu_cont_fatmarker)) {
		if (isglyph_code ("9830")) {
			menu_cont_fatmarker = "♦";
		} else {
			menu_cont_fatmarker = "┃";
		}
	}
  } else if (very_limited_marker_font) {
	glyphs = "";	/* pretend all checked glyphs unavailable */
  }


/* setup terminal modes */
  /* call raw_mode again (was already called for initial tty configuration) */
  raw_mode (True);


/* terminal height adjustment / detection
   - deprecated section, see checkwinsize ()
	terminal control codes useful here:
	ESC[<n>t	(n>=24)	resize to n lines
	ESC[18t		report size of text area as ESC[8;<height>;<width>t
	ESC[8;<height>;<width>t	resize text area
  */
  if (ansi_esc) {
#ifdef adjust_terminal_height	/* obsolete */
	if (strisprefix ("xterm", TERM) && konsole_version <= 0) {
		/* try to adjust the window to the size the tty assumes;
		   workaround for buggy Cygwin/X xterm tty size assumption,
		   also workaround for various rlogin/telnet size confusions;
		   also workaround for false explicit stty rows settings
		 */
		char resizebuf [19];
		int height = YMAX + 1 + MENU;
#ifdef adjust_lines_only
		if (height >= 24 && rxvt_version <= 0) {
			/* this short form doesn't work with rxvt 
			   (which would become extra large)
			 */
			build_string (resizebuf, "\033[%dt", height);
# ifdef debug_terminal_resize
			printf ("setting terminal to %d lines\n", height);
# endif
		} else {
			build_string (resizebuf, "\033[8;%d;%dt", height, XMAX + 1);
# ifdef debug_terminal_resize
			printf ("setting terminal to %dx%d\n", height, XMAX + 1);
# endif
		}
#else
		build_string (resizebuf, "\033[8;%d;%dt", height, XMAX + 1);
# ifdef debug_terminal_resize
		printf ("setting terminal to %dx%d\n", height, XMAX + 1);
# endif
#endif
		putescape (resizebuf);
	}
#else
#ifdef adjust_to_actual_termsize
	unsigned long c;
	/* response will be handled by ANSIseq */
	putescape ("\033[18t");	/* long timeout if response disabled? */
# ifdef debug_terminal_resize
	printf ("requesting terminal size\n");
# endif
	flush ();
#endif
#endif
  }
}


/*======================================================================*\
|*		Setup misc stuff					*|
\*======================================================================*/

/**
   generate names of paste, spool, panic files
 */
static
void
setup_temp_filenames ()
{
  char * temp_dir;
  char temp_dn [maxFILENAMElen];
  char * minedtemp_dir;
  char * username = envvar ("MINEDUSER");
  if (! username) {
	username = getusername ();
  }

#ifdef unix
  temp_dir = envvar ("TMPDIR");
  if (temp_dir == NIL_PTR || temp_dir [0] == '\0' || access (temp_dir, W_OK | X_OK) < 0) {
	temp_dir = envvar ("TMP");
  }
  if (temp_dir == NIL_PTR || temp_dir [0] == '\0' || access (temp_dir, W_OK | X_OK) < 0) {
	temp_dir = envvar ("TEMP");
  }
  if (temp_dir == NIL_PTR || temp_dir [0] == '\0' || access (temp_dir, W_OK | X_OK) < 0) {
	/* the less volatile traditional tmp dir (often link to /var/tmp) */
	temp_dir = "/usr/tmp";
  }
  if (access (temp_dir, W_OK | X_OK) < 0) {
	temp_dir = "/tmp";
  }
  if (access (temp_dir, W_OK | X_OK) < 0) {
	/* in case there is no $TMP, no /tmp etc (e.g. Android): */
	temp_dir = gethomedir ();
  }

  strcpy (temp_dn, temp_dir);
  strip_trailingslash (temp_dn);
  temp_dir = temp_dn;

  /* prefer $MINEDTMP for buffer and spool files (but not for panic files) */
  minedtemp_dir = envvar ("MINEDTMP");
  if (minedtemp_dir == NIL_PTR || minedtemp_dir [0] == '\0' || access (minedtemp_dir, W_OK | X_OK) < 0) {
	minedtemp_dir = temp_dir;
  }

  build_string (panic_file, "%s/minedrecover.%s.%d", temp_dir, username, (int) getpid ());
  build_string (yankie_file, "%s/mined.%s", minedtemp_dir, username);
  build_string (spool_file, "%s/minedprint.%s.%d", minedtemp_dir, username, (int) getpid ());
#endif
#ifdef vms
  if (envvar ("SYS$SCRATCH")) {
	temp_dir = "SYS$SCRATCH";
  } else {
	temp_dir = "SYS$LOGIN";
  }

  /* prefer $MINEDTMP for buffer and spool files (but not for panic files) */
  if (envvar ("SYS$MINEDTMP")) {
	minedtemp_dir = "SYS$MINEDTMP";
  } else {
	minedtemp_dir = temp_dir;
  }

  build_string (panic_file, "%s:$MINEDRECOVER$%s$%d", temp_dir, username, getpid ());
  build_string (yankie_file, "%s:$MINED$%s", minedtemp_dir, username);
  build_string (spool_file, "%s:$MINEDPRINT$%s$%d", minedtemp_dir, username, getpid ());
#endif
#ifdef msdos
  temp_dir = envvar ("TEMP");
  if (temp_dir == NIL_PTR || temp_dir [0] == '\0') {
	temp_dir = envvar ("TMP");
  }
  if (temp_dir == NIL_PTR || temp_dir [0] == '\0') {
	temp_dir = "\\";
  }

  /* prefer %MINEDTMP% for buffer and spool files (but not for panic files) */
  minedtemp_dir = envvar ("MINEDTMP");
  if (minedtemp_dir == NIL_PTR || minedtemp_dir [0] == '\0') {
	minedtemp_dir = temp_dir;
  }

  build_string (panic_file, "%s\\$minedsv_%s", temp_dir, username);
  build_string (yankie_file, "%s\\$minedbf_%s", minedtemp_dir, username);
  build_string (spool_file, "%s\\$minedpr_%s", minedtemp_dir, username);
#endif
}


/*======================================================================*\
|*		Main							*|
\*======================================================================*/

void
emul_mined ()
{
  int i;

  for (i = 0; i < 32; i ++) {
	key_map [i] = mined_key_map [i];
  }
  key_map [erase_char] = DPC;
  quit_char = '\034';

  emulation = 'm';
  shift_selection = False;
  visselect_autocopy = True;
  visselect_copydeselect = True;
  visselect_autodelete = False;

  emacs_buffer = False;
  paste_stay_left = False;
  JUSmode = 0;
}

void
emul_emacs ()
{
  int i;

  for (i = 0; i < 32; i ++) {
	key_map [i] = emacs_key_map [i];
  }
  key_map [erase_char] = DPC;
  quit_char = '\007';		/* ^G */

  emulation = 'e';
  shift_selection = False;
  visselect_autocopy = True;
  visselect_copydeselect = True;
  visselect_autodelete = False;

  emacs_buffer = True;
  paste_stay_left = False;
  JUSmode = 1;
}

void
emul_WordStar ()
{
  int i;

  for (i = 0; i < 32; i ++) {
	key_map [i] = ws_key_map [i];
  }
  key_map [erase_char] = DPC;
  quit_char = '\034';

  emulation = 's';
  shift_selection = True;
  visselect_autocopy = False;
  visselect_copydeselect = False;
  visselect_autodelete = True;

  emacs_buffer = False;
  paste_stay_left = True;
  JUSmode = 0;
}

void
emul_Windows ()
{
  int i;

  for (i = 0; i < 32; i ++) {
	key_map [i] = windows_key_map [i];
  }
  key_map [erase_char] = DPC;
  quit_char = '\034';

  emulation = 'w';
  shift_selection = True;
  visselect_autocopy = False;
  visselect_copydeselect = False;
  visselect_autodelete = True;

  emacs_buffer = False;
  paste_stay_left = False;
  JUSmode = 0;
}

void
emul_pico ()
{
  int i;

  for (i = 0; i < 32; i ++) {
	key_map [i] = pico_key_map [i];
  }
  key_map [erase_char] = DPC;
  quit_char = '\034';

  emulation = 'p';
  shift_selection = True;
  visselect_autocopy = False;
  visselect_copydeselect = False;
  visselect_autodelete = True;

  emacs_buffer = True;
  paste_stay_left = False;
  JUSmode = 1;
}

void
set_keypad_mined ()
{
  keypad_mode = 'm';
  shift_selection = False;
  visselect_autocopy = True;
  visselect_copydeselect = True;
  visselect_autodelete = False;
}

void
set_keypad_shift_selection ()
{
  keypad_mode = 'S';
  shift_selection = UNSURE;
  visselect_copydeselect = True;
  visselect_autocopy = False;
  visselect_autodelete = True;
}

void
set_keypad_windows ()
{
  keypad_mode = 'w';
  shift_selection = True;
  visselect_copydeselect = False;
  visselect_autocopy = False;
  visselect_autodelete = True;
}


/*
   check if string w matches initial words of string s (esp. a locale prefix)
   "ti" "ti_ER" -> True
   "ti" "tig_ER" -> False
   "ti_ER" "ti_ER.UTF-8" -> True
   "aa_E*" "aa_ER" -> True
 */
static
FLAG
matchwords (w, s)
  char * w;
  char * s;
{
  if (strisprefix (w, s)) {
	char fini = s [strlen (w)];
	if ((fini >= 'A' && fini <= 'Z') || (fini >= 'a' && fini <= 'z')) {
		return False;
	} else {
		return True;
	}
  } else if (w [strlen (w) - 1] == '*' && strncmp (s, w, strlen (w) - 1) == 0) {
	return True;
  } else {
	return False;
  }
}

static
FLAG
set_charmap_2 (term, charmap_term, charmap_text)
  FLAG term;
  char * charmap_term;
  char * charmap_text;
{
  if (term) {
	return set_term_encoding (charmap_term, ' ');
  } else {
	return set_text_encoding (charmap_text, ' ', "set_charmap_2");
  }
}

static
FLAG
set_charmap (term, charmap)
  FLAG term;
  char * charmap;
{
  if (term) {
	return set_term_encoding (charmap, ' ');
  } else {
	return set_text_encoding (charmap, ' ', "set_charmap");
  }
}

#ifdef debug_encoding

static
FLAG
do_set_charmap_2 (function, line, term, charmap_term, charmap_text)
  char * function;
  unsigned int line;
  FLAG term;
  char * charmap_term;
  char * charmap_text;
{
  if (term) {
	printf ("set_charmap [%s:%d] %s\n", function, line, charmap_term);
  }
  return set_charmap_2 (term, charmap_term, charmap_text);
}

#define set_charmap(term, charmap)	do_set_charmap_2 (__FUNCTION__, __LINE__, term, charmap, charmap)
#define set_charmap_2(term, charmap_term, charmap_text)	do_set_charmap_2 (__FUNCTION__, __LINE__, term, charmap_term, charmap_text)

#endif

static
struct {
	char * locale;
	char * quotes;
	char * altquotes;
} quotestyles [] = {
#include "quotes.t"
};

static
void
set_quote_styles (style, altstyle)
  char * style;
  char * altstyle;
{
  set_quote_style (altstyle);
  prev_quote_type = quote_type;
  set_quote_style (style);
}

void
handle_locale_quotes (lang, alt)
  char * lang;
  FLAG alt;
{
  int i;
  char * style = "“”";
  char * altstyle = NIL_PTR;

  if (! lang) {
	(void) locale_text_encoding ();
	lang = language_code;
	if (alt == UNSURE && language_preference) {
		/* enable smart quotes if using LANGUAGE= or TEXTLANG= */
		smart_quotes = VALID;
		alt = False;
	}
  }

  for (i = 0; i < arrlen (quotestyles); i ++) {
	if (matchwords (quotestyles [i].locale, lang)) {
		style = quotestyles [i].quotes;
		altstyle = quotestyles [i].altquotes;
		if (strcontains (quotestyles [i].locale, "_")) {
			break;
		}
	}
  }

  if (alt == UNSURE) {
	set_quote_styles ("\"\"", style);
  } else if (alt && altstyle != NIL_PTR) {
	set_quote_styles (altstyle, style);
  } else if (altstyle != NIL_PTR) {
	set_quote_styles (style, altstyle);
  } else {
	set_quote_style (style);
  }
  default_quote_type = quote_type;

#ifdef dont_set_spacing_here
  if (matchwords ("fr", lang) && ! matchwords ("fr_CH", lang)) {
	spacing_quotes = True;
  }
#endif
}

static
struct {
	char * locale;
	char * charmap;
	char * charmap_text;
} locmaps [] = {
#include "locales.t"
};

static
FLAG
handle_locale_encoding (term, encoding)
  FLAG term;
  char * encoding;
{
  /* determine language-specific text handling features */
  if (! term) {
	if (matchwords ("de", language_code)) {
		language_tag = 'g';
	} else if (matchwords ("da", language_code)) {
		language_tag = 'd';
	} else if (matchwords ("fr", language_code)) {
		language_tag = 'f';
	} else if (matchwords ("tr", language_code)
		|| matchwords ("az", language_code)
		/* https://bugzilla.mozilla.org/show_bug.cgi?id=231162#c17 */
		|| matchwords ("crh", language_code)
		|| matchwords ("tt", language_code)
		|| matchwords ("ba", language_code)
		) {
		language_tag = 't';
	} else if (matchwords ("lt", language_code)) {
		language_tag = 'l';
	} else if (matchwords ("nl", language_code)) {
		language_tag = 'n';	/* -> cap. IJsselmeer */
	}
  }

  /* detect encoding by locale suffix;
     if there is no encoding suffix (starting after "."), 
     as a fallback, try to interpret the country suffix 
     (starting with '_' as returned by the functions above)
   */
  if (strisprefix ("GB", encoding)
   || strisprefix ("gb", encoding)
   || strisprefix ("EUC-CN", encoding)
   || strisprefix ("euccn", encoding)
   || strisprefix ("eucCN", encoding)
     ) {
	return set_charmap (term, "GB");
  } else if (strisprefix ("BIG5", encoding)
	 || strisprefix ("Big5", encoding)
	 || strisprefix ("big5", encoding)
	) {
	return set_charmap (term, "Big5");
  } else if (strisprefix ("EUC-TW", encoding)
	 || strisprefix ("euctw", encoding)
	 || strisprefix ("eucTW", encoding)
	) {
	return set_charmap (term, "CNS");
  } else if (strisprefix ("UHC", encoding)
	 || strisprefix ("EUC-KR", encoding)
	 || strisprefix ("euckr", encoding)
	 || strisprefix ("eucKR", encoding)
	) {
	return set_charmap (term, "UHC");
  } else if (strisprefix ("EUC-JP", encoding)
	 || strisprefix ("eucjp", encoding)
	 || strisprefix ("eucJP", encoding)
	 || strisprefix ("ujis", encoding)
	) {
	return set_charmap (term, "EUC-JP");
  } else if (strisprefix ("Shift_JIS", encoding)
	 || strisprefix ("shiftjis", encoding)
	 || strisprefix ("sjis", encoding)
	 || strisprefix ("SJIS", encoding)
	) {
	return set_charmap (term, "CP932");
  } else if (strisprefix ("JOHAB", encoding)) {
	return set_charmap (term, "Johab");

  } else if (strisprefix ("@euro", encoding)) {
	if (matchwords ("fy_NL", language_code)) {
		return set_charmap (term, "UTF-8");
	} else if (matchwords ("hsb_DE", language_code)) {
		return set_charmap (term, "ISO-8859-2");
	} else {
		return set_charmap (term, "ISO-8859-15");
	}
  } else if (strisprefix ("@tradicional", encoding)) {
	return set_charmap (term, "ISO-8859-15");
  } else if (strisprefix ("@devanagari", encoding)) {
	return set_charmap (term, "UTF-8");
  } else if (strisprefix ("@latin", encoding)) {
	return set_charmap (term, "UTF-8");
  } else if (strisprefix ("@cyrillic", encoding)) {
	if (matchwords ("uz_UZ", language_code)) {
		return set_charmap (term, "UTF-8");
	} else {
		return set_charmap (term, "ISO-8859-5");
	}
  } else if (strisprefix ("@iqtelif", encoding)) {
	return set_charmap (term, "UTF-8");
  } else if (strisprefix ("iso8859", encoding)
	|| strisprefix ("ISO8859", encoding)
	|| strisprefix ("iso-8859", encoding)
	|| strisprefix ("ISO-8859", encoding)
	  ) {
	if (encoding [3] == '-') {
		encoding += 8;
	} else {
		encoding += 7;
	}
	if (* encoding == '-' || * encoding == '_') {
		encoding ++;
	}
	if (streq ("1", encoding)) {
		return set_charmap (term, "ISO-8859-1");
	} else if (streq ("5", encoding)) {
		return set_charmap (term, "ISO-8859-5");
	} else if (streq ("6", encoding)) {
		return set_charmap_2 (term, "ISO-8859-6", "MacArabic");
	} else if (streq ("7", encoding)) {
		return set_charmap (term, "ISO-8859-7");
	} else if (streq ("8", encoding)) {
		return set_charmap_2 (term, "ISO-8859-8", "CP1255");
	} else if (streq ("15", encoding)) {
		return set_charmap (term, "ISO-8859-15");

	} else if (streq ("2", encoding)) {
		return set_charmap (term, "ISO-8859-2");
	} else if (streq ("3", encoding)) {
		return set_charmap (term, "ISO-8859-3");
	} else if (streq ("4", encoding)) {
		return set_charmap (term, "ISO-8859-4");
	} else if (streq ("9", encoding)) {
		return set_charmap (term, "ISO-8859-9");
	} else if (streq ("10", encoding)) {
		return set_charmap (term, "ISO-8859-10");
	} else if (streq ("13", encoding)) {
		return set_charmap (term, "ISO-8859-13");
	} else if (streq ("14", encoding)) {
		return set_charmap (term, "ISO-8859-14");
	} else if (streq ("16", encoding)) {
		return set_charmap (term, "ISO-8859-16");
	} else if (streq ("11", encoding)) {
		return set_charmap (term, "TIS");
	}
  } else if (strisprefix ("koi8t", encoding)) {
	return set_charmap (term, "KOI8-T");
  } else if (strisprefix ("KOI8-T", encoding)) {
	return set_charmap (term, "KOI8-T");
  } else if (strisprefix ("koi8r", encoding)) {
	return set_charmap_2 (term, "KOI8-R", "KOI8-RU");
  } else if (strisprefix ("KOI8-R", encoding)) {
	return set_charmap_2 (term, "KOI8-R", "KOI8-RU");
  } else if (strisprefix ("koi8u", encoding)) {
	return set_charmap_2 (term, "KOI8-U", "KOI8-RU");
  } else if (strisprefix ("KOI8-U", encoding)) {
	return set_charmap_2 (term, "KOI8-U", "KOI8-RU");
  } else if (strisprefix ("koi", encoding)) {
	return set_charmap (term, "KOI8-RU");
  } else if (strisprefix ("tcvn", encoding)) {
	return set_charmap (term, "TCVN");
  } else if (strisprefix ("viscii", encoding)) {
	return set_charmap (term, "VISCII");
  } else if (strisprefix ("tis", encoding)
	|| strisprefix ("TIS", encoding)
	) {
	return set_charmap (term, "TIS");
  } else if (strisprefix ("roman", encoding)) {
	return set_charmap (term, "MacRoman");
  } else if (strisprefix ("cp1252", encoding)
	|| strisprefix ("CP1252", encoding)
	) {
	return set_charmap (term, "CP1252");
  } else if (strisprefix ("cp1251", encoding)
	|| strisprefix ("CP1251", encoding)
	) {
	return set_charmap (term, "CP1251");
  } else if (strisprefix ("cp850", encoding)
	|| strisprefix ("CP850", encoding)
	) {
	return set_charmap (term, "CP850");
  } else if (strisprefix ("cp1255", encoding)
	|| strisprefix ("CP1255", encoding)
	) {
	return set_charmap (term, "CP1255");
  } else if (strisprefix ("cp1131", encoding)
	|| strisprefix ("CP1131", encoding)
	) {
	return set_charmap (term, "CP1131");
  } else if (strisprefix ("georgianps", encoding)) {
	return set_charmap (term, "Georgian-PS");
  } else if (strisprefix ("pt154", encoding)) {
	return set_charmap (term, "PT154");
  } else if (strisprefix ("armscii", encoding)
	|| strisprefix ("ARMSCII", encoding)
	) {
	return set_charmap (term, "ARMSCII");
  } else if (strisprefix ("utf8", encoding)
	 || strisprefix ("UTF-8", encoding)
	) {
	return set_charmap (term, "UTF-8");

  } else if (strisprefix ("EBCDIC", encoding)
	|| strisprefix ("cp1047", encoding)
	|| strisprefix ("CP1047", encoding)
	) {
	return set_charmap (term, "CP1047");

  } else if (streq ("EUC", encoding)
	|| strisprefix ("euc", encoding)
	) {
	if (strisprefix ("zh_TW", language_code)) {
		return set_charmap (term, "CNS");
	} else if (strisprefix ("zh", language_code)) {
		return set_charmap (term, "GB");
	} else if (strisprefix ("ja", language_code)) {
		return set_charmap (term, "EUC-JP");
	} else if (strisprefix ("ko", language_code)) {
		return set_charmap (term, "EUC-KR");
	}

  } else if (encoding [0] != '\0' && set_charmap (term, encoding)) {
	/* has been set to other encoding suffix */
  } else {
    /* handle locale shortcut names that imply an encoding */
    int i;
    for (i = arrlen (locmaps) - 1; i >= 0; i --) {
	if (matchwords (locmaps [i].locale, language_code)) {
		if (! term && locmaps [i].charmap_text) {
			return set_charmap (False, locmaps [i].charmap_text);
		} else {
			return set_charmap (term, locmaps [i].charmap);
		}
	}
    }
  }

  return False;
}


static
void
eval_options (minedopt, command_line)
  char * minedopt;
  FLAG command_line;
{
  FLAG plus_opt = False;	/* set if options start with '+' */
  FLAG Cflag = False;	/* -C interacts with -E for compatibility */

  while (* minedopt != '\0') {
    switch (* minedopt) {
	case '+':
		plus_opt = True;
		break;
	case '-':
		plus_opt = False;
		minedopt ++;
		if (* minedopt == '-') {
			restricted = True;
		} else {
			minedopt --;
		}
		break;
	case ' ':
		break;
	case 'M':
		if (* (minedopt + 1) == ':') {
			minedopt ++;
			if (plus_opt) {
				MENU = 1;
				use_file_tabs = True;
			} else {
				use_file_tabs = False;
			}
			break;
		}
		if (plus_opt) {
			if (MENU) {
				if (quickmenu) {
					/*MENU ++;*/
					use_file_tabs = True;
				}
				quickmenu = True;
			} else {
				MENU = 1;
			}
		} else {
			if (use_file_tabs) {
				use_file_tabs = False;
			} else if (MENU) {
				MENU = 0;
			} else {
				quickmenu = False;
			}
		}
		break;
	case 'H':
		mark_HTML = plus_opt;
		break;
	case '@':
		if (plus_opt) {
			groom_stat = True;
		} else {
			groom_info_files = False;
			groom_info_file = False;
		}
		break;
	case '*':
		if (plus_opt) {
			if (use_mouse) {
				/* enable mouse move reporting without button pressed
				 (for menus) */
				if (use_mouse_anymove_inmenu) {
					use_mouse_anymove_always = True;
				} else {
					use_mouse_anymove_inmenu = True;
				}
			} else {
				use_mouse = True;
			}
		} else {
			if (use_mouse_anymove_inmenu) {
				/* disable mouse move reporting w/o button */
				use_mouse_anymove_inmenu = False;
			} else {
				/* disable mouse altogether */
				use_mouse = False;
			}
		}
		break;
	case '~':
		minedopt ++;
		if (* minedopt == '=') {
			minedopt ++;
			sethomedir (minedopt);
			homedir_selected = True;
			while (* minedopt) {
				minedopt ++;
			}
		}
		break;
	case 'L':
		minedopt ++;
		minedopt = scan_int (minedopt, & wheel_scroll);
		minedopt --;
		break;
	case 'v':
		viewonly_mode = True;
		break;
	case 'P':
		if (plus_opt) {
			hide_passwords = False;
		} else {
			hide_passwords = True;
		}
		break;
	case 'p':
		if (plus_opt) {
			proportional = True;
		} else {
			paradisp = True;
		}
		break;
	case 'r':
		minedopt ++;
		if (* minedopt == '?') {
			if (plus_opt) {	/* +r?	CRLF for new files */
				newfile_lineend = lineend_CRLF;
			} else {	/* -r?	LF for new files */
				newfile_lineend = lineend_LF;
			}
		} else {
			minedopt --;
			if (plus_opt) {	/* +r	convert LF to CRLF */
				lineends_LFtoCRLF = True;
			} else {	/* -r	convert CRLF to LF */
				lineends_CRLFtoLF = True;
			}
		}
		break;
	case 'R':
		if (plus_opt) {	/* +R	handle CR */
			lineends_detectCR = True;
		} else {	/* -R	convert CR to LF */
			lineends_detectCR = True;
			lineends_CRtoLF = True;
		}
		break;
	case 'Q':
		minedopt ++;
		if (* minedopt >= '0' && * minedopt <= '9') {
			if (plus_opt) {
				splash_level = * minedopt - '0';
			} else {
				menumargin = * minedopt - '0';
			}
		} else if (* minedopt == 'Q') {
			explicit_selection_style = command_line;
			use_stylish_menu_selection = True;
		} else if (* minedopt == 'q') {
			explicit_selection_style = command_line;
			use_stylish_menu_selection = False;
		} else if (* minedopt == 'a') {
			explicit_border_style = command_line;
			use_ascii_graphics = True;
			use_vt100_block_graphics = False;
			menu_border_style = 'r';
		} else if (* minedopt == 'v') {
			explicit_border_style = command_line;
			use_vt100_block_graphics = True;
			use_ascii_graphics = False;
		} else if (* minedopt == 'p' || * minedopt == 'P') {
			menu_border_style = * minedopt;
			explicit_border_style = command_line;
		} else if (* minedopt == 'b') {
			full_menu_bg = False;
		} else if (* minedopt == 'B') {
			full_menu_bg = True;
			if (menu_border_style == '@') {
				menu_border_style = 'p';
			}
		} else {
			menu_border_style = * minedopt;
			if (menu_border_style != 'd') {
				explicit_border_style = command_line;
			}
			if (menu_border_style == '@') {
				full_menu_bg = False;
			}
		}
		break;
	case 'O':
		use_script_colour = plus_opt;
		break;
	case 'f':
		if (menu_border_style == 's') {
			if (use_vt100_block_graphics) {
				use_ascii_graphics = True;
				menu_border_style = 'r';	/* by option */
				use_vt100_block_graphics = False;
			} else {
				use_vt100_block_graphics = True;
			}
		} else {
			menu_border_style = 's';
		}
		use_stylish_menu_selection = False;
		fine_scrollbar = False;
		break;
	case 'F':
		if (plus_opt) {
			if (very_limited_marker_font) {
				very_limited_marker_font = False;
			} else {
				limited_marker_font = False;
			}
		} else {
			if (limited_marker_font) {
				very_limited_marker_font = True;
			} else {
				limited_marker_font = True;
			}
		}
		break;
	case 'o':
		minedopt ++;
		explicit_scrollbar_style = command_line;
		switch (* minedopt) {
		case '0':
			disp_scrollbar = False;
			scrollbar_width = 0;
			break;
		case '1':
			disp_scrollbar = True;
			fine_scrollbar = False;
			scrollbar_width = 1;
			break;
		case '2':
			disp_scrollbar = True;
			fine_scrollbar = True;
			scrollbar_width = 1;
			break;
		case '8':
			update_scrollbar_lazy = False;
			break;
		case '9':
			update_scrollbar_lazy = True;
			break;
		case 'o':
			old_scrollbar_clickmode = True;	/* old left/right click semantics */
			break;
		default:
			minedopt --;
			/* disable/enable scrollbar */
			if (plus_opt) {
				disp_scrollbar = True;
				scrollbar_width = 1;
			} else {
				disp_scrollbar = False;
				scrollbar_width = 0;
			}
		}
		break;
	case 'c':
		if (plus_opt) {
			combining_screen = True;
			combining_mode = True;
			combining_screen_selected = True;
		} else {
			if (combining_mode_disabled) {
				combining_screen = False;
			} else {
				combining_mode = False;
				combining_mode_disabled = True;
			}
		}
		break;
	case 'C':
		if (plus_opt) {
			cjk_term = True;
			/* output CJK encoded characters transparently */
			if (suppress_unknown_cjk) {
				suppress_unknown_cjk = False;
			} else if (suppress_invalid_cjk) {
				suppress_invalid_cjk = False;
			} else {
				suppress_extended_cjk = False;
			}
			/* output non-Unicode codes transparently */
			suppress_non_BMP = NOT_VALID;
			if (suppress_EF) {
				suppress_EF = False;
			} else if (suppress_surrogates) {
				suppress_surrogates = False;
			} else {
				suppress_non_Unicode = False;
			}
		} else {
			Cflag = True;
		}
		break;
	case 'U':
		if (plus_opt) {
			if (U_mode_set) {
				bidi_screen = True;
				joining_screen = True;
				poormansbidi = False;
				/* disable scrollbar to prevent interference */
				disp_scrollbar = False;
				scrollbar_width = 0;
			} else {
				utf8_screen = True;
				utf8_input = True;
				combining_screen = True;
				combining_mode = True;
				U_mode_set = True;
				cjk_term = False;
				mapped_term = False;
			}
		} else if (bidi_screen) {
			joining_screen = False;
		} else if (utf8_input) {
			utf8_screen = False;
			utf8_input = False;
			combining_screen = False;
			combining_mode = False;
		} else {
			utf8_screen = True;
			utf8_input = True;
			combining_screen = True;
			combining_mode = True;
			cjk_term = False;
			mapped_term = False;
		}
		break;
	case 'u':
		auto_detect = False;
		if (plus_opt) {
			(void) set_text_encoding (NIL_PTR, 'L', "+u");
			utf8_lineends = False;
		} else {
			(void) set_text_encoding (NIL_PTR, 'U', "-u");
			text_encoding_selected = True;
		}
		break;
	case 'l':
		auto_detect = False;
		(void) set_text_encoding (NIL_PTR, 'L', "-l");
		text_encoding_selected = True;
		break;
	case 'E':
		minedopt ++;
		if (* minedopt == '?') {
			if (plus_opt) {
				only_detect_terminal_encoding = True;
			} else {
				only_detect_text_encoding = True;
			}
		} else if (* minedopt == 'u') {
			pastebuf_utf8 = True;
		} else if (* minedopt == 'U' || * minedopt == 'L') {
			if (plus_opt) {
				cjk_term = False;
				mapped_term = False;
				if (* minedopt == 'U') {
					utf8_screen = True;
					utf8_input = True;
					combining_screen = True;
					combining_mode = True;
				} else {
					utf8_screen = False;
					utf8_input = False;
					combining_screen = False;
					combining_mode = False;
				}
				cjk_term = False;
				mapped_term = False;
				term_encoding_selected = True;
			} else {
				if (set_text_encoding (NIL_PTR, * minedopt, "-EU/L")) {
					auto_detect = False;
					text_encoding_selected = True;
				}
			}
		} else if (* minedopt == '.') {
			minedopt ++;
			if (handle_locale_encoding (plus_opt, minedopt)) {
				if (plus_opt) {
					term_encoding_selected = True;
				} else {
					auto_detect = False;
					text_encoding_selected = True;
				}
			} else {
				if (plus_opt) {
					errprintf ("Unknown terminal encoding %s\n", minedopt);
				} else {
					errprintf ("Unknown text encoding %s\n", minedopt);
				}
				sleep (1);

			}
			while (* minedopt) {
				minedopt ++;
			}
		} else if (* minedopt == '=') {
			minedopt ++;
			if (set_charmap (plus_opt, minedopt)) {
				if (plus_opt) {
					term_encoding_selected = True;
				} else {
					auto_detect = False;
					text_encoding_selected = True;
				}
			} else {
				if (plus_opt) {
					errprintf ("Unknown terminal charmap %s\n", minedopt);
				} else {
					errprintf ("Unknown text charmap %s\n", minedopt);
				}
				sleep (1);
			}
			while (* minedopt) {
				minedopt ++;
			}
		} else if (* minedopt == ':') {
			if (set_charmap (plus_opt, minedopt)) {
				if (plus_opt) {
					term_encoding_selected = True;
				} else {
					auto_detect = False;
					text_encoding_selected = True;
				}
			} else {
				if (plus_opt) {
					errprintf ("Unknown terminal encoding flag %s\n", minedopt + 1);
				} else {
					errprintf ("Unknown text encoding flag %s\n", minedopt + 1);
				}
				sleep (1);
			}
			minedopt ++;
			if (* minedopt) {
				minedopt ++;
			}
			if (* minedopt) {
				minedopt ++;
			}
		} else if (* minedopt == '-' || * minedopt == '\0') {
			if (! plus_opt) {
				auto_detect = False;
			}
		} else {
			char enctag = * minedopt;
			switch (enctag) {
			case 'g':	if (plus_opt || Cflag) {
						gb18030_term = False;
						suppress_extended_cjk = True;
					}
					enctag = 'G';
					break;
			case 'j':	if (plus_opt || Cflag) {
						euc3_term = False;
						suppress_extended_cjk = True;
					}
					enctag = 'J';
					break;
			case 'c':	if (plus_opt || Cflag) {
						euc4_term = False;
						suppress_extended_cjk = True;
					}
					enctag = 'C';
					break;
			}
			if (plus_opt || Cflag) {
				if (set_term_encoding (NIL_PTR, enctag)) {
					term_encoding_selected = True;
				} else {
					errprintf ("Unknown terminal encoding tag %c\n", * minedopt);
					sleep (1);
					break;
				}
			}
			if (! plus_opt) {
				if (set_text_encoding (NIL_PTR, enctag, "-E")) {
					auto_detect = False;
					text_encoding_selected = True;
				} else {
					errprintf ("Unknown text encoding tag %c\n", * minedopt);
					sleep (1);
					break;
				}
			}
		}
		break;
	case 'b':
		if (plus_opt) {
			minedopt ++;
			if (* minedopt == '-' || * minedopt == 'o') {
				backup_mode = '\0';
			} else if (* minedopt == '\0') {
				backup_mode = 'a';
			} else if (* minedopt == '=') {
				minedopt ++;
				if (minedopt [0] == '~'
					&& (minedopt [1] == '/' || minedopt [1] == '\0')) {
					char expandedname [maxFILENAMElen];
					strcpy (expandedname, "");
					strappend (expandedname, gethomedir (), maxFILENAMElen);
					minedopt ++;
					strip_trailingslash (expandedname);
					strappend (expandedname, minedopt, maxFILENAMElen);
					backup_directory = dupstr (expandedname);
				} else {
					backup_directory = dupstr (minedopt);
				}
				return;
			} else {
				backup_mode = * minedopt;
			}
		} else {
			if (poormansbidi) {
				poormansbidi = False;
			} else {
				poormansbidi = True;
				bidi_screen = False;
			}
		}
		break;
	case 'z':
		/* file chooser sort options */
		minedopt ++;
		if (* minedopt == 'x') {
			sort_by_extension = plus_opt;
		} else if (* minedopt == 'd') {
			sort_dirs_first = plus_opt;
		}
		break;
	case 'B':
		if (plus_opt) {
			minedopt ++;
			if (* minedopt == 'p') {
				plain_BS = True;
			}
		} else {
			/*
			key_map ['\010'] = DPC;
			key_map ['\177'] = DCC;
			*/
			key_map ['\010'] = LFkey;
			key_map ['\177'] = DPC;
		}
		break;
	case 'K':
		if (plus_opt) {
			/* obsolete since 2000.15 as this is the default */
			enforce_keymap = True;
		} else {
			minedopt ++;
			if (* minedopt == '=') {
				minedopt ++;
				if (setKEYMAP (minedopt)) {
					explicit_keymap = True;
				} else {
					errprintf ("Unknown input method %s\n", minedopt);
					sleep (1);
				}
				return;
			} else {
				selection_space = * minedopt;
			}
		}
		break;
	case '[':
		rectangular_paste_flag = plus_opt;
		break;
	case 'D':
		configure_xterm_keyboard = plus_opt;
		break;
	case 'w':
		if (wordnonblank) {
			wordnonblank = False;
		} else {
			wordnonblank = True;
		}
		break;
	case 'Z':
		minedopt ++;
		switch (* minedopt) {
		case '_':
			strop_style = * minedopt;
			break;
		case 'Z':
			if (plus_opt) {
				lowcapstrop = True;
				dispunstrop = True;
			} else {
				lowcapstrop = False;
				dispunstrop = False;
			}
			strop_selected = True;
			break;
		}
		break;
	case 'q':
		smart_quotes = VALID;
		minedopt ++;
		if (* minedopt == '=' || * minedopt == ':') {
			minedopt ++;
			if (* minedopt >= 'A' && * minedopt <= 'z') {
				handle_locale_quotes (minedopt, plus_opt);
			} else if (* minedopt) {
				set_quote_style (minedopt);
				default_quote_type = quote_type;
			} else {
				handle_locale_quotes (NIL_PTR, plus_opt);
			}
			return;
		} else {
			minedopt --;
			handle_locale_quotes (NIL_PTR, plus_opt);
		}
		break;
	case 'a':
		append_flag = True;
		break;
	case 'j':
		if (plus_opt) {
			minedopt ++;
			if (* minedopt == '=') {
				minedopt ++;
				if (* minedopt >= '0' && * minedopt <= '2') {
					JUSlevel = * minedopt - '0';
				}
			} else {
				minedopt --;
				if (JUSlevel < 2) {
					JUSlevel ++;
				}
			}
		} else if (JUSlevel == 1) {
			JUSlevel = 2;
		} else {
			JUSlevel = 1;
		}
		break;
	case 'J':
		JUSlevel = 2;
		break;
	case 'T':
		if (plus_opt) {
			tab_right = True;
		} else {
			tab_left = True;
		}
		break;
	case 'V':
		minedopt ++;
		if (* minedopt == ':' || * minedopt == '=') {
			minedopt ++;
			if (* minedopt == 'k') {
				visselect_keeponsearch = plus_opt;
			} else if (* minedopt == 'c') {
				visselect_autocopy = plus_opt;
				visselect_copydeselect = plus_opt;
			}
		} else {
			minedopt --;
			if (plus_opt) {
				if (paste_stay_left) {
					paste_stay_left = False;
				} else {
					emacs_buffer = True;
				}
			} else {
				if (! paste_stay_left) {
					paste_stay_left = True;
				} else {
					emacs_buffer = False;
				}
			}
		}
		break;
	case 's':
		page_stay = True;
		break;
	case 'S':
		page_scroll = True;
		break;
	case 'x':
		xprot = exeprot;
		break;
	case 'X':
		use_window_title_query = use_window_title;
		use_window_title = plus_opt;
		break;
	case 'd':
		minedopt ++;
		if (* minedopt == '-') {
			display_delay = -1;
		} else if (* minedopt >= '0' && * minedopt <= '9') {
			display_delay = (int) * minedopt - (int) '0';
		} else {
			minedopt --;
		}
		break;
	case '':
		minedopt ++;
		if (* minedopt >= '0' && * minedopt <= '9') {
			cursor_style = (int) * minedopt - (int) '0';
		} else {
			minedopt --;
		}
		break;
	case '4':
		if (! tabsize_selected) {
			tabsize = 4;
			tabsize_selected = cmdline_selected;
			expand_tabs = plus_opt;
		}
		break;
	case '2':
		if (! tabsize_selected) {
			tabsize = 2;
			tabsize_selected = cmdline_selected;
			expand_tabs = plus_opt;
		}
		break;
	case '8':
		if (! tabsize_selected) {
			tabsize = 8;
			tabsize_selected = cmdline_selected;
			expand_tabs = plus_opt;
		}
		break;
	case '?':
		minedopt ++;
		if (* minedopt == 'c') {	/* enable char info */
			if (plus_opt) {
				always_disp_code = True;
				always_disp_fstat = False;
				if (! disp_Han_full) {
					always_disp_Han = False;
				}
			} else {
				always_disp_code = False;
			}
		} else if (* minedopt == 's') {	/* enable char/script info */
			if (plus_opt) {
				disp_scriptname = True;
				/* also enable char info: */
				always_disp_code = True;
				always_disp_fstat = False;
				if (! disp_Han_full) {
					always_disp_Han = False;
				}
			} else {
				disp_scriptname = False;
			}
		} else if (* minedopt == 'n') {	/* enable char/name info */
			if (plus_opt) {
				disp_charname = True;
				/* also enable char info: */
				always_disp_code = True;
				always_disp_fstat = False;
				if (! disp_Han_full) {
					always_disp_Han = False;
				}
			} else {
				disp_charname = False;
			}
		} else if (* minedopt == 'q') {	/* enable named seq info */
			if (plus_opt) {
				disp_charseqname = True;
				/* also enable char info: */
				always_disp_code = True;
				always_disp_fstat = False;
				if (! disp_Han_full) {
					always_disp_Han = False;
				}
			} else {
				disp_charseqname = False;
			}
		} else if (* minedopt == 'd') {	/* enable char/decomposition info */
			if (plus_opt) {
				disp_decomposition = True;
				disp_charname = False;
				/* also enable char info: */
				always_disp_code = True;
				always_disp_fstat = False;
				if (! disp_Han_full) {
					always_disp_Han = False;
				}
			} else {
				disp_decomposition = False;
			}
		} else if (* minedopt == 'm') {	/* enable char/menmos info */
			if (plus_opt) {
				disp_mnemos = True;
				disp_charname = False;
				/* also enable char info: */
				always_disp_code = True;
				always_disp_fstat = False;
				if (! disp_Han_full) {
					always_disp_Han = False;
				}
			} else {
				disp_mnemos = False;
			}
		} else if (* minedopt == 'f') {	/* enable file info */
			if (plus_opt) {
				always_disp_fstat = True;
				always_disp_code = False;
				if (! disp_Han_full) {
					always_disp_Han = False;
				}
			} else {
				always_disp_fstat = False;
			}
		} else if (* minedopt == 'h') {	/* enable popup Han info */
			if (plus_opt) {
				always_disp_Han = True;
				disp_Han_full = True;
			} else {
				disp_Han_full = False;
			}
		} else if (* minedopt == 'x') {	/* enable short Han info */
			if (plus_opt) {
				always_disp_Han = True;
				disp_Han_full = False;
				always_disp_fstat = False;
				always_disp_code = False;
			} else {
				always_disp_Han = False;
			}
		}
		break;
	case '#':
		dark_term = plus_opt;
		break;
	case 'W':
		emul_WordStar ();
		break;
	case 'e':
		if (plus_opt) {
			minedopt ++;
			switch (* minedopt) {
			case 'e':	emul_emacs (); break;
			case 's':	emul_WordStar (); break;
			case 'p':	emul_pico (); break;
			case 'W':	newfile_lineend = lineend_CRLF;
					prefer_comspec = True;
					/*FALLTHROUGH*/
			case 'w':	emul_Windows (); break;
			case 'm':	emul_mined (); break;
			default:	errprintf ("Unknown emulation mode %c\n", * minedopt);
					sleep (1);
			}
		} else {
			emul_emacs ();
		}
		break;
	case 'k':
		if (plus_opt) {
			use_appl_keypad = True;
		} else {
			minedopt ++;
			switch (* minedopt) {
			case 'c':	small_home_selection = True;
					break;
			case 'C':	home_selection = True;
					break;
			case 'm':	set_keypad_mined ();
					break;
			case 's':
			case 'S':	set_keypad_shift_selection ();
					break;
			case 'w':	set_keypad_windows ();
					break;
			default:	minedopt --;
					if (mined_keypad) {
						mined_keypad = False;
					} else {
						mined_keypad = True;
					}
			}
		}
		break;
	case 't':	/* deprecated; 2012.21 compatibility */
		if (plus_opt) {
			if (shift_selection == True) {
				set_keypad_shift_selection ();
			} else {
				set_keypad_windows ();
			}
		} else {
			set_keypad_mined ();
		}
		break;
	case 'n':
		minedopt ++;
		if (* minedopt == 'c') {
			suppress_colour = True;
			bw_term = True;
		}
		break;
	case '':
		/* viewing mined help file */
		viewing_help = True;	/* also triggers mark_HTML = True */
		viewonly_mode = True;
		restricted = True;
		ext_status = ++ minedopt;
		return;
	case '_':
		ext_status = ++ minedopt;
		return;
	case '/':
		minedopt ++;
		inisearch = minedopt;
		return;
	case '':
		minedopt ++;
		if (* minedopt) {
			filtering_read = plus_opt;
			free_space (filter_read);
			filter_read = dupstr (minedopt);
		}
		return;
	case '':
		minedopt ++;
		if (* minedopt) {
			filtering_write = plus_opt;
			free_space (filter_write);
			filter_write = dupstr (minedopt);
		}
		return;
	default:
		errprintf ("Unknown option %c\n", * minedopt);
		if (command_line) {
			sleep (1);
		}
		break;
    }
    if (* minedopt != '\0') {
	minedopt ++;
    }
  }
}


static int mined_argc;
static char * * mined_argv;
static int initlini = 0;

static
int
eval_commandline ()
{
  char * minedopt;
  int argi = 1;
  FLAG goon = True;

  if (mined_argv [0]) {
	minedprog = mined_argv [0];
	minedopt = strrchr (mined_argv [0], '/');
	if (minedopt) {
		minedopt ++;
	} else {
		minedopt = mined_argv [0];
	}
	if (strisprefix ("r", minedopt)) {
		restricted = True;
		minedopt ++;
	}
	if (strisprefix ("minma", minedopt)) {
		eval_options ("-e", False);
	} else if (strisprefix ("mstar", minedopt)) {
		eval_options ("-W", False);
	} else if (strisprefix ("mpico", minedopt)) {
		emul_pico ();
	}
  } else {
	minedprog = "mined";
  }

  do {
	if (argi < mined_argc) {
		if (streq (mined_argv [argi], "++")) {
			argi += 1;
			goon = False;
		} else if (* mined_argv [argi] == '+' && mined_argv [argi] [1] >= '0' && mined_argv [argi] [1] <= '9') {
			initlini = argi;
			argi += 1;
		} else if (* mined_argv [argi] == '-'
			      || * mined_argv [argi] == '+'
#ifdef msdos
			      || * mined_argv [argi] == '/'
#endif
			      )
		{
			minedopt = mined_argv [argi];
			eval_options (minedopt, True);
			argi += 1;
		} else {
			goon = False;
		}
	} else {
		goon = False;
	}
  } while (goon);

  return argi;
}


#ifdef __GNUC__
#define mempoi void *
#else
/* most non-GNU C compilers, following standard C, do not permit 
   pointer arithmetic on void *
 */
#define mempoi char *
#endif

static mempoi prefbuf = 0;
static mempoi prefpoi;
static int prefsaving;
static int preflen = 200;

#define dont_debug_preferences

#ifdef debug_preferences
#define trace_pref(params)	printf params
#else
#define trace_pref(params)	
#endif

static
void
#ifdef debug_preferences
do_prefmov (varpoi, varsize, varname)
  void * varpoi;
  int varsize;
  char * varname;
#else
do_prefmov (varpoi, varsize)
  void * varpoi;
  int varsize;
#endif
{
  if (prefsaving) {
	if (prefpoi - prefbuf + varsize > preflen) {
#define prefinc 50
		mempoi newbuf = realloc (prefbuf, preflen + prefinc);
		if (newbuf) {
			trace_pref (("realloc %p->%p (%d)\n", prefbuf, newbuf, preflen + prefinc));
			prefpoi = newbuf + (prefpoi - prefbuf);
			prefbuf = newbuf;
			preflen += prefinc;
		} else {
			trace_pref (("realloc failed\n"));
			return;
		}
	}
	memcpy (prefpoi, varpoi, varsize);
	trace_pref (("%p:%04X <- %s (%d)\n", prefpoi, * (unsigned int *) prefpoi, varname, varsize));
	prefpoi += varsize;
  } else {
	trace_pref (("%p:%04X -> %s (%d)\n", prefpoi, * (unsigned int *) prefpoi, varname, varsize));
	memcpy (varpoi, prefpoi, varsize);
	prefpoi += varsize;
  }
}

#ifdef debug_preferences
#define prefmov(var)	do_prefmov (& var, sizeof (var), #var)
#else
/* stringification (#var) not supported on various systems (e.g. HPUX) */
#define prefmov(var)	do_prefmov (& var, sizeof (var))
#endif

static
void
preferences (save_restore)
  FLAG save_restore;
{
  prefsaving = save_restore != REVERSE;

  if (prefsaving && ! prefbuf) {
	prefbuf = alloc (preflen);
  }
  if (! prefbuf) {
	return;
  }

  prefpoi = prefbuf;

  prefmov (restricted);	/* just to be safe (cannot be toggled) */
  prefmov (use_file_tabs);
  prefmov (backup_mode);

  prefmov (smart_quotes);
  prefmov (quote_type);
  prefmov (prev_quote_type);
  prefmov (wordnonblank);
  prefmov (JUSlevel);
  prefmov (JUSmode);
  prefmov (mark_HTML);
  prefmov (mark_JSP);
  prefmov (viewonly_mode);
  prefmov (lowcapstrop);
  prefmov (dispunstrop);
  prefmov (strop_style);
  prefmov (autoindent);
  prefmov (autonumber);
  prefmov (backnumber);
  prefmov (backincrem);
  prefmov (tabsize);
  prefmov (tabsize_selected);
  prefmov (expand_tabs);
  prefmov (hide_passwords);
  prefmov (filtering_read);
  prefmov (filtering_write);

  trace_pref (("preferences (saving %d) length %d\n", prefsaving, prefpoi - prefbuf));
}

static struct {
	char * name;
	char * eval;
} config_options [] = {
	{"tab",		"-%s"},
	{"tabstop",	"-%s"},
	{"tabwidth",	"-%s"},
	{"tabexpand",	"-+%s"},
	{"rectangular",	"+["},
	{"square",	"+["},
	{"emul",	"+e%s"},
	{"keypad_windows",		"-kw"},
	{"keypad_shift_selection",	"-kS"},
	{"keypad_mined",		"-km"},
	{"home_selection",		"-kM"},
	{"small_home_selection",	"-km"},
	{"im_space",	"-K%s"},
	{"im",		"-K=%s"},
	{"strop_bold",		"+ZZ"},
	{"strop_underline",	"+ZZZ_"},
	{"unicode_paste_buffer",	"-Eu"},
	{"separated_display",	"-c"},
	{"backup_mode",		"+b%s"},
	{"backup_directory",	"+b=%s"},
	{"file_chooser",	"+z%s"},
	{"file_tabs",		"+M:"},
	{"tab_snap",		"%sT"},
	{"justify",		"+j=%s"},
	{"display_mac",		"+R"},
	{"display_paragraphs",	"-p"},
	{"page_stay",	"-s"},
	{"paste_stay",	"-V"},
	{"delete_to_buffer",	"+VV"},
	{"no_window_title",	"-X"},
	{"word_skip",	"-w"},
	{"smart_quotes",	"-q=%s"},
	{"highlight_html",	"+H"},
	{"highlight_xml",	"+H"},
	{"cursor_style",	"-%s"},
	{"hide_passwords",	"-P"},
	{"show_passwords",	"+P"},
	{"sel_keep_on_search",	"+V:k"},
	{"sel_auto_copy",	"+V:c"},
	{"display_charinfo",	"+?c"},
	{"no_display_charinfo",	"-?c"},
	{"display_script",	"+?s"},
	{"display_charname",	"+?n"},
	{"display_namedseq",	"+?q"},
	{"display_decomposition",	"+?d"},
	{"display_mnemos",	"+?m"},
	{"display_fileinfo",	"+?f"},
	{"display_haninfo_short",	"+?x"},
	{"display_haninfo_full",	"+?h"},
	{"plain_BS",		"+Bp"},
	{"filter_read",		"+%s"},
	{"filter_write",	"+%s"},
	{"menu_corners_simple",	"-Qs"},
	{"menu_corners_round",	"-Qr"},
	{"menu_borders_fat",	"-Qf"},
	{"menu_borders_double",	"-Qd"},
	{"menu_borders_ascii",	"-Qa"},
	{"menu_borders_vt100",	"-Qv"},
	{"menu_borders_plain",	"-Qp"},
	{"menu_borders_none",	"-QP"},
	{"menu_background",	"-QB"},
	{"menu_selection_fancy",	"-QQ"},
	{"menu_selection_plain",	"-Qq"},
	{"splash_logo",	"+Q%s"},
	{NIL_PTR}
};

/**
   Read and evaluate mined runtime configuration file.
   applyall = True: initially evaluate common options
              False: apply conditional options only
   Preferences management concept:
	initially:
		configure_preferences (True);	... calling eval_options
		... check various environment preferences
		(void) eval_commandline ();	... overriding preset values
		preferences (FORWARD);		... saving preferences
	load_file:
		update_file_name:
			configure_preferences (False); ... restoring initial preferences first
				preferences (REVERSE); ... restore saved preferences
				... read file name specific preferences
		get_open_pos	... override saved file preferences
		set_file_type_flags ()
			... deriving generic file specific preferences
		configure_preferences (OPEN);	... restore command line options
			(void) eval_commandline ();
	However, the latter does not work as long as there are 
	cumulative options, i.e. options that have different effect 
	if applied multiple times; thus, for now, specific checks 
	are still needed (e.g. tabsize_selected).
	This applies to the following options:
		M
		*
		f
		F
		c
		C
		U
		-b
		w
		-j
		V
		+t
		-k (plain)
	and possibly to the following guarding flags:
		tabsize_selected
		strop_selected
		explicit_keymap
		explicit_border_style
		explicit_scrollbar_style
		explicit_selection_style
		default_quote_type
		preselect_quote_style
		default_text_encoding
		text_encoding_selected
		default_lineend
		homedir_selected
 */
#define dont_debug_confpref
void
configure_preferences (applycommon)
  FLAG applycommon;
{
  char rcfn [maxFILENAMElen];
  int rc_fd;
  FLAG condition = applycommon;
  char pref_name [maxFILENAMElen];	/* filename to check */
  strcpy (pref_name, file_name);

  /* mark subsequent selections to be preconfigured (e.g. .minedrc) */
  cmdline_selected = False;

  if (applycommon == OPEN) {
#ifdef apply_cmdline_options_again
#warning implementation incomplete, some options may cumulate
	/* recover command line options */
	(void) eval_commandline ();
#endif
	return;
  } else if (! applycommon) {
	/* restore initial preferences */
	preferences (REVERSE);
  }

  strcpy (rcfn, gethomedir ());
#ifdef vms
  strappend (rcfn, ".minedrc", maxFILENAMElen);
#else
  strip_trailingslash (rcfn);
  strappend (rcfn, "/.minedrc", maxFILENAMElen);
#endif
  rc_fd = open (rcfn, O_RDONLY | O_BINARY, 0);

  if (rc_fd >= 0) {
	static off_t seek_position = 0;
	int dumlen;

	if (! applycommon) {
		if (lseek (rc_fd, seek_position, SEEK_SET) < 0) {
			/* ignore, just read from beginning */
		}
	}

	reset_get_line (False);
	while (line_gotten (get_line (rc_fd, text_buffer, & dumlen, False))) {
		long linelen = strlen (text_buffer);
		if (text_buffer [0] == '[') {
			/* determine condition to apply configuration options */
			char * condopt = strrchr (text_buffer, ']');
			if (condopt) {
				* condopt = '\0';
			}
			condition = False;
			/* check if considering common or specific options */
			if (applycommon) {
				break;
			} else {
				/* check specific preferences filter */
				condopt = & text_buffer [2];
				if (text_buffer [1] == '=') {
					/* filter terminal type */
					if (streq (condopt, TERM)) {
						condition = True;
					}
#ifdef pref_match_only_suffix
				} else if (text_buffer [1] == '.') {
					/* filter filename suffix */
					char * suf = strstr (pref_name, condopt);
					if (suf && strlen (suf) == strlen (condopt)) {
						condition = True;
					}
#endif
				} else if (wildcard (& text_buffer [1], pref_name)) {
					condition = True;
				}
#ifdef debug_confpref
				printf ("conf %d %s\n", condition, text_buffer);
#endif
			}
		} else if (condition && text_buffer [0] > '#') {
			char * option = text_buffer;
			char * negoption = NIL_PTR;
			char * optval;
			character * optpoi;	/* unsigned ! */
			char optvalsep = ' ';

			if (* option == '-') {
				option ++;
			} else if (strisprefix ("set", option) && option [3] <= ' ') {
				option += 3;
				while (* option == ' ' || * option == '\t') {
					option ++;
				}
			} else if (strisprefix ("unset", option) && option [5] <= ' ') {
				option += 5;
				while (* option == ' ' || * option == '\t') {
					option ++;
				}
				negoption = option;
			}
			/*option [strlen (option) - 1] = '\0';*/
			if (strisprefix ("no_", option)) {
				if (negoption) {
					negoption = NIL_PTR;
					option += 3;
				} else {
					negoption = option + 3;
				}
			}

			/* skip option name, terminate it with NUL */
			optpoi = option;
			while (* optpoi > ' ' && * optpoi != '=') {
				if (* optpoi == '-') {
					* optpoi = '_';
				}
				optpoi ++;
			}
			while (* optpoi == ' ' || * optpoi == '\t' || * optpoi == '=') {
				* optpoi ++ = '\0';
			}

			/* check option value delimiter */
			if (* optpoi == '"' || * optpoi == '\'') {
				optvalsep = * optpoi;
				optpoi ++;
			}

			/* option value starts here */
			optval = optpoi;

			/* skip option value, terminate it with NUL */
			while (* optpoi >= ' ' && * optpoi != optvalsep) {
				optpoi ++;
			}
			* optpoi = '\0';

#ifdef debug_confpref
			printf ("conf %s=%s\n", option, optval);
#endif
			if (streq (option, "<<")) {
				strncpy (pref_name, optval, maxFILENAMElen);
				pref_name [maxFILENAMElen - 1] = '\0';
			} else {
			    int opt_i = 0;
			    while (config_options [opt_i].name != NIL_PTR) {
				if (streq (option, config_options [opt_i].name)) {
					char eval [maxCMDlen];
					build_string (eval, config_options [opt_i].eval, optval);
#ifdef debug_confpref
					printf ("     ->%s\n", eval);
#endif
					eval_options (eval, False);
					if (strisprefix ("filter_", option)) {
						char * suf = strrchr (pref_name, '.');
						if (suf) {
							* suf = '\0';
						}
					}
					break;
				} else if (negoption
					&& streq (negoption, config_options [opt_i].name)
					&& config_options [opt_i].eval [0] == '+'
					) {
					char eval [maxCMDlen];
					build_string (eval, config_options [opt_i].eval, optval);
					eval [0] = '-';
#ifdef debug_confpref
					printf ("     ->%s\n", eval);
#endif
					eval_options (eval, False);
					break;
				}
				opt_i ++;
			    }
			}
		}
		if (applycommon) {
			seek_position += linelen;
		}
	}
	(void) close (rc_fd);
	clear_filebuf ();	/* which needs to call clear_get_line too */
  }
}


/**
   configuration and initialisation;
   load the file (if any);
   main loop of the editor
 */
int
main (argc, argv)
  int argc;
  char * argv [];
{
  int argi;
  unsigned long inputchar;
  int initlinenum;
  LINE * initline;
  char * env;
  char * minedopt;

  debug_mined = envvar ("DEBUG_MINED");
  TERM = envstr ("TERM");

  mined_argc = argc;
  mined_argv = argv;

/* determine various modes and display options from environment settings */
  if (envvar ("NoCtrlSQ") || envvar ("NoControlSQ")) {
	/* ^S and ^Q may come arbitrarily from terminal, so don't use them */
	controlQS = True;
	key_map ['\021'] = I;
	key_map ['\023'] = I;
  }

  transout = envstr ("MINEDOUT");
  if (* transout != '\0') {
	translen = strlen (transout);
	translate_output = True;
	use_ascii_graphics = True;
	menu_border_style = 'r';
  }
#ifdef unix
  if (envvar ("MINEDMODF")) {
	mined_modf = envvar ("MINEDMODF");
  }
#endif
#ifdef pc_winlowdelay
  if (is_Windows ()) {
	display_delay = 0;
  }
#endif

#ifdef debug_graphics
  printf ("(mn) ascii %d, vga %d, vt100 %d\n", use_ascii_graphics, use_vga_block_graphics, use_vt100_block_graphics);
#endif


/* derive screen mode and default text encoding from environment */

#ifdef __CYGWIN__
/* check misconfigured cygwin console and ignore $TERM if detected;
	if $WINDIR is in environment rather than $windir,
	there has been an underlying cygwin instance 
	before spawning the console and thus the value of $TERM is bogus
	*/
  if (strisprefix ("/dev/cons", unnull (ttyname (2))) && envvar ("WINDIR")) {
	TERM = "cygwin";
  }
#endif

  if (streq (TERM, "cygwin")) {
	cygwin_console = True;
  }

/* detect PC terminal modes for native PC and for telnet from PC to Unix */
#ifdef pc_term
  if (envvar ("USERPROFILE")) {
	(void) set_term_encoding ("CP850", 'P');
  } else {
	(void) set_term_encoding ("CP437", 'p');
  }
#endif
  if (strisprefix ("nansi", TERM)
	|| strisprefix ("ansi.", TERM)
	|| strisprefix ("opennt", TERM)
	|| strcontains (TERM, "-emx")
	|| strisprefix ("interix", TERM)
     ) {
#ifdef __CYGWIN__
	TERM = "cygwin";
#else
	dark_term = True;
	(void) set_term_encoding ("CP437", 'p');
	term_encoding_selected = True;
	if (! text_encoding_selected) {
		(void) set_text_encoding ("CP850", 'P', "TERM=nansi");
	}
	use_vga_block_graphics = True;
	use_mouse_button_event_tracking = False;
#endif
  } else if (strisprefix ("pcansi", TERM)
		|| strisprefix ("hpansi", TERM)
		|| streq ("ansi", TERM)
		|| streq ("ansi-nt", TERM)
     ) {
#ifdef __CYGWIN__
	TERM = "cygwin";
#else
	dark_term = True;
	(void) set_term_encoding ("CP850", 'P');
	term_encoding_selected = True;
	if (! text_encoding_selected) {
		(void) set_text_encoding ("CP850", 'P', "TERM=pcansi");
	}
	use_vga_block_graphics = True;
	use_mouse_button_event_tracking = False;
#endif
  }
#ifdef __CYGWIN__
  {
	char * path = envstr ("PATH");
	char * p = strstr (path, "WindowsPowerShell");
	if (p) {
		char * sys = strstr (path, "%SystemRoot%");
		if (sys && sys + 22 == path) {
			/* Windows PowerShell */
			TERM = "cygwin";
			dark_term = True;
		}
	}
  }
#endif
  if (strisprefix ("ansi", TERM)) {
	can_report_props = False;
	use_ascii_graphics = True;
	menu_border_style = 'r';
  }
  if (strisprefix ("Eterm", TERM)) {
	bright_term = True;
  }

/* check if cygwin console is configured to be in PC character set mode */
  if (streq (TERM, "cygwin")) {
	can_report_props = False;
	dark_term = True;
	/* avoid_reverse_colour = True; workaround obsolete */
	use_script_colour = False;
#ifdef msdos
	/* if the djgpp version of mined is called from a cygwin shell,
	   it will still see TERM=cygwin but the cygwin encoding emulation 
	   is not applicable (it's done by the cygwin DLL, not the terminal);
	   djgpp mined assumes CP850 terminal "codepage" by default,
	   may be overridden by codepage detection below
	 */
	(void) set_term_encoding ("CP850", 'P');
#else
	if (strcontains (envstr ("CYGWIN"), "codepage:oem")) {
		/* CP850 */
		(void) set_term_encoding ("CP850", 'P');
		term_encoding_selected = True;
	} else if (strcontains (envstr ("CYGWIN"), "codepage:utf8")) {
		(void) set_term_encoding (NIL_PTR, 'U');
		term_encoding_selected = True;
	} else {
		use_pc_block_graphics = True;
# ifdef __CYGWIN__
#  if CYGWIN_VERSION_DLL_MAJOR < 1007	/* avoid langinfo for MSYS */
#   if CYGWIN_VERSION_DLL_MAJOR < 1005	/* MSYS */
			use_ascii_graphics = True;
			menu_border_style = 'r';
#   endif
			if (! set_term_encoding (":cw", ' ')) {
				(void) set_term_encoding ("CP1252", 'W');
			}
#  else
			if (setlocale (LC_CTYPE, "")) {
				(void) set_term_encoding (nl_langinfo (CODESET), ' ');
				term_encoding_selected = True;
			} else {
#   ifdef fallback_builtin
				(void) locale_terminal_encoding ();
				if (streq (language_code, "")) {
					(void) set_term_encoding (NIL_PTR, 'U');
				} else {
					/* leave to handle_locale_encoding below */
				}
#   else
				(void) set_term_encoding (NIL_PTR, 'U');
				term_encoding_selected = True;
#   endif
			}
#  endif
# else
		/* set default terminal encoding assumption 
		   for remote login from a cygwin console
		 */
		/* cygwin 1.5: */
		/*(void) set_term_encoding ("CP1252", 'W');*/
		/* cygwin 1.7: */
		/*(void) set_term_encoding (NIL_PTR, 'U');*/
		/* safe common fall-back: */
		(void) set_term_encoding ("ASCII", ' ');
		/* encodings should be indicated properly in environment ...
		 */
# endif
	}

	/* limit terminal to ASCII in POSIX locale */
	(void) locale_terminal_encoding ();
/* relic of early cygwin 1.7 versions:
	if (streq (language_code, "C") || streq (language_code, "POSIX")) {
		(void) set_term_encoding ("ASCII", ' ');
	}
*/

	/* enable mouse move reporting without button pressed (for menus) */
	use_mouse_anymove_inmenu = True;
	/* adjust Unicode limitations in case UTF-8 is configured later */
	/* (e.g. with LC_... after rlogin) */
	width_data_version = 0;
	utf_cjk_wide_padding = False;
	suppress_non_BMP = True;
	suppress_extended_cjk = True;
	if (limited_marker_font) {
		very_limited_marker_font = True;
	} else {
		limited_marker_font = True;
	}
	if (! explicit_scrollbar_style) {
		fine_scrollbar = False;
	}
	use_vga_block_graphics = True;

	/* workaround for unusual keypad assignments */
	if (mined_keypad) {
		mined_keypad = False;
	} else {	/* in case option -k reversed it already */
		mined_keypad = True;
	}
#endif

	if (! text_encoding_selected) {
		/*(void) set_text_encoding ("CP1252", 'W', "TERM=cygwin");*/
		/* align with changed non-locale terminal encoding */
		(void) set_text_encoding ("ISO-8859-1", 'L', "TERM=cygwin");
	}
	trace_encoding ("cygwin");

	/* numeric encoding of mouse coordinates */
	use_mouse_1015 = True;
  }

/* now derive screen mode and default text encoding from locale environment */
  if (! term_encoding_selected) {
#ifdef msdos
	char codepagename [13];
# ifdef debug_encoding
	printf ("codepage %d\n", get_codepage ());
# endif
	build_string (codepagename, "CP%d", get_codepage ());
	if (! set_term_encoding (codepagename, ' ')) {
		(void) set_term_encoding ("ASCII", ' ');
	} else if (! text_encoding_selected) {
		/* transparent editing in current codepage */
		(void) set_text_encoding (codepagename, ' ', "codepage");
	}
	if (lookup_mappedtermchar (0xB2) != 0x2593) {
		/* codepage does not include VGA block graphics */
		use_ascii_graphics = True;
		menu_border_style = 'r';
	}
	trace_encoding ("codepage");
#else
	(void) handle_locale_encoding (True, locale_terminal_encoding ());
	trace_encoding ("term locale");
#endif
  }
  if (! text_encoding_selected) {
	(void) handle_locale_encoding (False, locale_text_encoding ());
	trace_encoding ("text locale");
  }


/* fall-back/default config actions */
  handle_locale_quotes (NIL_PTR, UNSURE);


/* configuration common preferences */
  configure_preferences (True);	/* calling eval_options */


/* backup and recovery environment settings */
  if (envvar ("BACKUP_DIRECTORY")) {
	backup_directory = envvar ("BACKUP_DIRECTORY");
  }
  if (! backup_directory || backup_directory [0] == '\0') {
	if (envvar ("BACKUPDIR")) {
		backup_directory = envvar ("BACKUPDIR");
	}
  }
  if (backup_directory && backup_directory [0] == '\0') {
	backup_directory = NIL_PTR;
  }
  if (backup_directory && is_absolute_path (backup_directory)) {
	if (mkdir (backup_directory, 0700) == 0 || geterrno () == EEXIST) {
		/* backup directory OK */
	} else {
		backup_directory = NIL_PTR;
	}
  }
  if ((minedopt = envvar ("VERSION_CONTROL")) != NIL_PTR) {
	if (streq ("none", minedopt) || streq ("off", minedopt)) {
		/* never make backups (cp: "even if --backup is given") */
		backup_mode = '\0';
	} else if (streq ("numbered", minedopt) || streq ("t", minedopt)) {
		/* make numbered backups (emacs style) */
		backup_mode = 'e';
	} else if (streq ("existing", minedopt) || streq ("nil", minedopt)) {
		/* numbered if numbered backups exist, simple otherwise */
		backup_mode = 'a';
	} else if (streq ("simple", minedopt) || streq ("never", minedopt)) {
		/* always make simple backups */
		backup_mode = 's';
	}
  }
  recover_directory = envvar ("AUTO_SAVE_DIRECTORY");
  if (! recover_directory || recover_directory [0] == '\0') {
	recover_directory = envvar ("AUTOSAVEDIR");
  }
  if (recover_directory && recover_directory [0] == '\0') {
	recover_directory = NIL_PTR;
  }
  if (recover_directory && is_absolute_path (recover_directory)) {
	if (mkdir (recover_directory, 0700) == 0 || geterrno () == EEXIST) {
		/* recovery directory OK */
	} else {
		recover_directory = NIL_PTR;
	}
  }
  if (! recover_directory || recover_directory [0] == '\0') {
	recover_directory = backup_directory;
  }


/* process options configured in environment */
  minedopt = envvar ("MINEDOPT");
  if (minedopt == NIL_PTR) {
#ifdef vms
	minedopt = envvar ("MINED$OPT");
#else
	minedopt = envvar ("MINED");
#endif
  }
  if (minedopt != NIL_PTR) {
	eval_options (minedopt, False);
  }


/* set configured keyboard mapping */
  (void) setKEYMAP (envvar ("MINEDKEYMAP"));


/* mark subsequent selections to be manually selected */
  cmdline_selected = True;

/* evaluate command line options (not file names) */
  argi = eval_commandline ();
  cmdline_selected = False;

/* save initial preferences */
  preferences (FORWARD);


#ifdef debug_graphics
  printf ("(op) ascii %d, vga %d, vt100 %d\n", use_ascii_graphics, use_vga_block_graphics, use_vt100_block_graphics);
#endif
  trace_encoding ("options");

#ifdef debug_wildcard_expansion
#define trace_expansion(params)	printf params
#else
#define trace_expansion(params)	
#endif

  if (argi < argc) {
	int fi;
	for (fi = argi; fi < argc; fi ++) {
#ifdef wildcard_expansion
	    char * fnami = argv [fi];
	    FLAG match = False;
	    trace_expansion (("expanding %d <%s>\n", fi, fnami));
	    if (strchr (fnami, '*') || strchr (fnami, '?')
#ifdef vms
		|| strchr (fnami, '%')
#else
		|| (strchr (fnami, '[') && strchr (fnami, ']'))
#endif
		) {
		char * bn;
		char dn [maxFILENAMElen];
		DIR * dir;
		* dn = '\0';
		strappend (dn, fnami, maxFILENAMElen);
		bn = getbasename (dn);
		if (bn == dn) {
			strcpy (dn, "");
		} else {
			* bn = '\0';
		}
		bn = getbasename (fnami);

		trace_expansion (("dir <%s>\n", dn));
		if (* dn) {
			dir = opendir (dn);
		} else {
			dir = opendir (".");
		}
		if (dir) {
#ifdef vms
		    char * prevfn = "";
#endif
		    struct dirent * direntry;
		    while ((direntry = readdir (dir)) != 0) {
			trace_expansion (("matching <%s> in <%s>\n", bn, direntry->d_name));
			if (wildcard (bn, direntry->d_name)) {
				char * fn = alloc (strlen (dn) + strlen (direntry->d_name) + 2);

				if (! fn) {
					break;
				}
				strcpy (fn, dn);
				strcat (fn, direntry->d_name);
				trace_expansion (("-> <%s>\n", fn));

				match = True;

#ifdef vms
				/* does pattern contain version? */
				if (strchr (bn, ';')) {
					/* add match with version */
					trace_expansion ((" -> added with version\n"));
					filelist_add (fn, True);
				} else {
					/* handle []x* -> X.;1 X.;2 */

					/* strip version from match */
					char * ver = strrchr (fn, ';');
					if (ver) {
						* ver = '\0';
					}

					if (streq (prevfn, fn)) {
						/* drop duplicate versions */
						trace_expansion ((" -> dropped\n"));
					} else {
						trace_expansion ((" -> added\n"));
						filelist_add (fn, True);
					}
				}
				prevfn = fn;
#else
				filelist_add (fn, True);
#endif
			}
		    }
		    closedir (dir);
		}
		trace_expansion (("match: %d\n", match));
	    }
	    if (! match) {	/* no wildcard or no match */
		filelist_add (fnami, True);
	    }
#else
	    filelist_add (argv [fi], True);
#endif
	}
	argi = 0;
  } else {
	argi = -1;	/* indicate "no file" */
  }


/* handling of input/output redirection */
  /* Note: this must be called before terminal mode initialisation 
	(get_term)
   */
  /* Note:
     - input redirection does not work with DOS version
     - handling of input/output redirection has to be suppressed in 
       DOSBox or mined cannot start there (probably a DOSBox bug)
  */
#ifdef msdos
  if (strisprefix ("Z:\\", envstr ("COMSPEC")))
  {
	/* DOSBox detected */
  }
  else
#endif
  {
#ifndef __MINGW32__
    if (! isatty (STD_IN)) {	/* Reading from pipe */
	if (argi >= 0) {
		panic ("Cannot read both pipe and file", NIL_PTR);
	}
	reading_pipe = True;

	/* wait for pipe data available in order to prevent screen control
	   interference with previous pipe programs;
	   must be called before changing input_fd
	   and before calling terminal_configure_init ();
	   this enhances pipe robustness slightly; a better solution would 
	   read the complete pipe before tackling the screen
	 */
	(void) inputreadyafter (-1);

	/* set modified flag not to loose buffer */
	set_modified ();

	/* open terminal tty */
#ifdef msdos
	panic ("Cannot edit after input from pipe", "MSDOS incompatibility");
#else
	if ((input_fd = open ("/dev/tty", O_RDONLY, 0)) < 0) {
		panic ("Cannot open /dev/tty for reading", serror ());
	}
#endif
    }

    if (! isatty (STD_OUT)) {
	writing_pipe = True;
	set_modified (); /* Set modified flag not to ignore buffer on exit */
	if ((output_fd = open ("/dev/tty", O_WRONLY, 0)) < 0) {
		panic ("Cannot open /dev/tty for writing", serror ());
	}
    }
#endif
  }


/* setup terminal, auto-detect encoding, fine-tune modes */
  /*if (! only_detect_text_encoding)?*/
  terminal_configure_init ();


/* install workaround for cygwin console delaying some escape sequences */
  install_console_pipe ();


/* determine terminal-specific escape sequences and restrictions */
  if (strisprefix ("vt1", TERM)) {
	set_fkeymap ("vt100");
  }
  if (strisprefix ("rxvt", TERM) && mintty_version == 0) {
	set_fkeymap ("rxvt");
  }
  if (strisprefix ("hp", TERM)
   || streq ("xterm-hp", TERM)
   || strisprefix ("superbee", TERM)
   || strisprefix ("sb", TERM)
   || strisprefix ("microb", TERM)
     ) {
	if (! streq ("hpansi", TERM)) {
		set_fkeymap ("hp");
	}
	use_ascii_graphics = True;
	menu_border_style = 'r';
	use_bold = False;
  }
  if (strisprefix ("hp", TERM)) {
	bw_term = True;
  }
  if (streq ("vt52", TERM)
   || strcontains (TERM, "-vt52")
     ) {
	set_fkeymap ("52");
	(void) set_term_encoding ("ASCII", ' ');
	use_ascii_graphics = True;
	menu_border_style = 'r';
	use_bold = False;
  }
  if (strisprefix ("scoansi", TERM)
   || streq ("xterm-sco", TERM)
   || strisprefix ("cons", TERM)	/* FreeBSD console */
   || streq ("att605-pc", TERM)
   || streq ("ti_ansi", TERM)
   || streq ("mgterm", TERM)
     ) {
	set_fkeymap ("o");
  }
  if (strcontains (TERM, "koi")) {
	(void) set_term_encoding ("KOI8-R", ' ');
  }
  if (strisprefix ("cons", TERM) && strlen (TERM) >= 6) {
	/* FreeBSD console */
	if (strisprefix ("r", TERM + 6)) {
		(void) set_term_encoding ("KOI8-R", ' ');
	} else if (strisprefix ("l1", TERM + 6)) {
		(void) set_term_encoding ("ISO-8859-1", ' ');
	} else {
		(void) set_term_encoding ("CP437", 'p');
	}
  }
  if (strisprefix ("mac", TERM)) {
	(void) set_term_encoding ("MacRoman", 'M');
  }
  if (strisprefix ("nsterm", TERM)) {
	if (strcontains (TERM, "7")) {
		(void) set_term_encoding ("ASCII", ' ');
	} else if (streq ("+mac", TERM + 6)
	   || streq ("-m", TERM + 6)
	   || streq ("-m-s", TERM + 6)
	   || streq ("", TERM + 6)
	   || streq ("-c", TERM + 6)
	   || streq ("-s", TERM + 6)
	   || streq ("-c-s", TERM + 6)
	) {
		(void) set_term_encoding ("MacRoman", 'M');
	}
  }
  if (strisprefix ("9780", TERM)) {
	set_fkeymap ("siemens");
	use_ascii_graphics = True;
	menu_border_style = 'r';
  }
  if (strisprefix ("interix", TERM)) {
	set_fkeymap ("interix");
	use_mouse = False;
  }
#ifdef assume_sco_dtterm_broken_default_font
  if (streq ("dtterm", TERM)) {
	TABdefault = '.';
	RETdefault = '<';
	(void) set_term_encoding ("ASCII", ' ');
  }
#endif
#ifdef __ANDROID__
  if (streq ("screen", TERM)) {
	/* Terminal Emulator (Jack Palevich) */
	suppress_colour = True;
  }
#endif


  if (! use_bold || menu_border_style == 'f' || menu_border_style == 'd') {
	bold_border = False;
  }


/* get/update ANSI screen mode configuration */
  get_ansi_modes ();	/* also calls determine_dim_mode */


/* if request with option +E?, only report terminal encoding */
  if (only_detect_terminal_encoding) {
	/* display character data version information (for +E? option) */
	raw_mode (False);
	print_terminal_info ();
	exit (0);
  }


/* remember determined default text encoding for further files */
  default_text_encoding = get_text_encoding ();

/* read filter that defines which CJK encodings may be auto-detected */
  detect_encodings = envvar ("MINEDDETECT");
  /* if not defined, fallback to default set of auto-detected encodings */
  if (! detect_encodings) {
	detect_encodings = "BGC JSXx KH LWP";
  }


/* get preconfigured smart quotes style */
  preselect_quote_style = envvar ("MINEDQUOTES");


/* get selection of Han character information to be displayed */
  env = envvar ("MINEDHANINFO");
  if (env) {
	if (* env) {
		always_disp_Han = True;
		disp_Han_description = False;
		disp_Han_full = False;
	} else {
		always_disp_Han = False;
	}
	if (strchr (env, 'M') != NIL_PTR) {
		disp_Han_Mandarin = True;
	}
	if (strchr (env, 'C') != NIL_PTR) {
		disp_Han_Cantonese = True;
	}
	if (strchr (env, 'J') != NIL_PTR) {
		disp_Han_Japanese = True;
	}
	if (strchr (env, 'S') != NIL_PTR) {
		disp_Han_Sino_Japanese = True;
	}
	if (strchr (env, 'H') != NIL_PTR) {
		disp_Han_Hangul = True;
	}
	if (strchr (env, 'K') != NIL_PTR) {
		disp_Han_Korean = True;
	}
	if (strchr (env, 'V') != NIL_PTR) {
		disp_Han_Vietnamese = True;
	}
	if (strchr (env, 'P') != NIL_PTR) {
		disp_Han_HanyuPinlu = True;
	}
	if (strchr (env, 'Y') != NIL_PTR) {
		disp_Han_HanyuPinyin = True;
	}
	if (strchr (env, 'X') != NIL_PTR) {
		disp_Han_XHCHanyuPinyin = True;
	}
	if (strchr (env, 'T') != NIL_PTR) {
		disp_Han_Tang = True;
	}
	if (strchr (env, 'D') != NIL_PTR) {
		disp_Han_description = True;
	}
	if (strchr (env, 'F') != NIL_PTR) {
		disp_Han_description = True;
		disp_Han_full = True;
	}
  }


  setup_temp_filenames ();


/* determine default permission setting for new files */
  fprot0 = ~ umask (0) & 0666;

/* option +@: grooming .@mined */
  if (groom_stat) {
	GROOM_INFO ();
	if (argi < 0) {
		raw_mode (False);
		exit (0);
	}
  }


/* ----------------------------------------------------------------------- */

/* prepare editing buffer */
  header = tail = alloc_header (); /* Make header of list */
  if (header == NIL_LINE) {
	panic ("Cannot allocate memory", NIL_PTR);
  }
  header->text = NIL_PTR;
  header->next = tail->prev = header;
  header->syntax_mask = syntax_none;
  header->sel_begin = NIL_PTR;
  header->sel_end = NIL_PTR;

/* load the file (if any) */
  if (argi < 0) {
	if (! only_detect_text_encoding) {
		if (homedir_selected) {
			(void) chdir (gethomedir ());
		}
		load_wild_file (NIL_PTR, reading_pipe, False);
	}
  } else {
	load_wild_file (filelist_get (argi), False, False);
  }
  trace_encoding ("load");

  if (only_detect_text_encoding) {
	raw_mode (False);
	if (argi < 0) {
		printf ("%s\n", get_text_encoding ());
	} else {
		printf ("%s: %s\n", filelist_get (argi), get_text_encoding ());
		viewonly_mode = True;
		while (argi + 1 < filelist_count ()) {
			argi ++;
			raw_mode (True);
			load_wild_file (filelist_get (argi), False, True);
			raw_mode (False);
			printf ("%s: %s\n", filelist_get (argi), get_text_encoding ());
		}
	}
	exit (0);
  }

  loading = True;	/* keep loading flag True until entering main loop */

  if (initlini != 0) {
     (void) scan_int (argv [initlini] + 1, & initlinenum);
     if (initlinenum > 0) {
	if (initlinenum <= 0 || (initline = proceed (header->next, initlinenum - 1)) == tail) {
	   error2 ("Invalid line number: ", dec_out ((long) initlinenum));
	} else {
	   move_to (x, find_y_w_o_RD (initline));
	   fstatus ("Loaded", -1L, -1L);
	}
     }
  }

  if (inisearch != NIL_PTR) {
	if (search_ini (inisearch, FORWARD, True) == False) {
		sleep (1);	/* show "not found" message */
	}
	if (! viewing_help) {
		fstatus ("Loaded", -1L, -1L);
	}
  }

  if (writing_pipe) {
	file_name [0] = '\0'; /* don't let user believe he's editing a file */
	fstatus ("Editing for standard output", -1L, -1L);
  } else if (reading_pipe) {
	file_name [0] = '\0'; /* switch from [standard input] to [no file] */
  }

  loading = False;
  catch_signals ();

/* display text which was postponed for the sake of +N or +/expr options */
  display (0, top_line, last_y, 0);
  flush ();

  trace_encoding ("main");
/* main loop of the editor */
  for (;;) {
	/* FLAG cursor_somewhere = False; */
	if (last_mouse_event == releasebutton && in_selection_mouse_mode) {
		selection_mouse_mode (False);
	}
	/* update selection highlighting */
	display_flush ();
	/* ... cursor_somewhere = True; */

	/* display status line contents unless there is a message */
	if (! stat_visible) {
		if (ext_status != NIL_PTR) {
			/* interpret status text in terminal encoding? */
			/*?*/
			/* interpret status text in text file encoding? */
			/*status_fmt2 (ext_status, NIL_PTR);*/
			/* interpret status text in Unicode */
			status_uni (ext_status);
		} else if (always_disp_fstat) {
			FSTATUS ();
		} else if (always_disp_code) {
			display_the_code ();
		} else if (always_disp_Han && ! disp_Han_full) {
			display_Han (cur_text, False);
		} else if (always_disp_help) {
			display_FHELP ();
		}
	}
	if (always_disp_Han && disp_Han_full) {
		display_Han (cur_text, False);
	}

	if ((hop_flag_displayed && ! hop_flag) || (hop_flag && ! hop_flag_displayed)) {
		/* since hop_flag is often cleared on-the-fly as needed in 
		   editing functions, we need extra detection and handling
		 */
		flags_changed = True;
	}

	/* refresh display items (top line, scrollbar, cursor position) */
	if (MENU && top_line_dirty) {
		displaymenuline (True);
		flags_changed = False;
		/*cursor_somewhere = True;*/
	}
	if (text_screen_dirty) {
		RDwin ();
		text_screen_dirty = False;
	}
	if (MENU && flags_changed) {
		displayflags ();
		/*cursor_somewhere = True;*/
	}
	if (disp_scrollbar) {
		if (display_scrollbar (update_scrollbar_lazy)) {
			/*cursor_somewhere = True;*/
		}
	}
/*
	if (cursor_somewhere) {
		set_cursor_xy ();
	}
*/
	set_cursor_xy ();

	flush ();	/* Flush output (if any) */
	debuglog (0, "flush", 0);

input:

	if (key_replaying) {
		inputchar = KEYreplay ();	/* also set keyshift */
	} else {
		do {
			inputchar = readcharacter_unicode_mapped ();
			trace_keytrack ("main", inputchar);
		} while (inputchar == CHAR_UNKNOWN);
	}
	if (key_recording) {
		KEYrecord (inputchar);		/* also log keyshift */
	}

	if (command (inputchar) != Scharacter
	 && command (inputchar) != Sdash
	 && command (inputchar) != Ssinglequote
	 && command (inputchar) != Sdoublequote
	   ) {
		reset_smart_replacement ();
	}

	if (command (inputchar) != Scharacter) {
		keyproc = command (inputchar);
		inputchar = FUNcmd;
	} else {
		inputchar = encodedchar (inputchar);
	}

#ifdef adjust_to_actual_termsize
	if (command (inputchar) == ANSIseq) {
		ANSIseq ();
		goto input;
	}
#endif

	if (command (inputchar) == FOCUSin) {
#ifdef debug_mouse
		printf ("FOCUSin received\n");
#endif
		check_file_modified ();
		goto input;
	}

	if (always_disp_Han) {
		clean_menus ();
	}

	if (stat_visible) {
		clear_status ();
	}
	if (quit == False) {	/* Call the function for the typed key */
		invoke_key_function (inputchar);
		if (hop_flag > 0) {
			hop_flag --;
			flags_changed = True;
		}
		if (buffer_open_flag > 0) {
			buffer_open_flag --;
#ifdef debug_ring_buffer
			if (buffer_open_flag == 0) {
				flags_changed = True;
			}
#endif
		}
	}
	if (quit) {
		if (hop_flag > 0) {
			flags_changed = True;
		}
		CANCEL ();
		quit = False;
	}
  }
  /* NOTREACHED */
}


/*======================================================================*\
|*		End							*|
\*======================================================================*/
