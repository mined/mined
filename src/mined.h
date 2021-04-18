/***************************************************************************\
	mined text editor main include file
\***************************************************************************/

#include "_proto.h"

#define _FLAG_H


/***************************************************************************\
	system environment configuration
\***************************************************************************/

/* MSDOS basic configuration */
#ifdef __MSDOS__

#undef unix	/* needed for djgcc! */
#define msdos
#define pc
#define pc_term

# ifndef __TURBOC__
# define __GCC__

#  ifdef __strange__
#  undef msdos	/* in this weird case, a more specific distinction */
#  define unix	/* has to be made everywhere according to the 'pc' flag */
#  endif
# endif
#endif


/* system options */
#ifdef _AIX
#define _XOPEN_SOURCE_EXTENDED 1
#define _POSIX_SOURCE
#include <sys/access.h>
#endif

#ifdef __QNX__
/* prototypes getpwuid, ttyname */
#define _POSIX_SOURCE
#endif

#ifdef __minix
#define stat _stat
#define fstat _fstat
#define errprintf(msg, par)	{char errbuf [maxMSGlen]; sprintf (errbuf, msg, par); write (2, errbuf, strlen (errbuf));}
#else
#define errprintf(msg, par)	fprintf (stderr, msg, par)
#endif


/* common includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined (__TURBOC__) || defined (VAXC)
# ifdef __TURBOC__
# define off_t long
# endif
#else
#define __USE_XOPEN_EXTENDED
#include <unistd.h>
#endif


/* PC includes and declarations */
#ifdef __MINGW32__
#warning [41m mined does not compile with the MinGW compiler - use the MSYS compiler instead [0m
#warning [41m see COMPILE.DOC for details [0m
#endif

#ifdef __MINGW32__
/*#define sleep _sleep - this is __MINGW_ATTRIB_DEPRECATED*/
#define sleep(n) usleep ((unsigned long) n * 1000000)
#endif

#ifdef __MINGW32__
#define mkdir__ mkdir
#define mkdir(f, m)	mkdir__(f)
#endif

#ifdef msdos
extern void delay ();
extern void exit ();

#include <io.h>		/* this is not "io.h" */
#  undef putchar
#  include <fcntl.h>

extern char * getenv ();
extern char * getcwd ();

#ifdef __TURBOC__
extern int setdisk ();
extern void sleep ();
#define time_t	unsigned long
#endif

#endif /* msdos */


/* VMS includes and declarations */
#ifdef vms
# include <unixio.h>
# include <file.h>	/* for the O_... attributes to open () */
# include <unixlib.h>
# undef putchar
# undef FALSE
# undef TRUE
# define use_usleep
#endif


/* Unix and terminal I/O defines */
#ifdef TERMIO
#ifndef sysV
# define sysV
#endif
#endif

#ifdef sysV
#ifndef unix
# define unix
#endif
/* # define TERMIO ? */
#endif


/* Unix includes and defines */
#ifdef unix

#  include <fcntl.h>
#  undef putchar

#  ifdef sysV
   extern char * getcwd ();
#  else
   extern char * getwd ();
#  endif

extern void exit ();
extern char * getenv ();
/*extern int printf ();*/

#ifdef __GCC__
#include <sys/types.h>
#endif

#ifdef _CLASSIC_ID_TYPES
/* HP, AIX, IRIX */
#define pid_t int
#endif

#ifdef __sgi
/* IRIX: no vfork */
#define FORK
#endif

#ifdef DEFVFORK
/* already declared in <unistd.h> */
extern pid_t vfork ();
#endif

#ifdef __CYGWIN__
extern pid_t vfork ();
#endif

extern pid_t wait ();

extern int select ();
#ifndef __MINGW32__
extern unsigned int sleep ();
#endif
#ifdef use_usleep
extern unsigned int usleep ();
#endif

#endif /* unix */


/* PC-specific defines */
#ifdef __CYGWIN__
#define __pcgcc__
#undef pc_term
#endif
#ifdef __MSDOS__
#define __pcgcc__
#endif
#ifdef __pcgcc__
#define PROT int
#endif


#ifndef PROT
#define PROT unsigned int
#endif


/* this is again unused since it broke compilation on SunOS 5.10 */
#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif


/* missing definitions */
#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef F_OK
#define F_OK 0
#endif
#ifndef X_OK
#define X_OK 1
#endif
#ifndef W_OK
#define W_OK 2
#endif


/***************************************************************************\
	declaration of system functions
\***************************************************************************/

#ifdef define_read_write	/* don't define to be compatible */
extern int write ();
extern int read ();
extern ssize_t write ();	/* but how do I find out if */
extern ssize_t read ();		/* ssize_t is defined on a system? */
#endif
extern int access ();
/*extern int open ();*/
extern int close ();
/*extern int creat ();*/
/*extern int chdir ();*/
extern int system ();
extern int isatty ();
#ifndef strcmp
extern int strcmp ();
#endif


/***************************************************************************\
	declaration of types, basic macros, and constants
\***************************************************************************/

#define arrlen(arr)	(sizeof (arr) / sizeof (* arr))

typedef unsigned char character;


/* Screen size and display definitions. Coordinates start at 0, 0 */
extern int YMAX;
extern int XMAX;
extern short MENU;
#define SCREENMAX	(YMAX - 1)	/* last line displayed (first is 0) */
#define XBREAK		(XMAX - scrollbar_width)	/* Shift line display at this column */
#define XBREAK_bottom	(XMAX - 1)
#define SHIFT_SIZE	(tab8 (XMAX / 4 + 1))	/* Number of chars to shift */

/* Adjust buffer sizes, align with typical max screen width
	screen size 1600x1200
		font size 5x7:	172 x 319
		font size 4x6:	200 x 399
	screen size 2560x1600 (WQXGA+)
		font size 4x6:	267 x 639
   Restrain to reasonable size assumption (esp. @ reasonable font size), 
   so prompt line buffer stays @ reasonable size < maxLINElen,
   so let maxXMAX < maxLINElen / 3
 */
