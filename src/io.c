/*======================================================================*\
|*		Editor mined						*|
|*		operating system dependent I/O				*|
\*======================================================================*/

#ifdef _AIX
#define _ALL_SOURCE	/* get struct winsize defined */
#endif


#include "mined.h"
#include "io.h"
#include "termprop.h"


#ifdef vms

#define __BSD44_CURSES

#undef CURSES
#define ANSI

#include "vmsio.c"

#endif


#ifdef ANSI
#undef conio
#endif

#ifdef msdos

#ifndef __TURBOC__
#undef unix
/* don't #define ANSI */
#endif

#ifndef conio
#define ANSI
#endif

#endif


#include <errno.h>
#include <signal.h>
#ifndef __TURBOC__
extern int kill _((pid_t, int));
#endif

/* include configuration for SIGWINCH information retrieval */
#ifdef SGTTY
#undef TERMIO
#define include_SGTTY
#endif
#ifdef TERMIO
#define include_TERMIO
#endif


/* terminal handling selection */
#ifdef CURSES
#define _SGTTYB_	/* avoid duplicate definitions on SunOS */
#include <curses.h>
# undef FALSE
# undef TRUE

# ifdef SETLOCALE
# include <locale.h>
# endif

/* ensure window size detection capability */
# ifndef SGTTY
# define include_TERMIO
# endif

# ifdef vms
# undef include_TERMIO
# endif

# undef TERMIO	/* \ must be  */
# undef SGTTY	/* / disabled */
#endif


#ifdef include_TERMIO
# ifdef questionable_but_recommended_by_some_SuSe_Guru_doesnt_work_on_HP
#  if defined (__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 1))
#	include <termios.h>		/* define POSIX.1 termios */
#	include <sys/ioctl.h>		/* declare ioctl() for winsize */
#	undef   TCGETS		/* Use POSIX.1 instead */
#  else
/* Compatible <termio.h> for old 'struct termio' ioctl interface. */
#	include <termio.h>
#  endif
# else
#	include <termios.h>		/* define POSIX.1 termios */
#	include <sys/ioctl.h>		/* declare ioctl() for winsize */
# endif
#endif

#ifdef include_SGTTY
#define BSD_COMP /* let SunOS #include <sys/ttold.h> in <sys/ioctl.h> */
#include <sys/ioctl.h>
#include <sgtty.h> /* after <sys/ttold.h> */
/*extern int ioctl ();*/
#endif


#ifdef __CYGWIN__
#include <sys/ioctl.h>
#endif

#ifdef SIGPHONE		/* this trick was taken from less' screen.c */
#include <sys/window.h>	/* for window size detection */
#endif

#ifdef msdos
#define _getch_
#define getch dosgetch
#endif

#ifdef msdos
/* make REGS "x." branch work with djgpp */
#define _NAIVE_DOS_REGS
#include <dos.h>
#ifndef __TURBOC__
#include <dpmi.h>
#include <go32.h> /* define transfer buffer __tb and function dosmemget */
#endif
#endif


#ifdef unix
# undef _POSIX_SOURCE
# undef _XOPEN_SOURCE
#ifndef __MINGW32__
/* use select () for peek/read? */
#define selectpeek
#define selectread
#endif
#endif

#ifdef __CYGWIN__
#include <cygwin/version.h>	/* check CYGWIN_VERSION_DLL_* */
#if CYGWIN_VERSION_DLL_MAJOR == 1007 && CYGWIN_VERSION_DLL_MINOR <= 17
/* should in theory be checked dynamically, by calling uname(2) within #ifdef __CYGWIN__ ... */
/* support immediate response to WINCH */
#undef selectread
#endif
#endif

#define dont_debug_timeout_read
#ifdef debug_timeout_read
#undef selectread
#endif

#ifdef __EMX__
#undef selectpeek
#undef selectread
# ifdef CURSES
# define _getch_
# endif
#endif

#ifdef vms
# include <socket.h>	/* for select () and struct timeval */
# ifdef CURSES
# define _getch_
# endif
#endif

#ifdef CURSES_INPUT	/* currently not defined */
#define _getch_	/* not necessarily needed for Unix, but might be better 
		   with new curs_readchar loop in inputreadyafter () */
#endif

#ifdef _getch_
#ifndef CURSES
extern int getch ();
#endif
#endif


#ifndef __TURBOC__
#include <sys/time.h>	/* for struct timeval (for select in inputreadyafter) */
#endif


/*======================================================================*\
|*			Mode indication					*|
\*======================================================================*/

/* terminal capabilites and feature usage */
FLAG can_scroll_reverse = True;
FLAG can_add_line = True;
FLAG can_delete_line = True;
FLAG can_clear_eol = True;
FLAG can_clear_eos = True;
FLAG can_erase_chars = True;
FLAG can_reverse_mode = True;
FLAG can_hide_cursor = False;
FLAG can_dim = False;
FLAG can_alt_cset = True;
FLAG use_mouse_button_event_tracking = True;
FLAG colours_256 = False;
FLAG colours_88 = False;
FLAG ansi_esc = True;
FLAG standout_glitch = False;
FLAG use_appl_cursor = True;
FLAG use_appl_keypad = False;

/* menu border style */
FLAG use_ascii_graphics = False;	/* use ASCII graphics for borders */
FLAG use_vga_block_graphics = False;	/* charset is VGA with block graphics */
FLAG use_pc_block_graphics = False;	/* alternate charset is VGA with block graphics */
FLAG use_vt100_block_graphics = False;
FLAG full_menu_bg = False;		/* full coloured background */
char menu_border_style = 's';
int menumargin = 0;
FLAG explicit_border_style = False;
FLAG explicit_scrollbar_style = False;
FLAG explicit_keymap = False;
FLAG use_stylish_menu_selection = True;
FLAG bold_border = True;
#define use_normalchars_boxdrawing ((utf8_screen || use_ascii_graphics) && ! use_vt100_block_graphics && ! use_vga_block_graphics)

int
use_unicode_menubar ()
{
  if (use_stylish_menu_selection
      && use_normalchars_boxdrawing
      && ! use_ascii_graphics
      && menu_border_style != 'P'
      && menu_border_style != '@') {
	return 1 + (menu_border_style == 'd');
  } else {
	return 0;
  }
}

/* feature usage options */
FLAG use_script_colour = True;
FLAG use_mouse = True;
FLAG use_mouse_release = True;
FLAG use_mouse_anymove_inmenu = True;
FLAG use_mouse_anymove_always = False;
FLAG use_mouse_extended = False;	/* UTF-8 encoding of mouse coordinates */
FLAG use_mouse_1015 = False;		/* numeric encoding of mouse coordinates */
FLAG use_bold = True;
FLAG use_bgcolor = True;
FLAG avoid_reverse_colour = False;
FLAG use_modifyOtherKeys = False;
int cursor_style = 0;

/* state */
static FLAG in_menu_mouse_mode = False; /* used in dosmouse.c */
FLAG in_menu_border = False;
/*int current_cursor_y = 0;*/
static FLAG is_mouse_button_event_mode_enabled = False;

/* screen attributes: ANSI sequences */
char * markansi = 0;		/* line indicator marking (dim/red) */
char * emphansi = 0;		/* status line emphasis mode (red bg) */
char * borderansi = 0;		/* menu border colour (red) */
char * selansi = 0;		/* menu selection */
char * selfgansi = 0;		/* menu selection background */
char * ctrlansi = 0;		/* Control character display */
char * uniansi = 0;		/* Unicode character display */
char * specialansi = 0;		/* Unicode (lineend) marker display */
char * combiningansi = 0;	/* combining character display */
char * menuansi = 0;		/* menu line */
char * HTMLansi = 0;		/* HTML display */
char * XMLattribansi = 0;	/* HTML/XML attribute display */
char * XMLvalueansi = 0;	/* HTML/XML value display */
char * diagansi = 0;		/* dialog (bottom status) line */
char * scrollfgansi = 0;	/* scrollbar foreground */
char * scrollbgansi = 0;	/* scrollbar background */

#define nonempty(ansi)	(ansi && * ansi)

static int colour_token = -1;

void
set_colour_token (token)
  int token;
{
	colour_token = token;
}


/*======================================================================*\
|*			I/O channels					*|
\*======================================================================*/

/* tty file handles */
int input_fd = STD_IN;
#ifdef __EMX__
int output_fd = STD_OUT;
#else
int output_fd = STD_ERR;
#endif

#define tty_fd	output_fd


/*======================================================================*\
|*			Interrupt signal handling			*|
\*======================================================================*/

#define dont_debug_winchg

#ifdef debug_winchg
#define trace_winchg(params)	printf params
#else
#define trace_winchg(params)
#endif


/* interrupt control */
FLAG tty_closed = False;
FLAG hup = False;	/* set when SIGHUP is caught */
/* don't use 'static' for variables used in signal handler */
FLAG intr_char = False;	/* set when SIGINT is caught */
int sigcaught = 0;	/* set to signal caught */
FLAG continued = False;	/* set when SIGCONT is receive */


/**
   Signal handler for system
 */
static
void
catch_interrupt (signum)
  int signum;
{
  sigcaught = signum;
  /*catch_signals ();*/
  signal (signum, catch_interrupt);

  /* To converge towards BSD signal() semantics: 
     don't handle_interrupt here; postpone (setting sigcaught flag)
     and handle after select(), maybe after read(), elsewhere?
   */
  handle_interrupt (signum);
}

static
void
catch_crash (signum)
  int signum;
{
  sigcaught = signum;
  /*catch_signals ();*/
  signal (signum, catch_crash);

  /* To converge towards BSD signal() semantics: 
     don't handle_interrupt here; postpone (setting sigcaught flag)
     and handle after select(), maybe after read(), elsewhere?
   */
  handle_interrupt (signum);
/*kill (getpid (), SIGABRT); trying to enforce core dump - not working */
}

void
catch_signals ()
{
#if defined (SIGRTMIN) && defined (SIGRTMAX)
  int i;
  for (i = SIGRTMIN; i <= SIGRTMAX; i ++) {
	signal (i, catch_interrupt);
  }
#endif

#ifdef SIGABRT
  signal (SIGABRT, catch_crash);
#endif
#ifdef SIGSEGV
  signal (SIGSEGV, catch_crash);
#endif
#ifdef SIGBUS
  signal (SIGBUS, catch_crash);
#endif
#ifdef SIGFPE
  signal (SIGFPE, catch_crash);
#endif
#ifdef SIGILL
  signal (SIGILL, catch_crash);
#endif

#ifdef SIGHUP
  signal (SIGHUP, catch_interrupt);
#endif
#ifdef SIGTRAP
  signal (SIGTRAP, catch_interrupt);
#endif
#ifdef SIGEMT
  signal (SIGEMT, catch_interrupt);
#endif
#ifdef SIGSYS
  signal (SIGSYS, catch_interrupt);
#endif
#ifdef SIGPIPE
  signal (SIGPIPE, catch_interrupt);
#endif
#ifdef SIGALRM
  signal (SIGALRM, catch_interrupt);
#endif
#ifdef SIGTERM
  signal (SIGTERM, catch_interrupt);
#endif
#ifdef SIGXCPU
  signal (SIGXCPU, catch_interrupt);
#endif
#ifdef SIGXFSZ
  signal (SIGXFSZ, catch_interrupt);
#endif
#ifdef SIGVTALRM
  signal (SIGVTALRM, catch_interrupt);
#endif
#ifdef SIGPROF
  signal (SIGPROF, catch_interrupt);
#endif
#ifdef SIGPWR
  signal (SIGPWR, catch_interrupt);
#endif

#ifdef SIGIO
  /* or should this rather be ignored? */
  signal (SIGIO, catch_interrupt);
#else
#ifdef SIGPOLL
  signal (SIGPOLL, catch_interrupt);
#endif
#endif
#ifdef SIGLOST
  /* linux man 7 signal says "SIGIO and SIGLOST have the same value."
     but on cygwin, they have different values
   */
  signal (SIGLOST, catch_interrupt);
#endif

#ifdef SIGUSR1
  signal (SIGUSR1, catch_interrupt);
#endif
#ifdef SIGUSR2
  signal (SIGUSR2, catch_interrupt);
#endif
#ifdef SIGUSR3
  signal (SIGUSR3, catch_interrupt);
#endif
}

#ifdef SIGWINCH
/*
 * Catch the SIGWINCH signal sent to mined.
 */
static
void
catchwinch (dummy)
  int dummy;
{
  winchg = True;
  interrupted = True;
  trace_winchg (("catching winchg\n"));

  /* workaround for interference of WINCH and FOCUSin handling */
  window_focus = True;

/* This is now performed in __readchar () to prevent display garbage 
   in case this interrupts occurs during display output operations which 
   get screen size related values changed while relying on them.
  if (waitingforinput) {
	RDwinchg ();
  }
*/

  signal (SIGWINCH, catchwinch); /* reinstall the signal */
  kill (getppid (), SIGWINCH);	/* propagate to parent (e.g. shell) */
}
#endif

#ifdef SIGQUIT
/*
 * Catch the SIGQUIT signal (^\) sent to mined. It turns on the quitflag.
 */
static
void
catchquit (dummy)
  int dummy;
{
#ifdef UNUSED /* Should not be needed with new __readchar () */
/* Was previously needed on Sun but showed bad effects on Iris. */
  static char quitchar = '\0';
  if (waitingforinput) {
	/* simulate input to enable immediate break also during input */
	(void) ioctl (input_fd, TIOCSTI, & quitchar);
  }
#endif
  quit = True;
  signal (SIGQUIT, catchquit); /* reinstall the signal */
}
#endif

#ifdef SIGBREAK
/*
 * Catch the SIGBREAK signal (control-Break) and turn on the quitflag.
 */
static
void
catchbreak (dummy)
  int dummy;
{
  quit = True;
  signal (SIGBREAK, catchbreak); /* do we need this ? */
}
#else
# ifdef msdos
static
int
controlbreak ()
{
  quit = True;
  return 1; /* continue program execution */
}
# endif
#endif

#ifdef SIGINT
/*
 * Catch the SIGINT signal (^C) sent if it cannot be ignored by tty driver
 */
static
void
catchint (dummy)
  int dummy;
{
  intr_char = True;
  signal (SIGINT, catchint); /* reinstall the signal */
}
#endif

#ifdef SIGTSTP
/**
   Catch external TSTP signal to try to reset terminal modes
 */
static
void
catchtstp (dummy)
  int dummy;
{
  interrupted = True;
  /* for now, do nothing, effectively ignoring the external signal */
  signal (SIGTSTP, catchtstp); /* reinstall the signal */
}
#endif

#ifdef SIGCONT
/**
   Catch CONT signal to refresh screen
 */
static
void
catchcont (dummy)
  int dummy;
{
  /* trigger screen refresh */
  winchg = True;
  interrupted = True;
  continued = True;
  signal (SIGCONT, catchcont); /* reinstall the signal */
}
#endif


#ifdef SIGTSTP
#ifdef __ANDROID__
FLAG cansuspendmyself = False;
#else
FLAG cansuspendmyself = True;
#endif
#else
FLAG cansuspendmyself = False;
#endif

void
suspendmyself ()
{
#ifdef SIGTSTP
  signal (SIGTSTP, SIG_DFL); /* uninstall catch of external TSTP */
  kill (getpid (), SIGTSTP);
  signal (SIGTSTP, catchtstp); /* reinstall the signal */
#endif
}


/*======================================================================*\
|*			Unix terminal capabilities			*|
\*======================================================================*/

#if defined (unix) || defined (ANSI) || defined (vms)

#ifndef CURSES

static FLAG avoid_tputs = False;

#ifdef __CYGWIN__avoiding_tputs
/* workaround: tputs not working anymore if terminfo not installed, 
   esp. when running stand-alone (without cygwin environment) */
#warning deprecated workaround replaced by dynamic alternative
#define termputstr(str, aff)	putstring (str)
#else
#ifdef vms
#define termputstr(str, aff)	putescape (str)
#else
#ifdef ANSI
#define termputstr(str, aff)	putstring (str)
#else

extern int tputs _((char *, int, intfunc));
/*#define termputstr(str, aff)	(void) tputs (str, aff, (intfunc) __putchar)*/
static
void
termputstr (str, aff)
  char * str;
  int aff;
{
  if (avoid_tputs) {
	putstring (str);
  } else {
	(void) tputs (str, aff, (intfunc) __putchar);
  }
}

#endif
#endif
#endif

static char
     * cCL, * cCE, * cCD, * cEC, * cSR, * cAL, * cDL, * cCS, * cSC, * cRC,
     * cCM, * cSO, * cSE, * cVS, * cVE, * cVI,
     * cTI, * cTE, * cKS, * cKE,
     * cAS, * cAE, * cEA, * cdisableEA, * cAC, * cS2, * cS3,
     * cME, * cMR, * cMH, * cMD, * cUL, * cBL, * cAF, * cAB;

#endif

static char * cMouseEventAnyOn = "\033[?1003h";
static char * cMouseEventAnyOff = "\033[?1003l";
static char * cMouseEventBtnOn = "\033[?1002h";
#ifndef CURSES
static char * cMouseEventBtnOff = "\033[?1002l";
static char * cMouseButtonOn = "\033[?1000h";
static char * cMouseButtonOff = "\033[?1000l";
static char * cMouseX10On = "\033[?9h";
static char * cMouseX10Off = "\033[?9l";

static char * cMouseExtendedOn = "\033[?1005h";
static char * cMouseExtendedOff = "\033[?1005l";
static char * cMouse1015On = "\033[?1015h";
static char * cMouse1015Off = "\033[?1015l";

static char * cMouseFocusOn = "\033[?1004h";
static char * cMouseFocusOff = "\033[?1004l";
static char * cAmbigOn = "\033[?7700h";
static char * cAmbigOff = "\033[?7700l";
#endif

#endif	/* defined (unix) || defined (vms) */


#ifdef unix

/**
   termcap/termlib API and storage
 */
#if ! defined (ANSI) && ! defined (CURSES)
#define TERMAPI
#define TERMCAP	/* disable to use termlib/terminfo API */
#endif

#ifdef CURSES
#define TERMAPI
#undef TERMCAP
#endif

#ifdef __EMX__
#define TERMCAP
#endif


#ifdef __CYGWIN__avoiding_tputs
#warning deprecated workaround replaced by dynamic alternative
/* strip "$<2>" delay suffixes if not using tputs */
static char * tstrfix (tc, tstr)
  char * tc;
  char * tstr;
{
  if (tstr) {
	char * junk = strchr (tstr, '$');
	if (junk && * (junk + 1) == '<') {
		char * fixtstr = malloc (junk - tstr + 1);
		* fixtstr = '\0';
		strncat (fixtstr, tstr, junk - tstr);
		return fixtstr;
	}
  }
  return tstr;
}
#else
#define tstrfix(tc, tstr)	tstr
#endif

#ifdef TERMAPI
#ifdef TERMCAP
/* termcap API */
#define term_setup(TERM)	(tgetent (term_rawbuf, TERM) == 1)
#define term_getstr(tc, ti)	tstrfix (tc, tgetstr (tc, & term_capbufpoi))
#define term_getnum(tc, ti)	tgetnum (tc)
#define term_getflag(tc, ti)	tgetflag (tc)

#else	/* #ifdef TERMCAP */
/* terminfo API */
#define term_setup(TERM)	(setterm (TERM) == 0)
#define term_getstr(tc, ti)	tstrfix (tc, tigetstr (ti))
#define term_getnum(tc, ti)	tigetnum (ti)
#define term_getflag(tc, ti)	tigetflag (ti)

#endif	/* #else TERMCAP */
#endif


#ifndef CURSES

#ifdef TERMCAP

extern int tgetent _((char * rawbuf, char * TERM));
extern char * tgetstr _((char *, char * * capbufpoi));
extern int tgetnum _((char *));
extern int tgetflag _((char *));

#define term_capbuflen 1024
static char term_capbuf [term_capbuflen];
static char * term_capbufpoi = term_capbuf;

#else	/* #ifdef TERMCAP */

#ifdef BSD
extern int setterm _((char * TERM));
#else
extern int setupterm _((char * TERM, int , int *));
#define setterm(TERM)	setupterm (TERM, 1, 0)
#endif
extern char * tigetstr _((char *));
extern int tigetnum _((char *));
extern int tigetflag _((char *));

#endif	/* #else TERMCAP */


extern char * tgoto _((char *, int, int));

#define aff1 0
#define affmax YMAX

#endif	/* #ifndef CURSES */


#define dont_debug_terminfo

#ifndef ANSI

#define fkeymap_terminfo_len 155
struct fkeyentry fkeymap_terminfo [fkeymap_terminfo_len + 1] = {
	{NIL_PTR}
};
static int fkeymap_terminfo_i = 0;

static
void
add_keymap_entry (cap, f, shift_state)
  char * cap;
  voidfunc f;
  unsigned char shift_state;
{
  if (fkeymap_terminfo_i < fkeymap_terminfo_len) {
	fkeymap_terminfo [fkeymap_terminfo_i].fk = cap;
	fkeymap_terminfo [fkeymap_terminfo_i].fp = f;
	fkeymap_terminfo [fkeymap_terminfo_i].fkeyshift = shift_state;
	fkeymap_terminfo_i ++;
	fkeymap_terminfo [fkeymap_terminfo_i].fk = NIL_PTR;
  }
}


