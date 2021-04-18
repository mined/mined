/*======================================================================*\
|*		Editor mined						*|
|*		display output functions				*|
\*======================================================================*/

#include "mined.h"
#include "charprop.h"
#include "io.h"
#include "termprop.h"

#include <errno.h>


/*======================================================================*\
|*			Scroll bar display				*|
\*======================================================================*/

static
void
disp_scrollbar_cellend ()
{
}

static
void
disp_scrollbar_end ()
{
  if (! standout_glitch) {
	disp_scrollbar_off ();
  }
}

/* mined displays last_y + 1 lines in a display area of YMAX lines */
/* the file has (approx.?) total_lines lines */
/* the file position was in line # line_number */

/* debug traces: */
#define dont_debug_scrollbar
#define dont_debug_scrollbar_calc
#define dont_debug_partial_scroll
#define dont_debug_dirty
/* visible debug scrollbar: */
#define dont_debug_lazy_scrollbar

static int prev_disp_start = 0;
static int prev_disp_end = 0;

int
check_scrollbar_pos ()
{
	int unit = utf8_screen && fine_scrollbar ? 8 : 1;
	int mouse_pos = mouse_ypos * unit;
	if (mouse_pos < prev_disp_start) {
		return -1;
	} else if (mouse_pos > prev_disp_end) {
		return 1;
	} else {
		return 0;
	}
}

static char shiftskipdouble = '�';	/* indicator for shifted double width char */

static FLAG scrollbar_dirty = True;	/* does scrollbar need refresh ? */
static int first_dirty = -1;
static int last_dirty = -1;

#ifdef debug_dirty
#define printf_dirty(s, n)	printf ("%s %d - (prev %d..%d) [%d dirty %d..%d]\n", s, n, prev_disp_start, prev_disp_end, scrollbar_dirty, first_dirty, last_dirty);
#else
#define printf_dirty(s, n)	
#endif


static
void
set_scrollbar_dirty (scry)
  int scry;
{
  scrollbar_dirty = True;
  if (scry < first_dirty || first_dirty < 0) {
	first_dirty = scry;
  }
  if (scry > last_dirty) {
	last_dirty = scry;
  }
#ifdef debug_partial_scroll
	printf ("scrollbar_dirty @ %d, -> dirty %d..%d\n", scry, first_dirty, last_dirty);
#endif
	printf_dirty ("set_dirty", scry);
}

void
scrollbar_scroll_up (from)
  int from;
{
	int unit = utf8_screen && fine_scrollbar ? 8 : 1;

	if (prev_disp_start >= (from + 1) * unit) {
		prev_disp_start -= unit;
	}
	if (prev_disp_end >= (from + 1) * unit) {
		prev_disp_end -= unit;
	}
	set_scrollbar_dirty (SCREENMAX - 1);
#ifdef debug_partial_scroll
	printf ("scrollbar_scroll_up from %d, -> prev %d..%d\n", from, prev_disp_start, prev_disp_end);
#endif
	printf_dirty ("scroll_up", from);
}

void
scrollbar_scroll_down (from)
  int from;
{
	int unit = utf8_screen && fine_scrollbar ? 8 : 1;

	if (prev_disp_start >= from * unit) {
		prev_disp_start += unit;
	}
	if (prev_disp_end >= from * unit) {
		prev_disp_end += unit;
	}
	set_scrollbar_dirty (from);
#ifdef debug_partial_scroll
	printf ("scrollbar_scroll_down from %d, -> prev %d..%d\n", from, prev_disp_start, prev_disp_end);
#endif
	printf_dirty ("scroll_down", from);
}

static
FLAG
fine_grained_scrollbar (force)
  FLAG force;
{
/* calculate scroll bar display using Unicode 
   character cell vertical eighth blocks U+2581..U+2587 ▁▂▃▄▅▆▇
   as follows:
 * screen has lines 0...last_y, screen_lines = last_y + 1,
   with fields = screen_lines * 8,
   e.g. 10 lines 0...9, 80 fields 0...79;
 * to be mapped to buffer line numbers 1...total_lines;
   scroll block size in fields is s / fields = screen_lines / total_lines,
   s = screen_lines * fields / total_lines, 
   minimum 1 (display) / 7 (Unicode) / 8 (algorithm),
   max_start = fields - s;
 * scroll block start position is in range 0...(80 - s),
   to be mapped to position of last screen line 
   last_line_number = (line_number - y + last_y):
   if last_line_number <= screen_lines ==> p = 0
   if last_line_number == total_lines ==> p = max_start
   so roughly:
   p / max_start
   	= (last_line_number - screen_lines) 
   	  / (total_lines - screen_lines);
   but to accomodate rounding problems and avoid zero division map range
   last_line_number = screen_lines + 1 ==> p = start1,
   last_line_number = total_lines - 1 ==> p = max_start - 1,
   where start1 corresponds to the delta caused by scrolling by 1 line;
   start1 / fields = 1 / (total_lines - screen_lines), min 1,
   but replace this with an adjustment (see below) for better computation;
 - distribute equally, so map
   (max_start - start1) fields to (total_lines - 1 - screen_lines) lines
   where both will taken to zero by an offset for scaling:
   fields start1 ... max_start-1 
   	==> consider result to be p - start1
   lines screen_lines + 1 ... total_lines - 1 
   	==> consider ref_last/total = last/total_line_number - (screen_lines + 1):
   p - start1 / max_start
   	= (last_line_number - screen_lines - 1) / (total_line_number - screen_lines - 1)
 * scroll block extends from p to p + s - 1
*/

  int unit = utf8_screen && fine_scrollbar ? 8 : 1;

  int screen_lines;
  long fields;
  int s;
  int max_start;
  long last_line_number;
  int disp_start;
  int disp_end;
  FLAG painted = False;
  int slices = 0;
  int oldslices = 0;

  screen_lines = last_y + 1;
  fields = screen_lines * unit;
  s = fields * screen_lines / total_lines;
  if (s < unit) {
	s = unit;
  }
  max_start = fields - s;
  last_line_number = line_number - y + last_y;
  if (last_line_number <= screen_lines) {
	disp_start = 0;
  } else if (last_line_number == total_lines) {
	disp_start = max_start;
  } else {
	/* compensate by + 1 for adjustment at the beginning */
	disp_start = max_start * 
			(last_line_number - screen_lines - 1 + 1) 
			/ (total_lines - screen_lines - 1);
	/* assure distance if not quite at beginning or end */
	if (disp_start == 0) {
		disp_start = 1;
	} else if (disp_start >= max_start) {
		disp_start = max_start - 1;
	}
  }
  disp_end = disp_start + s - 1;

#ifdef debug_scrollbar_calc
	printf ("last_line_number %d/%d, screen_lines %d (0-%d)\n", last_line_number, total_lines, screen_lines, last_y);
	printf (" size %d, maxstart %d, ~ * %d / %d, pos %d-%d/%d\n", 
		s, max_start, 
		last_line_number - screen_lines, total_lines - screen_lines, 
		disp_start, disp_end, fields);
#endif

  if (disp_scrollbar) {
	int i;
#ifdef debug_scrollbar
	if (update_scrollbar_lazy) {
		printf ("scrollbar (%d) %d - %d (prev. %d - %d)\n", force, disp_start, disp_end, prev_disp_start, prev_disp_end);
	}
#endif

/* last_y / SCREENMAX */
	for (i = 0; i < fields; i ++) {
		FLAG klopsinslot;
		FLAG oldklopsinslot;
		if (i >= disp_start && i <= disp_end) {
			slices ++;
			klopsinslot = True;
		} else {
			klopsinslot = False;
		}
		if (i >= prev_disp_start && i <= prev_disp_end) {
			oldslices ++;
			oldklopsinslot = True;
		} else {
			oldklopsinslot = False;
		}
		if (((i + 1) % unit) == 0) {
		    int scri = i / unit;
		    if (slices != oldslices 
			|| klopsinslot != oldklopsinslot
			|| force
			|| (scrollbar_dirty && scri >= first_dirty && scri <= last_dirty)
		       ) {
			painted = True;
			set_cursor (XMAX, scri);
			if (slices == 0) {
				disp_scrollbar_background ();
				putchar (' ');
				disp_scrollbar_cellend ();
			} else if (slices == unit) {
				disp_scrollbar_foreground ();
				putchar (' ');
				disp_scrollbar_cellend ();
#ifdef debug_scrollbar_calc
				if (klopsinslot == False) {
					printf ("@ %d (%d) ", scri, i / unit * unit);
				}
				printf ("%d", unit);
#endif
			} else {
				/* choose among the eighths blocks */
				/* U+2581..U+2587 ▁▂▃▄▅▆▇ */
				if (klopsinslot) {
					/* display top segment part */
					disp_scrollbar_background ();
					put_unichar (0x2580 + slices);
					disp_scrollbar_cellend ();
#ifdef debug_scrollbar_calc
					if (! klopsinslot) {
						printf ("@ %d (%d) ", scri, i + 1 - slices);
					}
					printf ("%d", slices);
#endif
				} else {
					/* display bottom segment */
					disp_scrollbar_foreground ();
					put_unichar (0x2588 - slices);
					disp_scrollbar_cellend ();
#ifdef debug_scrollbar_calc
					printf ("%d", slices);
#endif
				}
			}
#ifdef debug_lazy_scrollbar
		    } else {
			painted = True;
			set_cursor (XMAX, scri);
			if (slices > 0) {
				disp_scrollbar_foreground ();
			} else {
				disp_scrollbar_background ();
			}
			putchar ('X');
			disp_scrollbar_cellend ();
#endif
		    }
		    slices = 0;
		    oldslices = 0;
		}
	}
	disp_scrollbar_end ();
#ifdef debug_scrollbar_calc
	printf ("\n");
#endif
  }

  printf_dirty ("scrollbar", force);
  prev_disp_start = disp_start;
  prev_disp_end = disp_end;
  scrollbar_dirty = False;
  first_dirty = SCREENMAX;
  last_dirty = -1;
  printf_dirty ("> scrollbar", force);
  return painted;
}

#ifdef use_cell_scrollbar

