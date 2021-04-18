/*======================================================================*\
|	Define macro to conditionally provide prototype arguments
|	if the compiler can take them
\*======================================================================*/

#ifndef _PROTO_H
#define _PROTO_H

#ifdef __STDC__
#define DEFPROTO
#endif

#ifdef __TURBOC__
#define DEFPROTO
#endif

#ifdef NOPROTO
#undef DEFPROTO
#endif

#undef _

#ifdef DEFPROTO
#define _(x) x
#else
#define _(x) ()
#endif

#endif


/*======================================================================*\
|	End
\*======================================================================*/
