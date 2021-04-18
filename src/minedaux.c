/*======================================================================*\
|*		Editor mined						*|
|*		auxiliary functions					*|
\*======================================================================*/

#include "mined.h"
#include "version.h"
extern char * buildstamp _((void));

#include <errno.h>	/* ENOENT */
#include <signal.h>
#ifndef VAXC
#define __USE_XOPEN_EXTENDED
/* declare fchmod ... */
# ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE
# endif
# ifndef _XOPEN_SOURCE_EXTENDED
# define _XOPEN_SOURCE_EXTENDED 1
# endif
#include <sys/stat.h>
#endif

#include "charprop.h"
#include "io.h"
#include "textfile.h"	/* panicwrite (), unlock_file () */

#if defined (__MINGW32__) && ! defined (pc)
#define pc
#endif

#if defined (unix) && ! defined (__MINGW32__)
#include <pwd.h>	/* for getpwuid () */
#endif

#if ! defined (__TURBOC__) && ! defined (VAXC)
#define use_debuglog
#endif

#ifdef use_debuglog
#include <time.h>	/* for localtime (), strftime () */
#include <sys/time.h>	/* for gettimeofday () */
#endif

#ifdef __CYGWIN__
#include <sys/cygwin.h>	/* cygwin_create_path, CCP_POSIX_TO_WIN_A */
#endif


/*======================================================================*\
|*			Debug stuff					*|
\*======================================================================*/

static FILE * log = 0;

/**
   Write debug log entry.
 */
void
debuglog (tag, l1, l2)
  char * tag;
  char * l1;
  char * l2;
{
#ifdef use_debuglog
  if (debug_mined) {
    if (! log) {
	char logfn [maxFILENAMElen];
	strcpy (logfn, gethomedir ());
#ifdef vms
	strappend (logfn, ".minedlog", maxFILENAMElen);
#else
	strip_trailingslash (logfn);
	strappend (logfn, "/.minedlog", maxFILENAMElen);
#endif
	log = fopen (logfn, "a");
    }
    if (tag) {
	char timbuf [99];
	struct timeval now;
	gettimeofday (& now, 0);
	strftime (timbuf, sizeof (timbuf), "%m-%d %H:%M:%S", localtime (& now.tv_sec));
	fprintf (log, "[%d@%s.%03d] %s: <%s> <%s>\n",
				(int) getpid (), 
				timbuf, (int) now.tv_usec / 1000,
				tag,
				unnull (l1), unnull (l2)
		);
    } else if (l1) {
	fflush (log);
    } else {
	fclose (log);
    }
  }
#endif
}


/*======================================================================*\
|*			Auxiliary routines				*|
\*======================================================================*/

/*
 * Convert a void string into an empty string.
 */
char *
unnull (s)
  char * s;
{
  if (s == NIL_PTR) {
	return "";
  } else {
	return s;
  }
}

/**
   Strip trailining slash from directory name (to avoid double slash).
 */
void
strip_trailingslash (name)
  char * name;
{
  int lasti = strlen (name) - 1;
  if (lasti >= 0 && name [lasti] == '/') {
	name [lasti] = '\0';
  }
}

/*
 * Convert an (unsigned) long to a 10 digit number string (no leading zeros).
 */
char *
num_out (number, radix)
  long number;
  long radix;
{
  static char num_buf [11];		/* Buffer to build number */
  register long digit;			/* Next digit of number */
  register long pow;			/* Highest radix power of long */
  FLAG digit_seen = False;
  int i;
  int maxdigs;
  char * bufp = num_buf;

  if (radix == 16) {
	pow = 268435456L;
	maxdigs = 8;
  } else {
	pow = 1000000000L;
	maxdigs = 10;
	if (number < 0) {
		* bufp ++ = '-';
		number = - number;
	}
  }

  for (i = 0; i < maxdigs; i ++) {
	digit = number / pow;		/* Get next digit */
	if (digit == 0L && digit_seen == False && i != 9) {
	} else {
		if (digit > 9) {
			* bufp ++ = 'A' + (char) (digit - 10);
		} else {
			* bufp ++ = '0' + (char) digit;
		}
		number -= digit * pow;	/* Erase digit */
		digit_seen = True;
	}
	pow /= radix;			/* Get next digit */
  }
  * bufp = '\0';
  return num_buf;
}

char *
dec_out (number)
  long number;
{
  return num_out (number, 10);
}

/*
 * scan_int () converts a string into a natural number
 * returns the character pointer behind the last digit
 */
char *
scan_int (str, nump)
  char * str;
  int * nump;
{
  register char * chpoi = str;
  int negative = False;

  while (* chpoi == ' ') {
	chpoi ++;
  }
  if (* chpoi == '-') {
	negative = True;
	chpoi ++;
  }
  if (* chpoi >= '0' && * chpoi <= '9') {
	* nump = 0;
  } else {
	return str;
  }
  while (* chpoi >= '0' && * chpoi <= '9' && quit == False) {
	* nump *= 10;
	* nump += * chpoi - '0';
	chpoi ++;
  }
  if (negative) {
	* nump = - * nump;
  }
  return chpoi;
}

/*
 * copy_string () copies the string 'from' into the string 'to' which 
   must be long enough
 */
void
copy_string (to, from)
  register char * to;
  register char * from;
{
  while ((* to ++ = * from ++) != '\0')
	{}
}

/*
 * length_of () returns the number of bytes in the string 'string'
 * excluding the '\0'.
 */
int
length_of (string)
  register char * string;
{
  register int count = 0;

  if (string != NIL_PTR) {
	while (* string ++ != '\0') {
		count ++;
	}
  }
  return count;
}

char *
dupstr (s)
  char * s;
{
  char * s1 = alloc (strlen (s) + 1);
  if (s1) {
	copy_string (s1, s);
  }
  return s1;
}

char *
nextutf8 (s)
  char * s;
{
  int follow = UTF8_len (* s) - 1;
  s ++;
  while (follow > 0 && (* s & 0xC0) == 0x80) {
	s ++;
	follow --;
  }
  return s;
}

int
stringcompare_ (s1, s2)
  char * s1;
  char * s2;
{
  if (* s1) {
	if (* s2) {
		unsigned long c1 = case_convert (utf8value (s1), -1);
		unsigned long c2 = case_convert (utf8value (s2), -1);
		if (c1 < c2) {
			return -1;
		} else if (c1 > c2) {
			return 1;
		} else {
			return stringcompare_ (nextutf8 (s1), nextutf8 (s2));
		}
	} else {
		return 1;
	}
  } else {
	if (* s2) {
		return -1;
	} else {
		return 0;
	}
  }
}