static
FLAG
cell_grained_scrollbar (force)
  FLAG force;
{
/* last_y / YMAX */
  int disp_start = ((long) line_number - y) * last_y / total_lines;
  int disp_end = ((long) line_number - y + last_y + 1) * last_y / total_lines;
  FLAG painted = False;

  if (disp_scrollbar) {
	int i;
#ifdef debug_scrollbar
	if (update_scrollbar_lazy) {
		printf ("scrollbar (%d) %d - %d (prev. %d - %d)\n", force, disp_start, disp_end, prev_disp_start, prev_disp_end);
	}
#endif

/* last_y / SCREENMAX */
	for (i = 0; i <= last_y; i ++) {
	    FLAG isin_newklops = i >= disp_start && i <= disp_end;
	    FLAG isin_oldklops = i >= prev_disp_start && i <= prev_disp_end;
	    if (isin_newklops != isin_oldklops || force) {
		painted = True;
		set_cursor (XMAX, i);
		if (isin_newklops) {
#ifdef CJKterm_not_coloured
			if (cjk_term)
			{
				reverse_on ();
				putchar ('<');
				reverse_off ();
			}
			else
#endif
			{
				disp_scrollbar_foreground ();
				putchar (' ');
				disp_scrollbar_cellend ();
			}
		} else {
			disp_scrollbar_background ();
			putchar (' ');
			disp_scrollbar_cellend ();
		}
#ifdef debug_lazy_scrollbar
	    } else {
		painted = True;
		set_cursor (XMAX, i);
		if (isin_newklops) {
			disp_scrollbar_foreground ();
		} else {
			disp_scrollbar_background ();
		}
		putchar ('X');
		disp_scrollbar_cellend ();
#endif
	    }
	}
	disp_scrollbar_end ();
  }

  prev_disp_start = disp_start;
  prev_disp_end = disp_end;
  scrollbar_dirty = False;
  first_dirty = SCREENMAX;
  last_dirty = -1;
  return painted;
}

FLAG
display_scrollbar (update)
  FLAG update;
{
  if (utf8_screen && fine_scrollbar) {
	return fine_grained_scrollbar (update == False);
  } else {
	return cell_grained_scrollbar (update == False);
  }
}

#else

FLAG
display_scrollbar (update)
  FLAG update;
{
  return fine_grained_scrollbar (update == False);
}

#endif


/*======================================================================*\
|*			Screen functions				*|
\*======================================================================*/

void
clearscreen ()
{
  clear_screen ();
  top_line_dirty = True;
  set_scrollbar_dirty (0);
  set_scrollbar_dirty (SCREENMAX - 1);
}


/**
   Manage nested/combined invocations of
	combiningdisp_on/off
	ctrldisp_on/off
	unidisp_on/off
	specialdisp_on/off
	mark_on/off
	disp_script ();
		disp_colour ();
		disp_normal ();
	disp_syntax ();
		dispXML_value ();
		dispXML_attrib ();
		dispHTML_jsp ();
		dispHTML_comment ();
		dispHTML_code ();
	reverse_on/off (for selection highlighting)
 */

#define dont_debug_attr

/* screen attributes are listed in order of precedence (increasing) */
#define attr_html	0x0001
#define attr_jsp	0x0002
#define attr_comment	0x0004
#define attr_attrib	0x0008
#define attr_value	0x0010

#define attr_script	0x0020

#define attr_uni	0x0040
#define attr_special	0x0080
#define attr_mark	0x0100
#define attr_combining	0x0200
#define attr_control	0x0400

#define attr_keyword	0x0800

#define attr_selected	0x1000

#define attr_all	0x1FFF

static unsigned short attr = 0;
static int attr_colour = 0;
static FLAG colour_256;

static
void
do_set_attr (set)
  unsigned short set;
{
#ifdef debug_attr
  if (set == attr_script)
	printf ("do_set_attr %03X colour %d\n", set, attr_colour);
  else
	printf ("do_set_attr %03X\n", set);
#endif
  /* set attribute on screen unless preceded by an overriding one */
  switch (set) {
	case attr_combining:
		combiningdisp_on ();
		return;
	case attr_uni:
		unidisp_on ();
		return;
	case attr_mark:
		mark_on ();
		return;
	case attr_special:
		specialdisp_on ();
		return;
	case attr_script:
		disp_colour (attr_colour, colour_256);
		return;
	case attr_value:
		dispXML_value ();
		return;
	case attr_attrib:
		dispXML_attrib ();
		return;
	case attr_jsp:
		dispHTML_jsp ();
		return;
	case attr_comment:
		dispHTML_comment ();
		return;
	case attr_html:
		dispHTML_code ();
		return;
	case attr_keyword:
		if (strop_style == '_') {
			underline_on ();
		} else {
			bold_on ();
		}
		return;
	case attr_control:
		if (attr & attr_selected) {
			disp_normal ();
		} else {
			ctrldisp_on ();
		}
		return;
	case attr_selected:
		if (attr & attr_control) {
			disp_normal ();
		} else {
			reverse_on ();
		}
		return;
    }
}

static
void
refresh_attrs ()
{
  /* combine selected attributes and refresh them on screen */

  if (attr & attr_uni) {
	do_set_attr (attr_uni);
  } else if (attr & attr_mark) {
	do_set_attr (attr_mark);
  } else if (attr & attr_special) {
	do_set_attr (attr_special);
  } else if (attr & attr_combining) {
	do_set_attr (attr_combining);
  } else if (attr & attr_keyword) {
	do_set_attr (attr_keyword);
  } else if (attr & attr_control && ! (attr & attr_selected)) {
	do_set_attr (attr_control);
  } else {
	if (attr & attr_value) {
		do_set_attr (attr_value);
	} else if (attr & attr_attrib) {
		do_set_attr (attr_attrib);
	} else if (attr & attr_jsp) {
		do_set_attr (attr_jsp);
	} else if (attr & attr_comment) {
		do_set_attr (attr_comment);
	} else if (attr & attr_html) {
		do_set_attr (attr_html);
	}
	if (attr & attr_script) {
		do_set_attr (attr_script);
	}
  }

  if (attr & attr_selected && ! (attr & attr_control)) {
	do_set_attr (attr_selected);
  }
}

static
void
clear_attr (clear)
  unsigned short clear;
{
#ifdef debug_attr
  printf ("clear_attr %03X (%03X->%03X)\n", clear, attr, attr & ~ clear);
#endif
  attr &= ~ clear;

  /* clear all attributes on screen */
  disp_normal ();

  refresh_attrs ();
}

static
void
set_attr (set)
  unsigned short set;
{
#ifdef debug_attr
  printf ("set_attr %03X (%03X->%03X)\n", set, attr, attr | set);
#endif
  if (set > attr || set == attr_script || set == attr_selected) {
	do_set_attr (set);
	attr |= set;
  } else if ((attr | set) != attr) {
	attr |= set;
	refresh_attrs ();
  } else {
	attr |= set;
  }
}

static
void
clear_colour_attr ()
{
  clear_attr (attr_script);
}

static
void
set_colour_attr (text_colour)
  int text_colour;
{
  attr_colour = text_colour;
#ifdef debug_attr
  printf ("set_colour_attr attr_colour = %d\n", attr_colour);
#endif

  set_attr (attr_script);
}


/*======================================================================*\
|*			Character display substitution			*|
\*======================================================================*/

static FLAG glyphs_checked = False;
static FLAG glyphs_available = False;
static struct {
	unsigned long unichar;
	unsigned long subst;
	FLAG checked;
} glyphs_subst [] = {
	{0x20AC, 'E', False},	/* € */
	{0x2713, 'V', False},	/* ✓ */
	{0x2717, 'X', False},	/* ✗ */
	{0x21AF, 'Z', False},	/* ↯ */
	{0x2118, 'P', False},	/* ℘ */
	{0x02BB, '\'', False},	/* glottal stop ʻokina */
};

static
unsigned long
disp_subst (unichar, check_glyph)
  unsigned long unichar;
  FLAG check_glyph;
{
  if (check_glyph && ! glyphs_checked) {
	glyphs_available = isglyph_uni (0);
	glyphs_checked = True;
  }
  if (glyphs_available || ! check_glyph) {
	int i;
	for (i = 0; i < arrlen (glyphs_subst); i ++) {
		if (glyphs_subst [i].unichar == unichar) {
			if (glyphs_subst [i].checked == False) {
				if (isglyph_uni (unichar)) {
					/* do not substitute */
					glyphs_subst [i].subst = 0;
				}
				glyphs_subst [i].checked = True;
			}
			return glyphs_subst [i].subst;
		}
	}
  }
  return 0;
}


/*======================================================================*\
|*			Marked character output				*|
\*======================================================================*/

static
character
char7bit (c)
  character c;
{
  if (c >= 0xA0 && c < 0xC0) {
	/*     " ¡¢£¤¥¦§\¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿"	Latin-1 */
	return " !cL%Y|S\"Ca<--R_0+23'mP.,10>///?" [c - 0xA0];
  } else if (c == (character) '�') {
	return 'x';
  } else if (c == (character) '�') {
	return ':';
  } else if (c == (character) '�') {
	return ';';
  } else {
	return '%';
  }
}

static
void
putnarrowchar (unichar)
  unsigned long unichar;
{
  if (mapped_term || cjk_term) {
	unsigned long termchar = mappedtermchar (unichar);
	if (no_char (termchar)) {
		__putchar (char7bit (unichar));
	} else if (termchar >= 0x100) {
		__putchar (char7bit (unichar));
	} else {
		__putchar (termchar);
	}
  } else if (iswide (unichar)) {
	if (unichar == 0xB7) {		/* · */
		put_unichar ('.');
	} else if (unichar == 0xA4) {	/* ¤ */
		putnarrowchar (0xB7);	/* · */
	} else if (unichar == 0x0085) {	/* NEXT LINE */
		put_unichar ('<');
	} else if (unichar == 0x2028) {	/* LINE SEPARATOR */
		put_unichar (0xAB);	/* « */
	} else if (unichar == 0x2029) { /* PARAGRAPH SEPARATOR */
		put_unichar ('P');
	} else if (unichar == 0x00B6) { /* ¶ */
		put_unichar ('P');
	} else if (unichar == 0xBA) {	/* º */
		put_unichar ('0');
	} else {
		putnarrowchar (0xA4); /* ¤ */
	}
  } else {
	put_unichar (unichar);
  }
}