#define maxXMAX		638
#define maxPROMPTlen	(maxXMAX + 1)	/* limit prompt to max screen width */
#define maxFILENAMElen	maxPROMPTlen
#define maxCMDlen	2 * maxPROMPTlen + 9

#define maxLINElen	1024		/* max chars on one line of text */
#define maxMSGlen	maxLINElen

/* pseudo x coordinates to indicate positioning to start/end of line */
	/* LINE_START must be rounded up to the lowest SHIFT_SIZE */
#define LINE_START	(((-maxLINElen - 1) / SHIFT_SIZE) * SHIFT_SIZE - SHIFT_SIZE)
	/* LINE_END must be larger than highest x-coordinate for line; 
	   considering wide characters ands TABs */
#define LINE_END	(maxLINElen * 8)


/* Return values of functions */
#define ERRORS		-1
#define NO_LINE		(ERRORS - 1)	/* Must be < 0 */
#define NUL_LINE	(ERRORS - 2)	/* Must be < 0 */
#define SPLIT_LINE	(ERRORS - 3)	/* Must be < 0 */
#define FINE		0
#define NO_INPUT	(FINE + 1)

#define STD_IN		0		/* Input file # */
#define STD_OUT		1		/* Terminal output file # */
#define STD_ERR		2

#define REPORT_CHANGED_LINES	1	/* Report change of lines on # lines */

/*
 * mouse button selection
 */
typedef enum {
	releasebutton,
	leftbutton, middlebutton, rightbutton,
	movebutton,
	wheelup, wheeldown,
	focusout, focusin
} mousebutton_type;

#define shift_button	4
#define alt_button	8
#define control_button	16

/*
 * Common enum type for all flags used in mined.
 */
typedef enum {
/* General flags */
  False,
  True,

/* yank_status and other */
  NOT_VALID,
  VALID,

/* expression flags */
  FORWARD,
  REVERSE,

/* yank flags */
  SMALLER,
  BIGGER,
  SAME,
  NO_DELETE,
  DELETE,
  READ,
  WRITE,

/* smart quotes state */
  UNSURE,
  OPEN
} FLAG;

typedef enum {LEFTDOUBLE, RIGHTDOUBLE, LEFTSINGLE, RIGHTSINGLE} quoteposition_type;

#ifdef VAXC
/* prevent %CC-E-INVOPERAND */
#define FLAG unsigned char
#endif

/*
	line end definitions
 */
typedef unsigned char lineend_type;
#define lineend_NUL	'\0'
#define lineend_NONE	' '
#define lineend_LF	'\n'
#define lineend_CRLF	'\r'
#define lineend_CR	'R'
#define lineend_NL1	'n'	/* ISO 8859 NEXT LINE byte 0x85 */
#define lineend_NL2	'N'	/* Unicode NEXT LINE U+0085 */
#define lineend_LS	'L'	/* Unicode line separator U+2028 */
#define lineend_PS	'P'	/* Unicode paragraph separator U+2029 */

/*
 * The Line structure. Each line entry contains a pointer to the next line,
 * a pointer to the previous line, a pointer to the text and an unsigned char
 * telling at which offset of the line printing should start (usually 0).
 */
struct Line {
  struct Line * next;
  struct Line * prev;
  char * text;
  char * sel_begin;	/* beginning pos of selection, or NIL if none */
  char * sel_end;	/* end pos of selection, or NIL if beyond line */
  unsigned short shift_count;	/* horizontal display shifting */
  lineend_type return_type;
  character syntax_mask;	/* bitmask composed of display flags below */
  FLAG dirty;		/* flag that display attributes have changed */
  FLAG allocated;
};
/* display flags for struct Line */
#define syntax_unknown	0xFF
#define syntax_none	0x00
#define syntax_HTML	0x01
#define syntax_JSP	0x02
#define syntax_PHP	0x04
#define syntax_scripting	(syntax_JSP | syntax_PHP)
#define syntax_comment	0x08
#define syntax_attrib	0x10
#define syntax_value	0x20

typedef struct Line LINE;

/*
 * For casting functions with int/void results
 */
typedef void (* voidfunc) ();
typedef int (* intfunc) ();

/* NULL definitions */
#define NIL_PTR		((char *) 0)
#define NIL_LINE	((LINE *) 0)


#define dont_use_ebcdic_text	/* use ebcdic_file with transformation */


/***************************************************************************\
	variable declarations
\***************************************************************************/

extern char * debug_mined;	/* Debug indicator and optional flags */

extern int total_lines;		/* Number of lines in file */
extern long total_chars;	/* Number of characters in file */
extern LINE * header;		/* Head of line list */
extern LINE * tail;		/* Last line in line list */
extern LINE * top_line;		/* First line of screen */
extern LINE * bot_line;		/* Last line of screen */
extern LINE * cur_line;		/* Current line in use */
extern char * cur_text;		/* Pointer to char on current line in use */
extern int last_y;		/* Last y of screen, usually SCREENMAX */
extern int line_number;		/* current line # determined by file_status */
extern int lines_per_page;	/* assumption for file_status */

extern int x, y;		/* x, y coordinates on screen */

extern FLAG use_file_tabs;
#define viewonly	(viewonly_mode || viewonly_locked || viewonly_err)
extern FLAG viewonly_mode;	/* Set when view only mode is selected */
extern FLAG viewonly_locked;	/* Enforced view only status */
extern FLAG viewonly_err;	/* Error view only status */
extern FLAG modified;		/* Set when file is modified */
extern FLAG restricted;		/* Set when restricted mode is selected */
extern FLAG quit;		/* Set on SIGQUIT or quit character typed */
extern FLAG winchg;		/* Set when the window size changes */
extern FLAG interrupted;	/* Set when a signal interrupts */
extern FLAG waitingforinput;	/* Set while waiting for the next command key */
extern FLAG isscreenmode;	/* Set when screen mode is on */
extern char text_buffer [maxLINElen];	/* Buffer for modifying text */

