/*======================================================================*\
|	Library to determine and enquire terminal properties
|	(developed as part of mined editor)
\*======================================================================*/

#include "termprop.h"


/*======================================================================*\
|	Terminal properties to be determined
\*======================================================================*/

/* basic terminal encoding modes */
FLAG ascii_screen = False;	/* screen driven in ASCII mode ? */
FLAG utf8_screen = False;	/* screen driven in UTF-8 mode ? */
FLAG utf8_input = False;	/* keyboard input in UTF-8 mode ? */
FLAG combining_screen = False;	/* combining character terminal ? */
FLAG bidi_screen = False;	/* UTF-8 bidi terminal ? */
FLAG joining_screen = False;	/* UTF-8 terminal with Arabic LAM/ALEF joining ? */
FLAG halfjoining_screen = False;	/* Putty/mintty joining + spare space ? */

FLAG mapped_term = False;	/* terminal in mapped 8-bit mode ? */

FLAG cjk_term = False;		/* terminal in CJK mode ? */
FLAG cjk_uni_term = False;	/* terminal in CJK mode with Unicode widths ? */
FLAG cjk_wide_latin1 = True;	/* wide CJK chars in Latin-1 range ? */
FLAG gb18030_term = True;	/* does CJK terminal support GB18030 ? */
FLAG euc3_term = True;		/* does CJK terminal support EUC 3 byte ? */
FLAG euc4_term = True;		/* does CJK terminal support EUC 4 byte ? */
FLAG cjklow_term = True;	/* does CJK terminal support 81-9F range ? */
int cjk_tab_width;		/* width of CJK TAB indicator */
int cjk_lineend_width;		/* width of CJK line end indicator */
int cjk_currency_width = 0;	/* width of CJK cent/pound/yen characters */

/* basic data versions of Unicode; defaults overwritten by auto-detection */
#ifdef older_defaults
int width_data_version = U320;		/* Unicode 3.2 */
int combining_data_version = U320;	/* Unicode 3.2 */
#else
int width_data_version = U410;		/* Unicode 4.1 and later */
int combining_data_version = U500;	/* Unicode 5.0 */
#endif

/* special modes */
int cjk_width_data_version = 0;	/* xterm CJK legacy width mode -cjk_width */
FLAG hangul_jamo_extended = False;	/* U+D7B0... combining Jamo ? */

/* specific behaviour of specific terminals */
FLAG unassigned_single_width = False;	/* unassigned chars displayed single width? */
FLAG spacing_combining = False;		/* mlterm */
FLAG wide_Yijing_hexagrams = True;	/* wcwidth glitch */
FLAG printable_bidi_controls = False;	/* since xterm 230 */

/* version indications of specific terminals */
int decterm_version = 0;
int xterm_version = 0;
int gnome_terminal_version = 0;
int rxvt_version = 0;
int konsole_version = 0;
int mintty_version = 0;
int mlterm_version = 0;
int poderosa_version = 0;
int screen_version = 0;
int tmux_version = 0;

/* terminal features with respect to non-BMP characters (>= 0x10000) */
int nonbmp_width_data = 0x4;


/*======================================================================*\
|	Unicode version information
\*======================================================================*/

static struct name_table {
	int mode;
	char * name;
} Unicode_version_names [] = {
	{U300beta, "Unicode < 3.0 (beta? - xterm)"},
	{U300, "Unicode 3.0"},
	{U320beta, "Unicode < 3.2 (beta? - xterm CJK)"},
	{U320, "Unicode 3.2"},
	{U400, "Unicode 4.0"},
	{U410, "Unicode 4.1"},
	{U500, "Unicode 5.0"},
	{U510, "Unicode 5.1"},
	{U520, "Unicode 5.2"},
	{U600, "Unicode 6.0"},
	{U620, "Unicode 6.1/6.2"},
	{U630, "Unicode 6.3"},
	{U700, "Unicode 7.0"},
	{0, 0}
};

/* return name of Unicode version applied by terminal */
char *
term_Unicode_version_name (v)
  int v;
{
  struct name_table * t = Unicode_version_names;
  while (t->name) {
	if (v == t->mode) {
		return t->name;
	}
	t ++;
  }
  return "?";
}


/*======================================================================*\
|	End
\*======================================================================*/