static
void
putnarrowutf (utfmarker)
  char * utfmarker;
{
  unsigned long unichar;
  int utflen;

  utf8_info (utfmarker, & utflen, & unichar);
  if (iswide (unichar)) {
	putnarrowchar (unichar);
  } else {
	put_unichar (unichar);
  }
}


/**
   Indicate highlighted special character property (e.g. illegal code).
 */
static
void
indicate_uni (c, allow_wide, attr)
  unsigned long c;
  FLAG allow_wide;
  unsigned short attr;
{
  if (char_on_status_line) {
	reverse_off ();
  }
  set_attr (attr);
  if (allow_wide) {
	put_unichar (c);
  } else {
	putnarrowchar (c);
  }
  clear_attr (attr);
  if (char_on_status_line) {
	reverse_on ();
  }
}

/**
   Indicate coulored special character.
 */
static
void
indicate_special (c)
  character c;
{
  if (char_on_status_line) {
	reverse_off ();
  }
  set_attr (attr_special);
  putnarrowchar ((unsigned long) c);
  clear_attr (attr_special);
  if (char_on_status_line) {
	reverse_on ();
  }
}

#define dont_debug_indicate
#ifdef debug_indicate
#define trace_indicate	printf ("%d\n", __LINE__); 
#define indicate_special(c)	printf ("%d\n", __LINE__); indicate_special (c)
#else
#define trace_indicate	
#endif

#define indicate_illegal_UTF 	trace_indicate	indicate_marked
#define indicate_narrow 	trace_indicate	indicate_marked
#define indicate_marked(c) 	trace_indicate	indicate_uni(c, False, attr_uni)


/*
 * put mark character, possibly switching alternate character set
 * putmarkmode and endmarkmode are only called by putmark and for 
 * display of TAB markers
 */
static FLAG mark_mode_active = False;
static FLAG mark_alt_cset = False;

static
void
putmarkmode (marker, utfmarker, dim_me)
  character marker;
  char * utfmarker;
  FLAG dim_me;
{
  if (dim_me && ! mark_mode_active) {
	set_attr (attr_mark);
	mark_mode_active = True;
  }

  if (utf8_screen) {
	if (utfmarker != NIL_PTR && * utfmarker != '\0') {
		putnarrowutf (utfmarker);
	} else {
		if (marker >= '`' && marker <= '') {
			if (! mark_alt_cset) {
				altcset_on ();
				mark_alt_cset = True;
			}
			putchar (marker);
		} else {
			putnarrowchar (marker);
		}
	}
  } else if (marker >= '`' && marker <= '') {
	if (! mark_alt_cset) {
		altcset_on ();
		mark_alt_cset = True;
	}
	putchar (marker);
  } else {
	if (mark_alt_cset) {
		altcset_off ();
		mark_alt_cset = False;
	}
	if (mapped_term || cjk_term) {
		unsigned long termchar = mappedtermchar (marker);
		if (no_char (termchar)) {
			__putchar (char7bit (marker));
		} else {
			__putchar (termchar);
		}
	} else {
		putchar (marker);
	}
  }
}

static
void
endmarkmode ()
{
  if (mark_mode_active) {
	clear_attr (attr_mark);
	mark_mode_active = False;
  }
  if (mark_alt_cset) {
	altcset_off ();
	mark_alt_cset = False;
  }
}

/*
   Output a line status marker.
 */
void
putmark (marker, utfmarker)
  char marker;
  char * utfmarker;
{
  putmarkmode (marker, utfmarker, True);
  endmarkmode ();
}

static
void
putUmark (marker, utfmarker)
  character marker;
  char * utfmarker;
{
  set_attr (attr_special);
  putmarkmode (marker, utfmarker, False);
  endmarkmode ();
  clear_attr (attr_special);
}

void
put_menu_marker ()
{
  set_attr (attr_uni);
  putmarkmode (MENU_marker, UTF_MENU_marker, False);
  endmarkmode ();
  clear_attr (attr_uni);
}

void
put_submenu_marker (with_attr)
  FLAG with_attr;
{
  if (with_attr) {
	set_attr (attr_uni);
  }
  if (utf8_screen) {
	put_unichar (utf8value (submenu_marker));
  } else {
	unsigned long sub = (unsigned char) '�';
	if (cjk_term ||
		   ((mapped_term /*|| cjk_term*/)
		    && no_char (mappedtermchar (sub)))) {
		put_unichar ('>');
	} else {
		put_unichar (sub);
	}
  }
  if (with_attr) {
	clear_attr (attr_uni);
  }
}

static
void
putshiftmark (marker, utfmarker)
  character marker;
  char * utfmarker;
{
  if (! mark_mode_active) {
	set_attr (attr_mark);
	mark_mode_active = True;
  }
  reverse_on ();
  putmarkmode (marker, utfmarker, False);
  reverse_off ();
  endmarkmode ();
}

FLAG
marker_defined (marker, utfmarker)
  character marker;
  char * utfmarker;
{
  return marker || (utf8_screen && utfmarker && * utfmarker);
}

#define token_output

static
void
putret (type)
  unsigned char type;
{
  if (type == lineend_LF) {
	putmark (RET_marker, UTF_RET_marker);
  } else if (type == lineend_CRLF) {
	/* MSDOS line separator */
#ifdef token_output
	set_colour_token (4);	/* blue */
#endif
	putmark (DOSRET_marker, UTF_DOSRET_marker);
  } else if (type == lineend_CR) {
	/* Mac line separator */
#ifdef token_output
	set_colour_token (3);	/* yellow */
#endif
	putmark (MACRET_marker, UTF_MACRET_marker);
  } else if (type == lineend_NONE) {
	putmark ('�', "¬");
  } else if (type == lineend_NUL) {
	putmark ('�', "º");
  } else if (type == lineend_NL1 || type == lineend_NL2) {
	/* ISO 8859 NEXT LINE U+85 */
#ifdef token_output
	set_colour_token (6);	/* 4 blue / 6 cyan / 2 green */
#endif
	if (type == lineend_NL2) {
		putmark ('�', "«");
	} else {
		putmark (RET_marker, UTF_RET_marker);
	}
  } else if (type == lineend_LS) {
	/* Unicode line separator U+2028 */
	putUmark (RET_marker, UTF_RET_marker);
  } else if (type == lineend_PS) {
	/* Unicode paragraph separator U+2029 */
	putUmark (PARA_marker, UTF_PARA_marker);
  } else {
	putmark ('�', "¤");
  }
}

static int put_cjktermchar _((unsigned long cjkchar));

static
void
putCJKtab ()
{
  unsigned long mark = mappedtermchar (CJK_TAB_marker);
  set_attr (attr_mark);
  if (no_char (mark)) {
	put_cjkchar ('.');
	put_cjkchar ('.');
  } else {
	put_cjktermchar (mark);
  }
  clear_attr (attr_mark);
}

static
void
putCJKret (type)
  unsigned char type;
{
/* markers available in all CJK encodings:
	300A	《
	FF20	＠
	03BC	μ
	00B0	°
	00A7	§
*/
  unsigned long umark;
  unsigned long mark;

  if (type == lineend_LF) {
	umark = 0x300A;	/* 《 */
  } else if (type == lineend_CRLF) {
	/* MSDOS line separator */
	umark = 0x03BC;	/* μ */
  } else if (type == lineend_CR) {
	/* Mac line separator */
	umark = 0xFF20;	/* ＠ */
  } else if (type == lineend_NONE) {
	umark = '-';
  } else if (type == lineend_NUL) {
	umark = 0x00B0;	/* ° */
  } else if (type == lineend_NL1 || type == lineend_NL2) {
	umark = '<';
  } else if (type == lineend_LS) {
	/* Unicode line separator U+2028 */
	putUmark ('<', NIL_PTR);
	return;
  } else if (type == lineend_PS) {
	/* Unicode paragraph separator U+2029 */
	putUmark (0x00A7, NIL_PTR);
	return;
  } else {
	umark = 0x300A;	/* 《 */
  }

  mark = mappedtermchar (umark);
  set_attr (attr_mark);
  if (no_char (mark)) {
	put_cjkchar ('<');
	put_cjkchar ('<');
  } else {
	put_cjktermchar (mark);
  }
  clear_attr (attr_mark);
}


/*======================================================================*\
|*			Character output				*|
\*======================================================================*/

static int utfcount = 0;
static unsigned long unichar;
static unsigned short prev_cjkchar = 0L;

#define dont_debug_put_cjk
#ifdef debug_put_cjk
#define trace_put_cjk(tag, c)	printf ("put_cjkchar %04X %s\n", c, tag)
#else
#define trace_put_cjk(tag, c)	
#endif

/**
   check if character is not a valid (assigned) Unicode terminal character
   (i.e. considering the auto-detected Unicode version of the terminal)
 */
static
FLAG
no_validunichar (u)
  unsigned long u;
{
  return no_unichar (u) || ! term_isassigned (u);
}

#define dont_debug_subst_decompose

/*
   Put substitution or indication of Unicode character to screen
   if character cannot be displayed on terminal
 */