static
char *
add_terminfo_entry (tc, ti, f, shift_state)
  char * tc;
  char * ti;
  voidfunc f;
  unsigned char shift_state;
{
  if (f) {
	char * cap = term_getstr (tc, ti);
	if (cap) {
		add_keymap_entry (cap, f, shift_state);
#ifdef debug_terminfo
		printf ("termcap %s / terminfo %s : <^[%s> [shift %X]\n", tc, ti, cap + 1, shift_state);
#endif
		return cap;
	}
  }
  return 0;
}

static
void
add_terminfo_entries ()
{
  char * caphome;
  char * capend;
  caphome = add_terminfo_entry ("K1", "ka1",	HOMEkey, 0);	/* upper left of keypad */
  (void) add_terminfo_entry ("K3", "ka3",	PGUPkey, 0);	/* upper right of keypad */
  (void) add_terminfo_entry ("K2", "kb2",	HOP, 0);	/* center of keypad */
  capend = add_terminfo_entry ("K4", "kc1",	ENDkey, 0);	/* lower left of keypad */
  (void) add_terminfo_entry ("K5", "kc3",	PGDNkey, 0);	/* lower right of keypad */

  if (caphome && capend) {
    /* if keypad keys are explicitly defined, use them to assign HOME/END;
       if home/begin/home_down/end key definitions refer to different keys, 
       map "small" keypad functions to them;
       if they happen to refer to the same keys, the first setting 
       will have precedence
     */
    (void) add_terminfo_entry ("kh", "khome",	smallHOMEkey, 0);	/* home key */
    (void) add_terminfo_entry ("kH", "kll",	smallENDkey, 0);	/* lower-left key (home down) */
    (void) add_terminfo_entry ("@1", "kbeg",	smallHOMEkey, 0);	/* begin key */
    (void) add_terminfo_entry ("@7", "kend",	smallENDkey, 0);	/* end key */
    (void) add_terminfo_entry ("#2", "kHOM",	HOMEkey, shift_mask);	/* shifted home key */
    (void) add_terminfo_entry ("&9", "kBEG",	HOMEkey, shift_mask);	/* shifted begin key */
    (void) add_terminfo_entry ("*7", "kEND",	ENDkey, shift_mask);	/* shifted end key */
  } else {
    /* if keypad keys are not explicitly defined,
       assign HOME/END to home/begin and home_down/end keys
     */
    (void) add_terminfo_entry ("kh", "khome",	HOMEkey, 0);	/* home key */
    (void) add_terminfo_entry ("kH", "kll",	ENDkey, 0);	/* lower-left key (home down) */
    (void) add_terminfo_entry ("@1", "kbeg",	HOMEkey, 0);	/* begin key */
    (void) add_terminfo_entry ("@7", "kend",	ENDkey, 0);	/* end key */
    (void) add_terminfo_entry ("#2", "kHOM",	smallHOMEkey, shift_mask);	/* shifted home key */
    (void) add_terminfo_entry ("&9", "kBEG",	smallHOMEkey, shift_mask);	/* shifted begin key */
    (void) add_terminfo_entry ("*7", "kEND",	smallENDkey, shift_mask);	/* shifted end key */
  }

  (void) add_terminfo_entry ("kb", "kbs",	DPC, 0);	/* backspace key */
  (void) add_terminfo_entry ("kB", "kcbt",	MPW, 0);	/* back-tab key */
  (void) add_terminfo_entry ("@2", "kcan",	0, 0);	/* cancel key */
  (void) add_terminfo_entry ("ka", "ktbc",	0, 0);	/* clear-all-tabs key */
  (void) add_terminfo_entry ("kC", "kclr",	0, 0);	/* clear-screen or erase key */
  (void) add_terminfo_entry ("@3", "kclo",	0, 0);	/* close key */
  (void) add_terminfo_entry ("@4", "kcmd",	HOP, 0);	/* command key */
  (void) add_terminfo_entry ("@5", "kcpy",	COPY, 0);	/* copy key */
  (void) add_terminfo_entry ("@6", "kcrt",	0, 0);	/* create key */
  (void) add_terminfo_entry ("kt", "kctab",	0, 0);	/* clear-tab key */
  (void) add_terminfo_entry ("kD", "kdch1",	DELkey, 0);	/* delete-character key */
  (void) add_terminfo_entry ("kL", "kdl1",	0, 0);	/* delete-line key */
  (void) add_terminfo_entry ("kd", "kcud1",	DNkey, 0);	/* down-arrow key */
  (void) add_terminfo_entry ("kM", "krmir",	0, 0);	/* sent by rmir or smir in insert mode */
  (void) add_terminfo_entry ("@8", "kent",	SNL, 0);	/* enter/send key */
  (void) add_terminfo_entry ("kE", "kel",	0, 0);	/* clear-to-end-of-line key */
  (void) add_terminfo_entry ("kS", "ked",	0, 0);	/* clear-to-end-of-screen key */
  (void) add_terminfo_entry ("@9", "kext",	0, 0);	/* exit key */

  (void) add_terminfo_entry ("k0", "kf0",	F10, 0);	/* F0 function key */
  (void) add_terminfo_entry ("k1", "kf1",	F1, 0);	/* F1 function key */
  (void) add_terminfo_entry ("k2", "kf2",	F2, 0);	/* F2 function key */
  (void) add_terminfo_entry ("k3", "kf3",	F3, 0);	/* F3 function key */
  (void) add_terminfo_entry ("k4", "kf4",	F4, 0);	/* F4 function key */
  (void) add_terminfo_entry ("k5", "kf5",	F5, 0);	/* F5 function key */
  (void) add_terminfo_entry ("k6", "kf6",	F6, 0);	/* F6 function key */
  (void) add_terminfo_entry ("k7", "kf7",	F7, 0);	/* F7 function key */
  (void) add_terminfo_entry ("k8", "kf8",	F8, 0);	/* F8 function key */
  (void) add_terminfo_entry ("k9", "kf9",	F9, 0);	/* F9 function key */
  (void) add_terminfo_entry ("k;", "kf10",	F10, 0);	/* F10 function key */

#ifdef shift_f_keys
	there is no uniform mapping of actual shifted function keys 
	to terminfo F11..F63 names, so better leave these entries out
  (void) add_terminfo_entry ("F1", "kf11",	F11, 0);	/* F11 function key */
  (void) add_terminfo_entry ("F2", "kf12",	F12, 0);	/* F12 function key */
  (void) add_terminfo_entry ("F3", "kf13",	F1, shift_mask);	/* F13 function key */
  (void) add_terminfo_entry ("F4", "kf14",	F2, shift_mask);	/* F14 function key */
  (void) add_terminfo_entry ("F5", "kf15",	F3, shift_mask);	/* F15 function key */
  (void) add_terminfo_entry ("F6", "kf16",	F4, shift_mask);	/* F16 function key */
  (void) add_terminfo_entry ("F7", "kf17",	F5, shift_mask);	/* F17 function key */
  (void) add_terminfo_entry ("F8", "kf18",	F6, shift_mask);	/* F18 function key */
  (void) add_terminfo_entry ("F9", "kf19",	F7, shift_mask);	/* F19 function key */
  (void) add_terminfo_entry ("FA", "kf20",	F8, shift_mask);	/* F20 function key */
  (void) add_terminfo_entry ("FB", "kf21",	F9, shift_mask);	/* F21 function key */
  (void) add_terminfo_entry ("FC", "kf22",	F10, shift_mask);	/* F22 function key */
  (void) add_terminfo_entry ("FD", "kf23",	F11, shift_mask);	/* F23 function key */
  (void) add_terminfo_entry ("FE", "kf24",	F12, shift_mask);	/* F24 function key */
  (void) add_terminfo_entry ("FF", "kf25",	F1, ctrl_mask);	/* F25 function key */
  (void) add_terminfo_entry ("FG", "kf26",	F2, ctrl_mask);	/* F26 function key */
  (void) add_terminfo_entry ("FH", "kf27",	F3, ctrl_mask);	/* F27 function key */
  (void) add_terminfo_entry ("FI", "kf28",	F4, ctrl_mask);	/* F28 function key */
  (void) add_terminfo_entry ("FJ", "kf29",	F5, ctrl_mask);	/* F29 function key */
  (void) add_terminfo_entry ("FK", "kf30",	F6, ctrl_mask);	/* F30 function key */
  (void) add_terminfo_entry ("FL", "kf31",	F7, ctrl_mask);	/* F31 function key */
  (void) add_terminfo_entry ("FM", "kf32",	F8, ctrl_mask);	/* F32 function key */
  (void) add_terminfo_entry ("FN", "kf33",	F9, ctrl_mask);	/* F33 function key */
  (void) add_terminfo_entry ("FO", "kf34",	F10, ctrl_mask);	/* F34 function key */
  (void) add_terminfo_entry ("FP", "kf35",	F11, ctrl_mask);	/* F35 function key */
  (void) add_terminfo_entry ("FQ", "kf36",	F12, ctrl_mask);	/* F36 function key */
  (void) add_terminfo_entry ("FR", "kf37",	F1, ctrlshift_mask);	/* F37 function key */
  (void) add_terminfo_entry ("FS", "kf38",	F2, ctrlshift_mask);	/* F38 function key */
  (void) add_terminfo_entry ("FT", "kf39",	F3, ctrlshift_mask);	/* F39 function key */
  (void) add_terminfo_entry ("FU", "kf40",	F4, ctrlshift_mask);	/* F40 function key */
  (void) add_terminfo_entry ("FV", "kf41",	F5, ctrlshift_mask);	/* F41 function key */
  (void) add_terminfo_entry ("FW", "kf42",	F6, ctrlshift_mask);	/* F42 function key */
  (void) add_terminfo_entry ("FX", "kf43",	F7, ctrlshift_mask);	/* F43 function key */
  (void) add_terminfo_entry ("FY", "kf44",	F8, ctrlshift_mask);	/* F44 function key */
  (void) add_terminfo_entry ("FZ", "kf45",	F9, ctrlshift_mask);	/* F45 function key */
  (void) add_terminfo_entry ("Fa", "kf46",	F10, ctrlshift_mask);	/* F46 function key */
  (void) add_terminfo_entry ("Fb", "kf47",	F11, ctrlshift_mask);	/* F47 function key */
  (void) add_terminfo_entry ("Fc", "kf48",	F12, ctrlshift_mask);	/* F48 function key */
  (void) add_terminfo_entry ("Fd", "kf49",	F1, alt_mask);	/* F49 function key */
  (void) add_terminfo_entry ("Fe", "kf50",	F2, alt_mask);	/* F50 function key */
  (void) add_terminfo_entry ("Ff", "kf51",	F3, alt_mask);	/* F51 function key */
  (void) add_terminfo_entry ("Fg", "kf52",	F4, alt_mask);	/* F52 function key */
  (void) add_terminfo_entry ("Fh", "kf53",	F5, alt_mask);	/* F53 function key */
  (void) add_terminfo_entry ("Fi", "kf54",	F6, alt_mask);	/* F54 function key */
  (void) add_terminfo_entry ("Fj", "kf55",	F7, alt_mask);	/* F55 function key */
  (void) add_terminfo_entry ("Fk", "kf56",	F8, alt_mask);	/* F56 function key */
  (void) add_terminfo_entry ("Fl", "kf57",	F9, alt_mask);	/* F57 function key */
  (void) add_terminfo_entry ("Fm", "kf58",	F10, alt_mask);	/* F58 function key */
  (void) add_terminfo_entry ("Fn", "kf59",	F11, alt_mask);	/* F59 function key */
  (void) add_terminfo_entry ("Fo", "kf60",	F12, alt_mask);	/* F60 function key */
  (void) add_terminfo_entry ("Fp", "kf61",	F1, altshift_mask);	/* F61 function key */
  (void) add_terminfo_entry ("Fq", "kf62",	F2, altshift_mask);	/* F62 function key */
  (void) add_terminfo_entry ("Fr", "kf63",	F3, altshift_mask);	/* F63 function key */
#endif

  (void) add_terminfo_entry ("@0", "kfnd",	FIND, 0);	/* find key */
  (void) add_terminfo_entry ("%1", "khlp",	HELP, 0);	/* help key */
  (void) add_terminfo_entry ("kI", "kich1",	INSkey, 0);	/* insert-character key */
  (void) add_terminfo_entry ("kA", "kil1",	0, 0);	/* insert-line key */
  (void) add_terminfo_entry ("kl", "kcub1",	LFkey, 0);	/* left-arrow key */
  (void) add_terminfo_entry ("%2", "kmrk",	MARK, 0);	/* mark key */
  (void) add_terminfo_entry ("%3", "kmsg",	0, 0);	/* message key */
  (void) add_terminfo_entry ("%4", "kmov",	0, 0);	/* move key */
  (void) add_terminfo_entry ("%5", "knxt",	0, 0);	/* next key */
  (void) add_terminfo_entry ("kN", "knp",	PGDNkey, 0);	/* next-page key */
  (void) add_terminfo_entry ("%6", "kopn",	EDIT, 0);	/* open key */
  (void) add_terminfo_entry ("%7", "kopt",	QUICKMENU, 0);	/* options key */
  (void) add_terminfo_entry ("kP", "kpp",	PGUPkey, 0);	/* previous-page key */
  (void) add_terminfo_entry ("%8", "kprv",	0, 0);	/* previous key */
  (void) add_terminfo_entry ("%9", "kprt",	PRINT, 0);	/* print key */
  (void) add_terminfo_entry ("%0", "krdo",	COPY, 0);	/* redo key */
  (void) add_terminfo_entry ("&1", "kref",	0, 0);	/* reference key */
  (void) add_terminfo_entry ("&2", "krfr",	0, 0);	/* refresh key */
  (void) add_terminfo_entry ("&3", "krpl",	GR, 0);	/* replace key */
  (void) add_terminfo_entry ("&4", "krst",	0, 0);	/* restart key */
  (void) add_terminfo_entry ("&5", "kres",	AGAIN, 0);	/* resume key */
  (void) add_terminfo_entry ("kr", "kcuf1",	RTkey, 0);	/* right-arrow key */
  (void) add_terminfo_entry ("&6", "ksav",	WT, 0);	/* save key */
  (void) add_terminfo_entry ("&0", "kCAN",	0, shift_mask);	/* shifted cancel key */
  (void) add_terminfo_entry ("*1", "kCMD",	HOP, shift_mask);	/* shifted command key */
  (void) add_terminfo_entry ("*2", "kCPY",	COPY, shift_mask);	/* shifted copy key */
  (void) add_terminfo_entry ("*3", "kCRT",	0, shift_mask);	/* shifted create key */
  (void) add_terminfo_entry ("*4", "kDC",	DELkey, shift_mask);	/* shifted delete-character key */
  (void) add_terminfo_entry ("*5", "kDL",	0, shift_mask);	/* shifted delete-line key */
  (void) add_terminfo_entry ("*6", "kslt",	MARK, 0);	/* select key */
  (void) add_terminfo_entry ("*8", "kEOL",	0, shift_mask);	/* shifted clear-to-end-of-line key */
  (void) add_terminfo_entry ("*9", "kEXT",	0, shift_mask);	/* shifted exit key */
  (void) add_terminfo_entry ("kF", "kind",	SD, 0);	/* scroll-forward key */
  (void) add_terminfo_entry ("*0", "kFND",	FIND, shift_mask);	/* shifted find key */
  (void) add_terminfo_entry ("#1", "kHLP",	HELP, shift_mask);	/* shifted help key */
  (void) add_terminfo_entry ("#3", "kIC",	INSkey, shift_mask);	/* shifted insert-character key */
  (void) add_terminfo_entry ("#4", "kLFT",	LFkey, shift_mask);	/* shifted left-arrow key */
  (void) add_terminfo_entry ("%a", "kMSG",	0, shift_mask);	/* shifted message key */
  (void) add_terminfo_entry ("%b", "kMOV",	0, shift_mask);	/* shifted move key */
  (void) add_terminfo_entry ("%c", "kNXT",	PGDNkey, shift_mask);	/* shifted next key */
  (void) add_terminfo_entry ("%d", "kOPT",	handleFlagmenus, shift_mask);	/* shifted options key */
  (void) add_terminfo_entry ("%e", "kPRV",	PGUPkey, shift_mask);	/* shifted previous key */
  (void) add_terminfo_entry ("%f", "kPRT",	PRINT, shift_mask);	/* shifted print key */
  (void) add_terminfo_entry ("kR", "kri",	SU, 0);	/* scroll-backward key */
  (void) add_terminfo_entry ("%g", "kRDO",	COPY, shift_mask);	/* shifted redo key */
  (void) add_terminfo_entry ("%h", "kRPL",	REPL, shift_mask);	/* shifted replace key */
  (void) add_terminfo_entry ("%i", "kRIT",	RTkey, shift_mask);	/* shifted right-arrow key */
  (void) add_terminfo_entry ("%j", "kRES",	AGAIN, shift_mask);	/* shifted resume key */
  (void) add_terminfo_entry ("!1", "kSAV",	WTU, shift_mask);	/* shifted save key */
  (void) add_terminfo_entry ("!2", "kSPD",	SUSP, shift_mask);	/* shifted suspend key */
  (void) add_terminfo_entry ("kT", "khts",	0, 0);	/* set-tab key */
  (void) add_terminfo_entry ("!3", "kUND",	UNDO, shift_mask);	/* shifted undo key */
  (void) add_terminfo_entry ("&7", "kspd",	SUSP, 0);	/* suspend key */
  (void) add_terminfo_entry ("&8", "kund",	UNDO, 0);	/* undo key */
  (void) add_terminfo_entry ("ku", "kcuu1",	UPkey, 0);	/* up-arrow key */
  (void) add_terminfo_entry ("Km", "kmous",	DIRECTxterm, 0);	/* Mouse event has occurred */
}

#endif	/* #else ANSI */

#endif	/* #ifdef unix */


#ifdef ANSI
struct fkeyentry fkeymap_terminfo [1] = {
	{NIL_PTR}
};
#endif


/*======================================================================*\
|*			TTY and terminal mode switcher			*|
\*======================================================================*/

static void start_screen_mode _((int kb));
static void end_screen_mode _((void));
static void setup_terminal _((void));
static void reset_terminal _((void));


#ifdef CURSES

#define use_newterm	/* otherwise, redirection to stdout doesn't work */

#ifdef __EMX__		/* __PDCURSES__ ? */
#undef use_newterm
#endif
#ifdef msdos
#undef use_newterm
#endif
#ifdef vms
#undef use_newterm
#endif

static FLAG screen_acquired = False;
static void configure_screen _((void));
# ifdef use_newterm
static SCREEN * myscreen;
extern FILE * fdopen ();
# endif

#endif


#ifdef TERMIO
static FLAG old_termprops_not_saved = True;
#endif
#ifdef SGTTY
static FLAG old_termprops_not_saved = True;
#endif

static FLAG init_done = False;

/* the termios ISIG flag suppresses signal generation on 
   tty input of INTR, QUIT, SUSP, or DSUSP associated keys */
#define dont_use_ISIG

#ifdef TERMIO
#ifdef debug_termios
static
void
dump_termios (tios)
  struct termios * tios;
{
	int i;
	printf ("iflag %04X\n", tios->c_iflag);
	printf ("oflag %04X\n", tios->c_oflag);
	printf ("cflag %04X\n", tios->c_cflag);
	printf ("lflag %04X\n", tios->c_lflag);
	printf ("line  %02X\n", tios->c_line);
	for (i = 0; i < NCCS; i ++) {
		printf ("cc[%d] %02X\n", i, tios->c_cc [i]);
	}
}
#endif
#endif

/*
 * Set (True) or Restore (False) tty raw mode.
 * Also set signal characters (except for ^\) to UNDEF. ^\ is caught.
 */
