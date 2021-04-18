/*======================================================================*\
|*		Editor mined						*|
|*		VMS tty setup and keyboard input			*|
\*======================================================================*/

#include <stsdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <iodef.h>
#include <ttdef.h>
#include <tt2def.h>

#include <lib$routines.h>
#include <starlet.h>

#ifndef FINE
#define dont_debug_qio
#endif

#include <string.h>
#ifdef debug_qio
#include <stdio.h>
#endif


/* Event flag */
#define EFN 0

/* I/O status block */
struct io_status_block {
	short io_status;
	short io_count;
	long io_info;
};

static int oldmode [3];	/* Old TTY mode bits */
static int newmode [3];	/* New TTY mode bits */
static short iochan;	/* TTY I/O channel */
static short iopen = 0;

int ttrows;
int ttcols;
int vms_status;
#define VMS$ERROR -99


/*
 * This function is called once to set up the terminal device streams.
 * On VMS, it translates TT until it finds the terminal, then assigns
 * a channel to it and sets it raw. On CPM it is a no-op.
 */
int ttopen ()
{
	struct dsc$descriptor idsc;
	struct dsc$descriptor odsc;
	char oname [40];
	int status;
	struct io_status_block iosb;

	if (iopen) {
		return 0;
	}

	odsc.dsc$a_pointer = "TT";
	odsc.dsc$w_length = strlen (odsc.dsc$a_pointer);
	odsc.dsc$b_dtype = DSC$K_DTYPE_T;
	odsc.dsc$b_class = DSC$K_CLASS_S;
	idsc.dsc$b_dtype = DSC$K_DTYPE_T;
	idsc.dsc$b_class = DSC$K_CLASS_S;
	do {
		idsc.dsc$a_pointer = odsc.dsc$a_pointer;
		idsc.dsc$w_length = odsc.dsc$w_length;
		odsc.dsc$a_pointer = &oname [0];
		odsc.dsc$w_length = sizeof (oname);
		status = lib$sys_trnlog (&idsc, &odsc.dsc$w_length, &odsc);
		if (status != SS$_NORMAL && status != SS$_NOTRAN) {
			vms_status = status;
			return VMS$ERROR;
		}
		if (oname [0] == 0x1B) {
			odsc.dsc$a_pointer += 4;
			odsc.dsc$w_length -= 4;
		}
	} while (status == SS$_NORMAL);

	status = sys$assign (&odsc, &iochan, 0, 0);
	if (status != SS$_NORMAL) {
		vms_status = status;
		return VMS$ERROR;
	}

	status = sys$qiow (EFN, iochan, IO$_SENSEMODE, & iosb, 0, 0,
			   oldmode, sizeof (oldmode), 0, 0, 0, 0);
	if (status != SS$_NORMAL || iosb.io_status != SS$_NORMAL) {
		vms_status = status;
		return VMS$ERROR;
	}

	newmode [0] = oldmode [0];
	newmode [1] = oldmode [1] | TT$M_NOECHO | TT$M_EIGHTBIT | TT$M_LOWER;
	newmode [1] &= ~ (TT$M_ESCAPE | TT$M_NOTYPEAHD);
	newmode [1] &= ~ (TT$M_TTSYNC | TT$M_HOSTSYNC);
	newmode [2] = oldmode [2] | TT2$M_PASTHRU;

 	status = sys$qiow (EFN, iochan, IO$_SETMODE, & iosb, 0, 0,
			   newmode, sizeof (newmode), 0, 0, 0, 0);
	if (status != SS$_NORMAL || iosb.io_status != SS$_NORMAL) {
		vms_status = status;
		return VMS$ERROR;
	}

	ttrows = newmode [1] >> 24;
	ttcols = newmode [0] >> 16;

	iopen = 1;
	return 0;
}

/*
 * This function gets called just before we go back home to the command
 * interpreter. On VMS it puts the terminal back in a reasonable state.
 * Another no-operation on CPM.
 */