static
void
put_subst (termchar, unichar, iswide_unichar)
  unsigned long termchar;
  unsigned long unichar;
  int iswide_unichar;
{
#ifdef debug_subst_decompose
	printf ("put_subst term %04lX uni %04lX wide %d\n", termchar, unichar, iswide_unichar);
#endif
	if (iscombining_unichar (unichar)) {
		/*indicate_narrow ('\'');*/
		indicate_narrow ('`');
	} else {
		unsigned long substchar = disp_subst (unichar, False);
		if (substchar != 0) {
			indicate_narrow (substchar);
		} else if (isquotationmark (unichar)) {
			if (unichar == 0xAB || unichar == 0x2039) {
				indicate_narrow ('<');
			} else if (unichar == 0xBB || unichar == 0x203A) {
				indicate_narrow ('>');
			} else if ((unichar >= 0x2018 && unichar <= 0x201B)
				|| unichar == 0x300C || unichar == 0x300D
				|| (unichar >= 0xFE00 && unichar != 0xFF02)
				  )
				{
				indicate_narrow ('\'');
			} else {
				indicate_narrow ('"');
			}
		} else if (isdash (unichar)) {
			if (unichar == 0x301C || unichar == 0x3030) {
				indicate_narrow ('~');
			} else if (unichar == 0x2E17 || unichar == 0x30A0) {
				indicate_narrow ('=');
			} else {
				indicate_narrow ('-');
			}
		} else {
			unsigned long base = unichar;
			int iswide_base;
			FLAG printable;

#ifdef debug_subst_decompose
			printf ("	%04lX(w=%d)\n", base, 1+iswide (base));
#endif

			do {
				printable = True;
				base = decomposition_base (base);
				if (cjk_term || mapped_term) {
					termchar = mappedtermchar (base);
					if (no_char (termchar)) {
						printable = False;
					}
				} else if (base >= 0x100) {
					printable = False;
				}
#ifdef debug_subst_decompose
				printf ("	-> %04lX(w=%d) %sprintable\n", base, 1+iswide (base), printable ? "" : "not ");
#endif
			} while (base > 0 && ! printable);

			if (cjk_term || mapped_term) {
				iswide_base = cjkscrwidth (termchar, "", "") == 2;
			} else {
				iswide_base = iswide (base);
			}
#ifdef debug_subst_decompose
			printf ("	unichar %04lX w=%d -> base %04lX w=%d\n", 
				unichar, 1+iswide_unichar, base, 1+iswide_base);
#endif

			if (base > 0
#ifdef restrict_subst_decompose
			    && (! iswide_base && ! iswide_unichar)
#endif
			    && (! iswide_base || iswide_unichar)) {
				/*	wide -> narrow	indicate, append ' ' below
					wide -> wide	indicate, then return to skip adding ' '
					narrow -> narrow
					narrow -> wide	handled in else branch
				*/
				indicate_uni (base, True, attr_uni);
				if (iswide_base) {
					/* both (char and subst) are wide */
					return;
				}
			} else if (unichar != (character) UNI_marker) {
				indicate_narrow ((character) UNI_marker);
			} else {
				indicate_narrow (char7bit (UNI_marker));
			}
		}
	}

	if (iswide_unichar) {
		/* fill narrow repl. indication to two columns */
		indicate_narrow (' ');
	}
}

/**
   put CJK character to screen;
   to be called only if cjk_term == True
   returns screen length
 */
static
int
put_cjkcharacter (term, cjkchar, width)
  FLAG term;
  unsigned long cjkchar;
  int width;
{
  character cjkbytes [5];
  unsigned long unichar;
  char encoding_tag = term ? term_encoding_tag : text_encoding_tag;
  if (encoding_tag == 'X') {
	encoding_tag = 'J';	/* same cases below */
  }

  (void) cjkencode_char (term, cjkchar, cjkbytes);

  if (! valid_cjkchar (term, cjkchar, cjkbytes) && (suppress_invalid_cjk || ! cjk_term)) {
	trace_put_cjk ("! valid", cjkchar);
	/* character code does not follow encoding pattern */
	indicate_marked ('#');
	if (cjk_term && ! cjk_uni_term) {
		trace_put_cjk ("! valid 1", cjkchar);
		if (encoding_tag == 'J' && (cjkchar >> 8) == 0x8E) {
			return 1;
		} else {
			indicate_marked (' ');
			return 2;
		}
	} else if (utf_cjk_wide_padding && cjkchar >= 0x100) {
		trace_put_cjk ("! valid 2", cjkchar);
		indicate_marked (' ');
		return 2;
	}

  } else if (cjk_term) {
	trace_put_cjk ("cjk_term", cjkchar);

	if (width < 0) {
		if (cjkchar >= 0x100) {
			if (encoding_tag == 'J' && (cjkchar >> 8) == 0x8E) {
				width = 1;
			} else {
				width = 2;
			}
		} else {
			width = 1;
		}
	}

	unichar = lookup_encodedchar (cjkchar);
	if (unichar == 0xA0 /* nbsp */) {
		indicate_special ('�');	/* indicate nbsp */
		return 1;
	}

	/* remap cjkchar to cjk_term */
	if (! term && remapping_chars ()) {
		trace_put_cjk ("... mapped unichar", unichar);
		if (no_unichar (unichar)) {
			indicate_marked ('?');
			if (width == 2) {
				indicate_marked (' ');
			}
			return width;
		}
		cjkchar = mappedtermchar (unichar);
		trace_put_cjk ("... mapped c", cjkchar);
		if (no_char (cjkchar)) {
			if (iscombining_unichar (unichar)) {
				indicate_marked ('\'');
			} else {
				indicate_marked ('%');	/* UNI_marker ? */
			}
			if (width == 2) {
				indicate_marked (' ');
			}
			return width;
		}
		(void) cjkencode_char (True, cjkchar, cjkbytes);
		term = True;
	}

	if ((suppress_extended_cjk && cjkchar > 0xFFFF)
	 || (cjklow_term == False &&
	     ((cjkchar >= 0x8000 && cjkchar < 0xA100
	      && (encoding_tag != 'J' || (cjkchar >> 8) != 0x8E)
	      )
	      || ((cjkchar & 0xFF) >= 0x80 && (cjkchar & 0xFF) < 0xA0)
	     )
	    )
	   ) {
#ifdef old_crap
		/* character code is in extended encoding range and 
		   is assumed not to be displayable by terminal */
		trace_put_cjk ("... #", cjkchar);
		indicate_special ('#');
		if (width == 2) {
			indicate_special (' ');
		}
#else
		if (! cjk_uni_term) {
			width = 2;
		}
		put_subst (cjkchar, unichar, width == 2);
#endif
		return width;
	} else if (suppress_unknown_cjk && 
		   no_validunichar (term
				? lookup_mappedtermchar (cjkchar) 
				: lookup_encodedchar (cjkchar))
		  ) {
		/* character code has no mapping to Unicode */
		trace_put_cjk ("... ?", cjkchar);
		indicate_marked ('?');
		if (width == 2) {
			indicate_marked (' ');
		}
		return width;
	} else {
		character * cp = cjkbytes;
		trace_put_cjk ("... ", cjkchar);
		while (* cp != '\0') {
			putchar (* cp ++);
		}
		return width;
	}

  } else {
	unichar = lookup_encodedchar (cjkchar);
	if (no_unichar (unichar)) {
		indicate_marked ('?');
		if (utf_cjk_wide_padding && cjkchar >= 0x100) {
			indicate_marked (' ');
			return 2;
		}
	} else {
		put_unichar (unichar);
		if (iswide (unichar)) {
			return 2;
		} else if (utf_cjk_wide_padding && cjkchar >= 0x100) {
			/* simulate double screen width of CJK 
			   character even if corresponding 
			   Unicode character has only single width
			*/
			__putchar (' ');
			return 2;
		}
	}
  }

  return 1;
}

/**
   put CJK character (text encoding) to screen;
   to be called only if cjk_term == True
   returns screen length
 */
int
put_cjkchar (cjkchar)
  unsigned long cjkchar;
{
  return put_cjkcharacter (False, cjkchar, -1);
}

/**
   put CJK terminal character (terminal encoding) to screen;
   to be called only if cjk_term == True
   returns screen length
 */
static
int
put_cjktermchar (cjkchar)
  unsigned long cjkchar;
{
  return put_cjkcharacter (True, cjkchar, -1);
}

/**
   put Arabic character to screen, either
   - isolated, for separated display
   - ligature, for joined display
 */
static
void
put_arabic (subst)
  unsigned long subst;
{
  if ((cjk_term || mapped_term) && no_char (mappedtermchar (subst))) {
	/* use TATWEEL instead; could use one of
		U+061F ؟ ARABIC QUESTION MARK
		U+066D ٭ ARABIC FIVE POINTED STAR
		U+0640 ـ ARABIC TATWEEL
	   (or rather substitute the base char U+0644 ل ARABIC LETTER LAM)
	 */
	put_unichar (0x0640);
  } else {
	put_unichar (subst);
  }
}


static
void
put_uniend ()
{
  if (utf8_text && utfcount > 0) {
	/* previous character not terminated properly */
	indicate_illegal_UTF ((character) '�');
	utfcount = 0;
  }
}


#define REPLACEMENT_CHARACTER 0xFFFD

/*
 * put Unicode (UCS-4, actually) character to screen;
 * put as UTF-8 on UTF-8 terminal, or as Latin-1 otherwise
	7 bits	0xxxxxxx
	 8..11	110xxxxx 10xxxxxx
	12..16	1110xxxx 10xxxxxx 10xxxxxx
	17..21	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
	22..26	111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
	27..31	1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
 */