void
raw_mode (to_raw_state)
  FLAG to_raw_state;
{
#ifdef TERMIO
  static struct termios old_termio;
	 struct termios new_termio;
#ifdef TCGETS
# define gettermio(fd, iopoi)	ioctl (fd, TCGETS, iopoi)
# ifdef TCSETSW
# define settermio(fd, iopoi)	ioctl (fd, TCSETSW, iopoi)
# else
# define settermio(fd, iopoi)	ioctl (fd, TCSETS, iopoi)
# endif
#else
# define gettermio(fd, iopoi)	tcgetattr (fd, iopoi)
# ifdef TCSADRAIN
# define settermio(fd, iopoi)	tcsetattr (fd, TCSADRAIN, iopoi)
# else
# define settermio(fd, iopoi)	tcsetattr (fd, TCSANOW, iopoi)
# endif
#endif
#endif /* TERMIO */

#ifdef SGTTY
  static struct sgttyb old_tty;
	 struct sgttyb new_tty;
  static int oldlmode;
	 int lmode;
  static struct tchars old_tchars;
  static struct ltchars old_ltchars;

/* correspondence between the tchars/ltchars characters of the sgtty 
   interface and the c_cc characters of the termios interface (codes vary):
	sgtty		termio		sgtty		termio
	struct tchars			struct ltchars
	t_intrc		VINTR		t_suspc		VSUSP
	t_quitc		VQUIT		t_dsuspc	VDSUSP
	t_startc	VSTART		t_rprntc	VREPRINT
	t_stopc		VSTOP		t_flushc	VDISCARD
	t_eofc		VEOF (VMIN)	t_werasc	VWERASE
	t_brkc		VEOL (VTIME)	t_lnextc	VLNEXT
*/
#define NDEF '\377'
  static struct tchars new_tchars = {NDEF, 0, NDEF, NDEF, NDEF, NDEF};
  static struct tchars new_QStchars = {NDEF, 0, '\021', '\023', NDEF, NDEF};
  static struct ltchars new_ltchars = {NDEF, NDEF, NDEF, NDEF, NDEF, NDEF};

  new_tchars.t_quitc = quit_char;
  new_QStchars.t_quitc = quit_char;
#endif /* SGTTY */


  if (to_raw_state == False) {
	isscreenmode = False;

/* Reset screen */

	reset_terminal ();
	end_screen_mode ();

#ifndef CURSES
	flush ();
#endif


/* Reset tty */
#ifdef CURSES
	savetty ();
	endwin ();
# ifdef vms
	(void) system ("set terminal /ttsync /nopasthru");
# endif
#else
#ifdef vms
	ttclose ();
#endif
#endif

#ifdef TERMIO
	if (settermio (tty_fd, & old_termio) < 0) {
/*printf ("settermio failed: %s\n", serror ());*/
	}
/*	one byte of output swallowed here on SunOS	*/
#endif
#ifdef SGTTY
	(void) ioctl (tty_fd, TIOCSETP, & old_tty);
	(void) ioctl (tty_fd, TIOCSETC, & old_tchars);
/*	one byte of output swallowed here on SunOS	*/
	(void) ioctl (tty_fd, TIOCSLTC, & old_ltchars);
	(void) ioctl (tty_fd, TIOCLSET, & oldlmode);
#endif

/*	output a byte that SunOS may swallow:	*/
	(void) write (output_fd, " \r", 2);

#if defined (TIOCEXCL) && defined (TIOCNXCL)
	/* release tty lock */
	(void) ioctl (tty_fd, TIOCNXCL);
#endif

	return;
  }


  else /* (to_raw_state == True) */ {
	if (isscreenmode) {
		/* first terminal mode initialization is delayed 
		   and de-coupled from tty configuration */
		start_screen_mode (1);
		setup_terminal ();
		init_done = True;
		return;
	}

	/* first call: don't rely on checkwinsize() yet! */

	isscreenmode = True;

/* Init tty */
#ifdef CURSES
# ifdef vms
	(void) system ("set terminal /pasthru /nottsync");
# endif

	if (screen_acquired == False) {
# ifdef use_newterm
		FILE * ttyin = fdopen (input_fd, "r");
		FILE * ttyout = fdopen (output_fd, "w");
		myscreen = newterm (NIL_PTR, ttyout, ttyin);
		if (! myscreen) {
			printf ("Cannot open terminal: %s\n", envvar ("TERM"));
			exit (-1);
		}
# else
		initscr ();
# endif

		add_terminfo_entries ();

		screen_acquired = True;
		configure_screen ();
	} else {
		resetty ();
	}

	getwinsize ();
#else
#ifdef vms
	ttopen ();
#endif
#endif

#ifdef TERMIO
# ifdef ioctl_twice
	if (old_termprops_not_saved) {
		/* Save old tty settings */
		if (gettermio (tty_fd, & old_termio) < 0) {
/*printf ("gettermio failed: %s\n", serror ());*/
		}

		/* Determine configured erase character */
		erase_char = old_termio.c_cc [VERASE];
		/*kill_char = old_termio.c_cc [VKILL];*/

		old_termprops_not_saved = False;
	}
# endif

	if (gettermio (tty_fd, & new_termio) < 0) {
/*printf ("gettermio failed: %s\n", serror ());*/
	}

# ifndef ioctl_twice
	if (old_termprops_not_saved) {
		/* Save old tty settings */
		memcpy (& old_termio, & new_termio, sizeof (struct termios));

		/* Determine configured erase character */
		erase_char = old_termio.c_cc [VERASE];
		/*kill_char = old_termio.c_cc [VKILL];*/

		old_termprops_not_saved = False;
	}
# endif

	if (controlQS == False) {
		new_termio.c_iflag &= ~(ISTRIP|IXON|IXOFF);
	} else {
		new_termio.c_iflag &= ~(ISTRIP);
	}
	new_termio.c_oflag &= ~OPOST;
	new_termio.c_cflag &= ~(PARENB|CSIZE);
	new_termio.c_cflag |= CS8;
	new_termio.c_lflag &= ~(ICANON|ECHO
#ifdef use_ISIG
#ifdef ISIG
		|ISIG
#endif
#endif
		);
	new_termio.c_iflag &= ~ICRNL;	/* pass through ^M */
#define NDEF '\000'
	new_termio.c_cc [VMIN] = 1;
#ifdef selectread
	new_termio.c_cc [VTIME] = 0;
#else
	/* let read loop monitor WINCH */
	new_termio.c_cc [VTIME] = 3;	/* 3 is good trade-off */
#endif
	new_termio.c_cc [VQUIT] = quit_char;	/* don't use ISIG! */
	new_termio.c_cc [VINTR] = NDEF;
	new_termio.c_cc [VSUSP] = NDEF;
#ifdef VSTOP
	new_termio.c_cc [VSTOP] = NDEF;	/* or/also use IXOFF? */
#endif
#ifdef VDISCARD
	new_termio.c_cc [VDISCARD] = NDEF;
#endif
#ifdef VDSUSP
	new_termio.c_cc [VDSUSP] = NDEF;
#endif
#ifdef VLNEXT
	new_termio.c_cc [VLNEXT] = NDEF;
#endif
	if (settermio (tty_fd, & new_termio) < 0) {
/*printf ("settermio failed: %s\n", serror ());*/
	}
#endif /* TERMIO */
#ifdef SGTTY
	if (old_termprops_not_saved) {
		/* Save old tty settings */
		(void) ioctl (tty_fd, TIOCGETP, & old_tty);
		(void) ioctl (tty_fd, TIOCGETC, & old_tchars);
		(void) ioctl (tty_fd, TIOCGLTC, & old_ltchars);
		(void) ioctl (tty_fd, TIOCLGET, & oldlmode);

		/* Determine configured erase character */
		erase_char = old_tty.sg_erase;
		/*kill_char = old_tty.sg_kill;*/

		old_termprops_not_saved = False;
	}

	/* Set line mode */
	/* If this feature should not be available on some system, 
	   RAW must be used instead of CBREAK below to enable 8 bit output */
	lmode = oldlmode;
	lmode |= LPASS8; /* enable 8 bit characters on input in CBREAK mode */
	lmode |= LLITOUT; /* enable 8 bit characters on output in CBREAK mode;
	    this may not be necessary in newer Unixes, e.g. SunOS 4;
	    output handling is slightly complicated by LITOUT */
	(void) ioctl (tty_fd, TIOCLSET, & lmode);
	/* Set tty to CBREAK (or RAW) mode */
	new_tty = old_tty;
	new_tty.sg_flags &= ~ECHO;
	new_tty.sg_flags |= CBREAK;
	(void) ioctl (tty_fd, TIOCSETP, & new_tty);
	/* Unset signal chars */
	if (controlQS == False) {
		/* leave quit_char */
		(void) ioctl (tty_fd, TIOCSETC, & new_tchars);
	} else {
		/* leave quit_char, ^Q, ^S */
		(void) ioctl (tty_fd, TIOCSETC, & new_QStchars);
	}
	(void) ioctl (tty_fd, TIOCSLTC, & new_ltchars); /* Leaves nothing */
#endif /* SGTTY */

#if defined (TIOCEXCL) && defined (TIOCNXCL)
	/* prevent from other processes spoiling the editing screen;
	   acquire tty lock
		- does not lock against root processes writing to tty
		- does not work on cygwin (as of 1.7.30)
	 */
	(void) ioctl (tty_fd, TIOCEXCL);
#endif


/* Init screen */
	if (init_done) { /* postpone on first invocation of raw_mode */
		/* keyboard modes */
		start_screen_mode (1);
		/* specific terminal modes */
		setup_terminal ();
	}
	/* do not postpone actual screen mode */
	start_screen_mode (0);

#ifdef CURSES
	refresh ();
	/* Determine configured erase character */
	erase_char = erasechar ();
	/*kill_char = killchar ();*/
#else
	flush ();
#endif

/* Setup specific signal handlers */
#ifdef SIGQUIT
	signal (SIGQUIT, catchquit);	/* Catch quit_char */
#endif
#ifdef SIGBREAK
	signal (SIGBREAK, catchbreak);	/* control-Break (e.g. OS/2) */
#else
# ifdef msdos_suppressbreak
	ctrlbrk (controlbreak);	/* completely useless, stupid Turbo-C! */
# endif
#endif
#ifdef SIGINT
	signal (SIGINT, catchint);	/* Catch INTR char (^C) */
#endif
#ifdef SIGWINCH
	signal (SIGWINCH, catchwinch);	/* Catch window size changes */
#endif
#ifdef SIGTSTP
	signal (SIGTSTP, catchtstp);
#endif
#ifdef SIGCONT
	signal (SIGCONT, catchcont);
#endif
  }
}


/*======================================================================*\
|*			Terminal mode setup				*|
\*======================================================================*/

/**
   temporarily disable modifyOtherKeys for shifted letters
 */
void
disable_modify_keys (dis)
  FLAG dis;
{
#ifdef unix
  if (use_modifyOtherKeys && xterm_version > 0) {
	if (dis) {
		putescape ("\033[>4m");
	} else {
		putescape ("\033[>4;2m");
	}
	flush ();
  }
#endif
}


FLAG redefined_ansi2 = False;

#if defined (unix) || defined (ANSI) || defined (vms)

FLAG redefined_ansi3 = False;
FLAG redefined_ansi7 = False;
FLAG redefined_curscolr = False;

char set_ansi2 [27];
char restore_ansi2 [27];
char set_ansi3 [27];
char restore_ansi3 [27];
char set_ansi7 [27];
char restore_ansi7 [27];
char set_curscolr [27];
char restore_curscolr [27];

static void
setup_terminal ()
{
  /* set redefined ANSI colors */
  if (redefined_ansi7) {
	putescape (set_ansi7);
  }
  if (redefined_ansi2) {
	putescape (set_ansi2);
  }
  if (redefined_ansi3) {
	putescape (set_ansi3);
  }
  if (redefined_curscolr) {
	putescape (set_curscolr);
  }

  /* set special keyboard modes */
  if (use_modifyOtherKeys) {
	putescape ("\033[>4;2m");
  }

  /* screen style */
  if (cursor_style) {
	static char cs [15];
	build_string (cs, "\033[%d q", cursor_style);
	putescape (cs);
  }

  /* specific terminal configuration */
  if (configure_xterm_keyboard && xterm_version > 0) {
	putescape ("\033[?1037s"); /* save DEC mode value */
	/* send DEL from the editing‐keypad Delete key */
	putescape ("\033[?1037h");
	if (xterm_version < 122) {
		putescape ("\033[?1034s"); /* save DEC mode value */
		/* send ESC when Meta modifies a key (eightBitInput: false) */
		putescape ("\033[?1034l");
	} else {
		putescape ("\033[?1036s"); /* save DEC mode value */
		/* send ESC when Meta modifies a key (metaSendsEscape: true) */
		putescape ("\033[?1036h");
	}
  }
#ifdef mintty_scrollbar_workaround
  if (mintty_version > 0) {
	putescape ("\033[?30l"); /* disable scrollbar to facilitate mouse wheel */
# ifdef debug_terminal_resize
	printf ("disabling scrollbar\n");
# endif
  }
#endif
  if (mintty_version >= 403) {
	/* this mode interferes with fkeymap_rxvt 
	   if inappropriately TERM=rxvt in mintty (workaround in mined1.c) */
	putescape ("\033[?7727h"); /* application escape key mode: ESC sends ^[O[ */
  }
  if (mintty_version >= 501) {
	putescape ("\033[?7783h"); /* shortcut override on */
  }
}

static void
reset_terminal ()
{
  /* restore redefined ANSI colors */
  if (redefined_ansi7) {
	putescape (restore_ansi7);
  }
  if (redefined_ansi2) {
	putescape (restore_ansi2);
  }
  if (redefined_ansi3) {
	putescape (restore_ansi3);
  }
  if (redefined_curscolr) {
	putescape (restore_curscolr);
  }

  /* reset special keyboard modes */
  if (use_modifyOtherKeys) {
#ifdef reset_modifyOtherKeys
	if (mintty_version == 400) {
		putescape ("\033[>4;0m");
	} else {
		putescape ("\033[>4m");
	}
#else
	putescape ("\033[>4m");
#endif
  }

/* screen style */
  if (cursor_style) {
	putescape ("\033[0 q");
  }

  /* restore specific terminal modes */
  if (configure_xterm_keyboard && xterm_version > 0) {
	putescape ("\033[?1037r"); /* restore DEC mode value */
	if (xterm_version < 122) {
		putescape ("\033[?1034r"); /* restore DEC mode value */
	} else {
		putescape ("\033[?1036r"); /* restore DEC mode value */
	}
  }
#ifdef mintty_scrollbar_workaround
  if (mintty_version > 0) {
	putescape ("\033[?30h"); /* enable scrollbar if it was enabled */
# ifdef debug_terminal_resize
	printf ("enabling scrollbar\n");
# endif
  }
#endif
  if (mintty_version >= 403) {
	putescape ("\033[?7727l"); /* reset application escape key mode */
  }
  if (mintty_version >= 501) {
	putescape ("\033[?7783l"); /* shortcut override off */
  }
}

#else

static void
setup_terminal ()
{
}

static void
reset_terminal ()
{
}

#endif


/*======================================================================*\
|*			Escape code I/O routine				*|
\*======================================================================*/

#ifdef CURSES
void
_putescape (s)
  char * s;
{
	/* synchronize with buffered screen output */
	flush ();
	/* bypass curses for escape sequence */
	if (write (output_fd, s, strlen (s)) <= 0) {
		/* terminal write error; cannot do anything */
	}
}
#else
#define _putescape(s)	putstring (s)
#endif

void
putescape (s)
  char * s;
{
  if (screen_version > 0) {
	/* pass through transparently to host terminal: */
	_putescape ("\033P");
	_putescape (s);
	_putescape ("\033\\");
  } else {
	_putescape (s);
  }
}


/*======================================================================*\
|*			Menu borders					*|
\*======================================================================*/

/*	block graphics mapping
	note: VGA (PC) encoding also used on Linux console!
VT100	VGA	curses
j	D9	ACS_LRCORNER
k	BF	ACS_URCORNER
l	DA	ACS_ULCORNER
m	C0	ACS_LLCORNER
n		ACS_PLUS
o		ACS_S1
p		ACS_S3
q	C4	ACS_HLINE
r		ACS_S7
s		ACS_S9
t	C3	ACS_LTEE
u	B4	ACS_RTEE
v	C1	ACS_BTEE
w	C2	ACS_TTEE
x	B3	ACS_VLINE
*/

/**
   ASCII block graphics mapping
   +-+-+
   +-+-+
   | | |
   +-+-+
 */
static
char *
asciiblockchar (c)
  character c;
{
    if (menu_border_style == 'r') {
	switch (c) {
		case 'j':	return "/";	/* LR */
		case 'k':	return "\\";	/* UR */
		case 'l':	return "/";	/* UL */
		case 'm':	return "\\";	/* LL */
		case 'n':	return "+";	/* crossing (not used) */
		case 'q':	if (cjk_width_data_version) {
					return "--";	/* hor. line */
				} else {
					return "-";	/* hor. line */
				}
		case 't':	return "+";	/* left tee */
		case 'u':	return "+";	/* right tee */
		case 'v':	return "+";	/* bottom tee */
		case 'w':	return "+";	/* top tee */
		case 'x':	return "|";	/* vert. line */
		case 'f':
		case 'g':	/* scrolling indication */
				if (utf8_screen) {
					return "¦";
				} else if (cjk_term || mapped_term) {
					return "|";
				} else {
					return "�";
				}
		default: return "";
	}
    } else {
	switch (c) {
		case 'j':	return "+";	/* LR */
		case 'k':	return "+";	/* UR */
		case 'l':	return "+";	/* UL */
		case 'm':	return "+";	/* LL */
		case 'n':	return "+";	/* crossing (not used) */
		case 'q':	if (cjk_width_data_version) {
					return "--";	/* hor. line */
				} else {
					return "-";	/* hor. line */
				}
		case 't':	return "+";	/* left tee */
		case 'u':	return "+";	/* right tee */
		case 'v':	return "+";	/* bottom tee */
		case 'w':	return "+";	/* top tee */
		case 'x':	return "|";	/* vert. line */
		case 'f':
		case 'g':	/* scrolling indication */
				if (utf8_screen) {
					return "¦";
				} else if (cjk_term || mapped_term) {
					return "|";
				} else {
					return "�";
				}
		default: return "";
	}
    }
}

#ifndef CURSES

/**
   VGA (PC/DOS) block graphics mapping
 */
static
character
vgablockchar (c)
  character c;
{
    if (menu_border_style == 'd') {
	switch (c) {
		case 'j':	return 0xBC;	/* LR */
		case 'k':	return 0xBB;	/* UR */
		case 'l':	return 0xC9;	/* UL */
		case 'm':	return 0xC8;	/* LL */
		case 'n':	return 0xCE;	/* crossing (not used) */
		case 'q':	return 0xCD;	/* hor. line */
		case 't':	return 0xCC;	/* left tee */
		case 'u':	return 0xB9;	/* right tee */
		case 'v':	return 0xCA;	/* bottom tee */
		case 'w':	return 0xCB;	/* top tee */
		case 'x':	return 0xBA;	/* vert. line */
		case 'f':	/* scrolling indication up */
		case 'g':	/* scrolling indication down */
				return '';
#ifdef choose_scrolling_indicators
				return '';	/* diamond */
				return 0xBA;	/* double line */
				return '';	/* arrow up */
				return '';	/* arrow down */
				return 0xB2;	/* box */
#endif
		default:	return c;
	}
    } else {
	switch (c) {
		case 'j':	return 0xD9;	/* LR */
		case 'k':	return 0xBF;	/* UR */
		case 'l':	return 0xDA;	/* UL */
		case 'm':	return 0xC0;	/* LL */
		case 'n':	return 0xC5;	/* crossing (not used) */
		case 'q':	return 0xC4;	/* hor. line */
		case 't':	return 0xC3;	/* left tee */
		case 'u':	return 0xB4;	/* right tee */
		case 'v':	return 0xC1;	/* bottom tee */
		case 'w':	return 0xC2;	/* top tee */
		case 'x':	return 0xB3;	/* vert. line */
		case 'f':	/* scrolling indication up */
				return '';	/* arrow up */
		case 'g':	/* scrolling indication down */
				return '';	/* arrow down */
#ifdef choose_scrolling_indicators
				return '';	/* arrow up */
				return '';	/* arrow down */
				return '';	/* diamond */
				return 0xBA;	/* double line */
				return 0xB2;	/* box */
#endif
		default:	return c;
	}
    }
}

#if defined (unix) || defined (vms)

/**
   block graphics mapping using termcap "ac" / terminfo "acsc" capability, 
   or VT100 block graphics by default
 */
static
char *
acblockchar (c)
  character c;
{
  character cout = '\0';
  static char sout [3];
  char * spoi = sout;

  if (c == 'f' || c == 'g') {
	c = '`';	/* choose one of '`' 'a' '|' 'f' */
  }

  if (* cAC) {
	int i;
	for (i = 0; i < strlen (cAC); i += 2) {
		if (cAC [i] == c) {
			cout = cAC [i + 1];
			break;
		}
	}
	if (cout == '\0') {
		/*don't return asciiblockchar (c) while in graphics mode*/
		/*don't skip wide handling below anyway*/
		cout = c;
	}
  } else {
	cout = c;
  }

  * spoi ++ = cout;
  if (c == 'q' && cjk_width_data_version && cjk_term) {
	* spoi ++ = cout;
  }
  * spoi = '\0';
  return sout;
}

#endif

#endif

/**
   Unicode block graphics mapping
   ┌─┬─┐  ╭─┬─╮
   ├─┼─┤  ├─┼─┤
   │ │ │  │ │ │
   └─┴─┘  ╰─┴─╯
   ┏━┳━┓  ╔═╦═╗
   ┣━╋━┫  ╠═╬═╣
   ┃ ┃ ┃  ║ ║ ║
   ┗━┻━┛  ╚═╩═╝
 */
