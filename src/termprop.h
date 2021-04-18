/*======================================================================*\
|	Library to determine and enquire terminal properties
|	(developed as part of mined editor)
\*======================================================================*/

#include "_proto.h"


#ifndef _FLAG_H
typedef enum {False, True} FLAG;
#define _FLAG_H
#endif


/*======================================================================*\
|	Terminal properties to be determined
\*======================================================================*/

/* basic terminal encoding modes */
extern FLAG ascii_screen;	/* screen driven in ASCII mode ? */
extern FLAG utf8_screen;	/* screen driven in UTF-8 mode ? */
extern FLAG utf8_input;		/* keyboard input in UTF-8 mode ? */
extern FLAG combining_screen;	/* UTF-8 combining character terminal ? */
extern FLAG bidi_screen;	/* UTF-8 bidi terminal ? */
extern FLAG joining_screen;	/* UTF-8 terminal with Arabic LAM/ALEF joining ? */
extern FLAG halfjoining_screen;	/* Putty/mintty joining + spare space ? */

extern FLAG mapped_term;	/* terminal in mapped 8-bit mode ? */

extern FLAG cjk_term;		/* terminal in CJK mode ? */
extern FLAG cjk_uni_term;	/* terminal in CJK mode with Unicode widths ? */
extern FLAG cjk_wide_latin1;	/* wide CJK chars in Latin-1 range ? */
extern FLAG gb18030_term;	/* does CJK terminal support GB18030 ? */
extern FLAG euc3_term;		/* does CJK terminal support EUC 3 byte ? */
extern FLAG euc4_term;		/* does CJK terminal support EUC 4 byte ? */
extern FLAG cjklow_term;	/* does CJK terminal support 81-9F range ? */
extern int cjk_tab_width;	/* width of CJK TAB indicator */
extern int cjk_lineend_width;	/* width of CJK line end indicator */
extern int cjk_currency_width;	/* width of CJK cent/pound/yen characters */


/* basic data versions of Unicode (or in earlier cases, terminal-specific) */
/* in addition to width_data_version, cjk_width_data_version is set 
   only in CJK legacy width mode (xterm -cjk_width);
   combining_data_version is provided separate from these two to 
   accomodate actual terminal behaviour which may not always be 
   consistent with a specific Unicode version
 */
typedef enum {
	U0,
	U300beta, U300, U320beta, U320,
	U400, U410,
	U500, U510, U520,
	U600, U620, U630,
	U700,
	WIDTH_DATA_MAX
} width_data_type;


/* return name of Unicode version applied by terminal */
extern char * term_Unicode_version_name _((int v));

/* basic data versions of Unicode */
extern int width_data_version;
extern int combining_data_version;

/* special modes */
extern int cjk_width_data_version;	/* xterm CJK legacy width mode -cjk_width */
extern FLAG hangul_jamo_extended;	/* U+D7B0... combining Jamo ? */

/* specific behaviour of specific terminals */
extern FLAG unassigned_single_width;	/* rxvt */
extern FLAG spacing_combining;		/* mlterm */
extern FLAG wide_Yijing_hexagrams;	/* wcwidth glitch */
extern FLAG printable_bidi_controls;	/* since xterm 230 */

/* version indications of specific terminals */
/* a value of -1 means the specific terminal has not been detected */
extern int decterm_version;
extern int xterm_version;
extern int gnome_terminal_version;
extern int rxvt_version;
extern int konsole_version;
extern int mintty_version;
extern int mlterm_version;
extern int poderosa_version;
extern int screen_version;
extern int tmux_version;

/* terminal features with respect to non-BMP characters (>= 0x10000) */
extern int nonbmp_width_data;
extern FLAG suppress_non_BMP;	/* suppressing display of non-BMP range */
#define plane_2_double_width	((nonbmp_width_data & 0x4) != 0)
#define plane_1_combining	((nonbmp_width_data & 0x2) == 0)
#define plane_14_combining	((nonbmp_width_data & 0x1) == 0)


/*======================================================================*\
|	Enquire character properties on terminal
|	with respect to the configured (detected) 
|	Unicode version of the terminal
\*======================================================================*/

/* Check whether a Unicode character is a combining character, based on its 
   Unicode general category being Mn (Mark Nonspacing), Me (Mark Enclosing), 
   Cf (Other Format),
   or (depending on terminal features) Mc (Mark Spacing Combining).
 */
extern int term_iscombining _((unsigned long ucs));

/* Check whether a Unicode character is a wide character, based on its 
   Unicode category being East Asian Wide (W) or East Asian FullWidth (F) 
   as defined in Unicode Technical Report #11, East Asian Ambiguous (A) 
   if the terminal is running in CJK compatibility mode (xterm -cjk_width).
   Data taken from different versions of
   http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */
extern int term_iswide _((unsigned long ucs));

/* Check whether a Unicode code is a valid (assigned) Unicode character.
 */
extern int term_isassigned _((unsigned long ucs));


/*======================================================================*\
|	End
\*======================================================================*/