extern char * minedprog;	/* store argv [0] for help invocation */
extern char file_name [];	/* name of file being edited */
extern FLAG writable;		/* Set if file cannot be written */
extern FLAG file_is_dir;	/* Tried to open a directory as file? */
extern FLAG file_is_dev;	/* Tried to open a char/block device file? */
extern FLAG file_is_fifo;	/* Read from a named pipe? */
extern FLAG reading_pipe;	/* Set if file should be read from stdin */
extern FLAG writing_pipe;	/* Set if file should be written to stdout */

extern PROT xprot;		/* actual prot. mask representing +x option */
extern PROT fprot0;		/* default prot. mode for new files */
extern PROT fprot1;		/* prot. mode for new file being edited */
extern PROT bufprot;		/* prot. mode for paste buffer file */

#if defined (unix) || defined (ANSI)
extern char * title_string_pattern;
#endif

extern char yank_file [];	/* Temp. file for buffer */
extern char yankie_file [];	/* Temp. file for inter-window buffer */
extern char spool_file [];	/* Temp. file for printing */
extern char html_file [];	/* Temp. file for HTML embedding buffer */
extern char panic_file [];	/* file for panic-write-back */
extern FLAG yank_status;	/* Status of yank_file */
extern FLAG redraw_pending;	/* was a redraw suppressed in find_y ? */
extern long chars_saved;	/* # of chars saved in paste buffer */
extern long bytes_saved;	/* # of bytes saved in paste buffer */
extern int lines_saved;	/* # of lines saved in paste buffer */

extern int hop_flag;		/* set to 2 by HOP key function */
extern int hop_flag_displayed;
extern int buffer_open_flag;	/* counter flag for the collective buffer */
extern int JUSlevel;		/* keep justified while typing? */
extern int JUSmode;		/* 1: paragraphs end at empty line */
extern FLAG autoindent;		/* auto indent on input of Enter? */
extern FLAG autonumber;		/* Auto numbering on input of Enter? */
extern FLAG backnumber;		/* Auto-undent numbering on input of BS? */
extern FLAG backincrem;		/* Auto-increment numbering on input of BS? */
extern FLAG lowcapstrop;	/* capitalize letter symbols on input? */
extern FLAG dispunstrop;	/* display stropped letters in bold? */
extern FLAG strop_selected;	/* was stropping explictly selected? */
extern char strop_style;	/* bold/underline? */
extern FLAG mark_HTML;		/* display HTML marked ? */
extern FLAG mark_JSP;		/* Display JSP marked ? */
extern FLAG viewing_help;

/* emulation and keyboard assignment flags */
extern char emulation;		/* 'w' for WordStar, 'e' for emacs */
extern char keypad_mode;	/* 'm'ined, 'S'hift-select, 'w'indows */
extern FLAG shift_selection;	/* selection highlighting on shift-cursor */
#define apply_shift_selection (shift_selection && (shift_selection == True || (keyshift & shift_mask)))
extern FLAG mined_keypad;	/* Apply mined keypad assignments */
extern FLAG home_selection;	/* numeric keypad Home/End forced to Mark/Copy */
extern FLAG small_home_selection;	/* small keypad Home/End: Mark/Copy */
extern FLAG emacs_buffer;	/* enable emacs buffer fct for ^K/^T */
extern FLAG paste_stay_left;	/* cursor stays before pasted region */
extern FLAG tab_left;		/* should moving up/down on TAB go left? */
extern FLAG tab_right;		/* should moving up/down on TAB go right? */
extern FLAG plain_BS;	/* prefer BS to perform plain rather than smart? */

extern FLAG append_flag;	/* Set when buffer should be appended to */
extern FLAG pastebuf_utf8;	/* Paste buffer always treated as UTF-8? */
extern FLAG rectangular_paste_flag;	/* Copy/Paste rectangular area? */
extern FLAG visselect_key;	/* Visual selection on unmodified cursor key? */
extern FLAG visselect_anymouse;		/* Visual selection on any mouse move? */
extern FLAG visselect_keeponsearch;	/* Keep visual selection on search ? */
extern FLAG visselect_setonfind;	/* Select search result? */
extern FLAG visselect_autocopy;		/* Auto-copy selection? */
extern FLAG visselect_copydeselect;	/* Deselect on copy? */
extern FLAG visselect_autodelete;	/* Delete selection on insert? */

extern FLAG insert_mode;	/* insert or overwrite */

extern char backup_mode;	/* none, simple, numbered (e/v), automatic */
extern char * backup_directory;	/* directory name for backup files */
extern char * recover_directory;	/* directory name for recovery files */
extern character erase_char;	/* effective (configured) char for erase left */
extern FLAG prefer_comspec;	/* for ESC ! command */
extern character quit_char;	/* ^\/^G character to cancel command */
extern FLAG smart_quotes;	/* replace " with typographic quote ? */

extern FLAG suppress_unknown_cjk;	/* on CJK terminal if no Unicode mapping */
extern FLAG suppress_extended_cjk;	/* on CJK terminal if in extended code range */
extern FLAG suppress_invalid_cjk;	/* on CJK terminal if invalid CJK code */
extern FLAG suppress_surrogates;	/* suppress display of single Unicode surrogates */
extern FLAG suppress_non_BMP;		/* suppress display of non-BMP range */
extern FLAG suppress_EF;		/* suppress display of 0x*FFFE/F codes */
extern FLAG suppress_non_Unicode;	/* suppress display of non-Unicode codes */

extern FLAG utf_cjk_wide_padding;	/* always display CJK on UTF double-width ? */

extern FLAG dark_term;		/* dark colour terminal ? */
extern FLAG darkness_detected;	/* could background colour be queried ? */
extern FLAG fg_yellowish;	/* foreground colour near yellow ? */
extern FLAG bright_term;	/* need to improved contrast ? */
extern FLAG bw_term;		/* black/white terminal ? */
extern FLAG suppress_colour;	/* don't use ANSI color settings */