static
char *
unicodeblockchar (c)
  character c;
{
  if (use_ascii_graphics) {
	return asciiblockchar (c);
  } else {
    if (menu_border_style == 'f') {
	switch (c) {
		case 'j':	return "┛";	/* LR */
		case 'k':	return "┓";	/* UR */
		case 'l':	return "┏";	/* UL */
		case 'm':	return "┗";	/* LL */
		case 'n':	return "╋";	/* crossing (not used) */
		case 'q':	return "━";	/* hor. line */
		case 't':	return "┣";	/* left tee */
		case 'u':	return "┫";	/* right tee */
		case 'v':	return "┻";	/* bottom tee */
		case 'w':	return "┳";	/* top tee */
		case 'x':	return "┃";	/* vert. line */
		case 'f':	/* scroll indication up */
		case 'g':	/* scroll indication down */
				return menu_cont_fatmarker;
#ifdef choose_scrolling_indicators
				return "⬆";
				return "⬇";
#endif
		default: return "";
	}
    } else if (menu_border_style == 'd') {
	switch (c) {
		case 'j':	return "╝";	/* LR */
		case 'k':	return "╗";	/* UR */
		case 'l':	return "╔";	/* UL */
		case 'm':	return "╚";	/* LL */
		case 'n':	return "╬";	/* crossing (not used) */
		case 'q':	return "═";	/* hor. line */
		case 't':	return "╠";	/* left tee */
		case 'u':	return "╣";	/* right tee */
		case 'v':	return "╩";	/* bottom tee */
		case 'w':	return "╦";	/* top tee */
		case 'x':	return "║";	/* vert. line */
		case 'f':	/* scroll indication up */
		case 'g':	/* scroll indication down */
				return "║";	/* scrolling indication */
#ifdef choose_scrolling_indicators
				return "⇑";
				return "⇓";
#endif
		default: return "";
	}
    } else if (menu_border_style == 'r') {
	switch (c) {
		case 'j':	return "╯";	/* LR */
		case 'k':	return "╮";	/* UR */
		case 'l':	return "╭";	/* UL */
		case 'm':	return "╰";	/* LL */
		case 'n':	return "┼";	/* crossing (not used) */
		case 'q':	return "─";	/* hor. line */
		case 't':	return "├";	/* left tee */
		case 'u':	return "┤";	/* right tee */
		case 'v':	return "┴";	/* bottom tee */
		case 'w':	return "┬";	/* top tee */
		case 'x':	return "│";	/* vert. line */
		case 'f':	/* scroll indication up */
		case 'g':	/* scroll indication down */
				return menu_cont_marker; /* scroll indication */
#ifdef choose_scrolling_indicators
				return "↑";
				return "↓";
				return "⇡";
				return "⇣";
#endif
		default: return "";
	}
    } else {
	switch (c) {
		case 'j':	return "┘";	/* LR */
		case 'k':	return "┐";	/* UR */
		case 'l':	return "┌";	/* UL */
		case 'm':	return "└";	/* LL */
		case 'n':	return "┼";	/* crossing (not used) */
		case 'q':	return "─";	/* hor. line */
		case 't':	return "├";	/* left tee */
		case 'u':	return "┤";	/* right tee */
		case 'v':	return "┴";	/* bottom tee */
		case 'w':	return "┬";	/* top tee */
		case 'x':	return "│";	/* vert. line */
		case 'f':	/* scroll indication up */
		case 'g':	/* scroll indication down */
				return menu_cont_marker; /* scroll indication */
#ifdef choose_scrolling_indicators
				return "↑";
				return "↓";
				return "⇡";
				return "⇣";
#endif
		default: return "";
	}
    }
  }
}


/*======================================================================*\
|*			Mouse mode control				*|
\*======================================================================*/

#if defined (unix) || defined (ANSI) || defined (vms)

/**
   Adapt mouse mode to menu state (menu opened?)
   While a menu is open, mouse movement should be reported separately.
 */
void
menu_mouse_mode (menu)
  FLAG menu;
{
  if (! use_mouse) {
	return;
  }

#ifdef debug_mouse
  printf ("menu_mouse_mode %d\n", menu);
#endif

  if (menu) {
	if (in_menu_mouse_mode == False) {
#ifndef CURSES_mouse
		if (use_mouse_button_event_tracking) {
			/* activate button-event tracking */
			putescape (cMouseEventBtnOn);
			if (use_mouse_anymove_inmenu) {
				/* activate any-event tracking */
				putescape (cMouseEventAnyOn);
			}
		}
#endif
		/* hide cursor for menu display if reverse mode available 
		   for menu item marking
		 */
		if (can_reverse_mode) {
			disp_cursor (False);
		}
	}
  } else {
	if (in_menu_mouse_mode) {
#ifndef CURSES_mouse
		if (use_mouse_button_event_tracking) {
			if (use_mouse_anymove_inmenu && ! use_mouse_anymove_always) {
				/* deactivate any-event tracking */
				putescape (cMouseEventAnyOff);
			}
			/* reactivate button-event tracking */
			putescape (cMouseEventBtnOn);
			if (use_mouse_anymove_always) {
				/* upgrade to any-event tracking */
				putescape (cMouseEventAnyOn);
			}
		}
#endif
		/* turn cursor visible again; 
		   (was turned invisible only if reverse mode available)
		 */
		if (can_reverse_mode) {
			disp_cursor (True);
		}
	}
  }
  in_menu_mouse_mode = menu;
}

/**
   Enable/disable xterm button event reporting mode.
 */
void
mouse_button_event_mode (report)
  FLAG report;
{
  if (! use_mouse_button_event_tracking) {
	return;
  }

#ifdef debug_mouse
  printf ("mouse_button_event_mode %d\n", report);
#endif

#ifndef CURSES_mouse
  if (report) {
	/* activate button-event tracking */
	putescape (cMouseEventBtnOn);
	is_mouse_button_event_mode_enabled = True;
  } else if (is_mouse_button_event_mode_enabled) {
#ifdef switch_button_event_tracking
	putescape (cMouseEventBtnOff);
	flush ();
#endif
	is_mouse_button_event_mode_enabled = False;
  }
#endif
}

/**
   Switch normal/alternate screen buffer.
 */
void
screen_buffer (alt)
  FLAG alt;
{
#ifdef CURSES
  if (alt) {
	putescape ("[?47h");
  } else {
	putescape ("[?47l");
  }
  flush ();
#else
  if (alt) {
	termputstr (cTI, aff1);
  } else {
	termputstr (cTE, aff1);
  }
#endif
}

#endif

#ifdef CURSES

/*======================================================================*\
|*			Curses						*|
\*======================================================================*/


#define TRUE (char) 1
#define FALSE (char) 0
extern int curs_readchar _((void));


/* colour definitions from curses.h
// #define COLOR_BLACK     0
// #define COLOR_RED       1
// #define COLOR_GREEN     2
// #define COLOR_YELLOW    3
// #define COLOR_BLUE      4
// #define COLOR_MAGENTA   5
// #define COLOR_CYAN      6
// #define COLOR_WHITE     7
*/
/* colour pair index definitions */
#define ctrlcolor 0
#define menucolor 0
#define diagcolor 0

#define markcolor 1
#define emphcolor markcolor
#define menuborder markcolor
#define menuheader markcolor
#define scrollfgcolor 2
#define specialcolor 3
#define selcolor 4
#define menuitem 5
#define HTMLcolor 6
#define HTMLcomment 7
#define HTMLjsp 8
#define XMLattrib 9
#define XMLvalue 10
#define colour8_base 11

#if NCURSES_VERSION_MAJOR >= 5
#define use_default_colour
#endif
#if NCURSES_VERSION_MAJOR == 4
# if NCURSES_VERSION_MINOR >= 2
#define use_default_colour
# endif
#endif

#ifdef use_default_colour
#define COLOR_fg -1
#define COLOR_bg -1
#else
#define COLOR_fg COLOR_WHITE
#define COLOR_bg COLOR_BLACK
#endif

static
void
init_colours ()
{
  start_color ();
#ifdef use_default_colour
  use_default_colors ();
#endif
  if (! has_colors ()) {
	bw_term = True;
  }

/* This produces silly warnings if compiled with gcc option -Wconversion
   ("passing arg ... with different width due to prototype")
   although all parameters are short.
*/
#ifndef vms
  init_pair (markcolor, COLOR_RED, COLOR_bg);
  init_pair (scrollfgcolor, COLOR_CYAN, COLOR_BLUE);
  init_pair (specialcolor, COLOR_CYAN, COLOR_bg);
  init_pair (selcolor, COLOR_BLUE, COLOR_bg);

  init_pair (menuitem, COLOR_BLACK, COLOR_YELLOW);
  init_pair (HTMLcomment, COLOR_BLUE, COLOR_YELLOW);
  init_pair (HTMLcolor, COLOR_CYAN, COLOR_bg);
  init_pair (HTMLjsp, COLOR_BLUE, COLOR_bg);
  init_pair (XMLattrib, COLOR_RED, COLOR_bg);
  init_pair (XMLvalue, COLOR_MAGENTA, COLOR_bg);

  init_pair (colour8_base + 0, COLOR_BLACK, COLOR_bg);
  init_pair (colour8_base + 1, COLOR_RED, COLOR_bg);
  init_pair (colour8_base + 2, COLOR_GREEN, COLOR_bg);
  init_pair (colour8_base + 3, COLOR_YELLOW, COLOR_bg);
  init_pair (colour8_base + 4, COLOR_BLUE, COLOR_bg);
  init_pair (colour8_base + 5, COLOR_MAGENTA, COLOR_bg);
  init_pair (colour8_base + 6, COLOR_CYAN, COLOR_bg);
  init_pair (colour8_base + 7, COLOR_WHITE, COLOR_bg);
#endif
}


void
__putchar (c)
  register character c;
{
  addch (c);
}

void
putblockchar (c)
  register character c;
{
  if (menu_border_style == 'P' && c != 'q') {
	return;
  } else if (menu_border_style == 'p' && c != 'q') {
	putstring (" ");
	return;
  }

  if (use_normalchars_boxdrawing) {
	addstr (unicodeblockchar (c));
  } else {
#ifdef vms
	addstr (asciiblockchar (c));
#else
	switch (c) {
		case 'j':	addch (ACS_LRCORNER);	break;
		case 'k':	addch (ACS_URCORNER);	break;
		case 'l':	addch (ACS_ULCORNER);	break;
		case 'm':	addch (ACS_LLCORNER);	break;
		case 'n':	addch (ACS_PLUS);	break;
		case 'q':	addch (ACS_HLINE);	break;
		case 't':	addch (ACS_LTEE);	break;
		case 'u':	addch (ACS_RTEE);	break;
		case 'v':	addch (ACS_BTEE);	break;
		case 'w':	addch (ACS_TTEE);	break;
		case 'x':	addch (ACS_VLINE);	break;
		case 'f':
		case 'g':	addch (ACS_DIAMOND);	break;
		default:	addch (' ');
	}
#endif
  }
}

void
ring_bell ()
{
  beep ();
}

void
putstring (str)
  register char * str;
{
  addstr (str);
}

void
flush ()
{
  refresh ();
}

void
clear_screen ()
{
  clear ();
}

void
clear_eol ()
{
  clrtoeol ();
}

void
erase_chars (n)
  register int n;
{
  while (n -- > 0) {
	__putchar (' ');
  }
}

void
scroll_forward ()
{
  scroll (stdscr);
}

void
scroll_reverse ()
{ /* only to be called if can_add_line or cursor is at top of screen */
  insertln ();
}

void
add_line (y)
  register int y;
{ /* only to be called if can_add_line == True */
  move (y + MENU, 0);
  insertln ();
}

void
delete_line (y)
  register int y;
{
  move (y + MENU, 0);
  deleteln ();
}

void
set_cursor (x, y)
  register int x;
  register int y;
{
  /*current_cursor_y = y;*/
  move (y + MENU, x);
}

void
reverse_on ()
{
  standout ();
}

void
reverse_off ()
{
  standend ();
}


static
void
configure_screen ()
{
#ifdef __PDCURSES__
  keypad (stdscr, TRUE);
#endif

#ifdef KEY_MOUSE

# ifdef __PDCURSES__
  (void) mouse_set (ALL_MOUSE_EVENTS);
# else
#  ifdef NCURSES_VERSION
  (void) mousemask (BUTTON1_CLICKED | BUTTON2_CLICKED | BUTTON3_CLICKED
#   ifdef BUTTON_SHIFT
		    | BUTTON_SHIFT
#   endif
#   ifdef ALL_MOUSE_EVENTS
		    | ALL_MOUSE_EVENTS
#   endif
#   ifdef REPORT_MOUSE_POSITION
		    | REPORT_MOUSE_POSITION
#   endif
	, /*(mmask_t)*/ (unsigned long) 0);
#  endif
# endif
#endif

  init_colours ();

#define use_cbreak

#ifdef vms
  crmode ();
#else
# ifdef use_cbreak
  /* not sufficient to suppress ^S processing on Unix in pipe ?! */
  /* but now also VSTOP is explicitly removed */
  cbreak ();
# else
  raw ();
# endif
  nonl ();
#endif

  meta (stdscr, TRUE);	/* should force 8-bit-cleanness but doesn't work */
  noecho ();
  scrollok (stdscr, FALSE);

#if defined (unix) && ! defined (ultrix)
# ifndef __EMX__
  idlok (stdscr, TRUE);
# endif
# ifndef sun
  typeahead (input_fd);
# endif
#endif
}

static
void
start_screen_mode (kb)
  int kb;
{
  /* for curses, see configure_screen () above */
}

static
void
end_screen_mode ()
{
}


static
void
get_term_cap (TERMname)
  char * TERMname;
{
#ifdef SETLOCALE
  setlocale (LC_ALL, "");
#endif

  YMAX = LINES - 1 - MENU;	/* # of lines */
  XMAX = COLS - 1;	/* # of columns */
/*  getwinsize (); */
}

void
altcset_on ()
{
	attron (A_ALTCHARSET);
}

void
altcset_off ()
{
	attroff (A_ALTCHARSET);
}

void
disp_cursor (visible)
  FLAG visible;
{
	if (visible) {
		curs_set (1);
	} else {
		curs_set (0);
	}
}

void
set_screen_mode (m)
  int m;
{}

void
maximize_restore_screen ()
{
}

void
switch_textmode_height (cycle)
  FLAG cycle;
{}

void
resize_font (inc)
  int inc;
{
  if (mintty_version >= 503) {
	if (inc > 0) {		/* increase font size */
		putescape ("]7770;+1");
	} else if (inc < 0) {	/* decrease font size */
		putescape ("]7770;-1");
	} else {		/* reset to default */
		putescape ("]7770;");
	}
  } else if (xterm_version >= 203 /* best guess */) {
	if (inc > 0) {		/* increase font size */
		putescape ("]50;#+1");
	} else if (inc < 0) {	/* decrease font size */
		putescape ("]50;#-1");
	} else {		/* reset to default */
		putescape ("]50;#");
	}
  }
}

static
void
resize_screen (l, c)
  int l;
  int c;
{
  static char s [22];
  build_string (s, "\033[8;%d;%dt", l, c);
  putescape (s);
}

static
void
resize_vt (sb, keep_columns)
  FLAG sb;
  FLAG keep_columns;
{
  if (keep_columns == True) {
	/* change number of lines (VT420) */
	int lines = YMAX + 1 + MENU;
	static int vtlines [] = {24, 36, 48, 72, 144};
	int i;
	static char s [15];
	if (sb == SMALLER) {
		for (i = arrlen (vtlines) - 1; i >= 0; i --) {
			if (lines > vtlines [i]) {
				build_string (s, "\033[%dt", vtlines [i]);
				putescape (s);
				YMAX = vtlines [i] - 1 - MENU;
				return;
			}
		}
	} else {
		for (i = 0; i < arrlen (vtlines); i ++) {
			if (lines < vtlines [i]) {
				build_string (s, "\033[%dt", vtlines [i]);
				putescape (s);
				YMAX = vtlines [i] - 1 - MENU;
				return;
			}
		}
	}
  } else {
	/* switch 80/132 columns */
	if (sb == SMALLER) {
		putescape ("\033[?40h\033[?3l");
		XMAX = 79;
	} else {
		putescape ("\033[?40h\033[?3h");
		XMAX = 131;
	}
  }
}

void
resize_the_screen (sb, keep_columns)
  FLAG sb;
  FLAG keep_columns;
{
  if (decterm_version > 0) {
	resize_vt (sb, keep_columns);
  } else {
	int ldif = 6;
	int coldif = 13;
	if (sb == SMALLER) {
		ldif = - ldif;
		coldif = - coldif;
	}
	if (keep_columns == True) {
		coldif = 0;
	} else if (keep_columns == NOT_VALID) {
		ldif = 0;
	}

	resize_screen (YMAX + 1 + MENU + ldif, XMAX + 1 + coldif);
  }
}

#else	/* #ifdef CURSES */


/*======================================================================*\
			non-curses
\*======================================================================*/


#ifndef msdos
void
ring_bell ()
{
  putchar ('\07');
}
#endif


/*======================================================================*\
|*			Basic buffered screen output			*|
\*======================================================================*/

/* screen buffer length should be sufficient to keep one full window;
   not essential, though, on very large screen
 */
#define screenbuflen	22222

static char screenbuf [screenbuflen + 1];
static unsigned int screenbuf_count = 0;

#define dont_debug_write

#ifdef debug_write
static
void print_buf (s, len)
  char * s;
  int len;
{
  while (len > 0) {
	if ((* s & 0xE0) == 0) {
		printf ("^%c", * s + 0x40);
	} else {
		printf ("%c", * s);
	}
	s ++;
	len --;
  }
}
#endif

/*
 * Flush the screen buffer
 */
static
int
flush_screenbuf ()
{
  if (screenbuf_count == 0) {		/* There is nothing to flush */
	return FINE;
  }

#ifdef conio
  cputs (screenbuf);
#else
#ifdef BorlandC
  screenbuf [screenbuf_count] = '\0';
/* don't ask me why that crazy compiler doesn't work with write () below */
  printf ("%s", screenbuf);
#else
  {
	char * writepoi = screenbuf;
	int written = 0;
	int none_count = 0;
/*	int less_count = 0;	*/

#ifdef debug_write
	printf ("writing %d: ", screenbuf_count);
	print_buf (writepoi, screenbuf_count);
	printf ("\n");
	if (tty_closed) {
		printf ("(tty closed) - not written\n");
		sleep (1);
	}
#endif

	if (tty_closed) {
		screenbuf_count = 0;
		return ERRORS;
	}

	while (screenbuf_count > 0) {
#ifdef vms_async_output
		/* no noticeable speed improvement */
		written = ttputs (writepoi, screenbuf_count);
#else
		written = write (output_fd, writepoi, screenbuf_count);
#endif
#ifdef debug_write
		printf ("written %d (errno %d: %s)\n", written, geterrno (), serror ());
#endif
		if (written == -1) {
			if (geterrno () == EINTR && winchg) {
				/* try again */
			} else {
				tty_closed = True;
				panicio ("Terminal write error", serror ());
				screenbuf_count = 0;
				return ERRORS;
			}
		} else if (written == 0) {
			none_count ++;
			if (none_count > 20) {
				panicio ("Terminal write error", serror ());
				screenbuf_count = 0;
				return ERRORS;
			}
		} else {
			screenbuf_count -= written;
			writepoi += written;
		}
	}
  }
#endif
#endif

  screenbuf_count = 0;
  return FINE;
}

void
flush ()
{
  (void) flush_screenbuf ();
}

/*
 * putoutchar does a buffered output to screen.
 */
static
int
putoutchar (c)
  character c;
{
  if (c == '\n') {
	if (putoutchar ('\r') == ERRORS) {
		return ERRORS;
	}
  }
  if (translate_output) {
	if ((c & '\200') != '\0') {
		char clow = (c & '\177');
		altcset_on ();
		if ((int) clow < translen) {
			putoutchar (transout [(int) clow]);
		} else {
			putoutchar (clow);
		}
		altcset_off ();
		return FINE;
	}
  }
  if (screen_version > 0 && screen_version < 400 && c >= 0x80 && c < 0xA0) {
	/* embed C1 control characters (workaround for 'screen' bug) */
	putoutchar ('\033');
	putoutchar ('P');
	screenbuf [screenbuf_count ++] = c;
	if (screenbuf_count == screenbuflen) {
		(void) flush_screenbuf ();
	}
	putoutchar ('\033');
	putoutchar ('\\');
	return FINE;
  }
  screenbuf [screenbuf_count ++] = c;
  if (screenbuf_count == screenbuflen) {
	return flush_screenbuf ();
  }

#define dont_debug_all_output
#ifdef debug_all_output
  if (c == '\033') {
	printf ("^[");
  } else {
	printf ("%c", c);
  }
#endif
#ifdef debug_flush_output
  flush ();
#endif

  return FINE;
}

/*
 * Putoutstring writes the given string out to the terminal.
 * (buffered via misused screen buffer!)
putstring (str) does putoutstring (str) unless with curses or conio
 */
static
void
putoutstring (text)
  register char * text;
{
  while (* text != '\0') {
	(void) putoutchar (* text ++);
  }
}

void
__putchar (c)
  register character c;
{
#ifdef conio
  putch (c);
#else
  putoutchar (c);
#endif
}

void
putstring (str)
  register char * str;
{
#ifdef conio
  cputs (str);
#else
  putoutstring (str);
#endif
}

/**
   Output a menu border character. Decide which style to use, 
   block graphics (VT100 or VGA), Unicode graphics, ASCII graphics.
   This and it's setup is a mess and could do with some revision.
 */
void
putblockchar (c)
  register character c;
{
  if (menu_border_style == 'P' && c != 'q') {
	return;
  } else if (menu_border_style == 'p' && c != 'q') {
	putstring (" ");
	return;
  }

#ifdef conio
  if (use_ascii_graphics) {
	putstring (asciiblockchar (c));
  } else {
	putchar (vgablockchar (c));
  }
#else
  if (use_ascii_graphics) {
	putoutstring (asciiblockchar (c));
  } else if (use_vga_block_graphics) {
	putoutchar (vgablockchar (c));
  } else if (use_normalchars_boxdrawing) {
	putoutstring (unicodeblockchar (c));
  } else if (use_pc_block_graphics) {
	putoutchar (vgablockchar (c));
  } else {
# if defined (unix) || defined (vms)
	putoutstring (acblockchar (c));
# else
	putoutstring (asciiblockchar (c));
# endif
  }
#endif
}


/*======================================================================*\
|*			Screen control functions			*|
\*======================================================================*/