void
put_unichar (unichar)
  unsigned long unichar;
{
  if (unassigned_single_width) {
	if (rxvt_version > 0) {
		/* handle weird mapping of non-Unicode ranges */
		if (unichar < 0x80000000) {
			unichar &= 0x1FFFFF;
		}
	}
  }

  if (mapped_text && no_unichar (unichar)) {
	indicate_marked ('?');
  } else if (unichar < ' ') {
	indicate_uni (unichar + '@', False, attr_control);
  } else if (unichar == 0x7F) {
	indicate_uni ('?', False, attr_control);
  } else if (unichar >= 0x80 && unichar <= 0x9F) {
	if (unichar == 0x9F) {
		indicate_narrow ('?');
	} else {
		indicate_narrow (unichar - 0x20);
	}
  } else if (unichar == 0xA0 /* nbsp */) {
	indicate_special ('�');	/* indicate nbsp in UTF-8 / mapped terminal */
  } else if (suppress_EF && (unichar & 0xFFFE) == 0xFFFE) {
	indicate_illegal_UTF (REPLACEMENT_CHARACTER);
	if (iswide (unichar)) {
		indicate_illegal_UTF (' ');
	}
  } else if (suppress_non_Unicode && unichar >= 0x110000 && unichar < 0x80000000) {
	indicate_illegal_UTF (REPLACEMENT_CHARACTER);
	if (iswide (unichar)) {
		indicate_illegal_UTF (' ');
	}
  } else if (suppress_non_BMP == True /* sic! */ && unichar >= 0x10000) {
	indicate_illegal_UTF (REPLACEMENT_CHARACTER);
	if (iswide (unichar)) {
		indicate_illegal_UTF (' ');
	}
  } else if (suppress_surrogates && unichar >= 0xD800 && unichar <= 0xDFFF) {
	indicate_illegal_UTF (REPLACEMENT_CHARACTER);
	if (iswide (unichar)) {
		indicate_illegal_UTF (' ');
	}
  } else if (utf8_screen) {
	if (unichar < 0x80) {
		__putchar (unichar);
	} else if (unichar < 0x800) {
		__putchar (0xC0 | (unichar >> 6));
		__putchar (0x80 | (unichar & 0x3F));
	} else if (unichar < 0x10000) {
		__putchar (0xE0 | (unichar >> 12));
		__putchar (0x80 | ((unichar >> 6) & 0x3F));
		__putchar (0x80 | (unichar & 0x3F));
	} else if (unichar < 0x200000) {
		__putchar (0xF0 | (unichar >> 18));
		__putchar (0x80 | ((unichar >> 12) & 0x3F));
		__putchar (0x80 | ((unichar >> 6) & 0x3F));
		__putchar (0x80 | (unichar & 0x3F));
	} else if (unichar < 0x4000000) {
		__putchar (0xF8 | (unichar >> 24));
		__putchar (0x80 | ((unichar >> 18) & 0x3F));
		__putchar (0x80 | ((unichar >> 12) & 0x3F));
		__putchar (0x80 | ((unichar >> 6) & 0x3F));
		__putchar (0x80 | (unichar & 0x3F));
	} else if (unichar < 0x80000000) {
		__putchar (0xFC | (unichar >> 30));
		__putchar (0x80 | ((unichar >> 24) & 0x3F));
		__putchar (0x80 | ((unichar >> 18) & 0x3F));
		__putchar (0x80 | ((unichar >> 12) & 0x3F));
		__putchar (0x80 | ((unichar >> 6) & 0x3F));
		__putchar (0x80 | (unichar & 0x3F));
	} else {
		/* special encoding of 2 Unicode chars, 
		   mapped from 1 CJK character */
		put_unichar (unichar & 0xFFFF);
		unichar = (unichar >> 16) & 0x7FFF;
		if (combining_screen || ! iscombining (unichar)) {
			put_unichar (unichar);
		} else {
			/* if it's a combining char but the screen 
			   cannot handle it, just ignore it for now
			*/
		}
	}
  } else {	/* not utf8_screen */
	int iswide_unichar;
	/* only used if cjk_term || mapped_term: */
	unsigned long termchar = 0;	/* (avoid -Wmaybe-uninitialized) */

	if (cjk_term || mapped_term) {
		termchar = mappedtermchar (unichar);
	}

#ifdef wrong_attempt
	if (cjk_term || mapped_term) {
		termchar = mappedtermchar (unichar);
		iswide_unichar = cjkscrwidth (termchar, "", "") == 2;
	} else {
		iswide_unichar = iswide (unichar);
	}
#else
	/* here the width assumed by the caller of this function is needed */
	iswide_unichar = iswide (unichar);
#endif

	if (cjk_term) {
		if (! no_char (termchar)) {
			if (utf8_text && ! cjk_uni_term) {
				/* workaround for inconsistent width handling 
				   when replacing Unicode with "#" (-> "# ")
				 */
				(void) put_cjkcharacter (True, termchar, -1);
			} else {
				(void) put_cjkcharacter (True, termchar, 1 + iswide_unichar);
			}
			return;
		}
	} else if (mapped_term) {
		if (! no_char (termchar)) {
			__putchar (termchar);
			return;
		}
	} else if (unichar < 0x100) {
		__putchar (unichar);
		return;
	}

	/* character cannot be displayed on terminal;
	   display indication or substitution */
	put_subst (termchar, unichar, iswide_unichar);
  }	/* not utf8_screen */
}


/*
 * put_char () puts a character byte to the screen (via buffer)
 * If UTF-8 text is being output through put_char () but the screen 
 * is not in UTF-8 mode, put_char () transforms the byte sequences.
 * In CJK mode, this function should not be called anymore with 
 * multibyte CJK codes (Shift-JIS single-byte is OK).
 */
static
void
put_char (c)
  character c;
{
  if (utf8_text) {
	if (c < 0x80) {
		put_uniend ();
		__putchar (c);
	} else if ((c & 0xC0) == 0x80) {
		if (utfcount == 0) {
			indicate_illegal_UTF ('8');
			return;
		}
		unichar = (unichar << 6) | (c & 0x3F);
		utfcount --;

		if (utfcount == 0) {
			/* final UTF-8 byte */
			put_unichar (unichar);
		} else {
			/* character continues */
			return;
		}
	} else { /* first UTF-8 byte */
		if (utfcount > 0) {
			/* previous character not terminated properly */
			indicate_illegal_UTF ((character) '�');
			utfcount = 0;
		}
		if ((c & 0xE0) == 0xC0) {
			utfcount = 2;
			unichar = c & 0x1F;
		} else if ((c & 0xF0) == 0xE0) {
			utfcount = 3;
			unichar = c & 0x0F;
		} else if ((c & 0xF8) == 0xF0) {
			utfcount = 4;
			unichar = c & 0x07;
		} else if ((c & 0xFC) == 0xF8) {
			utfcount = 5;
			unichar = c & 0x03;
		} else if ((c & 0xFE) == 0xFC) {
			utfcount = 6;
			unichar = c & 0x01;
		} else {
			/* illegal UTF-8 code 254 (0xFE) or 255 (0xFF) */
			indicate_illegal_UTF ('4' + (c & 1));
			return;
		}
		utfcount --;
		return;
	}
  } else if (cjk_text && cjk_term == False) {
	/* prev_cjkchar mechanism obsolete (would not suffice anymore...) */
	if (prev_cjkchar != 0 || (c >= 0x80 && ! multichar (c))) {
		put_cjkchar ((prev_cjkchar << 8) | c);
		prev_cjkchar = 0;
	} else if (multichar (c)) {
		prev_cjkchar = c;
	} else {
		__putchar (c);
	}
  } else {
	if (unicode (c) == 0xA0 /* nbsp */) {
		indicate_special ('�');	/* indicate nbsp in Latin-1 terminal */
	} else {
		__putchar (c);
	}
  }
}

/*
 * putcharacter (c) outputs a single-byte non-UTF character
 */
void
putcharacter (c)
  character c;
{
  if (mapped_text) {
	put_unichar (lookup_encodedchar (c));
  } else if (utf8_screen && ! utf8_text) {
	put_unichar (c);
  } else if (mapped_term) {
	put_unichar (c);
  } else if (cjk_term && ! utf8_text && ! cjk_text) {
	/* remap non-ASCII Latin-1 char to cjk_term */
	put_unichar (c);
  } else {
	put_char (c);
  }
}


/*======================================================================*\
|*			Text line display				*|
\*======================================================================*/

#define no_script -999
static character syntax_mask = syntax_none;
static int current_script_colour_256;
static int current_script_colour_88;


void
put_blanks (endpos)
  int endpos;
{
  int startpos = 0;
  while (startpos ++ <= endpos) {
	putchar (' ');
  }
}

static
void
clear_wholeline ()
{
  if (can_clear_eol) {
	clear_eol ();
  } else {
	put_blanks (XMAX);
  }
}

void
clear_lastline ()
{
  if (can_clear_eol) {
	clear_eol ();
  } else {
	put_blanks (XMAX - 1);
  }
}


static
void
disp_syntax (prev_syntax_mask, syntax_mask)
  character prev_syntax_mask;
  character syntax_mask;
{
  if (syntax_mask != prev_syntax_mask) {
	character new_syntax = syntax_mask & ~ prev_syntax_mask;
	character old_syntax = prev_syntax_mask & ~ syntax_mask;

	if (viewing_help) {
		if (old_syntax & syntax_comment) {
			clear_attr (attr_jsp);
		}
		if (new_syntax & syntax_comment) {
			set_attr (attr_jsp);
		}
		return;
	}

	if (old_syntax & syntax_scripting) {
		clear_attr (attr_jsp);
	}
	if (old_syntax & syntax_comment) {
		clear_attr (attr_comment);
	}
	if (old_syntax & syntax_HTML) {
		clear_attr (attr_html);
	}
	if (old_syntax & syntax_attrib) {
		clear_attr (attr_attrib);
	}
	if (old_syntax & syntax_value) {
		clear_attr (attr_value);
	}

	if (new_syntax & syntax_HTML) {
		set_attr (attr_html);
	}
	if (new_syntax & syntax_comment) {
		set_attr (attr_comment);
	}
	if (new_syntax & syntax_scripting) {
		set_attr (attr_jsp);
	}
	if (new_syntax & syntax_attrib) {
		set_attr (attr_attrib);
	}
	if (new_syntax & syntax_value) {
		set_attr (attr_value);
	}
  }
}


static
void
disp_script (script_colour, for_256_colours)
  int script_colour;
  FLAG for_256_colours;
{
  if (use_script_colour) {
	if (script_colour == no_script) {
		clear_colour_attr ();
	} else {
		colour_256 = for_256_colours;
		set_colour_attr (script_colour);
	}
  }
}

static struct colour_mapping {
	char * scriptname;
	int colour;
	int colour0;
} colour_table [] = {
#include "colours.t"
};

/**
   Determine display colour for character script.
 */
