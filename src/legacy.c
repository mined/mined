/*======================================================================*\
|*		Editor mined						*|
|*		legacy/compatibility functions				*|
\*======================================================================*/

#include "_proto.h"

#include <stdio.h>
#include <string.h>

/* don't #include <unistd.h>, see below */
#ifdef vms
extern int getpid _((void));
#else
extern int getpgrp _((int));
#endif


/*======================================================================*\
|*			System error retrieval				*|
\*======================================================================*/

#ifdef __GNUC__

#include <errno.h>

#else	/* #ifdef __GNUC__ */

#ifdef vms

#define includeerrno
#  ifdef includeerrno
#  include <errno.h>
#  include <perror.h>
#  else
    extern volatile int noshare errno;
    extern volatile int noshare vaxc$errno; /* VMS error code when errno = EVMSERR */
#  define EVMSERR 65535
    extern volatile int noshare sys_nerr;
    extern volatile char noshare * sys_errlist [];
#  endif

#else	/* #ifdef vms */

#ifdef __CYGWIN__

#include <errno.h>

/* What's the purpose of this silly API renaming game ? */
#define sys_nerr _sys_nerr
#define sys_errlist _sys_errlist
/*const char *const *sys_errlist = _sys_errlist;*/
/* from a mail => initializer element is not constant */

#else	/* #ifdef __CYGWIN__ */

#if defined (BSD) && BSD >= 199306

#include <errno.h>

#else

# ifdef __linux__
#    include <errno.h>
# endif

  extern int errno;
  extern int sys_nerr;

#  ifndef __USE_BSD
#   ifndef __USE_GNU
/* Why did those Linux kernel hackers have to change the interface here, 
   inserting a completely superfluous "const"?
   All this voguish "assumedly-modern C" crap only increases 
   portability problems and wastes developers' time. 
   It's nothing but a nuisance. */
  extern char * sys_errlist [];
#   endif
#  endif

#endif	/* #else BSD >= 199306 */

#endif	/* #else __CYGWIN__ */

#endif	/* #else vms */

#endif	/* #else __GNUC__ */


/*
 * serrorof delivers the error message of the given errno value.
 * serror delivers the error message of the current errno value.
 * geterrno just returns the current errno value.
 */

char *
serrorof (errnum)
  int errnum;
{
#ifdef vms
  static char s [222];

  if (errnum == EVMSERR) {
	/* could also use system service $GETMSG to get details */
	sprintf (s, "VMS error %X: %s", vaxc$errno, strerror (errnum));
	return s;
  }
#endif

#if defined (__GNUC__) || defined (__MVS__)
  return strerror (errnum);
#else
  if ((errnum < 0) || (errnum >= sys_nerr)) {
# ifndef vms
	static char s [222];
# endif
	sprintf (s, "Unknown error %d", errnum);
	return s;
  } else {
	/* (char *) cast avoids bogus warnings about "const" crap */
	return (char *) sys_errlist [errnum];
  }
#endif
}

char *
serror ()
{
  return serrorof (errno);
}

int
geterrno ()
{
  return errno;
}


/*======================================================================*\
|*			Process ID retrieval				*|
\*======================================================================*/

#if ! defined (__TURBOC__) && ! defined (__MINGW32__)
/**
   This function uses its form on legacy BSD systems
   (which is available on modern systems as getpgid).
   It's actually called with getpid () as a parameter only,
   so on modern systems, getpgrp () (without parameter) would 
   do the same. (That's actually why this trick works).
   However, to be back-portable to legacy systems, this calling form 
   is needed, and it needs to be compiled without #include <unistd.h>.
 */
int
portable_getpgrp (pid)
  int pid;
{
#ifdef vms
  return getpid ();	/* bold assumption */
  /* #include <stropts.h> ioctl (output_fd, TIOCGPGRP, & pgrp) bogus */
#else
  return getpgrp (pid);
#endif
}
#endif


/*======================================================================*\
|*				End					*|
\*======================================================================*/