#if defined (unix) || defined (ANSI)

void
clear_screen ()
{
  termputstr (cCL, affmax);
}

void
clear_eol ()
{
  if (can_clear_eol) {
	termputstr (cCE, aff1);
  }
}

void
erase_chars (n)
  register int n;
{
#ifdef ANSI
  static char s [15];
  build_string (s, "\033[%dX", n);
  termputstr (s, aff1);
#else
  termputstr (tgoto (cEC, 0, n), aff1);
#endif
}

void
scroll_forward ()
{
  set_cursor (0, YMAX);
  termputstr ("\n", affmax);
}

void
scroll_reverse ()
{ /* only to be called if can_add_line or cursor is at top of screen */
  if (can_add_line) {
	add_line (0);
  } else {
	termputstr (cSR, affmax);
  }
}

static
void
scrolling_region (yb, ye)
  register int yb;
  register int ye;
{
#ifdef ANSI
  static char s [22];
  build_string (s, "\033[%d;%dr", yb + 1, ye + 1);
  termputstr (s, aff1);
#else
  termputstr (tgoto (cCS, ye, yb), aff1);
#endif
}

void
add_line (y)
  register int y;
{ /* only to be called if can_add_line == True */
  if (cAL) {
	set_cursor (0, y);
	termputstr (cAL, affmax);
  } else {
	set_cursor (0, y);
	termputstr (cSC, aff1);
	scrolling_region (MENU + y, MENU + YMAX);
	termputstr (cRC, aff1);
	termputstr (cSR, affmax);
	scrolling_region (0, MENU + YMAX);
	termputstr (cRC, aff1);
  }
}

void
delete_line (y)
  register int y;
{
  if (cDL) {
	set_cursor (0, y);
	termputstr (cDL, affmax);
  } else {
	set_cursor (0, y);
	termputstr (cSC, aff1);
	scrolling_region (MENU + y, MENU + YMAX);
	set_cursor (0, YMAX);
	termputstr ("\n", affmax);
	scrolling_region (0, MENU + YMAX);
	termputstr (cRC, aff1);
  }
}

void
set_cursor (x, y)
  register int x;
  register int y;
{
#ifdef ANSI
  static char s [22];
#endif

  if (screen_version > 0) {
	/* flush cursor position (workaround for 'screen' bug) */
#ifdef ANSI
	termputstr ("\033[1;1H", aff1);
#else
	termputstr (tgoto (cCM, 0, 0), aff1);
#endif
  }

  /*current_cursor_y = y;*/
#ifdef ANSI
  build_string (s, "\033[%d;%dH", y + 1 + MENU, x + 1);
  termputstr (s, aff1);
#else
  termputstr (tgoto (cCM, x, y + MENU), aff1);
#endif
}

#define dont_use_standout /* might add underline, e.g. within 'screen' */

void
reverse_on ()
{
#ifdef use_standout
  termputstr (cSO, aff1);
#else
  termputstr (cMR, aff1);
#endif
}

void
reverse_off ()
{
#ifdef use_standout
  termputstr (cSE, aff1);
#else
  termputstr (cME, aff1);
#endif
}

void
altcset_on ()
{
  if (use_pc_block_graphics) {
	termputstr (cS2, aff1);
  } else {
	termputstr (cAS, aff1);
  }
}

void
altcset_off ()
{
  if (use_pc_block_graphics) {
	termputstr (cS3, aff1);
  } else {
	termputstr (cAE, aff1);
  }
}

#endif	/* #if defined (unix) || defined (ANSI) */

#if defined (unix) && ! defined (ANSI)

static
void
get_terminfo (TERMname)
  char * TERMname;
{
	/* get screen size */
	YMAX = term_getnum ("li", "lines") - 1 - MENU;	/* # of lines */
	XMAX = term_getnum ("co", "cols") - 1;	/* # of columns */
	getwinsize ();

	/* get screen control escape sequences */
	cCL = term_getstr ("cl", "clear");	/* clear screen */
	cCE = term_getstr ("ce", "el");		/* clear to end of line */
	cCD = term_getstr ("cd", "ed");		/* clear to end of screen */
	cCM = term_getstr ("cm", "cup");	/* move cursor */

	cEC = term_getstr ("ec", "ech");	/* erase characters */
	cSR = term_getstr ("sr", "ri");		/* scroll down */
	cAL = term_getstr ("al", "il1");	/* add line */
	cDL = term_getstr ("dl", "dl1");	/* delete line */
	cCS = term_getstr ("cs", "csr");	/* change scrolling region */
	cSC = term_getstr ("sc", "sc");	/* save cursor    \ needed with VT100   */
	cRC = term_getstr ("rc", "rc");	/* restore cursor / for add/delete line */

	cME = term_getstr ("me", "sgr0");	/* turn off all attributes */
	cMR = term_getstr ("mr", "rev");	/* reverse */
	cMD = term_getstr ("md", "bold");	/* bold / extra bright */
	cMH = term_getstr ("mh", "dim");	/* dim / half-bright */
	cUL = term_getstr ("us", "smul");	/* underline */
	cBL = term_getstr ("mb", "blink");	/* blink */
	cSO = term_getstr ("so", "smso");	/* standout mode */
	cSE = term_getstr ("se", "rmso");	/* end standout mode */

	cVS = term_getstr ("vs", "cvvis");	/* visual cursor */
	cVE = term_getstr ("ve", "cnorm");	/* normal cursor */
	cVI = term_getstr ("vi", "civis");	/* invisible cursor */
	cTI = term_getstr ("ti", "smcup");	/* positioning mode */
	cTE = term_getstr ("te", "rmcup");	/* end " */
	cKS = term_getstr ("ks", "smkx");	/* keypad mode */
	cKE = term_getstr ("ke", "rmkx");	/* end " */
	cAS = term_getstr ("as", "smacs");	/* alternate character set */
	cAE = term_getstr ("ae", "rmacs");	/* end " */
	cEA = term_getstr ("eA", "enacs");	/* enable " */
	cAC = term_getstr ("ac", "acsc");	/* block graphics code mapping */
	cS2 = term_getstr ("S2", "smpch");	/* PC character set */
	cS3 = term_getstr ("S3", "rmpch");	/* end " */

	cAF = term_getstr ("AF", "setaf");	/* select foreground colour */
	cAB = term_getstr ("AB", "setab");	/* select background colour */
	if (term_getnum ("Co", "colors") == 0 && ! cAF) {
		bw_term = True;
	}
	standout_glitch = term_getflag ("xs", "xhp");	/* standout glitch */

	/* add function key escape sequences */
	add_terminfo_entries ();
}

#endif	/* #if defined (unix) || defined (ANSI) */


#if defined (vms) || defined (ANSI)

static
FLAG
term_setup (TERMname)
  char * TERMname;
{
  return False;
}

static
void
get_terminfo (TERMname)
  char * TERMname;
{
}

#endif


#if defined (unix) || defined (ANSI) || defined (vms)

void
disp_cursor (visible)
  FLAG visible;
{
	if (visible) {
		/* first turn it "normal" in case "very visible" does 
		   not work, then try "very visible"
		 */
		termputstr (cVE, affmax);
		termputstr (cVS, affmax);
	} else {
		termputstr (cVI, affmax);
	}
}

static
void
set_term_defaults (TERMname)
  char * TERMname;
{
	/* set common ANSI sequences */
	cCL = "[H[J";		/* clear screen */
	cCE = "[K";		/* clear to end of line */
	cCD = "[J";		/* clear to end of screen */
	cCM = "[%i%p1%d;%p2%dH";	/* move cursor */
	cCS = NIL_PTR;		/* change scrolling region */
	cEC = NIL_PTR;		/* erase characters */
	cSR = NIL_PTR;		/* scroll down */
	cAL = NIL_PTR;		/* add line */
	cDL = NIL_PTR;		/* delete line */
	cSC = NIL_PTR;		/* save cursor position */
	cRC = NIL_PTR;		/* restore cursor position */

	cVS = NIL_PTR;		/* visual cursor */
	cVE = NIL_PTR;		/* normal cursor */
	cVI = NIL_PTR;		/* invisible cursor */
	cTI = NIL_PTR;		/* positioning mode */
	cTE = NIL_PTR;		/* end " */
	cKS = NIL_PTR;		/* keypad mode */
	cKE = NIL_PTR;		/* end " */

	cME = "[0m";		/* turn off all attributes */
	cMR = "[7m";		/* reverse */
	cMD = "[1m";		/* bold */
	cMH = NIL_PTR;		/* dim */
	cUL = "[4m";		/* underline */
	cBL = "[5m";		/* blink */
	cSO = "[7m";		/* standout mode */
	cSE = "[0m";;		/* end standout */
	cAF = NIL_PTR;		/* set foreground colour */
	cAB = NIL_PTR;		/* set background colour */
	cAS = NIL_PTR;		/* alternate character set */
	cAE = NIL_PTR;		/* end " */
	cEA = NIL_PTR;		/* enable " */
	cAC = NIL_PTR;		/* block graphics code mapping */
	cS2 = NIL_PTR;		/* PC character set */
	cS3 = NIL_PTR;		/* end " */
#ifdef __CYGWIN__
	if (strisprefix ("interix", TERMname)) {
		/* running cygwin version in interix terminal, 
		   cygwin console terminal emulation is applicable
		 */
		TERMname = "cygwin";
	}
#endif
	if (streq (TERMname, "cygwin")) {
		cEC = "[%dX";		/* erase characters */
		cSR = "M";		/* scroll down */
		cAL = "[L";		/* add line */
		cDL = "[M";		/* delete line */
		cSC = "7";		/* save cursor position */
		cRC = "8";		/* restore cursor position */

		cTI = "7[?47h[2J";	/* positioning mode */
		cTE = "[2J[?47l8";	/* end " */

		cSE = "[27m";		/* end standout */
		cS2 = "[11m";		/* PC character set */
		cS3 = "[10m";		/* end " */
	} else if (strisprefix ("xterm", TERMname)) {
		cCS = "[%i%p1%d;%p2%dr";	/* change scrolling region */
		cEC = "[%dX";		/* erase characters */
		cSR = "M";		/* scroll down */
		cAL = "[L";		/* add line */
		cDL = "[M";		/* delete line */
		cSC = "7";		/* save cursor position */
		cRC = "8";		/* restore cursor position */

		cVS = "[?12;25h";	/* visual cursor */
		cVE = "[?12l[?25h";	/* normal cursor */
		cVI = "[?25l";		/* invisible cursor */
#ifdef newer_xterm_may_use
		/* xterm_version >= 90, not detected yet at this time */
		cTI = "[?1049h";	/* positioning mode */
		cTE = "[?1049l";	/* end " */
#else
		cTI = "7[?47h[2J";	/* positioning mode */
		cTE = "[2J[?47l8";	/* end " */
#endif
		cKS = "[?1h=";	/* keypad mode */
		cKE = "[?1l>";	/* end " */

		cSE = "[27m";		/* end standout */
		cAS = "(0";		/* alternate character set */
		cAE = "(B";		/* end " */
	} else if (strisprefix ("rxvt", TERMname)) {
		cCS = "[%i%p1%d;%p2%dr";	/* change scrolling region */
		cSR = "M";		/* scroll down */
		cAL = "[L";		/* add line */
		cDL = "[M";		/* delete line */
		cSC = "7";		/* save cursor position */
		cRC = "8";		/* restore cursor position */

		cVE = "[?25h";		/* normal cursor */
		cVI = "[?25l";		/* invisible cursor */
		cTI = "7[?47h[2J";	/* positioning mode */
		cTE = "[2J[?47l8";	/* end " */
		cKS = "=";		/* keypad mode */
		cKE = ">";		/* end " */

		cSE = "[27m";		/* end standout */
		cAS = "";		/* alternate character set */
		cAE = "";		/* end " */
		cEA = "(B)0";		/* enable " */
	} else if (strisprefix ("vt", TERMname)
		   && ! streq ("vt52", TERMname)
		   && ! strisprefix ("vt50", TERMname)
		) {
		cCS = "[%i%p1%d;%p2%dr";	/* change scrolling region */
		cSR = "M";		/* scroll down */
		cSC = "7";		/* save cursor position */
		cRC = "8";		/* restore cursor position */
		if (TERMname [2] > '1' || strisprefix ("vt102", TERMname)) {
			cAL = "[L";		/* add line */
			cDL = "[M";		/* delete line */
		}

		cKS = "[?1h=";	/* keypad mode */
		cKE = "[?1l>";	/* end " */

		cSE = "[m";		/* end standout */
		cAS = "";		/* alternate character set */
		cAE = "";		/* end " */
		cEA = "(B)0";		/* enable " */
	} else if (strisprefix ("linux", TERMname)) {
		cCS = "[%i%p1%d;%p2%dr";	/* change scrolling region */
		cEC = "[%dX";		/* erase characters */
		cSR = "M";		/* scroll down */
		cAL = "[L";		/* add line */
		cDL = "[M";		/* delete line */
		cSC = "7";		/* save cursor position */
		cRC = "8";		/* restore cursor position */

		cVS = "[?25h[?8c";	/* visual cursor */
		cVE = "[?25h[?0c";	/* normal cursor */
		cVI = "[?25l[?1c";	/* invisible cursor */

		cSE = "[27m";		/* end standout */
		cS2 = "[11m";		/* PC character set */
		cS3 = "[10m";		/* end " */
	} else if (strisprefix ("screen", TERMname)) {
		cCS = "[%i%p1%d;%p2%dr";	/* change scrolling region */
		cSR = "M";		/* scroll down */
		cAL = "[L";		/* add line */
		cDL = "[M";		/* delete line */
		cSC = "7";		/* save cursor position */
		cRC = "8";		/* restore cursor position */

		cVS = "[34l";		/* visual cursor */
		cVE = "[34h[?25h";	/* normal cursor */
		cVI = "[?25l";		/* invisible cursor */
		cTI = "[?1049h";	/* positioning mode */
		cTE = "[?1049l";	/* end " */
		cKS = "[?1h=";	/* keypad mode */
		cKE = "[?1l>";	/* end " */

		cSO = "[3m";		/* standout */
		cSE = "[23m";		/* end standout */
		cAS = "";		/* alternate character set */
		cAE = "";		/* end " */
	} else if (strisprefix ("interix", TERMname)) {
		cSR = "[T";		/* scroll down */
		cAL = "[L";		/* add line */
		cDL = "[M";		/* delete line */
		cSC = "[s";		/* save cursor position */
		cRC = "[u";		/* restore cursor position */

		cTI = "[s[1b";	/* positioning mode */
		cTE = "[2b[u\r[K";	/* end " */

		cSE = "[m";		/* end standout */
	} else {
		errprintf ("Unknown terminal %s - trying minimal ANSI terminal settings\n", TERMname);
#ifndef ANSI
		sleep (1);
#endif
	}
}


#define dont_debug_termcap

#ifdef debug_termcap
/**
   Debug termcap control strings.
 */
static
void
print_termcapstr (s, namecap, nameinfo)
  char * s;
  char * namecap;
  char * nameinfo;
{
  printf ("%s (%s): ", namecap, nameinfo);
  if (s) {
    while (* s) {
	if ((* s & 0xE0) == 0) {
		printf ("^%c", * s + 0x40);
	} else {
		printf ("%c", * s);
	}
	s ++;
    }
  } else {
    printf ("(NULL)");
  }
  printf ("\n");
}
#endif

static
void
get_term_cap (TERMname)
  char * TERMname;
{
#ifdef TERMCAP
  char term_rawbuf [2048];
#endif

  char * check;

  if (! term_setup (TERMname)) {
	avoid_tputs = True;
	/* support some terminals even without termcap/terminfo available,
	   especially for cygwin stand-alone version;
	   first set minimal set of ANSI sequences
	 */
	YMAX = 23 - MENU;
	XMAX = 79;
	getwinsize ();
	set_term_defaults (TERMname);
  } else {
	get_terminfo (TERMname);
  }
#ifdef debug_termcap
  print_termcapstr (cCL, "cl", "clear");	/* clear screen */
  print_termcapstr (cCE, "ce", "el");	/* clear to end of line */
  print_termcapstr (cCD, "cd", "ed");	/* clear to end of screen */
  print_termcapstr (cCM, "cm", "cup");	/* move cursor */

  print_termcapstr (cEC, "ec", "ech");	/* erase characters */
  print_termcapstr (cSR, "sr", "ri");	/* scroll down */
  print_termcapstr (cAL, "al", "il1");	/* add line */
  print_termcapstr (cDL, "dl", "dl1");	/* delete line */
  print_termcapstr (cCS, "cs", "csr");	/* change scrolling region */
  print_termcapstr (cSC, "sc", "sc");	/* save cursor    \ needed with VT100   */
  print_termcapstr (cRC, "rc", "rc");	/* restore cursor / for add/delete line */

  print_termcapstr (cME, "me", "sgr0");	/* turn off all attributes */
  print_termcapstr (cMR, "mr", "rev");	/* reverse */
  print_termcapstr (cMD, "md", "bold");	/* bold / extra bright */
  print_termcapstr (cMH, "mh", "dim");	/* dim / half-bright */
  print_termcapstr (cUL, "us", "smul");	/* underline */
  print_termcapstr (cBL, "mb", "blink");	/* blink */
  print_termcapstr (cSO, "so", "smso");	/* standout mode */
  print_termcapstr (cSE, "se", "rmso");	/* end standout mode */

  print_termcapstr (cVS, "vs", "cvvis");	/* visual cursor */
  print_termcapstr (cVE, "ve", "cnorm");	/* normal cursor */
  print_termcapstr (cVI, "vi", "civis");	/* invisible cursor */
  print_termcapstr (cTI, "ti", "smcup");	/* positioning mode */
  print_termcapstr (cTE, "te", "rmcup");	/* end " */
  print_termcapstr (cKS, "ks", "smkx");	/* keypad mode */
  print_termcapstr (cKE, "ke", "rmkx");	/* end " */
  print_termcapstr (cAS, "as", "smacs");	/* alternate character set */
  print_termcapstr (cAE, "ae", "rmacs");	/* end " */
  print_termcapstr (cEA, "eA", "enacs");	/* enable " */
  print_termcapstr (cAC, "ac", "acsc");	/* block graphics code mapping */
  print_termcapstr (cS2, "S2", "smpch");	/* PC character set */
  print_termcapstr (cS3, "S3", "rmpch");	/* end " */

  print_termcapstr (cAF, "AF", "setaf");	/* select foreground colour */
  print_termcapstr (cAB, "AB", "setab");	/* select background colour */
#endif

#ifdef TERMCAP
  if (term_capbufpoi > term_capbuf + term_capbuflen) {
	panic ("Terminal control strings don't fit", NIL_PTR);
  }
#endif

  /* derive and fix properties */
  if (!cSR) {
	cSR = cAL;
  }

  if (cAL || (cSR && cCS)) {
	can_add_line = True;
  } else {
	can_add_line = False;
  }
  if (cSR) {
	can_scroll_reverse = True;
  } else {
	can_scroll_reverse = False;
  }
  if (cDL || cCS) {
	can_delete_line = True;
  } else {
	can_delete_line = False;
  }
  if (cCE) {
	can_clear_eol = True;
  } else {
	can_clear_eol = False;
  }
  if (cCD) {
	can_clear_eos = True;
  } else {
	can_clear_eos = False;
  }
  if (cEC) {
	can_erase_chars = True;
  } else {
	can_erase_chars = False;
  }
  if (cVI) {
	can_hide_cursor = True;
  }
  if (cMH) {
	can_dim = True;
  }

#define denull(c)	if (c == NIL_PTR) c = ""

  denull (cSO);
  denull (cSE);
  denull (cTI);
  denull (cTE);
  denull (cVS);
  denull (cVE);
  denull (cVI);
  denull (cAS);
  denull (cAE);
  denull (cEA);
  denull (cAC);
  denull (cS2);
  denull (cS3);
  denull (cKS);
  denull (cKE);
  denull (cME);
  denull (cMR);
  denull (cMH);
  denull (cUL);
  denull (cBL);
  denull (cMD);
  denull (cAF);
  denull (cAB);

  /* if the terminal has no explicit "reverse" control, use standout instead */
  if (* cMR == '\0') {
	cMR = cSO;
	cME = cSE;
  }
  if (* cMR == '\0') {
	can_reverse_mode = False;
  }

  /* verify minimal feature set */
  if (!cCL || !cCM /* || !cSO || !cSE */ ) {
	panic ("Sorry, terminal features are insufficient for mined", NIL_PTR);
  }

  /* check if terminal accepts ANSI control sequence patterns */
  check = cME;
  if (* check == '\0') {
	check = cSE;
  }
  if (strcontains (check, "\033[") && strchr (check, 'm') != NIL_PTR) {
	ansi_esc = True;
  } else {
	ansi_esc = False;
  }

  /* set mouse mode controls */
  if (! ansi_esc) {
	/* disable xterm mouse escape sequences */
	cMouseX10On = "";
	cMouseX10Off = "";
	cMouseButtonOn = "";
	cMouseButtonOff = "";
	cMouseEventBtnOn = "";
	cMouseEventBtnOff = "";
	cMouseEventAnyOn = "";
	cMouseEventAnyOff = "";
	cMouseExtendedOn = "";
	cMouseExtendedOff = "";
	cMouseFocusOn = "";
	cMouseFocusOff = "";
	cAmbigOn = "";
	cAmbigOff = "";
  }

  if (* cAS == '\0') {
	if (use_vt100_block_graphics) {
		cAS = "";
		cAE = "";
	}
  }
  if (strcontains (TERMname, "linux") && use_vt100_block_graphics) {
	cAS = "";
	cAE = "";
  }
#ifdef terminal_already_detected
  /* which is not the case... */
  if (mintty_version >= 400) {
	cS2 = "[11m";
	cS3 = "[10m";
  }
#endif
  if (* cAS == '\0') {
	can_alt_cset = False;
  }
  if (* cEA == '\0' && * cAS == ''
      && (strisprefix ("xterm", TERMname)
       || strisprefix ("rxvt", TERMname)
       || strisprefix ("konsole", TERMname)
       || strisprefix ("screen", TERMname)
     )) {
	/* workaround for missing eA capability in terminfo / /etc/termcap */
	cEA = "\033(B\033)0";
  }
  if (strcontains (cEA, "\033)0")) {
	/* workaround for missing disable alternate charset capability */
	/* restore normal G1 character set on exit */
	cdisableEA = "\033)B";
  } else {
	cdisableEA = "";
  }
  if (* cS2 != '\0' && * cS3 != '\0') {
	use_pc_block_graphics = True;
  }

  if ((* cTI == '\0'
      || * (cTI + 1) == '@'	/* workaround for bug in Solaris terminfo */
      ) && strisprefix ("xterm", TERMname)
     ) {
	cTI = "7[?47h[2J";	/* positioning mode */
	cTE = "[2J[?47l8";	/* end " */
  }

  if (* cKS == '\0' && ansi_esc) {
	/* escape sequence to enable application keypad;
	   include ESC= / ESC> for terminals that don't understand 
	   ESC[?66;1h (e.g. linux console)
	 */
	if (strcontains (TERMname, "linux")) {
		cKS = "\033=";
		cKE = "\033>";
	} else {
		cKS = "\033=\033[?66;1h";
		cKE = "\033>\033[?66;1l";
	}
  }

  if (* cAF == '\0' && ansi_esc) {
	cAF = "\033[3%dm";
	cAB = "\033[4%dm";
  }

  if (* cVI == '\0' && ansi_esc) {
	cVI = "\033[?25l";
	if (* cVE == '\0') {
		cVE = "\033[?25h";
	}
  }
}