int
stringprefix_ (s1, s2)
  char * s1;
  char * s2;
{
  if (* s1) {
	if (* s2) {
		unsigned long c1 = case_convert (utf8value (s1), -1);
		unsigned long c2 = case_convert (utf8value (s2), -1);
		if (c1 == c2) {
			return stringprefix_ (nextutf8 (s1), nextutf8 (s2));
		} else {
			return 0;
		}
	} else {
		return 0;
	}
  } else {
	return 1;
  }
}


/*======================================================================*\
|*			Environment stuff				*|
\*======================================================================*/

/**
   Return the value of an environment variable, or NULL.
 */
char *
envvar (name)
  char * name;
{
  return getenv (name);
}

/**
   Return the string value of an environment variable.
   Do not return NULL.
 */
char *
envstr (name)
  char * name;
{
  char * val = envvar (name);
  if (val) {
	return val;
  } else {
	return "";
  }
}


/**
   Get a user name
 */
char *
getusername ()
{
  char * username;

#if defined (unix) && ! defined (__MINGW32__)
  struct passwd * pwd = getpwuid (geteuid ());
  if (pwd) {
	/* avoid $USER in case 'su' is effective, or USER has been changed */
	username = pwd->pw_name;
  } else {
	username = envvar ("USER");
	if (! username) {
		username = dec_out (geteuid ());
	}
  }
#endif
#ifdef __MINGW32__
  username = envvar ("USER");
  if (! username) {
	username = "000";
  }
#endif
#ifdef vms
  username = envvar ("USER");
  if (! username) {
	username = dec_out (geteuid ());
  }
#endif
#ifdef msdos
  if (is_Windows ()) {
	username = envvar ("USERNAME");
	if (! username) {
		username = "000";
	}
  } else {
	username = "000";
  }
#endif

  return username;
}


/**
   Return current working directory
 */
char *
get_cwd (dirbuf)
  char * dirbuf;
{
  char * res;
#ifdef unix
# ifdef sysV
  res = getcwd (dirbuf, maxFILENAMElen);
# else
  char bsdbuf [4096];
  res = getwd (bsdbuf);
  if (res) {
	dirbuf [0] = '\0';
	strncat (dirbuf, bsdbuf, maxFILENAMElen - 1);
  }
# endif
#else
  res = getcwd (dirbuf, maxFILENAMElen);
#endif

  if (res) {
	return res;
  } else {
	return "";
  }
}

static char * _homedir = NIL_PTR;

void
sethomedir (hd)
  char * hd;
{
  if (hd && * hd) {
	_homedir = hd;
#ifdef __CYGWIN__
	if (streq (hd, "%HOME%")) {
		char * regkey = "/proc/registry/HKEY_CURRENT_USER/Software/Microsoft/Windows/CurrentVersion/Explorer/Shell Folders/Personal";
		int regfd = open (regkey, O_RDONLY, 0);
		_homedir = envvar ("USERPROFILE");
		if (regfd >= 0) {
			static char buf [maxFILENAMElen];
			int rd = read (regfd, buf, maxFILENAMElen - 1);
			if (rd > 0) {
				buf [rd] = '\0';
				_homedir = buf;
			}
			(void) close (regfd);
		}
	}
#endif
  }
}

char *
gethomedir ()
{
  if (! _homedir) {
    _homedir = envvar ("HOME");
    if (! _homedir) {
#ifdef pc
#ifdef msdos
	if (is_Windows ()) {
		_homedir = envvar ("USERPROFILE");
	}
#else
#ifdef __CYGWIN__
	sethomedir ("%HOME%");
#else
	_homedir = envvar ("USERPROFILE");
#endif
#endif
	if (! _homedir) {
		_homedir = "/";
	}
#else	/* #ifdef pc */
#ifdef vms
	_homedir = "SYS$LOGIN";
#else
	struct passwd * pwd = getpwuid (geteuid ());
	if (pwd) {
		_homedir = pwd->pw_dir;
	} else {
		_homedir = "/";
	}
#endif
#endif
    }
  }
  return _homedir;
}


/*======================================================================*\
|*			Process stuff					*|
\*======================================================================*/

/**
   System call with screen mode adjustment.
   Optional message before call.
   Optional delay after call.
 */
int
systemcall (msg, cmd, delay)
  char * msg;
  char * cmd;
  int delay;
{
  int res;

  raw_mode (False);
  if (msg != NIL_PTR) {
	unidisp_on ();
	putstring ("mined: ");
	unidisp_off ();
	reverse_on ();
	putstring (msg);
	reverse_off ();
	putstring ("\r\n");
	flush ();
  }

  res = system (cmd);
#ifdef vms
  if (res == 1) {
	res = 0;
  }
#endif

  if (delay) {
	sleep (delay);
  }
  raw_mode (True);
  return res;
}

#if ! defined (msdos) && ! defined (__MINGW32__) && ! defined (vms)

/**
   Program call with screen mode adjustment.
   Optional message before call.
   Optional delay after call.
 */
int
progcallpp (msg, delay, binprepath, dir, prog, p1, p2, p3, p4)
  char * msg;
  int delay;
  char * binprepath [];
  char * dir;
  char * prog;
  char * p1;
  char * p2;
  char * p3;
  char * p4;
{
  register int res;	/* vfork parent: wait() result */
  int pid;
  int status;

  if (delay >= 0) {
	raw_mode (False);
  }
  if (msg != NIL_PTR) {
	unidisp_on ();
	putstring ("mined: ");
	unidisp_off ();
	reverse_on ();
	putstring (msg);
	reverse_off ();
	putstring ("\r\n");
	flush ();
  }

#ifdef FORK
  pid = fork ();
#else
  pid = vfork ();
#endif

  switch (pid) {
	case 0:		/* child process */
	    {
		int pathi = 0;
		if (dir) {
			if (chdir (dir)) {
				return -2;
			}
		}
		while (binprepath && binprepath [pathi]) {
			char pathbuf [maxFILENAMElen];
			int cmpi = 0;
			while (cmpi < pathi) {
				if (streq (binprepath [cmpi], binprepath [pathi])) {
					break;
				}
				cmpi ++;
			}
			if (cmpi == pathi) {
				build_string (pathbuf, "%s/bin/%s", binprepath [pathi], prog);
				execl (pathbuf, prog, p1, p2, p3, p4, NIL_PTR);
			}
			pathi ++;
		}
		execlp (prog, prog, p1, p2, p3, p4, NIL_PTR);
		_exit (127);	/* Exit with 127 */
	    }
	    /* NOTREACHED */
	case -1:	/* fork error */
		res = -1;
	default:	/* parent after successful fork */
		do {
			res = wait (& status);
		} while (res != pid && (res != -1 || geterrno () == EINTR));
		/* ignore geterrno (); */

		if (res == -1) {
			res = -2;
		} else if ((status & 0xFF) != 0) {
			res = status & 0xFF;
		} else {
			res = status >> 8;
		}
  }

  if (delay > 0) {
	sleep (delay);
  }
  if (delay >= 0) {
	raw_mode (True);
  }
  return res;
}