static
void
map_script_colour (scriptname, sc, sc0)
  char * scriptname;
  int * sc;
  int * sc0;
{
  int min = 0;
  int max = sizeof (colour_table) / sizeof (struct colour_mapping) - 1;
  int mid;
  int cmp;

  /* binary search in table */
  while (max >= min) {
    mid = (min + max) / 2;
    cmp = strcmp (colour_table [mid].scriptname, scriptname);
    if (cmp < 0) {
      min = mid + 1;
    } else if (cmp > 0) {
      max = mid - 1;
    } else {
      * sc = colour_table [mid].colour;
      * sc0 = colour_table [mid].colour0;
      return;
    }
  }

  * sc = no_script;
  * sc0 = -1;
}


static
void
restore_attr (restore_HTML)
  FLAG restore_HTML;
{
  if (mark_HTML && restore_HTML) {
	disp_syntax (syntax_none, syntax_mask);
  }

  if (current_script_colour_88 > 0) {
	disp_script (current_script_colour_88, False);
  }
  if (current_script_colour_256 > 0 && (colours_256 || current_script_colour_88 <= 0)) {
	disp_script (current_script_colour_256, True);
  }
}


#define dont_debug_put_line
#ifdef debug_put_line
#define trace_put_line(params)	printf params
#else
#define trace_put_line(params)	
#endif

#define dont_debug_width


/*
 * print_line: print line to given screen line
 */
void
print_line (ly, line)
  int ly;
  LINE * line;
{
  set_cursor (0, ly);
  put_line (ly, line, 0, True, False);
  line->dirty = False;
}

static
char *
after_password (tp)
  char * tp;
{
  char * afterpw = strcontains (tp, "assword");
  if (afterpw) {
	afterpw += 7;
	while (* afterpw == ':' || * afterpw == '=' || white_space (* afterpw)) {
		afterpw ++;
	}
  }
  return afterpw;
}

/*
 * Put_line prints the given line on the standard output.
 * If offset is not zero, printing will start at that x-coordinate.
 * If the FLAG clear_line is True, then the screen line will be cleared
 * when the end of the line has been reached.
 */