extern FLAG configure_xterm_keyboard;	/* deleteIsDEL, metaSendsEscape */

/* from mined1.c */
extern FLAG auto_detect;	/* Auto detect character encoding from file ? */
extern char * detect_encodings;	/* List of encodings to detect */
extern char language_tag;

extern FLAG cjk_text;		/* text in CJK encoding ? */
extern FLAG utf8_text;		/* text in UTF-8 representation ? */
extern FLAG utf16_file;		/* file encoded in UTF-16 ? */
extern FLAG utf16_little_endian;	/* UTF-16 file encoded little endian ? */
extern FLAG mapped_text;	/* text in 8 bit, non-Latin-1 representation ? */
extern FLAG ebcdic_text;
extern FLAG ebcdic_file;
extern FLAG utf8_lineends;	/* support UTF-8 LS and PS line ends ? */
extern FLAG poormansbidi;	/* poor man's bidirectional support ? */

extern FLAG combining_mode;	/* UTF-8 combining character support ? */
extern FLAG separate_isolated_combinings;	/* separated display of comb. at line beg./after TAB ? */
extern FLAG apply_joining;	/* apply LAM/ALEF joining ? */

extern FLAG disp_scrollbar;	/* shall scrollbar be displayed ? */
extern FLAG fine_scrollbar;	/* fine-grained UTF-8 scrollbar ? */
extern FLAG old_scrollbar_clickmode;	/* old left/right click semantics ? */
extern int scrollbar_width;
extern FLAG update_scrollbar_lazy;	/* partial scrollbar refresh as needed ? */

extern FLAG loading;		/* Loading a file? Init True for error handling */
extern FLAG only_detect_text_encoding;
extern FLAG wordnonblank;	/* handle all non-blank sequences as words */
extern FLAG proportional;	/* Enable support for proportional fonts? */
extern FLAG show_vt100_graph;	/* Display letters as VT100 block graphics ? */
extern FLAG hide_passwords;	/* Hide passwords in display */
extern FLAG filtering_read;	/* Use filter for reading file ? */
extern char * filter_read;	/* Filter for reading file */
extern FLAG filtering_write;	/* Use filter for writing file ? */
extern char * filter_write;	/* Filter for writing file */
extern FLAG translate_output;	/* Transform output diacritics to strings */
extern int translen;		/* length of " */
extern char * transout;		/* Output transformation table */
extern FLAG controlQS;		/* must respect ^Q/^S handshake ? */
extern int display_delay;	/* delay between display lines */
extern FLAG paradisp;		/* Shall paragraph end be distinguished? */

extern char UNI_marker;		/* Char to be shown in place of Unicode char */

extern char TAB_marker;		/* Char to be shown in place of tab chars */
extern char TAB0_marker;	/* Char to be shown at start of tab chars */
extern char TAB2_marker;	/* Char to be shown at end of tab chars */
extern char TABmid_marker;	/* Char to be shown in middle of tab chars */
extern unsigned long CJK_TAB_marker;	/* Char to be shown in place of tab */
extern char RET_marker;		/* Char indicating end of line (LF) */
extern char DOSRET_marker;	/* Char indicating DOS end of line (CRLF) */
extern char MACRET_marker;	/* Char indicating Mac end of line (CR) */
extern char PARA_marker;	/* Char indicating end of paragraph */
extern char RETfill_marker;	/* Char to fill the end of line with */
extern char RETfini_marker;	/* Char to fill last position of line with */
extern char SHIFT_marker;	/* Char indicating that line continues */
extern char SHIFT_BEG_marker;	/* Char indicating that line continues left */
extern char MENU_marker;	/* Char to mark selected item */
extern char * UTF_TAB_marker;		/* Char to be shown in place of tab chars */
extern char * UTF_TAB0_marker;		/* Char to be shown at start of tab chars */
extern char * UTF_TAB2_marker;		/* Char to be shown at end of tab chars */
extern char * UTF_TABmid_marker;	/* Char to be shown in middle of tab chars */
extern char * UTF_RET_marker;		/* Char indicating end of line */
extern char * UTF_DOSRET_marker;	/* Char indicating DOS end of line */
extern char * UTF_MACRET_marker;	/* Char indicating Mac end of line */
extern char * UTF_PARA_marker;		/* Char indicating end of paragraph */
extern char * UTF_RETfill_marker;	/* Char to fill the end of line with */
extern char * UTF_RETfini_marker;	/* Char to fill last position of line with */
extern char * UTF_SHIFT_marker;		/* Char indicating that line continues */
extern char * UTF_SHIFT_BEG_marker;	/* Char indicating that line continues left */
extern char * UTF_MENU_marker;	/* Char to mark selected item */
extern char * submenu_marker;	/* Char to mark submenu entry */
extern char * menu_cont_marker;	/* Char to mark menu border continuation */
extern char * menu_cont_fatmarker; /* Char to mark menu border continuation */

extern FLAG stat_visible;	/* Set if status line is visible */
extern FLAG top_line_dirty;	/* Was menu line cleared? */
extern FLAG text_screen_dirty;	/* Was text display position affected? */

extern FLAG page_scroll;	/* use scroll for page up/down */
extern FLAG page_stay;		/* stay at edge of page after page up/down */

/* from justify.c */
extern int first_left_margin;
extern int next_left_margin;
extern int right_margin;

/* from keyboard.c */
extern long last_delta_readchar;	/* delay between last 2 characters */
extern long average_delta_readchar;	/* average delay between last characters */


/***************************************************************************\
	declaration of mined functions
\***************************************************************************/