#endif


/*======================================================================*\
|*			System stuff					*|
\*======================================================================*/

#if defined (msdos) || defined (__MINGW32__)

int
is_Windows ()
{
#if defined (__TURBOC__) || defined (__EMX__)
  return 0;
#else
#ifdef unreliable_windir_test
  /* for obscure reasons, windir is not seen in djgpp programs */
  return envvar ("windir") || /* mangled by cygwin */ envvar ("WINDIR");
#else
  /* try some heuristics */
  return strisprefix ("Windows", envstr ("OS")) ||
	 (envvar ("USERPROFILE") && envvar ("APPDATA"));
#endif
#endif
}

#endif


/*======================================================================*\
|*			System-related command functions		*|
\*======================================================================*/

/* system-specific command to print a UTF-8 file used as a last resort 
   for printing */

#ifdef unix
# if defined (BSD) || defined (__INTERIX)
# define print_command "LC_ALL=en_US.UTF-8 lp '%s'"
# else
# define print_command "LC_ALL=en_US.UTF-8 lpr '%s'"
# endif
#endif

#ifdef vms
#define print_command "PRINT /DELETE \"%s\""
#endif

#ifdef msdos
#define print_command "copy \"%s\" prn > nul:"
#endif


/*
 * Called if an operation is not implemented on this system
 */
static
void
notavailable ()
{
  error ("Command not available");
}


/*
 * Change current directory.
 */
void
CHDI ()
{
  char new_dir [maxFILENAMElen];	/* Buffer to hold new dir. name */

  if (restricted) {
	restrictederr ();
	return;
  }

/*
  build_string (text_buffer, "Old directory: %s, change to:", get_cwd (new_dir));
*/
  build_string (text_buffer, "Change directory:");

  if (get_filename (text_buffer, new_dir, True) != FINE) {
	build_string (text_buffer, "Current directory is: %s", get_cwd (new_dir));
	status_msg (text_buffer);
	return;
  }

  /* Remove previous file lock (if any) */
  unlock_file ();

  if (chdir (new_dir) == 0) {
#ifdef msdos
	if (new_dir [0] != '\0' && new_dir [1] == ':') {
		setdisk (((int) new_dir [0] & (int) '\137') - (int) 'A');
	}
	RD ();	/* disk error message may be on screen after chdir */
#endif

	/* status line */
	/*clear_status ();*/
	build_string (text_buffer, "New current directory: %s", get_cwd (new_dir));
	status_msg (text_buffer);

	/* file buffer related changes */
	overwriteOK = False;	/* Same file base name ... */
	writable = True;

	set_modified ();	/* referring to different file now */
	relock_file ();

	/* file info handling */
	groom_info_file = groom_info_files;	/* groom again */
	if (groom_stat) {
		GROOM_INFO ();
	}
  } else {	/* chdir failed */
#ifdef msdos
	RD ();	/* disk error dialog may be on screen */
#endif
	relock_file ();
	error2 ("Cannot change current directory: ", serror ());
  }
}

/*
 * Print file status.
 */
void
FSTATUS ()
{
  fstatus ("", -1L, -1L);
}

void
FS ()
{
  if (hop_flag > 0) {
	if (always_disp_fstat) {
		always_disp_fstat = False;
	} else {
		always_disp_fstat = True;
	}
  } else {
	FSTATUS ();
  }
}


#ifndef msdos
#define lesshelp
#endif

#define edithelp

static
void
show_help (topic)
  char * topic;
{
  char * helpfile;
  char hfbuf [maxFILENAMElen];
  int hf = -1;

  if (hf == -1) {
	if (envvar ("MINEDDIR")) {
		strcpy (hfbuf, envvar ("MINEDDIR"));
		strcat (hfbuf, "/help/mined.hlp");
		helpfile = hfbuf;
		hf = open (helpfile, O_RDONLY);
	}
  }
  if (hf == -1) {	/* deprecated */
	helpfile = envstr ("MINEDHELPFILE");
	if (* helpfile != '\0') {
		hf = open (helpfile, O_RDONLY);
	}
  }
#ifdef vms
  if (hf == -1) {
	/* try logical name */
	helpfile = "MINED$HELP";
	hf = open (helpfile, O_RDONLY);
  }
#endif
  if (hf == -1) {
	/* look in program directory, useful for MSDOS and VMS */
	strcpy (hfbuf, minedprog);
	helpfile = getbasename (hfbuf);
	if (helpfile) {
		strcpy (helpfile, "mined.hlp");
		helpfile = hfbuf;
		hf = open (helpfile, O_RDONLY);
	}
  }
#ifdef RUNDIR
  if (hf == -1) {
	strcpy (hfbuf, RUNDIR);
	strcat (hfbuf, "/help/mined.hlp");
	helpfile = hfbuf;
	hf = open (helpfile, O_RDONLY);
  }
#endif
#ifdef LRUNDIR
  if (hf == -1) {
	strcpy (hfbuf, LRUNDIR);
	strcat (hfbuf, "/help/mined.hlp");
	helpfile = hfbuf;
	hf = open (helpfile, O_RDONLY);
  }
#endif
  if (hf == -1) {
	helpfile = "/usr/share/mined/help/mined.hlp";
	hf = open (helpfile, O_RDONLY);
  }
  if (hf == -1) {
	helpfile = "/usr/local/share/mined/help/mined.hlp";
	hf = open (helpfile, O_RDONLY);
  }
  if (hf == -1) {
	helpfile = "/usr/share/lib/mined/help/mined.hlp";
	hf = open (helpfile, O_RDONLY);
  }
  if (hf == -1) {
	helpfile = "/opt/mined/share/help/mined.hlp";
	hf = open (helpfile, O_RDONLY);
  }
  if (hf == -1) {
	helpfile = "/usr/share/doc/packages/mined/help/mined.hlp";
	hf = open (helpfile, O_RDONLY);
  }

  if (hf == -1) {
	status_msg ("Help file not found; configure $MINEDDIR in environment!");
	return;
  }

  (void) close (hf);

#if defined (edithelp) || defined (lesshelp)
  if (! (viewing_help /* || hop_flag > 0 */ )) {
#ifdef edithelp
#ifndef use_system_cmd
	char syscommand [maxCMDlen];	/* Buffer for full system command */
	int sysres;

# ifdef vms
	build_string (syscommand, 
		"%s \"+e%c\" \"-v\" --"
		" \"-Exit help mode with ESC ESC\""
		" \"+/mined help topic '%s'\" \"%s\"",
			"mined", emulation, topic, helpfile);
# else
	build_string (syscommand, 
		"%s +e%c -v -- '-Exit help mode with ESC ESC'"
		" +/\"mined help topic '%s'\" '%s'",
			minedprog, emulation, topic, helpfile);
# endif

	clear_status ();
	set_cursor (0, YMAX);
# ifdef unix
	clear_window_title ();
# endif

	sysres = systemcall (NIL_PTR, syscommand, 0);
#else
	char cmdtopic [maxCMDlen];
	char cmdenc [4];
	int sysres;

	build_string (cmdtopic, "+/mined help topic '%s'", topic);
	build_string (cmdenc, "+e%c", emulation);

	clear_status ();
	set_cursor (0, YMAX);
# ifdef unix
	clear_window_title ();
# endif

	sysres = progcallpp (NIL_PTR, 0, (char * *) 0,
		0,
		minedprog, 
		cmdenc, "-Exit help mode with ESC ESC", 
		cmdtopic, helpfile);
#endif
#else
	char syscommand [maxCMDlen];	/* Buffer for full system command */
	int sysres;

	build_string (syscommand, 
		"less '-QMPMMined help?e (end):?pb (%%pb\\%%).. - type '\\''h for help topic \"%s\", q to return to mined '"
		" +\"/mined help topic '%s'\nkmhG'h\" %s",
			topic, topic, helpfile);

	clear_status ();
	set_cursor (0, YMAX);
# ifdef unix
	clear_window_title ();
# endif

	sysres = systemcall (NIL_PTR, syscommand, 0);
#endif

	RDwin ();
	if (sysres != 0) {
		error ("Help topic could not be opened");
	}
	return;
  }
#else
#warning view_help in session not properly implemented
#endif

  /* last resort, or subsequent help topic */
  view_help (helpfile, topic);
}