void
put_line (ly, line, offset, clear_line, positioning)
  int ly;		/* current screen line */
  LINE * line;		/* line to print */
  int offset;		/* offset to start if positioning == False */
			/* position if positioning == True */
  FLAG clear_line;	/* clear to eoln if True */
  FLAG positioning;	/* positioning inside line if True (for prop. fonts) */
{
  char * textp = line->text;
  char * parap;
  int count = line->shift_count * - SHIFT_SIZE;
  int count_ini = count;
  int offset_start;
  int offset_stop;

  int charwidth;
  FLAG separate_combining = True;	/* at line beginning, after TAB */
  FLAG save_combining_mode = combining_mode;

  /* only used if mark_HTML: */
  character syntax_mask_old = syntax_none;
  character syntax_mask_new = syntax_none;	/* avoid -Wmaybe-uninitialized */
  FLAG after_indent = False;

  FLAG sel_highlighting = False;

  char * password = NIL_PTR;

  int line_script_colour = no_script;

  int qn = 0;	/* quote count for heuristic string context */
  FLAG do_strop = UNSURE;
  unsigned long small_unichar = 0;	/* if (do_strop); avoid -Wmaybe-uninitialized */
  unsigned long prev_unichar = 0;

#define dont_debug_syntax_state
#ifdef debug_syntax_state
  printf ("put_line @%d (%02X/%02X) @%s", offset, line->prev->syntax_mask, line->syntax_mask, line->text);
#endif

  clear_attr (attr_all);

  if (hide_passwords) {
	password = after_password (textp);
  }

  if (mark_HTML) {
	syntax_mask = line->prev->syntax_mask;
	syntax_mask_old = syntax_none;
	syntax_mask_new = syntax_mask;
#ifdef highlight_complete_line_for_uni_lineend
	/* renders line unreadable in dark terminals */
	switch (line->return_type) {
		case lineend_LS:
		case lineend_PS:
			line_script_colour = 6;
			break;
	}
#endif
  }
  current_script_colour_256 = line_script_colour;
  current_script_colour_88 = line_script_colour;

  if (positioning) {
	offset_start = 0;
	offset_stop = offset;
  } else {
	offset_start = offset;
	offset_stop = XBREAK;
  }

/* Skip all chars as indicated by the offset_start and the shift_count field */
  while (count < offset_start) {
	/* ... and perform all hidden state updates */
	if (mark_HTML) {
		syntax_mask_new = syntax_state (syntax_mask_new, syntax_mask_old, textp, line->text);
		syntax_mask_old = syntax_mask;
		syntax_mask = syntax_mask_new;
	}
	if (! white_space (* textp)) {
		after_indent = True;
	}
	if (dispunstrop) {
		unichar = unicodevalue (textp);
		if ((unichar == '"' || unichar == '\'')
		    && (qn % 2 == 0 || prev_unichar != '\\')) {
			qn ++;
			do_strop = UNSURE;
		} else if (qn % 2 == 0) {
			if (isLetter (unichar)) {
				small_unichar = case_convert (unichar, -1);
				if (small_unichar == unichar) {
					/* small letter */
					do_strop = False;
				} else if (do_strop) {	/* True or UNSURE */
					/* capital not preceded by small */
					do_strop = True;
				}
			} else {
				do_strop = UNSURE;
			}
		}
		prev_unichar = unichar;
	}
	if (password && textp >= password && white_space (* textp)) {
		password = after_password (textp);
	}

	advance_char_scr (& textp, & count, line->text);
  }

  if (standout_glitch) {
	clear_eol ();
	set_scrollbar_dirty (ly);
	disp_normal ();
  }

  if (count_ini < 0 && offset_start == 0) {
	/* displaying line shifted out left, starting on screen column 0 */
	if (marker_defined (SHIFT_BEG_marker, UTF_SHIFT_BEG_marker)) {
		/* display the shift marker */
		if (cjk_term) {
			putshiftmark ('<', NIL_PTR);
		} else {
			putshiftmark (SHIFT_BEG_marker, UTF_SHIFT_BEG_marker);
		}
		if (count == 0) {
			if (ebcdic_text ? * textp != code_TAB : * textp != '\t') {
				if (mark_HTML) {
					syntax_mask_new = syntax_state (syntax_mask_new, syntax_mask_old, textp, line->text);
					syntax_mask_old = syntax_mask;
					syntax_mask = syntax_mask_new;
				}
				if (iswide (unicodevalue (textp))) {
					/* skipped character is double-width, 
					   indicate */
					if (cjk_term) {
						putmark ('.', NIL_PTR);
					} else {
						putmark (shiftskipdouble, NIL_PTR);
					}
					count ++;
				}
				/* skip character replaced by shift marker */
				advance_char (& textp);
			}
			count ++;
		} else {
			/* last character was double-width, already skipped */
		}
	} else if (count > 0) {
		/* last character was double-width, already skipped */
		if (cjk_term) {
			putmark ('.', NIL_PTR);
		} else {
			putmark (shiftskipdouble, NIL_PTR);
		}
	}
  }

  restore_attr (after_indent);

  if (offset_start > 0 && * (textp - 1) != '\t') {
	/* fix display of isolated combinings in case of partial line display */
	separate_combining = False;
  }

  if (show_vt100_graph) {
	altcset_on ();
  }

  while (True) {
	unsigned long unichar;
	int follow;
	/* only used if cjk_text: (avoid -Wmaybe-uninitialized) */
	unsigned long cjkchar = 0;

	if (separate_isolated_combinings) {
		if (separate_combining) {
			combining_mode = False;
			separate_combining = False;
		} else {
			combining_mode = save_combining_mode;
		}
	}

	if (sel_highlighting && line->sel_end && textp >= line->sel_end) {
		clear_attr (attr_selected);
		sel_highlighting = False;
	} else if (line->sel_begin && ! sel_highlighting 
			&& textp >= line->sel_begin
			&& (! line->sel_end || textp < line->sel_end)) {
		set_attr (attr_selected);
		sel_highlighting = True;
	}

	if (* textp == '\n') {
		charwidth = 1;
		break;
	}

	if (utf8_text) {
		utf8_info (textp, & follow, & unichar);
		charwidth = uniscrwidth (unichar, textp, line->text);
	} else if (cjk_text) {
		cjkchar = charvalue (textp);
		unichar = lookup_encodedchar (cjkchar);
		charwidth = cjkscrwidth (cjkchar, textp, line->text);
#ifdef debug_width
	printf ("cjk %04X u %04X w %d\n", cjkchar, unichar, charwidth);
#endif
	} else if (mapped_text) {
		unichar = lookup_encodedchar ((character) * textp);
		charwidth = cjkscrwidth ((character) * textp, textp, line->text);
	} else /* Latin-1 */ {
		unichar = (character) * textp;
		if (cjk_term || cjk_width_data_version) {
			charwidth = uniscrwidth (unichar, textp, line->text);
		} else {
			charwidth = 1;
		}
	}
	if (unassigned_single_width) {
		if (rxvt_version > 0) {
			/* handle weird mapping of non-Unicode ranges */
			if (unichar < 0x80000000) {
				unichar &= 0x1FFFFF;
			}
		}
	}

	if (count >= offset_stop + 1 - charwidth) {
		break;
	}

	if (! white_space (* textp)) {
		restore_attr (! after_indent);
		after_indent = True;
	}
	if (mark_HTML) {
		syntax_mask_new = syntax_state (syntax_mask_new, syntax_mask_old, textp, line->text);
		syntax_mask_old = syntax_mask;
		if (syntax_mask_new != syntax_mask_old) {
			if (after_indent) {
				disp_syntax (syntax_mask_old, syntax_mask_new);
				syntax_mask = syntax_mask_new;
			}
		}
	}

	if (dispunstrop) {
		if ((unichar == '"' || unichar == '\'')
		    && (qn % 2 == 0 || prev_unichar != '\\')) {
			qn ++;
			do_strop = UNSURE;
		} else if (qn % 2 == 0) {
			if (isLetter (unichar)) {
				small_unichar = case_convert (unichar, -1);
				if (small_unichar == unichar) {
					/* small letter */
					do_strop = False;
				} else if (do_strop) {	/* True or UNSURE */
					/* capital not preceded by small */
					do_strop = True;
				}
			} else {
				do_strop = UNSURE;
			}
		}
		prev_unichar = unichar;
	}

	if (ebcdic_text ? * textp == code_TAB : * textp == '\t') {
		/* Expand tabs to spaces */
		int tab_count = tab (count);
		int tab_mid = count + (tab_count - count) / 2 + 1;

		if (separate_isolated_combinings) {
			separate_combining = True;
		}

		put_uniend ();
		if (cjk_term) {
			if ((count & 1) == 1) {
				count ++;
				if (cjk_tab_width == 1) {
					putCJKtab ();
				} else {
					__putchar (' ');
				}
			}
			while (count < offset_stop && count < tab_count) {
				putCJKtab ();
				count ++;
				if (cjk_tab_width == 2) {
					count ++;
				}
			}
		} else if (marker_defined (TAB0_marker, UTF_TAB0_marker)) {
			count ++;
			putmarkmode (TAB0_marker, UTF_TAB0_marker, True);
			while (count < offset_stop && count < tab_count - 1) {
				count ++;
				putmarkmode (TAB_marker, UTF_TAB_marker, True);
			}
			if (count < offset_stop && count < tab_count) {
				count ++;
				if (marker_defined (TAB2_marker, UTF_TAB2_marker )) {
					putmarkmode (TAB2_marker, UTF_TAB2_marker, True);
				} else {
					putmarkmode (TAB_marker, UTF_TAB_marker, True);
				}
			}
		} else while (count < offset_stop && count < tab_count) {
			count ++;
			if (count == tab_mid && marker_defined (TABmid_marker, UTF_TABmid_marker)) {
				putmarkmode (TABmid_marker, UTF_TABmid_marker, True);
			} else {
				putmarkmode (TAB_marker, UTF_TAB_marker, True);
			}
		}
		endmarkmode ();
		restore_attr (after_indent);

		textp ++;
	} else if (password && textp >= password && ! white_space (* textp)) {
		int len;

		put_uniend ();
		set_attr (attr_control);
		for (len = 0; len < charwidth; len ++) {
			putcharacter ('*');
		}
		clear_attr (attr_control);
		advance_char (& textp);
		if (white_space (* textp)) {
			password = after_password (textp);
		}
		count += charwidth;
	} else if (unichar == 0xA0 /* nbsp */) {
		put_uniend ();

		indicate_special ('�');

		restore_attr (True);

		advance_char (& textp);
		count += charwidth;
	} else if (iscontrol (* textp)) {
		/* this covers only 1 byte controls; 
		   GB18030 "C1" controls are handled elsewhere
		 */
		put_uniend ();

		if (unichar >= 0x80) {
			if (unichar == 0x9F) {
				indicate_narrow ('?');
			} else {
				indicate_narrow (unichar - 0x20);
			}
		} else {
			set_attr (attr_control);
			putcharacter (controlchar (unichar));
			clear_attr (attr_control);
		}

		restore_attr (True);

		advance_char (& textp);
		count ++;
	} else {
		FLAG disp_separate_combining = False;
		FLAG disp_separate_joining = False;
		FLAG disp_joined = False;

		if (utf8_text || cjk_text || mapped_text) {
			int last_script_colour_256 = current_script_colour_256;
			int last_script_colour_88 = current_script_colour_88;
			map_script_colour (script (unichar), 
				& current_script_colour_256, & current_script_colour_88);
			if (current_script_colour_256 == no_script) {
				current_script_colour_256 = line_script_colour;
			}

			if (current_script_colour_88 != last_script_colour_88) {
				if (current_script_colour_88 > 0) {
					disp_script (current_script_colour_88, False);
				}
			}
			if (current_script_colour_256 != last_script_colour_256) {
				if (colours_256 || current_script_colour_88 <= 0) {
					disp_script (current_script_colour_256, True);
				}
			}
		}

		if (utf8_text) {
			unsigned long substchar;

			follow --;

			if (iscombined (unichar, textp, line->text)) {
			    if (combining_screen && ! combining_mode) {
				/* enforce separated display */
				disp_separate_combining = True;
				set_attr (attr_combining);

				if (iswide (unichar)) {
					put_unichar (0x3000);
					count += 2;
				} else {
					if (isjoined (unichar, textp, line->text)) {
						disp_separate_joining = True;
#ifdef mlterm_fixed
						/* Separate display of 
						   Arabic ligature components;
						   don't output base blank 
						   and try to suppress 
						   Arabic ligature shaping
						   with one of
						   200B;ZERO WIDTH SPACE
						   200C;ZERO WIDTH NON-JOINER
						   200D;ZERO WIDTH JOINER
						   FEFF;ZERO WIDTH NO-BREAK SPACE
						   - none works in mlterm
						 */
						put_unichar (0x200C);
#else
						/* Transform character 
						   to its isolated form;
						   see below
						 */
#endif
					} else {
						putcharacter (' ');
					}
					count += 1;
				}
			    } else {
				if (! combining_screen) {
					disp_separate_combining = True;
					set_attr (attr_combining);
					count += charwidth;
				} else if (apply_joining
					&& isjoined (unichar, textp, line->text)) {
					disp_joined = True;
				}
			    }
			} else {
				if (iscombining_unichar (unichar)) {
					disp_separate_combining = True;
					set_attr (attr_combining);
				}
				count += charwidth;
			}

			if (disp_joined) {
				/* apply ligature joining in mined */
				/* clear previously displayed LAM first */
				putchar ('');
				put_unichar (ligature_lam_alef (unichar));
				/* skip rest of joining char */
				textp ++;
				while (follow > 0 && (* textp & 0xC0) == 0x80) {
					textp ++;
					follow --;
				}
#ifndef mlterm_fixed
			} else if (disp_separate_joining) {
				/* prevent terminal ligature by 
				   substituting joining character 
				   with its ISOLATED FORM */
				put_unichar (isolated_alef (unichar));
				/* skip rest of joining char */
				textp ++;
				while (follow > 0 && (* textp & 0xC0) == 0x80) {
					textp ++;
					follow --;
				}
#endif
			} else if ((substchar = disp_subst (unichar, True)) != 0) {
				indicate_uni (substchar, True, attr_uni);
				/* skip UTF-8 char */
				textp ++;
				while (follow > 0 && (* textp & 0xC0) == 0x80) {
					textp ++;
					follow --;
				}
			} else if (do_strop == True) {
				set_attr (attr_keyword);
				put_unichar (small_unichar);
				clear_attr (attr_keyword);
				textp ++;
				while (follow > 0 && (* textp & 0xC0) == 0x80) {
					textp ++;
					follow --;
				}
			} else {
				put_char (* textp ++);
				while (follow > 0 && (* textp & 0xC0) == 0x80) {
					put_char (* textp ++);
					follow --;
				}
			}
			if (disp_separate_combining) {
				clear_attr (attr_combining);
				restore_attr (True);
			}

		} else if (cjk_text) {
			if (do_strop == True) {
				int cjklen = CJK_len (textp);
				set_attr (attr_keyword);
				put_unichar (small_unichar);
				clear_attr (attr_keyword);

				while (cjklen > 0 && * textp != '\n' && * textp != '\0') {
					textp ++;
					cjklen --;
				}
				count += charwidth;
			} else if (multichar (* textp)) {
				int charlen;
				character cjkbytes [5];
				int cjklen = CJK_len (textp);
				char * this_textp = textp;

				charlen = cjkencode (cjkchar, cjkbytes);
				trace_put_line (("%04X %d + %d ", cjkchar, count, charwidth));

				while (cjklen > 0 && * textp != '\n' && * textp != '\0') {
					textp ++;
					cjklen --;
					charlen --;
				}
				if (cjklen != 0 || charlen != 0) {
					/* incomplete CJK char */
					indicate_marked ('<');
					count ++;
					if (utf_cjk_wide_padding) {
						if (charlen == 0 || charlen == -3) {
							indicate_marked (' ');
							count ++;
						}
					}
				} else {
					trace_put_line (("- put %04X ", cjkchar));

					if (iscombined (unichar, this_textp, line->text)) {
					    if (combining_screen && ! combining_mode) {
						/* enforce separated display */
						disp_separate_combining = True;
						set_attr (attr_combining);
						if (charwidth == 2) {
							put_unichar (0x3000);
						} else if (isjoined (unichar, this_textp, line->text)) {
							disp_separate_joining = True;
						} else {
							putcharacter (' ');
						}
					    } else {
						if (! combining_screen) {
							disp_separate_combining = True;
							set_attr (attr_combining);
						} else if (apply_joining
							&& isjoined (unichar, this_textp, line->text)) {
							disp_joined = True;
						}
					    }
					} else if (iscombining_unichar (unichar)) {
						disp_separate_combining = True;
						set_attr (attr_combining);
					}

					if (disp_joined) {
						/* apply ligature joining in mined */
						/* clear previously displayed LAM first */
						putchar ('');
						count -= charwidth;
						put_arabic (ligature_lam_alef (unichar));
					} else if (disp_separate_joining) {
						/* prevent terminal ligature 
						   by substituting joining character 
						   with its ISOLATED FORM */
						put_arabic (isolated_alef (unichar));
					} else {
						put_cjkcharacter (False, cjkchar, charwidth);
					}

					if (disp_separate_combining) {
						clear_attr (attr_combining);
						restore_attr (True);
					}

					count += charwidth;
				}
				trace_put_line (("-> %d\n", count));
			} else {
				put_cjkchar ((character) * textp);
				textp ++;
				count += charwidth;
			}
		} else /* ! utf_text && ! cjk_text */ {
			if (mapped_text) {
				if (iscombined (unichar, textp, line->text)) {
				    if (combining_screen && ! combining_mode) {
					/* enforce separated display */
					disp_separate_combining = True;
					set_attr (attr_combining);

					if (isjoined (unichar, textp, line->text)) {
						disp_separate_joining = True;
					} else {
						putcharacter (' ');
					}
				    } else {
					if (! combining_screen) {
						disp_separate_combining = True;
						set_attr (attr_combining);
					} else if (apply_joining
						&& isjoined (unichar, textp, line->text)) {
						disp_joined = True;
					}
				    }
				}
			}

			if (do_strop == True) {
				set_attr (attr_keyword);
				put_unichar (small_unichar);
				clear_attr (attr_keyword);
			} else if (mapped_text && unichar == 0xA0 /* nbsp */) {
				indicate_special ('�');
			} else {
				if (disp_joined) {
					/* apply ligature joining in mined */
					/* clear previously displayed LAM first */
					putchar ('');
					count -= charwidth;
					put_arabic (ligature_lam_alef (unichar));
				} else if (disp_separate_joining) {
					/* prevent terminal ligature 
					   by substituting joining character 
					   with its ISOLATED FORM */
					put_arabic (isolated_alef (unichar));
				} else {
					putcharacter ((character) * textp);
				}
			}

			if (disp_separate_combining) {
				clear_attr (attr_combining);
				restore_attr (True);
			}

			textp ++;
			count += charwidth;
		}

		if (mark_HTML && syntax_mask != syntax_mask_old) {
			if (after_indent) {
				disp_syntax (syntax_mask_old, syntax_mask);
			}
		}
	}
  }

  if (show_vt100_graph) {
	altcset_off ();
  }

  if (separate_isolated_combinings) {
	combining_mode = save_combining_mode;
  }

  put_uniend ();

  if (current_script_colour_256 != no_script || current_script_colour_88 > 0) {
	disp_script (no_script, UNSURE);
  }

  if (mark_HTML) {
	/* cancel syntax future state */
	(void) syntax_state (syntax_mask_new, syntax_mask_old, "", line->text);
	/* reset HTML display attribute */
	disp_syntax (syntax_mask, syntax_none);
  }

  if (positioning) {
	/* self-made cursor for terminals (such as xterm)
	   which have display problems with proportional screen fonts
	   and their cursor */
	if (sel_highlighting) {
		clear_attr (attr_selected);
		sel_highlighting = False;
	}

	reverse_on ();
	if (* textp != '\n') {
		if (utf8_text) {
			put_unichar (charvalue (textp));
			put_uniend ();
		} else if (cjk_text) {
			put_cjkchar (charvalue (textp));
		} else {
			putcharacter (* textp);
		}
	} else if (cjk_term) {
		putCJKret (line->return_type);
	} else if (RET_marker != '\0') {
		if (PARA_marker != '\0' && paradisp
		 && line->return_type != lineend_LS
		 && line->return_type != lineend_PS) {
			parap = textp;
			parap --;
			if (* parap == ' ' || * parap == '\t') {
				putret (line->return_type);
			} else {
				putmark (PARA_marker, UTF_PARA_marker);
			}
		} else {
			putret (line->return_type);
		}
	} else {
		putchar (' ');
	}
	reverse_off ();
	set_cursor (0, 0);
  } else /* (positioning == False) */ {
	/* If line is longer than XBREAK chars, print the shift_mark */
	if (count <= XBREAK && * textp != '\n') {
		if (sel_highlighting) {
			clear_attr (attr_selected);
			sel_highlighting = False;
		}
		if (count < XBREAK && charwidth == 2) {
			if (cjk_term) {
				putmark ('.', NIL_PTR);
			} else {
				putmark (shiftskipdouble, NIL_PTR);
			}
			count ++;
		}
		if (cjk_term) {
			putshiftmark ('>', NIL_PTR);
			count ++;
		} else {
			putshiftmark (SHIFT_marker, UTF_SHIFT_marker);
			count ++;
		}
		if (count == XBREAK) {
			putchar (' ');
			count ++;
		}
	}

	/* Mark end of line if desired */
	if (* textp == '\n') {
	    if (cjk_term) {
		if (RET_marker >= ' ' && (character) RET_marker <= '@') {
			putmark (RET_marker, NIL_PTR);
		} else {
			putCJKret (line->return_type);
			/* to ensure it fits, scrollbar_width is set to 1 
			   in CJK terminal mode even if scrollbar is switched off */
			if (cjk_lineend_width == 2) {
				count ++;
			}
		}
		if (sel_highlighting) {
			clear_attr (attr_selected);
			sel_highlighting = False;
		}
	    } else if (RET_marker != '\0') {
		if (PARA_marker != '\0' && paradisp
		 && line->return_type != lineend_LS
		 && line->return_type != lineend_PS) {
			parap = textp;
			parap --;
			if (* parap == ' ') {
				putret (line->return_type);
			} else {
				putmark (PARA_marker, UTF_PARA_marker);
			}
		} else {
			putret (line->return_type);
			if (standout_glitch && 
			    (line->return_type == lineend_LS
			    || line->return_type == lineend_PS)) {
				putchar (' ');
				count ++;
				if (count > XBREAK) {
					set_scrollbar_dirty (ly);
				}
			}
		}
		if (sel_highlighting) {
			clear_attr (attr_selected);
			sel_highlighting = False;
		}

		/* Fill end of line with markers if desired */
		count ++;
		if (marker_defined (RETfill_marker, UTF_RETfill_marker)) {
			while (count < XBREAK) {
				putmark (RETfill_marker, UTF_RETfill_marker);
				count ++;
			}
			if (count <= XBREAK) {
				if (marker_defined (RETfini_marker, UTF_RETfini_marker)) {
					putmark (RETfini_marker, UTF_RETfini_marker);
					count ++;
				} else {
					putmark (RETfill_marker, UTF_RETfill_marker);
					count ++;
				}
			}
		}
	    }
	}

#ifdef debug_width
	static FLAG mark_ret = UNSURE;
	if (mark_ret == UNSURE) {
		mark_ret = envvar ("mark_ret") != NIL_PTR;
	}
	if (mark_ret) {
		set_cursor (count + 1, ly);
		putchar ('*');
		count ++;
	}
#endif

	/* Clear the rest of the line if clear_line is True */
	if (clear_line) {
		if (can_erase_chars) {
			if (XBREAK - count + 1 > 0) {
				erase_chars (XBREAK - count + 1);
			}
		} else if (can_clear_eol
			 /* workaround for mlterm bug: */
			 && mlterm_version == 0
			  ) {
			if (count <= XBREAK) {
				clear_eol ();
				set_scrollbar_dirty (ly);
			}
		} else {
			while (count ++ <= XBREAK) {
				/* clear up to scrollbar */
				putchar (' ');
			}
		}
	}
  }
}