/* text buffer navigation */
/* from textbuf.c */
extern LINE * proceed _((LINE *, int));
extern void reset _((LINE * scr_top, int y));
extern void move_y _((int y));
extern void move_to _((int x, int y));
extern void move_address _((char *, int y));
extern void move_address_w_o_RD _((char *, int y));
extern int find_x _((LINE * line, char * address));
extern int get_cur_pos _((void));
extern int get_cur_col _((void));
extern int find_y _((LINE * line));
extern int find_y_w_o_RD _((LINE * line));

/* text buffer status handling */
/* from mined1.c */
extern void set_modified _((void));
/* from textbuf.c */
extern void file_status _((char * message, long bytecount, long charcount, 
			char * filename, int lines, FLAG textstat, 
			FLAG writefl, FLAG changed, FLAG viewing));
extern void recount_chars _((void));

/* from edit.c */
extern char syntax_state _((char prev_mask, char old_mask, char * spoi, char * sbeg));
extern void update_syntax_state _((LINE *));

/* from pastebuf.c */
extern void delete_yank_files _((void));
extern int yankfile _((FLAG read_write, FLAG append));
extern void yank_HTML _((FLAG remove));
extern void paste_HTML _((void));
extern FLAG checkmark _((LINE *, char *));
extern void delete_text_buf _((LINE * start_line, char * start_textp, LINE * end_line, char * end_textp));

/* selection highlighting */
/* from pastebuf.c */
extern void start_highlight_selection _((void));
extern void continue_highlight_selection _((int pos_x));
extern void update_selection_marks _((int pos_x));
extern void clear_highlight_selection _((void));
extern FLAG has_active_selection _((void));
extern void trigger_highlight_selection _((void));
extern void adjust_rectangular_mode _((FLAG alt_mouse));

/* editing functions */
/* from edit.c */
extern int delete_text _((LINE * startl, char * start, LINE * endl, char * end));
extern int delete_text_only _((LINE * startl, char * start, LINE * endl, char * end));
extern int insert_text _((LINE *, char * location, char * string));
extern LINE * line_insert_after _((LINE *, char *, int, lineend_type));

/* character input handling */
struct prefixspec {
	voidfunc prefunc;
	unsigned int preshift;
	char * accentname;
	char * accentsymbol;
	char * pat1;
	char * pat2;
	char * pat3;
};

/* from compose.c */
extern char * mnemos _((int ucs));
extern unsigned long compose_chars _((unsigned long, unsigned long));
extern unsigned long compose_mnemonic _((char *));
extern struct prefixspec * lookup_prefix _((voidfunc prefunc, unsigned int shift));
extern struct prefixspec * lookup_prefix_char _((unsigned long c));
extern unsigned long compose_patterns _((unsigned long base, struct prefixspec * ps, struct prefixspec * ps2, struct prefixspec * ps3));

/* from edit.c */
extern void COMPOSE _((void));
extern void key_0 _((void));
extern void key_1 _((void));
extern void key_2 _((void));
extern void key_3 _((void));
extern void key_4 _((void));
extern void key_5 _((void));
extern void key_6 _((void));
extern void key_7 _((void));
extern void key_8 _((void));
extern void key_9 _((void));
extern unsigned long CTRLGET _((FLAG check_prefix));


/* status line handling */
/* from prompt.c */
extern void rd_bottom_line _((void));
extern void redraw_prompt _((void));
extern void clear_lastline _((void));
/* file chooser */
extern int wildcard _((char * pat, char * s));
extern int get_filename _((char * message, char * filename, FLAG directory));
extern FLAG sort_by_extension;
extern FLAG sort_dirs_first;


/*
 * Convert cnt to nearest tab position
 */
extern int tabsize;		/* Width of tab positions, 2 or 4 or 8 */
extern FLAG tabsize_selected;	/* Tab width selected by explicit option? */
extern FLAG expand_tabs;	/* Expand TABs to Spaces? */
#define tab(cnt)		(((cnt) + tabsize) & ~(tabsize - 1))
#define tab8(cnt)		(((cnt) + 8) & ~07)
/*
 * Word definitions
 */
#ifdef use_ebcdic_text
#define white_space(c)	((c) == code_SPACE || (c) == code_TAB)
#define alpha(c)	((c) != code_SPACE && (c) != code_TAB && (c) != '\n')
#else
#define white_space(c)	((c) == ' ' || (c) == '\t')
#define alpha(c)	((c) != ' ' && (c) != '\t' && (c) != '\n')
#endif

/* from edit.c */
extern int ishex _((character));
extern unsigned int hexval _((character));
extern character hexdig _((unsigned int));
extern int get_idf _((char * idf_buf, char * text, char * start_line));
extern FLAG iscombined_unichar _((unsigned long ucs, char * charpos, char * linebegin));

/* from search.c */
extern void search_for _((char *, FLAG direction, FLAG case_ignore));
extern FLAG search_ini _((char *, FLAG direction, FLAG case_ignore));
extern void search_expr _((char *, FLAG direction, FLAG case_ignore));
extern void search_corresponding _((char *, FLAG, char *));

/* from mined1.c */
extern void splash_logo _((void));
extern void switchAltScreen _((void));
extern void KEYREC _((void));

extern void emul_emacs _((void));
extern void emul_WordStar _((void));
extern void emul_pico _((void));
extern void emul_Windows _((void));
extern void emul_mined _((void));
extern void set_keypad_mined _((void));
extern void set_keypad_shift_selection _((void));
extern void set_keypad_windows _((void));

extern void handle_locale_quotes _((char * lang, FLAG alt));
extern void configure_preferences _((FLAG applycommon));

extern void viewonlyerr _((void));
extern void restrictederr _((void));

extern void convlineend_cur_LF _((void));
extern void convlineend_cur_CRLF _((void));
extern void convlineend_all_LF _((void));
extern void convlineend_all_CRLF _((void));