/*
 * Show About mined information
 */
void
ABOUT ()
{
  static FLAG show_timestamp = False;
  char * __buildstamp__ = buildstamp ();
  char about [99];

  strcpy (about, "MinEd ");
  strcat (about, VERSION);
#ifdef __GNUC__
  strcat (about, " (gcc)");
#endif
#ifdef __CYGWIN__
  strcat (about, " (cygwin)");
#endif
#ifdef __DJGPP__
  strcat (about, " (djgpp)");
#endif
#ifdef __EMX__
  strcat (about, " (emx)");
#endif
#ifdef __TURBOC__
  strcat (about, " (turboc)");
#endif
  strcat (about, " (Unicode ");
  strcat (about, UNICODE_VERSION);
  if (show_timestamp && __buildstamp__) {
	strcat (about, ") - built ");
	strcat (about, buildstamp ());
  } else {
	strcat (about, ") - http://mined.sourceforge.net/");
  }
  show_timestamp = ! show_timestamp;

  status_uni (about);
}

static
void
dispatch_HELP (topics, Fn)
  FLAG topics;
  FLAG Fn;
{
  unsigned long c;

  if (topics && Fn) {
	status_uni ("F1/,/1/ Help on: about introduction keyboard function-keys commands menu");
  } else if (topics) {
	status_uni ("Help on: about introduction keyboard function-keys commands menu");
  } else if (Fn) {
	status_uni ("Show function key help bar: F1 / Shift-F1 / Alt-F1 / Ctrl-F1 / Alt-Ctrl-F1 etc");
  } else {
	status_uni ("Show accent help bar: . punctuation accents / 1/Alt-1/C-Alt-1 digit keys");
  }
  if (quit) return;

  c = readcharacter_unicode ();
  if (quit) return;

  clear_status ();
  if (command (c) == F1 || command (c) == F2 || command (c) == F3
   || command (c) == F4 || command (c) == F5 || command (c) == F6
   || command (c) == F7 || command (c) == F8 || command (c) == F9
   || command (c) == F10 || command (c) == F11 || command (c) == F12
     ) {
	FHELP (F1);
  } else if ((c >= '0' && c <= '9') || command (c) == key_0
   || command (c) == key_1 || command (c) == key_2 || command (c) == key_3
   || command (c) == key_4 || command (c) == key_5 || command (c) == key_6
   || command (c) == key_7 || command (c) == key_8 || command (c) == key_9
     ) {
	FHELP (key_1);
  } else if (command (c) == COMPOSE || c == ',' || c == '.' || c == '\''
   || c == ';' || c == ':' || c == '"' || c == '-' || c == '&' || c == '/'
   || c == '<' || c == '(' || c == ')' || c == '^' || c == '~' || c == '`'
   || c == 0x00B4 || c == 0x00B0
     ) {
	FHELP (COMPOSE);
  } else if (topics) switch (c) {
	case '\033': return;
	case 'a': ABOUT (); return;
	case 'i': show_help ("introduction"); return;
	case 'k': show_help ("keyboard"); return;
	case 'f': show_help ("function-keys"); return;
	case 'c': show_help ("commands"); return;
	case 'm': show_help ("menu"); return;
	default: {
		if (c == quit_char) {
			return;
		}
		status_msg ("No such help available");
		return;
	}
  }
}

/*
 * View Help topics / display function key help lines
 */
void
HELP ()
{
  dispatch_HELP (True, True);
}

void
HELP_topics ()
{
  dispatch_HELP (True, False);
}

void
HELP_Fn ()
{
  dispatch_HELP (False, True);
  always_disp_help = True;
}

void
HELP_accents ()
{
  dispatch_HELP (False, False);
  always_disp_help = True;
}


/**
   Copy text into temporary file for printing; convert to Unicode.
 */
static
int
write_unitext (fd)
  int fd;
{
  long chars_written = 0L;	/* chars written to buffer this time */
  int lines_written = 0;	/* lines written to buffer this time */
  LINE * line = header->next;
  char * textp = line->text;
  int count = 0;

  chars_written = char_count (textp) - 1;
  while (textp != tail->text) {
	if (* textp == '\n') {
		/* handle different line ends */
		if (line->return_type != lineend_NONE) {
			int ret = writechar (fd, '\n');
			if (ret == ERRORS) {
				(void) close (fd);
				return ERRORS;
			}
			lines_written ++;
			chars_written ++;
			count = 0;
		}

		/* move to the next line */
		line = line->next;
		textp = line->text;

		chars_written += char_count (textp) - 1;
	} else {
		unsigned long unichar = charvalue (textp);

		if (cjk_text || mapped_text) {
			unichar = lookup_encodedchar (unichar);
			if (no_unichar (unichar)) {
				unichar = 0x00A4;	/* ¤ */
			}
		}

		if (unichar == '\t') {
			int tab_count = tab (count);
			while (count < tab_count) {
				if (writechar (fd, ' ') == ERRORS) {
					(void) close (fd);
					return ERRORS;
				}
				count ++;
			}
		} else {
			character unibuf [13];
			char * up = (char *) unibuf;
			if (unichar >= 0x80000000) {
				/* special encoding of 2 Unicode chars, 
				   mapped from 1 CJK character */
				up += utfencode (unichar & 0xFFFF, up);
				if (is_wideunichar (unichar)) {
					count += 2;
				} else {
					count ++;
				}

				unichar = (unichar >> 16) & 0x7FFF;
			}

			(void) utfencode (unichar, up);
			/* ... iscombined (unichar, textp, line->text) ? */
			if (! iscombining_unichar (unichar)) {
				if (is_wideunichar (unichar)) {
					count += 2;
				} else {
					count ++;
				}
			}

			/* don't use write_line which might write UTF-16 ! */
			up = (char *) unibuf;
			while (* up != '\0') {
				if (writechar (fd, * up) == ERRORS) {
					(void) close (fd);
					return ERRORS;
				}
				up ++;
			}
		}

		advance_char (& textp);
	}
  }

/* Flush the I/O buffer and close file */
  if (flush_filebuf (fd) == ERRORS) {
	(void) close (fd);
	return ERRORS;
  }
  if (close (fd) < 0) {
	return ERRORS;
  }

  return FINE;
}


