/*======================================================================*\
|*		Editor mined						*|
|*		Han character description table				*|
\*======================================================================*/

#include "mined.h"


/* disable Han info on VAX (VMS and Ultrix), MSDOS, AIX, z/OS */
#if defined (msdos) || defined (vax) || defined (_AIX) || defined (__MVS__)
#define noHANinfo
#endif


#define use_HANinfo

#if defined (noHANinfo) || defined (NOCJK)
#undef use_HANinfo
#endif


/*======================================================================*\
|*		Han description table					*|
\*======================================================================*/

struct raw_hanentry hantable [] = {
#ifdef use_HANinfo
#include "handescr.t"
#else
#if defined (__TURBOC__) || defined (vms) || defined (__MVS__)
	{-1}
#endif
#endif
};

unsigned int hantable_len = arrlen (hantable);


/*======================================================================*\
|*				End					*|
\*======================================================================*/