/* special stuff */
/* from minedaux.c */
extern void debuglog _((char * tag, char * l1, char * l2));
extern char * envvar _((char * name));
extern char * envstr _((char * name));
extern char * getbasename _((char * s));
extern int is_absolute_path _((char * filename));
extern char * getusername _((void));
extern char * get_cwd _((char * dirbuf));
extern char * gethomedir _((void));
extern void sethomedir _((char *));
extern int systemcall _((char * msg, char * cmd, int delay));
#ifndef msdos
extern int progcallpp _((char * msg, int delay, char * binprepath [], 
	char * dir, char * prog, char * p1, char * p2, char * p3, char * p4));
#endif
#if defined (msdos) || defined (__MINGW32__)
extern int is_Windows _((void));
#endif
extern void panic _((char *, char *));
extern void panicio _((char * message, char * err));
extern char * scan_int _((char *, int *));
extern char * dec_out _((long));
extern int delete_file _((char *));
extern FLAG copyfile _((char * from_file, char * to_file));
extern char * unnull _((char *));
extern void strip_trailingslash _((char *));
extern char * num_out _((long number, long radix));
extern void copy_string _((char * to, char * from));
/* from legacy.c */
extern char * serror _((void));
extern char * serrorof _((int));
extern int geterrno _((void));
extern int portable_getpgrp _((int mypid));

/* memory allocation */
/* from minedaux.c */
extern void * alloc _((int bytes));
extern void free_space _((void *));
extern LINE * alloc_header _((void));
extern void free_header _((LINE *));


/* prompt line */
/* from prompt.c */
extern FLAG char_on_status_line; /* is output active on status line ? */
extern FLAG input_active;
extern int lpos;
extern long get_number _((char * message, char firstdigit, int * result));
extern int prompt_num_char _((unsigned long * result, unsigned long maxvalue));


/* scrollbar handling */
/* from output.c */
extern FLAG display_scrollbar _((FLAG update));
extern void scrollbar_scroll_up _((int from));
extern void scrollbar_scroll_down _((int from));
extern int check_scrollbar_pos _((void));

/* from mined1.c */
extern void RDwin _((void));
extern void RDwinchg _((void));
extern void RD_window_title _((void));
extern void clear_window_title _((void));
extern void RD_y _((int y_pos));
/* from output.c */
extern void display _((int y, LINE *, int count, int new_pos));
extern void display_flush _((void));
/* from output.c */
extern void putmark _((char mark, char * utfmark));
extern FLAG marker_defined _((character marker, char * utfmarker));
extern void put_blanks _((int endpos));
extern void set_cursor_xy _((void));
extern void put_line _((int scr_y, LINE *, int offset, FLAG clear, FLAG prop_pos));
extern void print_line _((int scr_y, LINE *));
/* put character values */
#define putchar(c)	__putchar ((character) c)
extern void putcharacter _((character));
extern void put_unichar _((unsigned long));
extern int put_cjkchar _((unsigned long));

/* character handling */
/* from minedaux.c */
extern int length_of _((char *));
extern char * dupstr _((char * s));
extern char * nextutf8 _((char *s));
extern int stringcompare_ _((char * s1, char * s2));
extern int stringprefix_ _((char * s1, char * s2));
/* from textfile.c */
extern int UTF8_len _((char));
extern int CJK_len _((character *));
extern int char_count _((char *));
extern int col_count _((char *));
extern void utf8_info _((char *, int *, unsigned long *));
extern int isjoined _((unsigned long, char *, char *));
extern int iscombined _((unsigned long, char *, char *));
#define multichar(c)	((character) c >= 0x80 && (cjk_text == False || (text_encoding_tag != 'S' && text_encoding_tag != 'x') || (character) c < 0xA1 || (character) c > 0xDF))
extern int iscombining _((unsigned long ucs));
extern int iswide _((unsigned long ucs));
extern int uniscrwidth _((unsigned long, char *, char *));
extern int cjkscrwidth _((unsigned long, char *, char *));
extern char * charbegin _((char *, char *));
extern void advance_utf8_scr _((char * *, int *, char *));
extern void advance_utf8 _((char * *));
extern void advance_char_scr _((char * *, int *, char *));
extern void advance_char _((char * *));
extern void precede_char _((char * *, char *));
extern unsigned long utf8value _((character *));
extern unsigned long charvalue _((character *));
extern unsigned long unicodevalue _((character *));
extern unsigned long unicode _((unsigned long));
extern unsigned long precedingchar _((char *, char *));
extern FLAG opensquote _((unsigned long prevchar));
/* from mined1.c */
extern int isglyph_uni _((unsigned long));

/* character mapping handling */
#define CHAR_UNKNOWN (unsigned long) -2	/* unknown mnemonic */
#define CHAR_INVALID (unsigned long) -1	/* not mappable to encoding */

#define FUNcmd (unsigned long) -7	/* indicates function key */

/* information display preferences */
extern FLAG always_disp_fstat;	/* Permanent file status display on status line? */
extern FLAG always_disp_help;	/* Permanent F2... help display on status line? */
extern FLAG always_disp_code;	/* Permanent char code display on status line? */
extern FLAG disp_scriptname;	/* display Unicode script range? */
extern FLAG disp_charname;	/* display Unicode character name? */
extern FLAG disp_charseqname;	/* display Unicode named sequences? */
extern FLAG disp_decomposition;	/* display Unicode character decomposition? */
extern FLAG disp_mnemos;	/* display mined input mnemos? */
extern FLAG always_disp_Han;	/* Permanent Han character description display on status line? */
extern FLAG disp_Han_Mandarin;	/* display this Han pronunciation ? */
extern FLAG disp_Han_Cantonese;	/* display this Han pronunciation ? */
extern FLAG disp_Han_Japanese;	/* display this Han pronunciation ? */
extern FLAG disp_Han_Sino_Japanese;	/* display this Han pronunciation ? */
extern FLAG disp_Han_Hangul;	/* display this Han pronunciation ? */
extern FLAG disp_Han_Korean;	/* display this Han pronunciation ? */
extern FLAG disp_Han_Vietnamese;	/* display this Han pronunciation ? */
extern FLAG disp_Han_HanyuPinlu;	/* display this Han pronunciation ? */
extern FLAG disp_Han_HanyuPinyin;	/* display this Han pronunciation ? */
extern FLAG disp_Han_XHCHanyuPinyin;	/* display this Han pronunciation ? */
extern FLAG disp_Han_Tang;	/* display this Han pronunciation ? */
extern FLAG disp_Han_description;	/* display Han description ? */
extern FLAG disp_Han_full;	/* display full popup Han description ? */