#define MAXSPOOLTRIALS 9

/**
   Make and open spool file for "name". Generate directory and file names.
   Cycle directory name index between invocations, wrap around at MAXSPOOLTRIALS.
   Try new file MAXSPOOLTRIALS times.
   Try overwriting existing file another MAXSPOOLTRIALS times.
 */
static
int
spoolfile (spoolfn, spooldir, name)
  char * spoolfn;
  char * spooldir;
  char * name;
{
  int try = 0;
  FLAG overwrite = False;
  static int print_n = 0;

  char * errmsg;

  while (try < MAXSPOOLTRIALS) {
	if (print_n >= MAXSPOOLTRIALS) {
		print_n = 0;
	}
	print_n ++;
#ifdef vms
	build_string (spoolfn, "%s$%d.lis", spool_file, print_n);
#else
	build_string (spooldir, "%s.%d", spool_file, print_n);
	build_string (spoolfn, "%s.%d/%s", spool_file, print_n, name);
	errmsg = "Cannot create spool dir ";
	if (mkdir (spooldir, 0700) == 0 || geterrno () == EEXIST)
#endif
	{
		int fd;
		errmsg = "Cannot create spool file ";
		if (overwrite) {
			fd = open (spoolfn, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, bufprot);
		} else {
			fd = open (spoolfn, O_CREAT | O_EXCL | O_WRONLY | O_BINARY, bufprot);
		}
		if (fd >= 0) {
			return fd;
		}
	}
	try ++;
	if (try == MAXSPOOLTRIALS && ! overwrite) {
		overwrite = True;
		try = 1;
	}
  }
  error2 (errmsg, spoolfn);
  return ERRORS;
}

#define printfaildelay 1

/*
 * Print buffer
 */
void
PRINT ()
{
  char cmd [maxCMDlen];	/* Buffer for print command */
  int sysres;
  char * msg;
  int fd;
  char print_name [maxFILENAMElen];	/* base name for print file */
  FLAG dosfilename = False;
  char print_dir [maxFILENAMElen];	/* spool dir for print file */
  char print_file [maxFILENAMElen];	/* temp. file for printing */

#ifdef msdos
  if (is_Windows ()) {
	/* should use long file name; 
	   make sure pathname doesn't get too long (see below) */
  } else {
	dosfilename = True;
  }
#endif
  if (dosfilename) {
	build_string (print_name, "%s", 
		file_name [0] == '\0' ? "_nofile_" : getbasename (file_name));
  } else {
	char * filename_underline = "________________________________________";
	build_string (print_name, "%s%s%s",
		filename_underline, 
		file_name [0] == '\0' ? "[no file]" : getbasename (file_name), 
		filename_underline);
  }
  fd = spoolfile (print_file, print_dir, print_name);
  if (fd == ERRORS) {
	return;
  }

  if (write_unitext (fd) == ERRORS) {
	error ("Cannot write spool file");
	return;
  }

  clear_status ();
  set_cursor (0, YMAX);
  flush ();

  /* try printing with $MINEDPRINT */
  if (envvar ("MINEDPRINT")) {
	build_string (cmd, envvar ("MINEDPRINT"), print_file);
	sysres = systemcall ("Printing with $MINEDPRINT ...", cmd, 1);

	if (sysres == 0) {
		msg = envvar ("MINEDPRINT");
	} else {
		status_uni ("Printing with $MINEDPRINT failed");
		sleep (printfaildelay);
	}
  } else {
	sysres = -99;
  }

#ifndef vms

#if defined (msdos) || defined (__MINGW32__)

  /* try printing with notepad */
  if (sysres != 0 && is_Windows ()) {
	char old_dir [maxFILENAMElen];	/* Buffer to hold dir. name */
	(void) get_cwd (old_dir);

	if (chdir (print_dir) != 0) {
		/* quite unlikely */
		error2 ("Cannot set to print spool directory: ", serror ());
		sleep (2);
	} else {
		build_string (cmd, "notepad /p '%s'", print_name);
		sysres = systemcall ("Printing with notepad ...", cmd, 1);
	}

	if (sysres == 0) {
		msg = "notepad";
	} else {
		status_uni ("Printing with notepad failed");
		sleep (printfaildelay);
	}

	if (chdir (old_dir) != 0) {
		/* quite unlikely */
		error2 ("Cannot reset to previous directory: ", serror ());
		sleep (1);
	}

  }

#else	/* #ifdef msdos */

  /* try printing with uprint unless cygwin DOS path used */
  if (sysres != 0
#ifdef pc
	&& file_name [1] != ':'
#endif
     ) {
	char * binpath [20];	/* Buffer for extra path elements */
	char * * binpathpoi = & binpath [0];
#ifdef unix
	if (envvar ("MINEDDIR")) {
		* binpathpoi ++ = envvar ("MINEDDIR");
	}
#ifdef RUNDIR
	* binpathpoi ++ = RUNDIR;
#endif
#ifdef LRUNDIR
	* binpathpoi ++ = LRUNDIR;
#endif
	* binpathpoi ++ = "/usr/share/mined/bin";
	* binpathpoi ++ = "/usr/local/share/mined/bin";
	* binpathpoi ++ = "/usr/share/lib/mined/bin";
	* binpathpoi ++ = "/opt/mined/share/bin";
	* binpathpoi ++ = "/usr/share/doc/packages/mined/bin";
#endif
	* binpathpoi = NIL_PTR;
	sysres = progcallpp ("Printing with uprint ...", 1, binpath,
		print_dir,
		"uprint", "-r", "-t", file_name, print_name);

	if (sysres == 0) {
		msg = "uprint";
	} else {
		status_uni ("Printing with uprint failed");
		sleep (printfaildelay);
	}
  }

#ifdef pc
  /* on PC, try printing with notepad */
  if (sysres != 0) {
#ifdef __CYGWIN__
	char * winf = cygwin_create_path (CCP_POSIX_TO_WIN_A, print_file);
	sysres = progcallpp ("Printing with notepad ...", 1, (char * *) 0,
		print_dir,
		"notepad", "/p", winf, NIL_PTR, NIL_PTR);
	free (winf);
#else
	sysres = progcallpp ("Printing with notepad ...", 1, (char * *) 0,
		print_dir,
		"notepad", "/p", print_name, NIL_PTR, NIL_PTR);
#endif

	if (sysres == 0) {
		msg = "notepad";
	} else {
		status_uni ("Printing with notepad failed");
		sleep (printfaildelay);
	}
  }
#endif

  /* try printing with $LPR */
  if (sysres != 0 && envvar ("LPR")) {
	build_string (cmd, "LC_ALL=en_US.UTF-8 %s '%s'", envvar ("LPR"), print_file);
	sysres = systemcall ("Printing with $LPR ...", cmd, 1);

	if (sysres == 0) {
		msg = envvar ("LPR");
	} else {
		status_uni ("Printing with $LPR failed");
		sleep (printfaildelay);
	}
  }

#endif	/* #else msdos */

#endif	/* #ifndef vms */

#ifndef msdos

  /* try print with system-specific print_command */
  if (sysres != 0) {
	build_string (cmd, print_command, print_file);
	sysres = systemcall ("Printing with system print command ...", cmd, 1);

	if (sysres == 0) {
		msg = "system print command";
	} else {
		status_uni ("Printing with system print command failed");
		sleep (printfaildelay);
	}
  }

#endif

  RDwin ();
  /*flush_keyboard ();*/

  if (sysres == 0) {
	status_line ("Printed with ", msg);
  } else {
	error ("Cannot print");
  }
}