int ttclose ()
{
	int status;
	struct io_status_block iosb;

	/*ttflush ();*/

	status = sys$qiow (EFN, iochan, IO$_SETMODE, & iosb, 0, 0,
			   oldmode, sizeof (oldmode), 0, 0, 0, 0);
	if (status != SS$_NORMAL || iosb.io_status != SS$_NORMAL) {
		vms_status = status;
		return VMS$ERROR;
	}

	status = sys$dassgn (iochan);
	if (status != SS$_NORMAL) {
		vms_status = status;
		return VMS$ERROR;
	}

	iopen = 0;
	return 0;
}


/**
   Write character buffer to terminal.
 */
static
int ttputs (s, len)
  char * s;
  int len;
{
	int status = sys$qio (EFN, iochan,
				IO$_WRITELBLK | IO$M_NOFORMAT,
				0, 0, 0,
				s, len, 0, 0, 0, 0);
	if (status != SS$_NORMAL) {
		return -1;
	} else {
		return len;
	}
}


/**
   Read 1 byte from terminal.
   Param: >= 0: use timeout, -1: read
   Return: byte read;
           if using timeout: if no byte available, return -1;
           on error: return VMS$ERROR.
 */
static
int ttgetc (timeout)
  int timeout;
{
	char ch;
	int status;
	struct io_status_block iosb;

	if (! iopen) {
		ttopen ();
	}

	status = sys$qiow (EFN, iochan,
			timeout >= 0
				? IO$_READLBLK | IO$M_NOECHO | IO$M_TIMED
				: IO$_READLBLK | IO$M_NOECHO,
			& iosb,
			0, 0,
			& ch, 1, 
			timeout > 0 ? timeout : 0, 
			0, 0, 0);
#ifdef debug_qio
	printf ("%d [%04X.%04X:%08X]\n", timeout, iosb.io_status, iosb.io_count, iosb.io_info);
	fflush (stdout);
#endif
	if (status != SS$_NORMAL) {
		vms_status = status;
		return VMS$ERROR;
	}

	status = iosb.io_status;

	if (status == SS$_NORMAL) {
		vms_status = 0;
		return ch;
	} else if (status == SS$_TIMEOUT) {
		vms_status = 0;
		return -1;
	} else if (status == SS$_DATAOVERUN) {
#ifdef debug_qio
		debuglog ("sys$qiow", "SS$_DATAOVERUN", "");
#endif
		vms_status = status;
		return 0;
	} else {
		vms_status = status;
		return VMS$ERROR;
	}
}

/* 1 byte lookahead buffer */
static int ch1buffer = -1;

/**
   read byte from terminal
 */
int tt1getchar ()
{
	if (ch1buffer >= 0) {
		char chbuf = ch1buffer;
		ch1buffer = -1;
		return chbuf;
	}

	return ttgetc (-1);
}

/**
   check for keyboard input after a timeout
   @param msec
   	<= 50: check if available; do not wait
   		(like kbhit function)
   	> 50: check with timeout; wait for max msec ms
   		(aka half-delay mode)
 */
int tt1haschar (msec)
  int msec;	/* timeout */
{
	if (ch1buffer >= 0) {
		return 1;	/* available */
	} else {
		int timeout;
		if (msec <= 50) {
			timeout = 0;
		} else if (msec >= 500) {
			timeout = 2;
		} else {
			timeout = 1;
		}
		ch1buffer = ttgetc (timeout);
		if (ch1buffer == -1) {
			return 0;	/* timeout */
		} else {
			return 1;	/* available */
		}
	}
}


#define chbuflen 80	/* real VMS cannot handle more anyway ... */
static char chbuffer [chbuflen + 1];
static int chbufi = 0;
static int chbufn = 0;

/**
   Read bytes from terminal.
   Param: >= 0: use timeout, -1: read
   Return: number of bytes read;
           if using timeout: if no byte available, return -1;
           on error: return VMS$ERROR.
 */