struct raw_hanentry {
	unsigned long unicode;
	char * text;
};

extern struct raw_hanentry hantable [];
extern unsigned int hantable_len;


/* various keyboard options */
extern FLAG detect_esc_alt;	/* Enable detection of Alt key by ESC prefix? */

/* keyboard mapping setup functions (keymaps.c) */
extern void toggleKEYMAP _((void));
extern void setupKEYMAP _((void));
extern FLAG setKEYMAP _((char *));
extern char * keyboard_mapping;
extern char * last_keyboard_mapping;

/* keyboard mapping (keymaps.c) */
extern int map_key _((char * str, int matchmode, char * * found, char * * mapped));
extern int keyboard_mapping_active _((void));

/* keyboard mapping menu handling */
extern char selection_space;
extern FLAG enforce_keymap;	/* enable keyboard mapping even on non-suitable terminal */
#define allow_keymap ((utf8_input && (utf8_text || cjk_text || mapped_text)) || cjk_term || enforce_keymap)
#define SPACE_NEXT	'n'
#define SPACE_NEXTROW	'r'
#define SPACE_SELECT	's'

/* menu handling */
typedef void (* _menufunc) ();
typedef int (* _flagfunc) ();
typedef struct {
	char * itemname;
	_menufunc itemfu;
	char * hopitemname;
	_flagfunc itemon;
	char * extratag;
	} menuitemtype;
typedef void (* menufunc) _((menuitemtype *, int));
typedef int (* flagfunc) _((menuitemtype *, int));

extern FLAG flags_changed;	/* Should flag menu area be redrawn? */
/* from mousemen.c */
extern void fill_menuitem _((menuitemtype * item, char * s1, char * s2));
extern void openmenuat _((int col));
extern int popup_menu _((menuitemtype *, int menulen, int column, int line, 
			char * title, 
			FLAG is_flag_menu, FLAG disp_only, char * select_keys));
extern void clean_menus _((void));
extern int is_menu_open _((void));
extern void displaymenuline _((FLAG show_filelist));
extern void redrawmenu _((void));
extern void displayflags _((void));
extern void toggle_append _((void));
extern void handleKeymapmenu _((void));
extern void handleQuotemenu _((void));
extern void handleEncodingmenu _((void));

/* from edit.c */
extern int quote_type;
extern int prev_quote_type;
extern int default_quote_type;
extern void set_quote_type _((int));
extern void set_quote_style _((char *));
extern void reset_smart_replacement _((void));
/* from mousemen.c */
extern int count_quote_types _((void));
extern FLAG spacing_quote_type _((int qt));
extern char * quote_mark _((int, quoteposition_type));
extern void set_quote_menu _((void));
extern int lookup_quotes _((char *));
extern menuitemtype * lookup_Keymap_menuitem _((char * hopitem));


/* main editing functions */
extern voidfunc command _((unsigned long));
extern void invoke_key_function _((unsigned long));

extern void QUICKMENU _((void)), FILEMENU _((void)), EDITMENU _((void));
extern void SEARCHMENU _((void)), PARAMENU _((void)), OPTIONSMENU _((void));
extern void handleFlagmenus _((void));
extern void MOUSEfunction _((void)), FOCUSout _((void)), FOCUSin _((void));
extern void AMBIGnarrow _((void)), AMBIGwide _((void));
extern void ANSIseq _((void));
extern void OSC _((void));
extern void MOVUP _((void)), MOVDN _((void)), MOVLF _((void)), MOVRT _((void));
extern void SD _((void)), SU _((void)), MOVPD _((void)), MOVPU _((void));
extern void BFILE _((void)), EFILE _((void));
extern void BLINEORUP _((void)), ELINEORDN _((void)), MOVLBEG _((void)), MOVLEND _((void));
extern void HIGH _((void)), LOW _((void));
extern void MNW _((void)), MPW _((void)), BSEN _((void)), ESEN _((void));
extern void MPPARA _((void)), MNPARA _((void));
extern void DPC _((void)), DCC _((void)), DLN _((void)), DNW _((void)), DPW _((void));
extern void DCC0 _((void)), DPC0 _((void));
extern void CTRLINS _((void)), DLINE _((void)), TOGINS _((void)), ctrlQ _((void)), ctrlK _((void)), ctrlO _((void));
extern void search_wrong_enc _((void));
extern void JUS _((void)), JUSclever _((void)), JUSandreturn _((void));
extern void QUED _((void)), WT _((void)), WTU _((void)), SAVEAS _((void)), EDIT _((void)), NN _((void));
extern void RECOVER _((void));
extern void SAVPOS _((void)), GROOM_INFO _((void));
extern void RD _((void)), RDwin _((void)), RDwin_nomenu _((void));
extern void RDcenter _((void)), I _((void));
extern void EXED _((void)), VIEW _((void));
extern void HOP _((void)), CANCEL _((void)), ESCAPE _((void));
extern void EMAX _((void)), META _((void)), UNDO _((void));
extern void CHDI _((void));
extern void PRINT _((void)), CMD _((void)), SH _((void)), SUSP _((void));
extern void LNCI _((void)), LNSW _((void));
extern void LOWCAP _((void)), LOWER _((void)), UPPER _((void));
extern void CAPWORD _((void)), LOWCAPWORD _((void));
extern void HTML _((void)), MARKER _((void)), GOMARKER _((void));
extern void EDITmode _((void)), VIEWmode _((void)), toggle_VIEWmode _((void));
extern void NXTFILE _((void)), PRVFILE _((void)), SELECTFILE _((void)), CLOSEFILE _((void));
extern void EXFILE _((void)), EXMINED _((void));
extern void ADJLM _((void)), ADJFLM _((void)), ADJNLM _((void)), ADJRM _((void)), ADJPAGELEN _((void));
extern void toggle_tabsize _((void)), toggle_tab_expansion _((void));
extern void changeuni _((void)), changehex _((void)), changeoct _((void)), changedec _((void));
extern void screensmaller _((void)), screenlesslines _((void)), screenlesscols _((void));
extern void screenbigger _((void)), screenmorelines _((void)), screenmorecols _((void));
extern void fontsmaller _((void)), fontbigger _((void)), fontdefault _((void));