/*
 * Pipe buffer
 */
void
CMD ()
{
  int fd;
  char cmd [maxPROMPTlen];	/* Buffer for command */
  char command [maxCMDlen];	/* Buffer for full command */

  if (restricted) {
	restrictederr ();
	return;
  }

  if ((fd = yankfile (READ, False)) == ERRORS) {
	error ("No buffer contents for command input");
	return;
  }
  (void) close (fd);
  if (get_string ("Command with buffer as input:", cmd, True, "") != FINE) {
	return;
  }
  build_string (command, "%s < %s", cmd, yank_file);
  clear_status ();
  set_cursor (0, YMAX);
#ifdef unix
  clear_window_title ();
#endif

  (void) systemcall (NIL_PTR, command, 1);

  RDwin ();

  check_file_modified ();
}


#if ! defined (vms) && ! defined (__TURBOC__) && ! defined (__MINGW32__)
#define do_suspend_check
#endif

#ifdef do_suspend_check

static FLAG allow_suspend_checked = False;

#ifdef debug_token_scanner
static
void
printtoken (s)
  char * s;
{
  while ((unsigned char) * s > ' ') {
	printf ("%c", * s);
	s ++;
  }
}

static
void
printline (s)
  char * s;
{
  while ((unsigned char) * s >= ' ') {
	printf ("%c", * s);
	s ++;
  }
  printf ("\n");
}
#endif

static
char *
gettoken (token)
  char * token;
{
  /* skip separator */
  while (white_space (* token)) {
	token ++;
  }
  return token;
}

static
char *
skiptoken (token)
  char * token;
{
  /* skip current token, if any */
  while ((unsigned char) * token > ' ') {
	token ++;
  }
  /* then get next token */
  return gettoken (token);
}

static
int
tokenlen (token)
  char * token;
{
  int l = 0;
  while ((unsigned char) * token > ' ') {
	token ++;
	l ++;
  }
  return l;
}

static
int
eqtoken (t1, t2)
  char * t1;
  char * t2;
{
  int l = tokenlen (t1);
  return l == tokenlen (t2) && strncmp (t1, t2, l) == 0;
}

static
int
istoken (token)
  char * token;
{
  return (unsigned char ) * token > ' ';
}

static
char *
nextline (s)
  char * s;
{
  /* skip rest of current line */
  while ((unsigned char) * s >= ' ') {
	s ++;
  }
  /* go to next line, if any */
  if (* s) {
	s ++;
  }
  return s;
}

#endif

/*
 * Suspend editor after writing back the file.
 */
void
SUSP ()
{
  if (restricted) {
	restrictederr ();
	return;
  }

#ifdef do_suspend_check
  if (cansuspendmyself && ! allow_suspend_checked) {
	/* disallow suspend in these cases:
	 - process ID and process group ID are different
	   (called in a shell script)
	 - terminal of process and parent process are different
	   (called directly (embedded) in a terminal, without shell)
	 */
	if (getpid () != portable_getpgrp (getpid ())) {
		cansuspendmyself = False;
	} else {
		char * ps_file = panic_file;
		char syscommand [maxCMDlen];
		build_string (syscommand, "ps -p %d > %s; ps -p %d >> %s", 
				(int) getpid (), ps_file, 
				(int) getppid (), ps_file);
#ifdef __CYGWIN__
		status_msg ("Checking whether it's safe to suspend to shell");
#endif
		if (system (syscommand) == 0) {
			int psfd = open (ps_file, O_RDONLY, 0);
			if (psfd >= 0) {
				char buf [maxCMDlen];
				int rd = read (psfd, buf, maxCMDlen - 1);
				if (rd > 0) {
					int colTT = 1;
					char * token = gettoken (buf);
					char pid [20];
					char ppid [20];
					char * pidTT = NIL_PTR;
					char * ppidTT = NIL_PTR;

					buf [rd] = '\0';

					/* find "TT" or "TTY" column */
					while (istoken (token) && ! strisprefix ("TT", token)) {
						token = skiptoken (token);
						colTT ++;
					}

					build_string (pid, "%d", (int) getpid ());
					build_string (ppid, "%d", (int) getppid ());
					while (* (token = nextline (token))) {
						char * curpid = gettoken (token);
						int i;
						token = curpid;
						for (i = 1; i < colTT; i ++) {
							token = skiptoken (token);
						}
						if (eqtoken (curpid, pid)) {
							pidTT = token;
						} else if (eqtoken (curpid, ppid)) {
							ppidTT = token;
						}
					}
					if (pidTT && ppidTT) {
						if (eqtoken (pidTT, ppidTT)) {
							/* same tty */
						} else {
							cansuspendmyself = False;
						}
					}
				}
				(void) close (psfd);
			}
		}
		delete_file (ps_file);
#ifdef __CYGWIN__
		clear_status ();
#endif
	}
	allow_suspend_checked = True;
  }
#endif

  if (cansuspendmyself) {
	if (hop_flag == 0 && modified) {
		if (write_text () == ERRORS) {
			return;
		}
	}
	set_cursor (0, YMAX);
#ifdef unix
	clear_window_title ();
#endif
	raw_mode (False);

	suspendmyself ();
	raw_mode (True);
	clear_status ();
	RDwin ();

	check_file_modified ();
  } else {
	notavailable ();
  }
}