/**
   the two invocation modes (kb or not) and their usage are a mess
   (also setup_terminal is used in addition)
   - but never touch a running system...
 */
static
void
start_screen_mode (kb)
  int kb;
{
  if (decterm_version > 0) {	/* setup DEC locator reports */
	cMouseX10On = "";
	cMouseX10Off = "";
	cMouseButtonOn = "\033[1'z\033[1'{\033[3'{";
	cMouseButtonOff = "\033[0'z";
	cMouseEventBtnOn = "";
	cMouseEventBtnOff = "";
	cMouseEventAnyOn = "";
	cMouseEventAnyOff = "";
	cMouseExtendedOn = "";
	cMouseExtendedOff = "";
  }

  if (kb) {
	/* adjust control sequences */
	if (! init_done) {
		/* if termcap/terminfo defines combined control sequences that 
		   set/reset both application cursor keys and application 
		   keypad mode, reduce them to application cursor keys only in 
		   order to maintain distinguishable control-keypad-keys
		 */
		if (! use_appl_keypad) {
			if (strncmp (cKS, "\033[?1h", 5) == 0) {
				cKS = "\033[?1h";
			}
			if (strncmp (cKE, "\033[?1l", 5) == 0) {
				cKE = "\033[?1l";
			}
		}
		if (! use_appl_cursor) {
			if (strncmp (cKS, "\033[?1h", 5) == 0) {
				cKS += 5;
			}
			if (strncmp (cKE, "\033[?1l", 5) == 0) {
				cKE += 5;
			}
		}
	}

	/* set keyboard modes */
	termputstr (cKS, affmax);

	if (xterm_version >= 280) {
		/* enable modifyKeyboard resource */
		putescape ("\033[>0;15m");
		/* and then enable VT220 Keyboard with Application Keypad mode */
		putescape ("\033[?1061h\033[?66h");
	}

	/* set mouse modes */
	if (use_mouse) {
#ifdef debug_mouse
		printf ("starting mouse mode\n");
#endif
		putescape (cMouseX10On);	/* activate mouse reporting */
		putescape (cMouseButtonOn);
		putescape (cMouseEventBtnOn);
		if (use_mouse_anymove_always) {
			putescape (cMouseEventAnyOn);
		}
		if (use_mouse_1015) {
			/* numeric encoding of mouse coordinates */
			putescape (cMouse1015On);
		} else if (use_mouse_extended) {
			/* UTF-8 encoding of mouse coordinates */
			putescape (cMouseExtendedOn);
		}
		putescape (cMouseFocusOn);	/* activate focus reports */
		putescape (cAmbigOn);	/* activate ambiguous width change reports */
	}
  } else {
	/* set screen modes */
	termputstr (cTI, affmax);
	termputstr (cVS, affmax);

	/* Reset scrolling region in case terminal is bigger than assumed;
	   terminal misconfiguration effect was observed after 
	   * window size changes of Sun windows
	   * remote login to VMS
	   * explicit misconfiguration with stty
	 */
	if (cCS) {
		/* YMAX as derived from tty may not yet been adjusted 
		   to actual terminal size (in case of inconsistence);
		   checkwinsize() would have to have been called before!
		   Possibly a reason to move this into the section above 
		   for start_screen_mode (1) but then we're getting other 
		   effects due to the cursor movement...
		 */
		/*scrolling_region (0, MENU + YMAX);*/
		scrolling_region (-1, -1);
	}

#ifndef CURSES
	/* enable alternate character set */
	termputstr (cEA, aff1);
#endif
  }
}

static
void
end_screen_mode ()
{
  /* reset mouse modes */
  if (use_mouse) {
	putescape (cMouseFocusOff);
	putescape (cAmbigOff);
	if (use_mouse_1015) {
		putescape (cMouse1015Off);
	} else if (use_mouse_extended) {
		putescape (cMouseExtendedOff);
	}
	putescape (cMouseEventBtnOff);
	putescape (cMouseButtonOff);
	putescape (cMouseX10Off);
#ifdef debug_mouse
	printf ("ended mouse mode\n");
#endif
  }
  /* if mouse was released quickly after, e.g., QUIT command, 
     swallow mouse release report already sent */
#ifndef vms_slow_lookahead
  if (char_ready_within (50, NIL_PTR)) {
	(void) _readchar_nokeymap ();
  }
#endif

  /* reset keyboard modes */
  termputstr (cKE, affmax);

  if (xterm_version >= 280) {
	/* reset modifyKeyboard resource */
	putescape ("\033[>0m");
	/* and then disable VT220 Keyboard with Application Keypad mode */
	putescape ("\033[?1061l\033[?66l");
  }

  /* reset screen modes */
  termputstr (cVE, affmax);
  termputstr (cTE, affmax);

#ifndef CURSES
/* disable alternate character set */
  termputstr (cdisableEA, aff1);
#endif

  flush ();
}

#endif	/* defined (unix) || defined (vms) */


#ifdef vms

void
add_terminfo_entries ()
{
}

#endif


#ifdef msdos

#ifdef __TURBOC__
#define peekbyte peekb
#define peekword peek
#endif
#ifdef __DJGPP__
#define peekbyte _farpeekb
#define peekword _farpeekw
#include <sys/farptr.h>
#endif

#ifdef conio
#include <conio.h>

static struct text_info scrinfo;

#ifdef __TURBOC__
static FLAG use_peek = NOT_VALID;	/* determine on first demand */
#else
static FLAG use_peek = False;	/* peek doesn't seem to work with djgpp */
#endif

/**
   Retrieve screen size from BIOS
	BIOS data:
	40:4A	word	video columns
	can also be determined with INT10h, function 0Fh
	40:84	byte	video lines - 1			(EGA/VGA required)
	40:85	word	character height (pixels)	(EGA/VGA required)
	can also be determined with INT10h, function 11h/30h
 */
void
getwinsize ()
{
  int bioslines;
  int bioscols;
  if (use_peek == NOT_VALID) {
	gettextinfo (& scrinfo);
	/* check whether this is reliable (EGA or VGA only): */
	bioslines = peekbyte (0x40, 0x84) + 1;
	use_peek = scrinfo.screenheight == bioslines;
  }
  if (use_peek) {
	bioslines = peekbyte (0x40, 0x84) + 1;
	bioscols = peekword (0x40, 0x4A);
  } else {
#ifdef __DJGPP__
	/* make sure changed size is detected (e.g. using vgamax) */
	gppconio_init ();
#endif
	gettextinfo (& scrinfo);
	bioslines = scrinfo.screenheight;
	bioscols = scrinfo.screenwidth;
  }

  YMAX = bioslines - 1 - MENU;
  XMAX = bioscols - 1;
}

#endif	/* #ifdef conio */

static int wincheck = 1;

#ifdef __TURBOC__	/* try to improve Turbo-C getch behaviour */

/*
 * getch () reads in a character from keyboard. We seem to need our 
 * own low-level input routine since (at least with Turbo-C) the use 
 * of getch () from conio.h makes the runtime system switch to normal 
 * text mode on startup so that extended text modes could not be used.
 * This is really a very stupid chicane of Turbo-C.
 */
int
dosgetch ()
{
  union REGS regs;
  int result;

#ifdef getchBIOS
  /* Don't use; for some reason, this crap doesn't work.
     And if it's tweaked to work, it doesn't provide keypad keys.
     And it doesn't even help to avoid ^S/^Q/^C/^P handling either.
   */
  static int BIOSgetch = -1;
  if (BIOSgetch == -1) {
	struct SREGS segs;
	regs.h.ah = 0xC0;
	int86x (0x15, & regs, & regs, & segs);
	if (regs.x.cflag == 0 && peekbyte (segs.es, regs.x.bx + 6) & 0x40) {
		regs.h.ah = 9;
		int86 (0x16, & regs, & regs);
		if (regs.h.al & 0x40) {
			BIOSgetch = 0x20;
		} else if (regs.h.al & 0x20) {
			BIOSgetch = 0x10;
		} else {
			BIOSgetch = 0x00;
		}
	} else {
		BIOSgetch = 0x00;
	}
  }

  regs.h.ah = BIOSgetch;
  int86 (0x16, & regs, & regs);
#else
  regs.h.ah = 0x07;
  intdos (& regs, & regs);
#endif

  result = regs.h.al;

  return result;
}

#endif /* __TURBOC__ */

void
ring_bell ()
{
  union REGS regs;

  regs.h.ah = 0x0E;
  regs.h.al = 7;
  regs.h.bh = 0;
  regs.h.bl = 0;
  int86 (0x10, & regs, & regs);
}

void
set_video_lines (r)
/* 0/1/2: 200/350/400 lines */
  int r;
{
  union REGS regs;

  regs.h.ah = 0x12;
  regs.h.bl = 0x30;
  regs.h.al = r;

  int86 (0x10, & regs, & regs);
}

int font_height = 16;
void
set_font_height (r)
/* set font height in character pixels, <= 32 */
  int r;
{
#ifdef __TURBOC__
#define useintr
#endif

#ifdef useintr
  struct REGPACK regs;

  regs.r_ax = 0x1130;
  regs.r_bx = 0;
  intr (0x10, & regs);
  regs.r_ax = 0x1110;
  regs.r_bx = r << 8;
  regs.r_cx = 256;
  regs.r_dx = 0;
/*
  regs.r_bp = 0;
  regs.r_es = 0;
*/
  intr (0x10, & regs);
#else
  union REGS regs;

  regs.h.ah = 0x11;
  regs.h.al = 0x10;
  regs.h.bh = r;
  font_height = r;
  regs.h.bl = 0;
  regs.x.cx = 256;
  regs.x.dx = 0;
/*  regs.x.bp = 0;	ignored by Turbo-C's int86 function */
/*  regs.x.es = 0;	not in structure but accepted by rotten C */
  int86 (0x10, & regs, & regs);
#endif
}

void
set_grafmode_height (r, l)
/* 0/1/2: font height 8/14/16 ; 1/2/3/n: 14/25/43/n lines */
  int r;
  int l;
{
  union REGS regs;

  regs.h.ah = 0x11;
  if (r == 0) {
	regs.h.al = 0x23;
  } else if (r == 1) {
	regs.h.al = 0x22;
  } else {
	regs.h.al = 0x24;
  }
  if (l <= 0) {
	regs.h.bl = 1;
  } else if (l <= 3) {
	regs.h.bl = l;
  } else {
	regs.h.bl = 0;
	regs.h.dl = l;
  }

  int86 (0x10, & regs, & regs);
}

void
set_fontbank (f)
/* 0..7 */
  int f;
{
  union REGS regs;

  regs.h.ah = 0x11;
  regs.h.al = 0x03;
  regs.h.bl = (f & 3) * 5 + (f & 4) * 12;

  int86 (0x10, & regs, & regs);
}

#ifdef conio
static unsigned char norm_attr;	/* default character colours */
static unsigned char inverse_attr;

void
clear_screen ()
{
  clrscr ();
}

void
clear_eol ()
{
  clreol ();
}

void
erase_chars (n)
  register int n;
{
  while (n -- > 0) {
	putchar (' ');
  }
}

void
scroll_forward ()
{
  set_cursor (0, YMAX);
  putchar ('\n');
}

void
scroll_reverse ()
{ /* only to be called if can_add_line or cursor is at top of screen */
  insline ();
}

void
add_line (y)
  register int y;
{ /* only to be called if can_add_line == True */
  set_cursor (0, y);
  insline ();
}

void
delete_line (y)
  register int y;
{
  set_cursor (0, y);
  delline ();
}

void
set_cursor (x, y)
{
  /*current_cursor_y = y;*/
  gotoxy (x + 1, y + 1 + MENU);
}

void
reverse_on ()
{
  textattr (inverse_attr);
/*highvideo ();*/
}

void
reverse_off ()
{
  textattr (norm_attr);
/*normvideo ();*/
}

void
altcset_on ()
{}

void
altcset_off ()
{}

static
void
start_screen_mode (kb)
  int kb;
{}

static
void
end_screen_mode ()
{}

void
menu_mouse_mode (menu)
  FLAG menu;
{
  in_menu_mouse_mode = menu;
}

void
screen_buffer (alt)
  FLAG alt;
{
}

void
get_term (TERM)
  char * TERM;
{
  getwinsize ();

  norm_attr = scrinfo.normattr;
  inverse_attr = ((norm_attr & 0x0F) << 4) | (norm_attr >> 4);
}

#endif	/* #ifdef conio */

#endif	/* #ifdef msdos */


/**************************************************************************
 * screen mode setting functions, alternatively by MSDOS BIOS calls or 
 * ANSI sequences
 */

static int textmode_height = 2;

#ifdef msdos

void
set_screen_mode (m)
  int m;
{
  union REGS regs;

  if (m >= 0) {
	regs.h.ah = 0x00;
	regs.h.al = m;
	int86 (0x10, & regs, & regs);
  }
}

void
set_textmode_height (r)
/* 0/1/2: font height 8/14/16 */
  int r;
{
  union REGS regs;

  regs.h.ah = 0x11;
  regs.h.bl = 0;
  textmode_height = r;
  if (r == 0) {
	regs.h.al = 0x12;
  } else if (r == 1) {
	regs.h.al = 0x11;
  } else {
	regs.h.al = 0x14;
  }

  int86 (0x10, & regs, & regs);
}

#else /* #ifdef msdos - else use ANSI driver: */

int screen_mode = 3	/* just an assumption, cannot be determined */;

void
set_screen_mode (m)
  int m;
{
  char resize_str [8];

  if (m >= 0) {
	if (m != 50 && m != 43) {
		screen_mode = m;
	}
	build_string (resize_str, "\033[=%dh", m);
	putescape (resize_str);
  }
}

void
set_textmode_height (r)
/* 0/1/2: font height 8/14/16 */
  int r;
{
  textmode_height = r;
  if (r == 0) {
	set_screen_mode (50);
  } else if (r == 1) {
	set_screen_mode (43);
  } else {
	set_screen_mode (screen_mode);
  }
}

#endif

void
switch_textmode_height (cycle)
/* True: cycle through font heights 8/14/16 
   False: switch between font heights 8/16 */
  FLAG cycle;
{
  if (textmode_height >= 2) {
	set_textmode_height (0);
  } else if (cycle) {
	set_textmode_height (textmode_height + 1);
  } else {
	set_textmode_height (2);
  }
}


#define djconiox
#ifdef djconio

int ltab [] = {25, 28, 35, 40, 43, 50};

void
maximize_restore_screen ()
{
}

void
resize_font (inc)
  int inc;
{
}

void
resize_the_screen (sb, keep_columns)
  FLAG sb;
  FLAG keep_columns;
{
/*  char resize_str [8]; */
  int lins = YMAX + 1 + MENU;
  int i;

  if (sb == BIGGER) {
	i = 0;
	while (i < arrlen (ltab) && ltab [i] <= lins) {
		i ++;
	}
	if (i == arrlen (ltab)) {
		i = 0;
	}
  } else {
	i = arrlen (ltab) - 1;
	while (i >= 0 && ltab [i] >= lins) {
		i --;
	}
	if (i < 0) {
		i = arrlen (ltab) - 1;
	}
  }
  _set_screen_lines (ltab [i]);
}

#else	/* #ifdef djconio */

#ifdef msdos

#include "dosvideo.t"

void
maximize_restore_screen ()
{
}

void
resize_font (inc)
  int inc;
{
}

void
resize_the_screen (sb, keep_columns)
  FLAG sb;
  FLAG keep_columns;
{
  if (keep_columns && ((sb == SMALLER && textmode_height > 0)
			|| (sb == BIGGER && textmode_height < 2)))
  {
	if (sb == SMALLER) {
		set_textmode_height (textmode_height - 1);
	} else {
		set_textmode_height (textmode_height + 1);
	}
  }
  else
  {
     int newtotal = 0;
     int newmode = -1;
     int totalchars = (XMAX + 1) * (YMAX + 1 + MENU);
     int i = 0;
     while (modetab [i].mode >= 0) {
	int curtotal = modetab [i].cols * modetab [i].lins;
	if (((sb == SMALLER && curtotal < totalchars && curtotal > newtotal) ||
	     (sb == BIGGER && curtotal > totalchars && (newtotal == 0 || curtotal < newtotal)))
	    && (keep_columns == False || modetab [i].cols == XMAX + 1))
	{
		newtotal = curtotal;
		newmode = modetab [i].mode;
	}
	i ++;
     }
     if (newmode >= 0) {
	set_screen_mode (newmode);
	if (keep_columns) {
		if (sb == BIGGER) {
			set_textmode_height (0);
		} else {
			set_textmode_height (2);
		}
	}
     }
  }
}

#else	/* #ifdef msdos */

static FLAG maximised = False;

void
maximize_restore_screen ()
{
  if (maximised) {
	putescape ("\033[9;0t");
	maximised = False;
  } else {
	/*putescape ("\033[9;1t");*/
	putescape ("\033[9;2t");	/* mintty: true full screen */
	maximised = True;
  }
}

void
resize_font (inc)
  int inc;
{
  if (mintty_version >= 503) {
	if (inc > 0) {		/* increase font size */
		putescape ("]7770;+1");
	} else if (inc < 0) {	/* decrease font size */
		putescape ("]7770;-1");
	} else {		/* reset to default */
		putescape ("]7770;");
	}
  } else if (xterm_version >= 203 /* best guess */) {
	if (inc > 0) {		/* increase font size */
		putescape ("]50;#+1");
	} else if (inc < 0) {	/* decrease font size */
		putescape ("]50;#-1");
	} else {		/* reset to default */
		putescape ("]50;#");
	}
  }
}

static
void
resize_screen (l, c)
  int l;
  int c;
{
  static char s [22];
  build_string (s, "\033[8;%d;%dt", l, c);
  putescape (s);
}

static
void
resize_vt (sb, keep_columns)
  FLAG sb;
  FLAG keep_columns;
{
  if (keep_columns == True) {
	/* change number of lines (VT420) */
	int lines = YMAX + 1 + MENU;
	static int vtlines [] = {24, 36, 48, 72, 144};
	int i;
	static char s [15];
	if (sb == SMALLER) {
		for (i = arrlen (vtlines) - 1; i >= 0; i --) {
			if (lines > vtlines [i]) {
				build_string (s, "\033[%dt", vtlines [i]);
				putescape (s);
				YMAX = vtlines [i] - 1 - MENU;
				return;
			}
		}
	} else {
		for (i = 0; i < arrlen (vtlines); i ++) {
			if (lines < vtlines [i]) {
				build_string (s, "\033[%dt", vtlines [i]);
				putescape (s);
				YMAX = vtlines [i] - 1 - MENU;
				return;
			}
		}
	}
  } else {
	/* switch 80/132 columns */
	if (sb == SMALLER) {
		putescape ("\033[?40h\033[?3l");
		XMAX = 79;
	} else {
		putescape ("\033[?40h\033[?3h");
		XMAX = 131;
	}
  }
}

void
resize_the_screen (sb, keep_columns)
  FLAG sb;
  FLAG keep_columns;
{
  if (decterm_version > 0) {
	resize_vt (sb, keep_columns);
  } else {
	int ldif = 6;
	int coldif = 13;
	if (sb == SMALLER) {
		ldif = - ldif;
		coldif = - coldif;
	}
	if (keep_columns == True) {
		coldif = 0;
	} else if (keep_columns == NOT_VALID) {
		ldif = 0;
	}

	resize_screen (YMAX + 1 + MENU + ldif, XMAX + 1 + coldif);
  }
}