extern void SFW _((void)), SRV _((void)), RES _((void)), RESreverse _((void));
extern void LR _((void)), GR _((void)), REPL _((void));
extern void SIDFW _((void)), SIDRV _((void));
extern void Stag _((void));
extern void AltX _((void));

extern void toggleKEYMAP _((void)), setupKEYMAP _((void));
extern void toggle_encoding _((void));
extern void change_encoding _((char * new_text_encoding));

extern void S0 _((character)), Scharacter _((unsigned long));
extern FLAG insert_unichar _((unsigned long));
extern void SNL _((void)), STAB _((void)), SSPACE _((void));
extern void APPNL _((void)), Underline _((void));
extern void Sdoublequote _((void)), Ssinglequote _((void)), Sdash _((void)), Sacute _((void)), Sgrave _((void));
extern void SCURCHARFW _((void)), SCURCHARRV _((void));

extern void goline _((int)), goproz _((int));

extern void MARK _((void)), COPY _((void)), CUT _((void));
extern void setMARK _((FLAG set_only)), toggleMARK _((void));
extern void toggle_rectangular_paste_mode _((void)), SELECTION _((void));
extern void Pushmark _((void)), Popmark _((void)), Upmark _((void));
extern void GOTO _((void)), GOHOP _((void)), GOMA _((void));
extern void MARKn _((int)), GOMAn _((int)), mark_n _((int));
extern void PASTE _((void)), PASTEEXT _((void)), PASTEstay _((void)), YANKRING _((void));
extern void WB _((void)), INSFILE _((void));

extern void SIDF _((FLAG)), SCURCHAR _((FLAG));
extern void UML _((char)), REPT _((char));
extern FLAG CONV _((void));
extern void BADch _((unsigned long cmd));
extern void SCORR _((FLAG pref_direction));

extern void checkout _((void)), checkin _((void));
extern void SPELL _((void));

extern void FS _((void)), FSTATUS _((void));
extern void display_code _((void)), display_the_code _((void));
extern void display_Han _((char * cpoi, FLAG force_utf8));
extern void display_FHELP _((void));
extern void ABOUT _((void));
extern void HELP _((void)), HELP_topics _((void)), HELP_Fn _((void)), HELP_accents _((void)), toggle_FHELP _((void));

extern void FHELP _((voidfunc));

extern void check_file_modified _((void));

/* function keys */
extern void LFkey _((void)), RTkey _((void)), UPkey _((void)), DNkey _((void));
extern void PGUPkey _((void)), PGDNkey _((void));
extern void HOMEkey _((void)), ENDkey _((void)), smallHOMEkey _((void)), smallENDkey _((void));
extern void INSkey _((void)), DELkey _((void)), smallINSkey _((void)), smallDELkey _((void));
extern void KP_plus _((void)), KP_minus _((void));
extern void DELchar _((void));
extern void FIND _((void)), AGAIN _((void));
extern void F1 _((void)), F2 _((void)), F3 _((void)), F4 _((void)), F5 _((void)), F6 _((void)), F7 _((void)), F8 _((void)), F9 _((void)), F10 _((void)), F11 _((void)), F12 _((void));


/*
 * String functions
 */
#define streq(s1, s2)		(strcmp (s1, s2) == 0)
#define strisprefix(p, s)	(strncmp (s, p, strlen (p)) == 0)
#define strcontains(s, c)	(strstr (s, c))
#define strappend(s, a, s_size)	strncat (s, a, s_size - strlen (s) - 1)

/*
 * Functions handling status_line.
 */
extern int bottom_line _((FLAG attrib, char *, char *, char * inbuf, FLAG statfl, char * term_input));
extern void status_uni _((char * msg));
extern character status2_prompt _((char * term, char * msg1, char * msg2));
extern character prompt _((char * term));
#define status_msg(str)		status_line (str, NIL_PTR)
#define status_line(str1, str2)	\
	(void) bottom_line (True, (str1), (str2), NIL_PTR, False, "")
#define status_fmt2(str1, str2)	\
	(void) bottom_line (VALID, str1, str2, NIL_PTR, False, "");
#define status_fmt(str1, str2)	\
	(void) bottom_line (SAME, str1, str2, NIL_PTR, False, "");
#define status_beg(str)		\
	(void) bottom_line (True, (str), NIL_PTR, NIL_PTR, True, "")
#define error(str1)		\
	(void) bottom_line (True, (str1), NIL_PTR, NIL_PTR, False, "")
#define error2(str1, str2)	\
	(void) bottom_line (True, (str1), (str2), NIL_PTR, False, "")
#define clear_status()		\
	(void) bottom_line (False, NIL_PTR, NIL_PTR, NIL_PTR, False, "")
#define get_string(str1, str2, fl, term_chars)	\
	bottom_line (True, (str1), NIL_PTR, (str2), fl, term_chars)
extern int get_string_uni _((char * str1, char * str2, FLAG fl, char * term_chars));


/*
 * Print info about current file and buffer.
 */
#define fstatus(mess, bytes, chars)	\
	file_status ((mess), (bytes), (chars), file_name, total_lines, True, writable, modified, viewonly)

/*
 * Build formatted string.
 */
#define build_string sprintf


/***************************************************************************\
	end
\***************************************************************************/