/*
 * Call an interactive shell.
 */
static
void
spawnSHELL ()
{
#if defined (msdos) || defined (__MINGW32__)
#define SHimplemented

  char old_dir [maxFILENAMElen];	/* Buffer to hold dir. name */
  char * shell;

  (void) get_cwd (old_dir);

  set_cursor (0, YMAX);

  shell = envvar ("SHELL");
  if (shell == NIL_PTR) {
	shell = envvar ("COMSPEC");
  }
  (void) systemcall (NIL_PTR, shell, 0);

  clear_status ();
  RDwin ();

  if (chdir (old_dir) == 0) {
#ifdef msdos
	if (old_dir [0] != '\0' && old_dir [1] == ':')
		setdisk (((int) old_dir [0] & (int) '\137') - (int) 'A');
	RD ();	/* disk error dialog may be on screen after chdir */
#endif
  } else {	/* chdir failed */
	overwriteOK = False;	/* Same file base name ... */
	writable = True;
	set_modified ();	/* would mean different file now */
	RD ();	/* disk error dialog may be on screen */
	error2 ("Cannot reset to previous directory: ", serror ());
  }

#else /* #ifdef msdos */

#ifdef unix
#define SHimplemented

  register int w;	/* vfork parent: wait() result */
  int pid;
  int status;
  int waiterr;
  char * shell;

  shell = envvar ("SHELL");
  if (shell == NIL_PTR) {
#ifdef __CYGWIN__
	if (prefer_comspec) {
		shell = envvar ("COMSPEC");
	} else {
		shell = "/bin/sh";
	}
#else
	shell = "/bin/sh";
#endif
  }

/* do screen stuff before fork for the sake of cygwin;
   if it's done in the child process, tty modes will not be restored 
   later in the parent
 */
  set_cursor (0, YMAX);
  putchar ('\n');
  clear_window_title ();
  raw_mode (False);

#ifdef FORK
  pid = fork ();
#else
  pid = vfork ();
#endif

  switch (pid) {
	case -1:			/* Error */
		/* restore screen mode */
		raw_mode (True);
		RDwin ();

		error2 ("Cannot fork command shell: ", serror ());
		return;
	case 0:				/* This is the child */
		if (reading_pipe) {	/* Fix stdin */
			if (close (STD_IN) < 0) {
				/*_exit (126);*/
			}
			if (open ("/dev/tty", O_RDONLY, 0) < 0) {
				_exit (126);
			}
		}
		execl (shell, shell, NIL_PTR);
		_exit (127);
		/* NOTREACHED */
	default:			/* This is the parent */
		do {
			w = wait (& status);
			waiterr = geterrno ();
		} while (w != pid && (w != -1 || waiterr == EINTR));

		/* restore screen mode */
		raw_mode (True);
		RDwin ();
  }

/* check and report fork or child errors */
  if (w == -1) {
	error2 ("Shell termination error: ", serrorof (waiterr));
  } else if ((status >> 8) == 127) {		/* Child died with 127 */
	error2 (shell, ": error invoking shell (not found / not enough memory ?)");
  } else if ((status >> 8) == 126) {
	error ("Cannot open /dev/tty as fd #0");
  }

#else	/* #ifdef unix */

#ifdef vms
#define SHimplemented

  set_cursor (0, YMAX);

  (void) systemcall (NIL_PTR, "SPAWN", 0);

  clear_status ();
  RDwin ();

#endif
#endif
#endif


#ifndef SHimplemented
  notavailable ();
#endif
}

void
SH ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	CMD ();
	return;
  }

  if (restricted) {
	restrictederr ();
	return;
  }

  spawnSHELL ();

  check_file_modified ();
}


/*======================================================================*\
|*			File operations					*|
\*======================================================================*/

/**
   Return base file name of path name.
   ("a/" and "/" would return "")
 */
char *
getbasename (fn)
  char * fn;
{
  char * b = fn;
  while (* b) {
	b ++;
  }

  while (b != fn) {
	b --;
	if (* b == '/'
#ifdef pc
	 || * b == '\\'
#endif
#ifdef vms
	 || * b == ']'
#endif
#ifndef unix
	 || * b == ':'
#endif
	) {
		b ++;
		return b;
	}
  }
  return fn;
}

/**
   Check whether path name is absolute
 */
int
is_absolute_path (fn)
  char * fn;
{
#ifdef pc
  if (fn [0] && fn [1] == ':') {
	fn += 2;	/* skip drive */
  }
#endif

  return
#ifdef pc
	fn [0] == '\\' ||
#endif
#ifdef vms
	(fn [0] == '[' && fn [1] != '.' && fn [1] != ']' && fn [1] != '-') ||
	strchr (fn, ':') ||
#endif
	fn [0] == '/';
}


/*
 * Delete file.
 */
int
delete_file (file)
  char * file;
{
#ifdef unix
  return unlink (file);
#endif
#ifdef msdos
  return unlink (file);
#endif
#ifdef vms
  return delete (file);
#endif
}

/**
   Copy file.
   Return NOT_VALID if source file does not exist.
   Return False if it fails otherwise.
 */
FLAG
copyfile (from_file, to_file)
  char * from_file;
  char * to_file;
{
  int from_fd;
  int to_fd;
  int cnt;	/* count check for read/write */
  int ret = True;
#ifdef VAXC
  PROT copyprot = bufprot;
#else
  struct stat fstat_buf;
  PROT copyprot;
#endif
#ifdef __CYGWIN__
  /* use large buffer to mitigate very slow clipboard operations */
# define use_large_buffer
# ifdef use_large_buffer
  char copybuf [5 * 63 * 512];	/* < 1 MB/s */
# else
  char copybuf [63 * 512];	/* < 100 KB/s */
# endif
#else
  /* use smaller buffer to accommodate legacy systems memory */
  char copybuf [8 * 512];
#endif

  if ((from_fd = open (from_file, O_RDONLY | O_BINARY, 0)) < 0) {
	if (geterrno () == ENOENT) {
		return NOT_VALID;
	} else {
		return False;
	}
  }
#ifndef VAXC
  if (fstat (from_fd, & fstat_buf) == 0) {
	copyprot = fstat_buf.st_mode;
  } else {
	copyprot = bufprot;
  }
#endif
  /* ensure backup file is writable */
  copyprot |= 0200;

  (void) delete_file (to_file);
  if ((to_fd = open (to_file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, copyprot)) < 0) {
	(void) close (from_fd);
	return False;
  }
#ifndef msdos
#if defined (vms) || defined (__MINGW32__)
  (void) chmod (to_file, copyprot);
#else
  (void) fchmod (to_fd, copyprot);
#endif
#endif

  while ((cnt = read (from_fd, copybuf, sizeof (copybuf))) > 0) {
	if (write (to_fd, copybuf, (unsigned int) cnt) != cnt) {
		ret = False;
		break;
	}
  }
  if (cnt < 0) {
	ret = False;
  }

  (void) close (from_fd);
  if (close (to_fd) < 0) {
	ret = False;
  }
  return ret;
}