#endif

#endif	/* #else djconio */


#endif	/* #else CURSES */


/*======================================================================*\
|*			OS special					*|
\*======================================================================*/

#ifdef msdos

unsigned int
get_codepage ()
{
  union REGS regs;

#ifndef __TURBOC__
  __dpmi_regs mregs;
  short parblock [2];

  mregs.x.ax = 0x440C;
  mregs.x.bx = 1;	/* device handle */
  mregs.x.cx = 0x036A;	/* 3: CON, 0x6A: query selected code page */
  mregs.x.ds = __tb >> 4;
  mregs.x.dx = __tb & 0x0F;
  mregs.x.flags |= 1;	/* preset to detect unimplemented call */
  __dpmi_int (0x21, & mregs);
  if ((mregs.x.flags & 1) == 0) {
	dosmemget (__tb, 4, parblock);
	return parblock [1];
  }
#endif

  /* 2F/AD02 -> BX=codepage
	works with NLSFUNC driver, otherwise returns BX=FFFF, CY set
  */
  regs.x.ax = 0xAD02;
  regs.x.cflag = 1;	/* preset to detect unimplemented call */
  regs.x.bx = 0;	/* preset to detect unset result register */
  int86 (0x2F, & regs, & regs);
  if (regs.x.cflag == 0 && regs.x.bx > 1) {
	return regs.x.bx;
  }

  regs.x.ax = 0x6601;
  regs.x.cflag = 1;	/* preset to detect unimplemented call */
  regs.x.bx = 0;	/* preset to detect unset result register */
  int86 (0x21, & regs, & regs);
  if (regs.x.cflag == 0) {
	return regs.x.bx;
  }

  return 0;
}

#endif


/*======================================================================*\
|*			OS input					*|
\*======================================================================*/

#define dont_debug_select


/*
 * Read a character from the operating system and handle interrupts.
 * Concerning problems due to the interference of read operations and
 * incoming signals (QUIT, WINCH) see the comments at readchar ().
 */

#ifdef cygwin_hup_hang_fix

static
void
catch_alarm (signum)
  int signum;
{
}

/**
   Call this before select () to rescue it from hanging...
   Update: with current cygwin (1.7.15) there doesn't seem to be a need 
   to care about hanging select after terminated terminal anymore...
 */
static
void
ialarm (msec)
  int msec;
{
	struct itimerval timeout;
	timeout.it_value.tv_sec = msec / 1000;
	timeout.it_value.tv_usec = (msec % 1000) * 1000;
	timeout.it_interval.tv_sec = 0;
	timeout.it_interval.tv_usec = 0;

	signal (SIGALRM, catch_alarm);
	setitimer (ITIMER_REAL, & timeout, (struct itimerval *) 0);
}

#endif


#ifdef TERMIO
#define buffered_TERMIO
#endif
#ifdef _getch_
#undef buffered_TERMIO
#endif
#undef buffered_TERMIO	/* works, but doesn't improve terminal pasting */

#ifdef buffered_TERMIO
#define cbuflen 22
static char cbuf [cbuflen];
static int cbufi = 0;
static int cbufn = 0;
#endif

/*
 * Is a character available within msec milliseconds from file handle?
 */
int
inputreadyafter (msec)
  int msec;
{
#ifdef buffered_TERMIO
  if (cbufi < cbufn) {
	return 1;
  }
#endif

  if (tty_closed) {
#ifdef debug_select
	printf ("(tty closed) select -> 0\n");
#endif
	return 0;
  } else {

#ifdef selectpeek

#ifdef FD_SET
	fd_set readfds;	/* causes mined not to work in pipe mode */
#else
	int readfds;	/* gives compiler warnings on System V without casting */
#endif

	struct timeval timeoutstru;
	register int nfds;

	timeoutstru.tv_sec = msec / 1000;
	timeoutstru.tv_usec = (msec % 1000) * 1000;

#ifdef FD_SET
	FD_ZERO (& readfds);
	FD_SET (input_fd, & readfds);
#else
	readfds = 1 << input_fd;
#endif

#ifdef debug_select
	printf ("inputreadyafter %d: select %04lX\n", msec, readfds);
#endif

	if (msec < 0) {
		nfds = select (input_fd + 1, & readfds, 0, 0, 0);
	} else {
		nfds = select (input_fd + 1, & readfds, 0, 0, & timeoutstru);
	}

#ifdef debug_select
	printf ("  ... -> #=%d\n", nfds);
#endif

	return nfds;

#else	/* #ifdef selectpeek */

#ifdef CURSES
	int key;
	int timerms = 0;
	#define timestep 10

	nodelay (stdscr, TRUE);
	while (timerms < msec && (key = curs_readchar ()) == ERR) {
		napms (timestep);
		timerms += timestep;
	}
	nodelay (stdscr, FALSE);
	if (key == ERR) {
		return 0;
	} else {
		ungetch (key);
		return 1;
	}
#else
#ifdef msdos
	if (mousegetch (msec) == -1) {
		return 0;
	} else {
		return 1;
	}
#else
#ifdef vms
	return tthaschar (msec);
#else
	if (msec < 500
	 && msec > 30	/* or flush_keyboard would block */
	) {
		return 1;
	} else {
		return 0;
	}
#endif
#endif
#endif

#endif	/* #else selectpeek */
  }
}


/*
 * Read a char from operating system, handle interrupts if possible, 
 * handle window size changes if possible.
 */

#define dont_debug__readchar

#ifdef debug__readchar
#define trace__readchar(params)	printf params
#else
#define trace__readchar(params)
#endif

#ifdef selectread

static
int
strange (err)
  char * err;
{
  ring_bell ();
  error2 ("Interrupted while reading from terminal: ", err);
  sleep (1);
  ring_bell ();
  return quit_char;
}

