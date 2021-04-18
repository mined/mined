/*======================================================================*\
|*		Editor mined						*|
|*		MSDOS mouse functions					*|
|*		currently included from io.c				*|
\*======================================================================*/

#define dpmi_call

#ifdef __TURBOC__
#undef dpmi_call
#endif

#ifdef dpmi_call
#include <dpmi.h>
#endif

#ifndef __TURBOC__
#include <pc.h>
#endif


extern void mouseinit _((void));
extern void hidemouse _((void));
extern void mousestat _((void));
extern int mousegetch _((int msec));
extern void DIRECTdosmousegetxy _((void));


/*======================================================================*\
|*		mouse functions and data				*|
\*======================================================================*/

#ifdef dpmi_call
static __dpmi_regs mregs;
#else
static union REGS mregs;
#endif


static
void
mousecall ()
{
	mregs.x.bx = 0;
#ifdef dpmi_call
	__dpmi_int (0x33, & mregs);
#else
	int86 (0x33, & mregs, & mregs);
#endif
}

void
mouseinit ()
{
	mregs.x.ax = 0;
	mousecall ();
}

static
void
showmouse ()
{
	mregs.x.ax = 1;
	mousecall ();
}

void
hidemouse ()
{
	mregs.x.ax = 2;
	mousecall ();
}

void
mousestat ()
{
	mregs.x.ax = 3;
	mousecall ();
}


static
void
yield_cpu ()
{
	mregs.x.ax = 0x1680;
#ifdef dpmi_call
	__dpmi_int (0x2F, & mregs);
#else
	/* no effect in this case !?! */
	int86 (0x2F, & mregs, & mregs);
#endif
}


static mousebutton_type new_button;
static int new_x, new_y;


#define dont_debug_mousegetch

static
long
timediff ()
{
  static long prevmsec = 0;
  long msec;
  long msecdiff;

#define gettime_2

#ifdef gettime_1
  {	struct timeval now;
	gettimeofday (& now, 0);
	msec = now.tv_sec * 1000 + now.tv_usec / 1000;
  }
#else
#ifdef gettime_2
  {	struct time now;
	gettime (& now);
	msec = ((now.ti_hour * 60 + now.ti_min) * 60 + now.ti_sec) * 1000 + now.ti_hund * 10;
  }
#else
#ifdef gettime_3
  {	struct _dostime_t now;
	_dos_gettime (& now);
	msec = ((now.hour * 60 + now.minute) * 60 + now.second) * 1000 + now.hsecond * 10;
  }
#else
#error no gettime method selected
#endif
#endif
#endif

  msecdiff = msec - prevmsec;
  if (msecdiff < 0) {	/* @ midnight */
	msecdiff += 24 * 60 * 60 * 1000;
  }
#ifdef debug_mousegetch
  printf (" - timediff %ld ms\n", msecdiff);
#endif

  prevmsec = msec;
  return msecdiff;
}


/**
   mousegetch: check for keyboard or mouse input
   @param msec
   	< 0: read; wait for keyboard or mouse event
   	= 0: check if available; do not wait
   		(like kbhit function)
   	> 0: check with timeout; wait for max msec ms
   		(aka half-delay mode)
   @return
   	>= 0: byte read (if msec < 0)
   	FUNcmd ( < -1): mouse input (if msec < 0);
   		call DIRECTdosmousegetxy () to retrieve parameters
   	-1: no input available / timeout (if msec >= 0)
   	-2: keyboard input available (if msec >= 0)
   	-3: mouse input available (if msec >= 0)
 */
int
mousegetch (msec)
  int msec;	/* timeout */
{
	int stat;
	long timeout = msec;	/* timeout count-down */
	static int mhit = 0;

#ifdef debug_mousegetch
	printf ("mousegetch (%d)\n", msec);
#endif

	/* return mouse event that was previously detected available */
	if (msec < 0 && mhit) {
		mhit = 0;
		return FUNcmd;
	}
	mhit = 0;

	showmouse ();
	mousestat ();
	stat = mregs.x.bx;
	new_y = mregs.x.dx;
	new_x = mregs.x.cx;

	if (msec >= 0) {
		(void) timediff ();
	}
/*	check key input with bioskey (1) or kbhit () */
	while (! kbhit () && mregs.x.bx == stat
		&& mregs.x.dx == new_y && mregs.x.cx == new_x
		&& (msec < 0 || timeout > 0)
		)
	{
#ifdef debug_mousegetch
		napms (222);	/* needs -lpdcurses */
#endif
		/* avoid using 99% CPU */
		yield_cpu ();	/* or call __dpmi_yield (); */

		/* check timeout */
		if (msec >= 0) {
			timeout -= timediff ();
		}

		/* check mouse again */
		mousestat ();
	}

	if (kbhit ()) {
		hidemouse ();
#ifdef debug_mousegetch
		printf ("  return key\n");
#endif
		if (msec < 0) {
#ifdef __TURBOC__
			return getch ();
#else
			return getxkey ();
#endif
		} else {
			return -2;	/* indicate keyboard input available */
		}
	} else {
		/* report timeout */
		if (msec >= 0 && timeout <= 0) {
			/* no need to check mouse events in this case? */
#ifdef debug_mousegetch
			printf ("  return timeout\n");
#endif
			return -1;	/* indicate no input available */
		}

	/*	bx: button - 1 = left, 2 = right, 4 = middle
		cx: column * 8 (first is 0)
		dx: line * 8 (first is 0)
	*/
		if (mregs.x.bx == stat) {
			if (in_menu_mouse_mode) {
				new_button = movebutton;
			} else {
#ifdef debug_mousegetch
				printf ("  recurse\n");
#endif
				return mousegetch (timeout);
			}
		} else if (mregs.x.bx == 1) {
			new_button = leftbutton;
		} else if (mregs.x.bx == 4) {
			new_button = middlebutton;
		} else if (mregs.x.bx == 2) {
			new_button = rightbutton;
		} else {
			new_button = releasebutton;
		}
		hidemouse ();
#ifdef debug_mousegetch
		printf ("  return mouse\n");
#endif
		if (msec < 0) {
			return FUNcmd;
		} else {
			mhit = 1;
			return -3;	/* indicate mouse input available */
		}
	}
}


void
DIRECTdosmousegetxy ()
{
  /* remember previous mouse click */
  if (mouse_button == leftbutton || mouse_button == rightbutton || mouse_button == middlebutton) {
	mouse_prevbutton = mouse_button;
	mouse_prevxpos = mouse_xpos;
	mouse_prevypos = mouse_ypos;
  }

  mouse_button = new_button;

  /*
	mouse_xpos: column (first is 0)
	mouse_ypos: line after menu (first is 0, menu line is -1)
  */
  mouse_ypos = (new_y >> 3) - MENU;
  mouse_xpos = new_x >> 3;
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