static
int ttread (timeout)
  int timeout;
{
	int status;
	struct io_status_block iosb;
	int n;

	if (! iopen) {
		ttopen ();
	}

	status = sys$qiow (EFN, iochan,
			IO$_READLBLK | IO$M_NOECHO | IO$M_TIMED,
			& iosb,
			0, 0,
			chbuffer, chbuflen, 0, 0, 0, 0);
#ifdef debug_qio
	printf ("%d [%04X.%04X:%08X] -> %d\n", timeout, iosb.io_status, iosb.io_count, iosb.io_info, iosb.io_count + (iosb.io_info >> 16));
	fflush (stdout);
#endif
	if (status != SS$_NORMAL) {
		vms_status = status;
		return VMS$ERROR;
	}

	status = iosb.io_status;
	n = iosb.io_count + (iosb.io_info >> 16);
	if (status == SS$_DATAOVERUN) {
#ifdef debug_qio
		debuglog ("sys$qiow", "SS$_DATAOVERUN", "");
#endif
		chbuffer [n ++] = 0;
		vms_status = status;
	} else {
		vms_status = 0;
	}

	if (n > 0) {
#ifdef debug_qio
		char log [9];
		sprintf (log, "%d", n);
		debuglog ("sys$qiow", "buffered", log);
#endif
		return n;
	}

	status = sys$qiow (EFN, iochan,
			timeout >= 0
				? IO$_READLBLK | IO$M_NOECHO | IO$M_TIMED
				: IO$_READLBLK | IO$M_NOECHO,
			& iosb,
			0, 0,
			chbuffer, 1, 
			timeout > 0 ? timeout : 0, 
			0, 0, 0);
#ifdef debug_qio
	printf ("%d [%04X.%04X:%08X]\n", timeout, iosb.io_status, iosb.io_count, iosb.io_info);
	fflush (stdout);
	debuglog ("sys$qiow", "single", iosb.io_status == SS$_NORMAL ? "OK" : "--");
#endif

	status = iosb.io_status;

	if (status == SS$_NORMAL) {
		vms_status = 0;
		return iosb.io_count + (iosb.io_info >> 16);
	} else if (status == SS$_TIMEOUT) {
		vms_status = 0;
		return -1;
	} else if (status == SS$_DATAOVERUN) {
#ifdef debug_qio
		debuglog ("sys$qiow", "SS$_DATAOVERUN", "");
#endif
		vms_status = status;
		return 0;
	} else {
		vms_status = status;
		return VMS$ERROR;
	}
}

/**
   read byte from terminal
 */
int ttngetchar ()
{
/*printf ("ttngetchar %d %d\n", chbufi, chbufn); fflush (stdout);*/
	while (chbufi >= chbufn) {
		chbufn = ttread (-1);
		chbufi = 0;
	}
	return (unsigned char) (chbuffer [chbufi ++]);
}

/**
   check for keyboard input after a timeout
   @param msec
   	<= 50: check if available; do not wait
   		(like kbhit function)
   	> 50: check with timeout; wait for max msec ms
   		(aka half-delay mode)
 */
int ttnhaschar (msec)
  int msec;	/* timeout */
{
/*printf ("ttnhaschar %d %d\n", chbufi, chbufn); fflush (stdout);*/
	if (chbufi < chbufn) {
		return 1;
	} else {
		int timeout;
		if (msec <= 50) {
			timeout = 0;
		} else if (msec >= 500) {
			timeout = 2;
		} else {
			timeout = 1;
		}
		chbufn = ttread (timeout);
		chbufi = 0;
		return chbufi < chbufn;
	}
}

#define ttnget
#ifdef ttnget
#define ttgetchar ttngetchar
#define tthaschar ttnhaschar
#else
#define ttgetchar tt1getchar
#define tthaschar tt1haschar
#endif


/*======================================================================*\
|*			test						*|
\*======================================================================*/

#ifndef FINE

#include <unistd.h>
#include <stdio.h>

outs (char * s)
{
	write (1, s, strlen (s));
}

main ()
{
	unsigned char c;
	ttopen ();

	do {
		char s [22];
		if (tthaschar (30)) {
			outs ("!!\n");
		} else if (tthaschar (100)) {
			outs ("!\n");
		}
		c = ttgetchar ();
		sprintf (s, "%02X\n", c);
		outs (s);
	} while (c != 'x');

	ttclose ();
}

#endif


/*======================================================================*\
|*				End					*|
\*======================================================================*/