/*
 * set_cursor_xy sets the cursor by either directly calling set_cursor
 * or, in the case of proportional font support, reprinting the line
 * up to the x position
 */
void
set_cursor_xy ()
{
  if (proportional) {
	set_cursor (0, y);
	if (x != 0) {
		put_line (y, cur_line, x, False, True);
	}
	/* cur_line may still be undefined if x == 0 */
  } else {
	set_cursor (x, y);
  }
}


/*======================================================================*\
|*			Text screen display				*|
\*======================================================================*/

#define dont_debug_display

/*
 * Display line at screen line y_pos if it lies between y_min and y_max.
 * If it is no text line (end of file), clear screen line.
 */
static
void
display_line_at (y_pos, line, y_min, y_max, first, dirty_only)
  int y_pos;
  int y_min;
  int y_max;
  LINE * line;
  FLAG first;	/* first line being displayed within screen */
  FLAG dirty_only;
{
  /* find line to be displayed */
  line = proceed (line, y_pos - y_min);

  /* skip if dirty_only and not dirty */
  if (dirty_only && line->dirty == False) {
	return;
  }

  if (y_pos >= y_min && y_pos <= y_max) {
	/* clear lines after text, don't postpone this */
	if (line == tail) {
		set_cursor (0, y_pos);
		clear_wholeline ();
		return;
	}

	/* postpone actual display unless dirty_only */
	if (! dirty_only) {
		line->dirty = True;
		return;
	}

#ifdef debug_display
	printf ("display line @ %d\n", y_pos); display_delay = 50;
#endif

	if (first == False) {
		if (display_delay >= 0) {
			flush ();
		}
#ifdef msdos
		if (display_delay > 0) {
			delay (display_delay);
		}
#else
# ifdef use_usleep
		if (display_delay > 0) {
			(void) usleep (1000 * display_delay);
		}
# else
		if (display_delay > 0) {
			(void) char_ready_within (display_delay, NIL_PTR);
		}
# endif
#endif
	}
	print_line (y_pos, line);
  }
}

/*
 * display () shows count + 1 lines on the terminal starting at the given 
   coordinates. At end of file, the rest of the screen is blanked.
   When count is negative, a backwards print from 'line' will be done.
 * display_dirty () refreshes only those lines that are marked dirty.
 */
static
void
do_display (y_coord, line, count, new_pos, dirty_only)
  int y_coord;
  LINE * line;
  int count;
  int new_pos;
  FLAG dirty_only;
{
  int y_max = y_coord + count;
  int y_off;

#ifdef debug_display
  printf ("display %d+%d @ %d (dirty_only %d)\n", y_coord, count, new_pos, dirty_only);
#endif

/* Find new startline if count is negative */
  if (count < 0) {
	line = proceed (line, count);
	count = - count;
  }

#ifdef obsolete
  clean_menus ();
  This led to recursive calls of display () resulting in wrong display;
  menus are now cleared before invoking functions from menu.
#endif

  display_line_at (new_pos, line, y_coord, y_max, True, dirty_only);
  y_off = 0;
  while (y_off <= count) {
	y_off ++;
	if (winchg) {
		return;
	}
	display_line_at (new_pos - y_off, line, y_coord, y_max, False, dirty_only);
	if (winchg) {
		return;
	}
	display_line_at (new_pos + y_off, line, y_coord, y_max, False, dirty_only);
  }
}

static
void
display_dirty (y_coord, line, count, new_pos)
  int y_coord;
  LINE * line;
  int count;
  int new_pos;
{
  do_display (y_coord, line, count, new_pos, True);
}

void
display_flush ()
{
  display_dirty (0, top_line, last_y, y);
}

void
display (y_coord, line, count, new_pos)
  int y_coord;
  LINE * line;
  int count;
  int new_pos;
{
  do_display (y_coord, line, count, new_pos, False);
  text_screen_dirty = False;
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