static
int
__readchar_reporting_winchg (report_winchg)
  FLAG report_winchg;
{
#ifdef FD_SET
  fd_set readfds;
  fd_set exceptfds;
#else
  int readfds;
  int exceptfds;	/* comments see above */
#endif

  int selret;

#ifndef _getch_
  character c;
  register int n;
#endif

#ifdef buffered_TERMIO
  if (cbufi < cbufn) {
	return cbuf [cbufi ++];
  }
#endif

  if (tty_closed) {
	trace__readchar (("(tty closed) read -> quit\n"));
	quit = True;
	return quit_char;
  }

select_getch:

  trace__readchar (("-> __readchar\n"));
  do {
	trace__readchar (("__readchar do\n"));
	if (winchg) {
		trace_winchg (("__readchar -> winchg -> RDwin\n"));

		if (isscreenmode && continued) {
			continued = False;
			/* recover from SIGSTOP */
			raw_mode (False);
			raw_mode (True);
		}

		trace__readchar (("< RDwin\n"));
		RDwinchg ();
		trace__readchar (("> RDwin\n"));
	}

/* the following would be obsolete with ISIG (termios)
   which is, however, not used anymore as it also suppresses ^\ interrupt;
   on the other hand, it is obsolete with setting VINTR to NDEF as well
 */
#ifdef __CYGWIN__
#define control_C_unsure
#endif
#ifdef CURSES
#define control_C_unsure
#endif

#ifdef control_C_unsure
	if (intr_char) {
		intr_char = False;
		return '\003';
	}
#endif

#ifdef FD_SET
	FD_ZERO (& readfds);
	FD_SET (input_fd, & readfds);
	FD_ZERO (& exceptfds);
	FD_SET (input_fd, & exceptfds);
#else
	readfds = 1 << input_fd;
	exceptfds = readfds;
#endif

	selret = select (input_fd + 1, & readfds, 0, & exceptfds, 0);
	trace__readchar (("select -> %d [%X %X] (errno %d: %s)\n", selret, readfds, exceptfds, geterrno (), serror ()));

#ifdef FD_SET
	if (selret > 0 && FD_ISSET (input_fd, & readfds)) {
#else
	if (selret > 0 && readfds != 0) {
#endif
#ifdef _getch_
	   return getch ();
#else	/* #ifdef _getch_ */
#ifdef buffered_TERMIO
	   cbufn = read (input_fd, & cbuf, cbuflen);
	   if (cbufn > 0) {
		n = 1;
		c = cbuf [0];
		cbufi = 1;
	   } else {
		n = cbufn;
	   }
#else
	   n = read (input_fd, & c, 1);
#endif
	   if (n == 1) {
		trace__readchar (("read: %02X\n", c));
		return c;
	   } else if ((n == 0) || (geterrno () != EINTR)) {
		trace__readchar (("read -> %d (errno %d: %s)\n", n, geterrno (), serror ()));
		tty_closed = True;
		panicio ("Terminal read error", serror ());
	   } else {
		trace__readchar (("read -> strange\n"));
		return strange (serror ());
	   }
#endif	/* #else _getch_ */
	} else {
	   if (quit) {
		trace__readchar (("select -> quit\n"));
		return quit_char;

	   } else if (winchg) {
		trace_winchg (("select -> winchg\n"));
		trace__readchar (("select -> winchg\n"));
		if (report_winchg) {
			trace_winchg (("-> return RDwin\n"));
			/* raise WINCH exception */
			keyproc = RDwin;
			return (int) FUNcmd;
		}

	   } else if (intr_char) {
		trace__readchar (("select -> intr_char\n"));
		intr_char = False;
		return '\003';

	   } else if (hup) {
		trace__readchar (("select -> hup -> repeat\n"));
		hup = False;
		goto select_getch;

	   } else {
		trace__readchar (("select -> strange\n"));
		return strange ("exception");
	   }
	}
  } while (1);
/* NOTREACHED */
}

#else	/* #ifdef selectread */

static
int
__readchar_reporting_winchg (report_winchg)
  FLAG report_winchg;
{
#ifdef _getch_

  int c;

  if (tty_closed) {
	trace__readchar (("(tty closed) read -> quit\n"));
	quit = True;
	return quit_char;
  }

#ifdef dosmouse
  c = mousegetch (-1);	/* no timeout */
  {
	union REGS regs;
	regs.h.ah = 2;
	int86 (0x16, & regs, & regs);
	if (c == FUNcmd) {
		if (regs.h.al & 0x3) {
			mouse_shift |= shift_button;
		}
		if (regs.h.al & 0x4) {
			mouse_shift |= control_button;
		}
		if (regs.h.al & 0x8) {
			mouse_shift |= alt_button;
		}
	} else {
		if (regs.h.al & 0x3) {
			keyshift |= shift_mask;
		}
		if (regs.h.al & 0x4) {
			if (c == 0 || c >= ' ') {
				keyshift |= ctrl_mask;
			}
			/* else: don't add another ctrl flag to ctrl keys */
		}
		if (regs.h.al & 0x8) {
			keyshift |= alt_mask;
		}
	}
  }
#else
  c = getch ();
#endif

#ifdef conio
  if (wincheck) {
	int oldymax = YMAX;
	int oldxmax = XMAX;
	getwinsize ();
	if (YMAX != oldymax || XMAX != oldxmax) {
		winchg = True;
		interrupted = True;
		trace_winchg (("getch -> winchg\n"));
		/* in MSDOS, RDwin calls getch, 
		   so it cannot be called directly here */
	}
  }
#endif

  if (intr_char) {
	/* turn SIGINT signal into intr character */
	/* Note: doesn't seem to be necessary */
	intr_char = False;
	c = '\003';
  }

#else	/* #ifdef _getch_ */

#ifdef vms
  int ret;
#endif
  character c;

  if (tty_closed) {
	trace__readchar (("(tty closed) read -> quit\n"));
	quit = True;
	return quit_char;
  }

#ifdef vms

  ret = ttgetchar ();
  if (ret == VMS$ERROR) {
	tty_closed = True;
	panicio ("Terminal read error", serror ());
  }
  c = ret;

#else

#ifdef debug_select
  printf ("__readchar_reporting_winchg ...\n");
#endif

read_getch:

  if (read (input_fd, & c, 1) != 1 && quit == False) {
#ifdef debug_select
	printf ("  ... != 1\n");
#endif

	if (geterrno () == EINTR || geterrno () == EAGAIN) {
	  if (winchg) {
	    if (report_winchg) {
		trace_winchg (("__readchar_reporting_winchg -> winchg\n"));
		/* raise WINCH exception */
		keyproc = RDwin;
		return (int) FUNcmd;
	    } else {	/* handle WINCH */
		trace_winchg (("__readchar -> winchg -> RDwin\n"));

		if (isscreenmode && continued) {
			continued = False;
			/* recover from SIGSTOP */
			raw_mode (False);
			raw_mode (True);
		}

		trace__readchar (("< RDwin\n"));
		RDwinchg ();
		trace__readchar (("> RDwin\n"));

		goto read_getch;
	    }
	  } else {
		goto read_getch;
	  }
	} else {
		tty_closed = True;
		panicio ("Terminal read error", serror ());
	}
  }
#ifdef debug_select
  printf ("  ... OK\n");
#endif

#endif	/* #else vms */

#endif	/* #else _getch_ */

  if (quit) {
	/* turn SIGQUIT or SIGBREAK signals into quit character */
	/* Note: doesn't seem to work */
	c = quit_char;
  }

  return c;
}

#endif	/* #else selectread */

#define dont_debug___readchar

static
int
__readchar_reporting_winchg_trace (reporting_winchg)
  FLAG reporting_winchg;
{
  int ch = __readchar_reporting_winchg (reporting_winchg);
#ifdef debug_qio
  char log [2];
  log [0] = ch;
  log [1] = 0;
  debuglog ("__readchar", log, "");
#endif
#ifdef debug___readchar
  if ((char) ch >= ' ')
	printf ("__readchar %02X '%c'\n", ch, ch);
  else
	printf ("__readchar %02X\n", ch);
#endif

  return ch;
}

int
__readchar ()
{
  return __readchar_reporting_winchg_trace (False);
}

int
__readchar_report_winchg ()
{
  return __readchar_reporting_winchg_trace (True);
}


/*======================================================================*\
|*			Enquire terminal properties			*|
\*======================================================================*/

#ifdef vms

void
get_term (TERM)
  char * TERM;
{
  ttopen ();
  get_term_cap ("vt100");
}

void
getwinsize ()
{
  YMAX = ttrows - 1 - MENU;	/* # of lines */
  XMAX = ttcols - 1;	/* # of columns */
}

#else

#if defined (unix) || defined (ANSI)

/*
 * Get current window size
   If this does not work (e.g. on SCO Caldera Linux, or VMS (remote login), 
   or likely whenever a real terminal is connected), 
   the issue will be caught and handled by checkwinsize() by querying the terminal.
 */
void
getwinsize ()
{
#ifdef TIOCGWINSZ
  struct winsize winsiz;
  static int get_winsz = 1; /* check only once; on SunOS there was a 
				bug that after initial result values 0 0 
				(size not known to SunOS), further calls 
				would return erratic random values.
			    */

  if (get_winsz) {
	(void) ioctl (output_fd, TIOCGWINSZ, & winsiz);
	if (winsiz.ws_row != 0) {
		YMAX = winsiz.ws_row - 1 - MENU;
	}
	if (winsiz.ws_col != 0) {
		XMAX = winsiz.ws_col - 1;
	} else {
		get_winsz = 0;
	}
  }
#else
#ifdef TIOCGSIZE
  struct ttysize ttysiz;

  (void) ioctl (output_fd, TIOCGSIZE, & ttysiz);
  if (ttysiz.ts_lines != 0) {
	YMAX = ttysiz.ts_lines - 1 - MENU;
  }
  if (ttysiz.ts_cols != 0) {
	XMAX = ttysiz.ts_cols - 1;
  }
#else
#ifdef WIOCGETD
  struct uwdata uwdat;

  (void) ioctl (output_fd, WIOCGETD, & uwdat);
  if (uwdat.uw_height > 0) {
	YMAX = uwdat.uw_height / uwdat.uw_vs - 1 - MENU;
  }
  if (uwdat.uw_width > 0) {
	XMAX = uwdat.uw_width / uwdat.uw_hs - 1;
  }
#else
#warning [41m system does not support terminal size retrieval (but terminal is queried) [0m
  /* leave previous size assumption */
  return;
#endif
#endif
#endif

#ifdef CURSES
# if defined (__PDCURSES__) || defined (NCURSES_VERSION)
  if (init_done) {
	resize_term (YMAX + 1 + MENU, XMAX + 1);
  }
# endif
#endif

  trace_winchg (("getwinsize: %d * %d\n", XMAX + 1, YMAX + 1 + MENU));
}

/*
 * Get terminal information
 */
void
get_term (TERMname)
  char * TERMname;
{
  if (TERMname == NIL_PTR) {
#ifdef __EMX__
	TERMname = "ansi";
#else
	panic ("Terminal not specified", NIL_PTR);
#endif
  }

  get_term_cap (TERMname);
  if (strisprefix ("xterm", TERMname)) {
	title_string_pattern = "\033]2;%s\007\033]1;%s\007";
  } else if (strisprefix ("rxvt", TERMname)) {
	title_string_pattern = "\033]2;%s\007\033]1;%s\007";
#ifndef CURSES
  } else if (streq (TERMname, "cygwin")) {
	title_string_pattern = "\033]2;%s\007\033]1;%s\007";
	if (explicit_border_style) {
		use_vga_block_graphics = False;
	} else {
		use_vt100_block_graphics = False;
		use_pc_block_graphics = True;
	}
#endif
#ifndef __MINGW32__
  } else if (strncmp (TERMname, "sun", 3) == 0 && ! streq (ttyname (output_fd), "/dev/console")) {
	title_string_pattern = "\033]l%s\033\\\033]L%s\033\\";
#endif
  } else if (strisprefix ("aixterm", TERMname)) {
	title_string_pattern = "\033]2;%s\007\033]1;%s\007";
  } else if (strisprefix ("dtterm", TERMname)) {
	title_string_pattern = "\033]2;%s\007\033]1;%s\007";
  } else if (strisprefix ("iris-", TERMname)) {
	title_string_pattern = "\033P1.y%s\033\\\033P3.y%s\033\\";
  } else if (strisprefix ("hpterm", TERMname)) {
	title_string_pattern = "\033&f0k%dD%s\033&f-1k%dD%s";
  } else {
	title_string_pattern = "";
  }

  if (! ansi_esc) {
	use_mouse_button_event_tracking = False;
  }

/*
  build_string (text_buffer, "Terminal is %s, %d * %d.\n", TERMname, YMAX+1, XMAX+1);
  putstring (text_buffer);
*/
}

#endif	/* #ifdef unix || ANSI */

#endif	/* #else vms */


/*======================================================================*\
|*			Screen attribute control			*|
\*======================================================================*/

#define dont_debug_disp_colour

static
int
rgb (col, for_256_colours)
  int col;
  FLAG for_256_colours;
{
	if (col < 16) {
		if (col >= 8) {
			col -= 8;
		}
		switch (col) {
		case 0:	/* black */
			return 0x000000;
		case 1:	/* red */
			return 0xFF0000;
		case 2:	/* green */
			return 0x00FF00;
		case 3:	/* yellow */
			return 0xFFFF00;
		case 4:	/* blue */
			return 0x0000FF;
		case 5:	/* magenta */
			return 0xFF00FF;
		case 6:	/* cyan */
			return 0x00FFFF;
		case 7:	/* white */
			return 0xFFFFFF;
		default:	return 0;
		}
	} else if (for_256_colours || col >= 88) {
		/* 256 colours: 16 VGA + 6×6×6 colour cube + 24 gray shades */
		if (col >= 232) {
			int gray = (col - 231) * 256 / 25;
			return gray << 16 | gray << 8 | gray;
		} else {
			int r, g, b;
			col -= 16;	/* clear VGA offset */
			r = col / 36;
			g = (col % 36) / 6;
			b = col % 6;
			r *= 0x33;
			g *= 0x33;
			b *= 0x33;
			return r << 16 | g << 8 | b;
		}
	} else {
		/* 88 colours: 16 VGA + 4×4×4 colour cube + 8 gray shades */
		if (col >= 80) {
			int gray = (col - 79) * 256 / 9;
			return gray << 16 | gray << 8 | gray;
		} else {
			int r, g, b;
			col -= 16;	/* clear VGA offset */
			r = col / 16;
			g = (col % 16) / 4;
			b = col % 4;
			r *= 0x55;
			g *= 0x55;
			b *= 0x55;
			return r << 16 | g << 8 | b;
		}
	}
}

static
int
R (rgb0)
  int rgb0;
{
	return rgb0 >> 16;
}

static
int
G (rgb0)
  int rgb0;
{
	return (rgb0 >> 8) & 0xFF;
}

static
int
B (rgb0)
  int rgb0;
{
	return rgb0 & 0xFF;
}

static
unsigned int
dist (rgb1, rgb2)
  int rgb1, rgb2;
{
	int dr = R (rgb1) - R (rgb2);
	int dg = G (rgb1) - G (rgb2);
	int db = B (rgb1) - B (rgb2);
	return dr * dr + dg * dg + db * db;
}

static
int
map8 (col, for_256_colours)
  int col;
  FLAG for_256_colours;
{
	unsigned int d = (unsigned int) -1;	/* max unsigned int */
	int i;
	int rgb_col = rgb (col, for_256_colours);
	int repl_col = -1;

	if (col >= 244) {
		return 7;
	} else if (col >= 232) {
		return 0;
	}
	for (i = 0; i < 8; i ++) {
		unsigned int dist_col = dist (rgb (i, for_256_colours), rgb_col);
		if (dist_col < d) {
			d = dist_col;
			repl_col = i;
		}
#ifdef debug_disp_colour
		printf (" %06X %06X dist %d col %d\n", rgb_col, rgb (i, for_256_colours), dist_col, repl_col);
#endif
	}
	if (repl_col == 0) {
		return 4;	/* replace black with blue */
	}
	if (repl_col == 7) {
		return 3;	/* replace white with yellow */
	}
	return repl_col;
}


#ifdef CURSES

void
disp_colour (c, for_256_colours)
  int c;
  FLAG for_256_colours;
{
	attrset (COLOR_PAIR (colour8_base + map8 (c, for_256_colours)));
	if (dark_term) {
		attron (A_BOLD);
	}
}

void
emph_on ()
{
	attrset (COLOR_PAIR (emphcolor));
	if (dark_term) {
		attron (A_BOLD);
	}
}

void
mark_on ()
{
	if (colour_token >= 0) {
		disp_colour (colour_token, UNSURE);
		colour_token = -1;
	} else if (nonempty (markansi)) {
		attrset (COLOR_PAIR (markcolor));
	} else if (! dark_term) {
		attrset (A_DIM);
	}
	if (dark_term) {
		attron (A_BOLD);
	}
}

void
mark_off ()
{
	standend ();
}


void
bold_on ()
{
	attrset (A_BOLD);
}

void
bold_off ()
{
	standend ();
}

void
underline_on ()
{
	attrset (A_UNDERLINE);
}

void
underline_off ()
{
	standend ();
}

void
blink_on ()
{
	attrset (A_BLINK);
}

void
blink_off ()
{
	standend ();
}

void
disp_normal ()
{
	standend ();
}

void
disp_selected (bg, border)
  FLAG bg;
  FLAG border;
/* for selected menu item */
{
/*	| selected item |
	left border: bg && border (if graphic)
	  selected item: bg && ! border
	                right border: ! bg && border (if graphic)
*/
	if (avoid_reverse_colour) {
		/* workaround for buggy cygwin reverse bold colour handling */
		if (bg) {
			if (border) {
				attrset (COLOR_PAIR (selcolor));
			} else {
				reverse_off ();
				attrset (COLOR_PAIR (selcolor) | A_REVERSE | A_BOLD);
			}
		} else {
			attrset ((border ? COLOR_PAIR (selcolor) : COLOR_PAIR (selcolor) | A_REVERSE));
		}
	} else {
		if (nonempty (selansi)) {
			attrset ((border ? COLOR_PAIR (selcolor) : COLOR_PAIR (selcolor) | A_REVERSE)
				 | (bg ? A_REVERSE : 0)
				 | (dark_term ? A_BOLD : 0)
				 );
		} else {
			standout ();
		}
	}
}


void
unidisp_on ()
/* for non-displayable or illegal Unicode characters */
{
	if (nonempty (uniansi)) {
		/*attrset (COLOR_PAIR (unicolor));*/
		attrset (COLOR_PAIR (specialcolor) | A_REVERSE);
	} else {
		standout ();
	}
}

void
unidisp_off ()
{
	standend ();
}


void
specialdisp_on ()
/* for Unicode line end indications */
{
	if (nonempty (specialansi)) {
		attrset (COLOR_PAIR (specialcolor));
	} else {
		standout ();
	}
}

void
specialdisp_off ()
{
	standend ();
}


void
combiningdisp_on ()
/* for Unicode combining character indication in separated display mode */
{
	if (nonempty (combiningansi)) {
		/*attrset (COLOR_PAIR (combiningcolor));*/
		attrset (COLOR_PAIR (specialcolor) | A_REVERSE);
	} else {
		standout ();
	}
}

void
combiningdisp_off ()
{
	standend ();
}


void
ctrldisp_on ()
{
	if (nonempty (ctrlansi)) {
		attrset (COLOR_PAIR (ctrlcolor));
	} else {
		standout ();
	}
}

void
ctrldisp_off ()
{
	standend ();
}


void
dispHTML_code ()
{
	if (nonempty (HTMLansi)) {
		attrset (COLOR_PAIR (HTMLcolor));
	} else {
		standout ();
	}
}

void
dispHTML_comment ()
{
	if (nonempty (HTMLansi)) {
		attrset (COLOR_PAIR (HTMLcomment));
	} else {
		standout ();
	}
}

void
dispHTML_jsp ()
{
	if (nonempty (HTMLansi)) {
		attrset (COLOR_PAIR (HTMLjsp));
	} else {
		standout ();
	}
}

void
dispXML_attrib ()
{
	if (nonempty (XMLattribansi)) {
		attrset (COLOR_PAIR (XMLattrib));
	} else {
		standout ();
	}
}

void
dispXML_value ()
{
	if (nonempty (XMLvalueansi)) {
		attrset (COLOR_PAIR (XMLvalue));
	} else {
		standout ();
	}
}

void
dispHTML_off ()
{
	standend ();
}


void
menudisp_on ()
{
	if (nonempty (menuansi)) {
		attrset (COLOR_PAIR (menucolor));
	} else {
		standout ();
	}
}

void
menudisp_off ()
{
	standend ();
}

void
diagdisp_on ()
{
	if (nonempty (diagansi)) {
		attrset (COLOR_PAIR (diagcolor));
	} else {
		standout ();
	}
}

void
diagdisp_off ()
{
	standend ();
}


void
disp_scrollbar_foreground ()
{
	if (nonempty (scrollbgansi)) {
		attrset (COLOR_PAIR (scrollfgcolor));
	} else {
		standout ();
	}
}

void
disp_scrollbar_background ()
{
	if (nonempty (scrollbgansi)) {
		attrset (COLOR_PAIR (scrollfgcolor) | A_REVERSE);
	}
}

void
disp_scrollbar_off ()
{
	standend ();
}


void
menuborder_on ()
{
	if (! use_normalchars_boxdrawing) {
		altcset_on ();
	}
#ifdef use_default_colour
	attrset (COLOR_PAIR (menuborder) | (bold_border ? A_BOLD : 0));
#endif
	in_menu_border = True;
}

void
menuborder_off ()
{
	standend ();
	if (! use_normalchars_boxdrawing) {
		altcset_off ();
	}
	in_menu_border = False;
}

void
menuitem_on ()
{
#ifdef use_default_colour
	attrset (COLOR_PAIR (menuitem) /* | A_BOLD */);
#endif
}

void
menuitem_off ()
{
	standend ();
}

void
menuheader_on ()
{
	if (! use_normalchars_boxdrawing) {
		altcset_off ();
	}
	if (avoid_reverse_colour) {
		/* workaround for buggy cygwin reverse bold colour handling */
		disp_normal ();
		attrset (COLOR_PAIR (menuheader));
	} else {
		attrset (COLOR_PAIR (menuheader) | A_BOLD | A_REVERSE);
	}
	in_menu_border = False;
}

void
menuheader_off ()
{
	standend ();
	in_menu_border = False;
}

#else	/* #ifdef CURSES */


#ifdef conio

void
disp_colour (c, for_256_colours)
  int c;
  FLAG for_256_colours;
{
	switch (map8 (c, for_256_colours)) {
		case 0: textcolor (BLACK); break;
		case 1: textcolor (RED); break;
		case 2: textcolor (GREEN); break;
		case 3: textcolor (YELLOW); break;
		case 4: textcolor (BLUE); break;
		case 5: textcolor (MAGENTA); break;
		case 6: textcolor (CYAN); break;
		case 7: textcolor (WHITE); break;
	}
	if (dark_term) {
		highvideo ();
	}
}

void
emph_on ()
{
	/* check emphansi ? */
	textcolor (RED);
	if (dark_term) {
		highvideo ();
	}
}

void
mark_on ()
{
	/* check markansi ? */
	if (colour_token >= 0) {
		disp_colour (colour_token, UNSURE);
		colour_token = -1;
	} else {
		textcolor (RED);
	}
	if (dark_term) {
		highvideo ();
	}
}

void
mark_off ()
{
	normvideo ();
}


void
bold_on ()
{
	highvideo ();
}

void
bold_off ()
{
	normvideo ();
}

void
underline_on ()
{
	highvideo ();
}

void
underline_off ()
{
	normvideo ();
}

void
blink_on ()
{
#ifdef __DJGPP__
	blinkvideo ();
#else
	highvideo ();
#endif
}

void
blink_off ()
{
	normvideo ();
}

void
disp_normal ()
{
	normvideo ();
}

void
disp_selected (bg, border)
  FLAG bg;
  FLAG border;
{
	/* check selansi ? */
	if (bg) {
		if (dark_term) {
			highvideo ();
		} else {
			normvideo ();
		}
		textbackground (BLUE);
		if (! border) {
			textcolor (YELLOW);
		}
	} else {
		textcolor (BLUE);
	}
}


void
unidisp_on ()
/* for non-displayable or illegal Unicode characters */
{
	/* check uniansi ? */
	textbackground (CYAN);
}

void
unidisp_off ()
{
	normvideo ();
}


void
specialdisp_on ()
/* for Unicode line end indications */
{
	textcolor (CYAN);
}

void
specialdisp_off ()
{
	normvideo ();
}


void
combiningdisp_on ()
/* for Unicode combining character indication in separated display mode */
{
	textbackground (CYAN);
	textcolor (BLACK);
}

void
combiningdisp_off ()
{
	normvideo ();
}


void
ctrldisp_on ()
{
	reverse_on ();
}

void
ctrldisp_off ()
{
	reverse_off ();
}


void
dispHTML_code ()
{
	textcolor (LIGHTBLUE);
}

void
dispXML_attrib ()
{
	textcolor (RED);
}

void
dispXML_value ()
{
	textcolor (MAGENTA);
}

void
dispHTML_comment ()
{
	textcolor (LIGHTBLUE);
	textbackground (YELLOW);
}

void
dispHTML_jsp ()
{
	textcolor (CYAN);
	textbackground (BLACK);
}

void
dispHTML_off ()
{
	normvideo ();
}


void
menudisp_on ()
{
	reverse_on ();
}

void
menudisp_off ()
{
	reverse_off ();
}

void
diagdisp_on ()
{
	reverse_on ();
}

void
diagdisp_off ()
{
	reverse_off ();
}


void
disp_scrollbar_foreground ()
{
/*	textattr (CYAN);	*/
	textbackground (BLUE);
}

void
disp_scrollbar_background ()
{
/*	textattr (BLUE);	*/
	textbackground (CYAN);
}

void
disp_scrollbar_off ()
{
	normvideo ();
}


void
menuborder_on ()
{
	if (! use_normalchars_boxdrawing) {
		altcset_on ();
	}
	textattr (RED);
	if (bold_border) {
		highvideo ();
	}
	in_menu_border = True;
}

void
menuborder_off ()
{
	normvideo ();
	textattr (norm_attr);
	if (! use_normalchars_boxdrawing) {
		altcset_off ();
	}
	in_menu_border = False;
}

void
menuitem_on ()
{
	textbackground (YELLOW);
	/* highvideo (); */
}

void
menuitem_off ()
{
	textattr (norm_attr);
}

void
menuheader_on ()
{
	if (! use_normalchars_boxdrawing) {
		altcset_off ();
	}
	/*reverse_on ();*/
	textbackground (RED);
	textcolor (LIGHTGRAY);
	in_menu_border = False;
}

void
menuheader_off ()
{
	textattr (norm_attr);
	in_menu_border = False;
}

#else	/* #ifdef conio */


static
FLAG
putansistring (s)
  char * s;
{
	if (ansi_esc && nonempty (s)) {
		if (suppress_colour && (* s == '3' || * s == '4')) {
			/* e.g. for Android Terminal Emulator */
			if (s == scrollfgansi) {
				return False;
			} else if (s == scrollbgansi) {
				return True;
			} else if (strstr (s, ";1")) {
				putansistring ("1");
				return False;
			} else if (strstr (s, ";7")) {
				putansistring ("7");
				return False;
			} else {
				return False;
			}
		}
		putescape ("\033[");
		putescape (s);
		putescape ("m");
		return True;
	} else {
		return False;
	}
}

void
disp_colour (c, for_256_colours)
  int c;
  FLAG for_256_colours;
{
	char ctrl [19];
	int col8 = map8 (c, for_256_colours);

	if (c < 8) {
		build_string (ctrl, "3%d", c);
	} else if (c < 16) {
		build_string (ctrl, "9%d", c - 8);
	} else {
		if (colours_256 || colours_88) {
			build_string (ctrl, "38;5;%d", c);
		} else {
			build_string (ctrl, "3%d", col8);
		}
	}
#ifdef debug_disp_colour
	printf ("disp_colour %d put <%s>\n", c, ctrl);
#endif
	(void) putansistring (ctrl);

	if (col8 == 0 || col8 == 1 || col8 == 4) {
		/* black, red, blue */
		if (dark_term) {
			bold_on ();
		}
	} else {
		if (! dark_term) {
			bold_on ();
		}
	}
}

void
emph_on ()
{
	if (dark_term) {
		bold_on ();
	}
	(void) putansistring (emphansi);
}

void
mark_on ()
{
	if (dark_term) {
		bold_on ();
	} else {
#if defined (unix) || defined (vms)
		if (can_dim && ! screen_version && ansi_esc &&
		    (! cMH || ! * cMH)) {
			cMH = "[2m";
		}
		termputstr (cMH, aff1);
#endif
	}
	if (colour_token >= 0) {
		disp_colour (colour_token, UNSURE);
		colour_token = -1;
	} else {
		(void) putansistring (markansi);
	}
}

void
mark_off ()
{
	if (! putansistring ("0")) {
		termputstr (cME, aff1);
	}
}

void
bold_on ()
{
	if (use_bold) {
		termputstr (cMD, aff1);
	}
}

void
bold_off ()
{
	if (use_bold) {
		termputstr (cME, aff1);
	}
}

void
underline_on ()
{
	termputstr (cUL, aff1);
}

void
underline_off ()
{
	termputstr (cME, aff1);
}

void
blink_on ()
{
	termputstr (cBL, aff1);
}

void
blink_off ()
{
	termputstr (cME, aff1);
}

void
disp_normal ()
{
	if (screen_version > 0) {
		/* workaround to reset attributes passed by transparently */
		reverse_on ();
		reverse_off ();
	}
	termputstr (cME, aff1);
}

static
char *
reverse_colour (ansi)
  char * ansi;
{
	static char revansi [3];
	if (ansi [0] == '3') {
		revansi [0] = '4';
	} else {
		revansi [0] = '3';
	}
	revansi [1] = ansi [1];
	revansi [2] = 0;
	return revansi;
}

void
disp_selected (bg, border)
  FLAG bg;
  FLAG border;
{
/*	| selected item |
	left border: bg && border (if graphic)
	  selected item: bg && ! border
	                right border: ! bg && border (if graphic)
*/
	if (avoid_reverse_colour) {
		/* workaround for buggy cygwin reverse bold colour handling */
		if (bg) {
			if (border) {
				(void) putansistring (selansi);
				reverse_on ();
			} else {
				reverse_off ();
				(void) putansistring (reverse_colour (selansi));
				(void) putansistring (reverse_colour (selfgansi));
				bold_on ();
			}
		} else {
			(void) putansistring (selansi);
			if (! border) {
				(void) putansistring (selfgansi);
			}
		}
	} else {
		(void) putansistring (selansi);
		if (! border) {
			(void) putansistring (selfgansi);
		}
		if (bg) {
			reverse_on ();
		}
	}
}


void
unidisp_on ()
/* for non-displayable or illegal Unicode characters */
{
	if (! putansistring (uniansi)) {
		reverse_on ();
	}
}

void
unidisp_off ()
{
	if (nonempty (uniansi)) {
		if (! putansistring ("0")) {
			reverse_off ();
		}
	} else {
		reverse_off ();
	}
}


void
specialdisp_on ()
/* for Unicode line end indications */
{
	if (! putansistring (specialansi)) {
		reverse_on ();
	}
}

void
specialdisp_off ()
{
	if (nonempty (specialansi)) {
		if (! putansistring ("0")) {
			reverse_off ();
		}
	} else {
		reverse_off ();
	}
}


void
combiningdisp_on ()
/* for Unicode combining character indication in separated display mode */
{
	if (! putansistring (combiningansi)) {
		reverse_on ();
	}
}

void
combiningdisp_off ()
{
	if (nonempty (combiningansi)) {
		if (! putansistring ("0")) {
			reverse_off ();
		}
	} else {
		reverse_off ();
	}
}


void
ctrldisp_on ()
{
	if (! putansistring (ctrlansi)) {
		reverse_on ();
	}
}

void
ctrldisp_off ()
{
	if (nonempty (ctrlansi)) {
		if (! putansistring ("0")) {
			reverse_off ();
		}
	} else {
		reverse_off ();
	}
}


void
dispHTML_code ()
{
	if (! putansistring (HTMLansi)) {
		mark_on ();
	}
}

void
dispXML_attrib ()
{
	if (! putansistring (XMLattribansi)) {
		if (! putansistring ("31")) {
			mark_on ();
		}
	}
}

void
dispXML_value ()
{
	if (! putansistring (XMLvalueansi)) {
		if (! putansistring ("35")) {
			mark_on ();
		}
	}
}

void
dispHTML_comment ()
{
	char ctrl [19];

	if (nonempty (HTMLansi)) {
		if (fg_yellowish) {
			build_string (ctrl, "%s;44;33", HTMLansi);
		} else {
			build_string (ctrl, "%s;43", HTMLansi);
		}
		if (! putansistring (ctrl)) {
			mark_on ();
		}
	} else {
		mark_on ();
	}
}

void
dispHTML_jsp ()
{
	if (nonempty (HTMLansi)) {
		/*(void) putansistring ("36");*/
		if (! putansistring ("36;40")) {
			mark_on ();
		}
	} else {
		mark_on ();
	}
}

void
dispHTML_off ()
{
	if (nonempty (HTMLansi)) {
		if (! putansistring ("0")) {
			mark_off ();
		}
	} else {
		mark_off ();
	}
}


void
menudisp_on ()
{
	if (! putansistring (menuansi)) {
		reverse_on ();
	}
}

void
menudisp_off ()
{
	if (nonempty (menuansi)) {
		if (! putansistring ("0")) {
			reverse_off ();
		}
	} else {
		reverse_off ();
	}
}

void
diagdisp_on ()
{
	if (! putansistring (diagansi)) {
		reverse_on ();
	}
}

void
diagdisp_off ()
{
	if (nonempty (diagansi)) {
		if (! putansistring ("0")) {
			reverse_off ();
		}
	} else {
		reverse_off ();
	}
}


void
disp_scrollbar_foreground ()
{
	if (suppress_colour) {
		reverse_off ();
	} else if (nonempty (scrollfgansi)) {
		if (! putansistring (scrollfgansi)) {
			reverse_on ();
		}
	} else {
		(void) putansistring (scrollbgansi);
		reverse_on ();
	}
}

void
disp_scrollbar_background ()
{
	if (suppress_colour) {
		reverse_on ();
	} else {
		reverse_off ();
		if (! putansistring (scrollbgansi)) {
			termputstr (cSO, aff1);
		}
	}
}

void
disp_scrollbar_off ()
{
	if (nonempty (scrollbgansi)) {
		if (! putansistring ("0")) {
			reverse_off ();
		}
	} else {
		reverse_off ();
	}
}


void
menuborder_on ()
{
	if (! use_normalchars_boxdrawing) {
		altcset_on ();
	}
	if (bold_border) {
		bold_on ();
	}
	(void) putansistring (borderansi);
	in_menu_border = True;
}

void
menuborder_off ()
{
	(void) putansistring ("0");
	if (! use_normalchars_boxdrawing) {
		altcset_off ();
	}
	in_menu_border = False;
}

void
menuitem_on ()
{
	if (use_bgcolor) {
		/* try to keep it visible on old buggy hanterm: */
		/* - hopefully not needed anymore; */
		/* - interferes unpleasantly with buggy 
		     bold/colour combination on gnome-terminal/konsole */
		/* bold_on (); */

		if (! putansistring (fg_yellowish ? "44;33" : "43;30")) {
			reverse_on ();
		}
	} else {
		if (! putansistring ("0;1")) {
			reverse_on ();
		}
	}
}

void
menuitem_off ()
{
	if (! putansistring ("0")) {
		reverse_off ();
	}
}

void
menuheader_on ()
{
	if (! use_normalchars_boxdrawing) {
		altcset_off ();
	}
	if (avoid_reverse_colour) {
		/* workaround for buggy cygwin reverse bold colour handling */
		disp_normal ();
		if (dark_term) {
			(void) putansistring ("37");
		}
		(void) putansistring (reverse_colour (borderansi));
	} else {
		if (dark_term) {
			bold_on ();
		}
		/* Note: menu border colour is already switched on */
		if (! putansistring ("7")) {
			reverse_on ();
		}
	}
	in_menu_border = False;
}

void
menuheader_off ()
{
	if (! putansistring ("0")) {
		reverse_off ();
	}
	in_menu_border = False;
}

#endif	/* #else conio */


#endif	/* #else CURSES */


/*======================================================================*\
|*			DOS mouse API					*|
\*======================================================================*/

#ifdef dosmouse
#include "dosmouse.c"
#endif


/*======================================================================*\
|*				End					*|
\*======================================================================*/