/*======================================================================*\
|*			Memory allocation				*|
\*======================================================================*/

#ifdef msdos
# ifdef __TURBOC__
#include <alloc.h>
#define allocate farmalloc
#define freemem farfree
# else
extern void * malloc ();
extern void free ();
#define allocate malloc
#define freemem free
# endif
#else
extern void * malloc ();
extern void free ();
#define allocate malloc
#define freemem free
#endif

#define dont_debug_out_of_memory

void *
alloc (bytes)
  int bytes;
{
#ifdef debug_out_of_memory
  static int allocount = -1;
  if (allocount < 0) {
	char * env = envvar ("DEBUG_ALLOC");
	if (env) {
		(void) scan_int (env, & allocount);
	}
	if (allocount < 0) {
		allocount = 5000;
	}
  }
  if (bytes > allocount) {
	return NIL_PTR;
  }
  allocount -= bytes;
#endif
  return allocate ((unsigned) bytes);
}

void
free_space (p)
  void * p;
{
  if (p) {
	freemem (p);
  }
}

/*
 * free header list
 */

#define pointersize	sizeof (void *)
#define blocksizeof(typ) ((sizeof (typ) + pointersize - 1) / pointersize * pointersize)

LINE * free_header_list = NIL_LINE;

static
void
alloc_headerblock (n)
  int n;
{
  LINE * new_header;
  LINE * new_list;
  int i = 0;

  new_list = alloc (n * blocksizeof (LINE));
  if (new_list == NIL_LINE) {
	free_header_list = NIL_LINE;
  } else {
	while (i < n) {
		new_header = (LINE *) ((long) new_list + i * blocksizeof (LINE));
		new_header->next = free_header_list;
		free_header_list = new_header;
		i ++;
	}
  }
}

LINE *
alloc_header ()
{
/*  return alloc (sizeof (LINE)); */
  LINE * new_header;

  if (free_header_list == NIL_LINE) {
	alloc_headerblock (64);
	if (free_header_list == NIL_LINE) {
		alloc_headerblock (16);
		if (free_header_list == NIL_LINE) {
			alloc_headerblock (4);
			if (free_header_list == NIL_LINE) {
				alloc_headerblock (1);
				if (free_header_list == NIL_LINE) {
					return NIL_LINE;
				}
			}
		}
	}
  }
  new_header = free_header_list;
  free_header_list = free_header_list->next;
  return new_header;
}

void
free_header (hp)
  LINE * hp;
{
/*  freemem (hp); */
  hp->next = free_header_list;
  free_header_list = hp;
}


/*======================================================================*\
|*			Interrupt condition handling			*|
\*======================================================================*/

#define dont_debug_intr

static int panic_level = 0;	/* To adjust error handling to situation */

/*
 * Panic () is called with a mined error msg and an optional system error msg.
 * It is called when something unrecoverable has happened.
 * It writes the message to the terminal, resets the tty and exits.
 * Ask the user if he wants to save his file.
 */
static
void
panic_msg (msg)
  char * msg;
{
  if (isscreenmode) {
	/* don't use status_msg here, msg may contain filename characters */
	status_line (msg, NIL_PTR);
	sleep (2);
  } else {
	(void) printf ("%s\n", msg);
  }
}

static
void
panicking (message, err, signum)
  register char * message;
  register char * err;
  register int signum;
{
  panic_level ++;
#ifdef debug_intr
  printf ("panicking (%s), panic_level ++: %d\n", message, panic_level);
#endif

  if (panic_level <= 2) {
	if (loading == False && modified) {
		if (panicwrite () == ERRORS) {
			sleep (2);
			build_string (text_buffer, "Error writing panic file %s", panic_file);
		} else {
			build_string (text_buffer, "Panic file %s written", panic_file);
		}
		ring_bell ();
		panic_msg (text_buffer);
	}
	if (signum != 0) {
		build_string (text_buffer, message, signum);
	} else if (err == NIL_PTR) {
		build_string (text_buffer, "%s", message);
	} else {
		build_string (text_buffer, "%s (%s)", message, err);
	}
	panic_msg (text_buffer);

	/* "normal" panic handling: */
	if (loading == False) {
		QUED ();	/* Try to save the file and quit */
		/* QUED returned: something wrong */
		if (tty_closed == False) {
			sleep (2);
			panic_msg ("Aborted writing file in panic mode - trying to continue");
			panic_level --;
#ifdef debug_intr
			printf ("panic_level --: %d\n", panic_level);
#endif
		}
		return;
	}
  }

  if (panic_level <= 3) {
	if (isscreenmode) {
		set_cursor (0, YMAX);
		putchar ('\n');
#ifdef unix
		clear_window_title ();
#endif
		raw_mode (False);
	}
	/* Remove file lock (if any) */
	unlock_file ();
	delete_yank_files ();
  }
#ifdef debug_intr
  printf ("exiting\n");
#endif
  exit (1);	/* abort () sends a signal which would again be caught */
}

void
panic (message, err)
  register char * message;
  register char * err;
{
  panicking (message, err, 0);
}

void
panicio (message, err)
  register char * message;
  register char * err;
{
/* Should panic_level already be increased here ? */
  panic (message, err);
}


void
handle_interrupt (signum)
  int signum;
{
#ifdef debug_intr
  printf ("handle_interrupt %d @ panic level %d\n", signum, panic_level);
#endif
  if (signum == SIGTERM && panic_level == 0) {
	panic_level ++;
#ifdef debug_intr
	printf ("handle_interrupt exiting, panic_level ++: %d\n", panic_level);
#endif
	EXMINED ();
	panic_level --;
#ifdef debug_intr
	printf ("handle_interrupt exiting, panic_level --: %d\n", panic_level);
#endif
#ifdef SIGHUP
  } else if (signum == SIGHUP && panic_level == 0) {
	panic_level ++;
#ifdef debug_intr
	printf ("handle_interrupt quitting, panic_level ++: %d\n", panic_level);
#endif
	QUED ();
	hup = True;
	panic_level --;
#ifdef debug_intr
	printf ("handle_interrupt quitting, panic_level --: %d\n", panic_level);
#endif
#endif
  } else {
	panicking ("External signal %d caught - terminating", NIL_PTR, signum);
  }
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
