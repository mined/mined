/*======================================================================*\
|*		Editor mined						*|
|*		paragraph justification					*|
\*======================================================================*/

#include "mined.h"

#include "charprop.h"


int first_left_margin = 0;
int next_left_margin = 0;
int right_margin = 71;


/*======================================================================*\
|*			Line wrap around				*|
\*======================================================================*/

#define dont_debug_jc
#ifdef debug_jc
#define trace_jc(params)	printf params
#else
#define trace_jc(params)	
#endif

#define dont_debug_justi
#ifdef debug_justi
#define trace_justi(params)	printf params
#else
#define trace_justi(params)	
#endif

#ifdef debug_justi

/*
 * Return string without '\n' for tracing
 */
static char tracebuf [maxLINElen];

static
char *
trcstr (text, poi)
  char * text;
  char * poi;
{
	char * p = text;
	char * outp = tracebuf;

	while (* p != '\0') {
		if (p == poi) {
			* outp ++ = '^';
		}
		if (p == cur_text) {
			* outp ++ = '!';
		}
		if (* p != '\n') {
			* outp ++ = * p;
		}
		p ++;
	}
	* outp = '\0';
	return tracebuf;
}

#endif


/*
 * determine if the next line contains visible text
 */
static
int
nonblank_line_follows ()
{
  LINE * next_line = cur_line->next;
  char * next_char;

  if (next_line == NIL_LINE) return 0;

  next_char = next_line->text;
  if (next_char == NIL_PTR) return 0;

  while (white_space (* next_char)) {
	next_char ++;
  }
  return * next_char != '\n';
}

/*
 * justify justifies the current line according to the current margins
 */
static
void
justi_line (left_margin, jushop, first_line, justi_tabs, space_entered, auto_jus)
  int left_margin;
  int jushop;
  FLAG first_line;
  FLAG justi_tabs;
  int space_entered;	/* was last appended character a space? */
  int auto_jus;		/* is this a case of justify while typing? */
{
  char * poi;
  char * last_blank;
  char * very_last_blank;
  int column;

  poi = cur_line->text;
  column = 0;
/*
old:
  while (column < left_margin && (poi < cur_text || white_space (* poi)))
try:
  while (column < left_margin && (poi != cur_text && white_space (* poi)))
*/
  while (column < left_margin && 
		(poi < cur_text ||
		 (first_line && white_space (* poi))
		)
	)
  {
	if (* poi == '\t') {
		justi_tabs = True;
	}
	advance_char_scr (& poi, & column, cur_line->text);
  }
  trace_justi (("justi (%d, %d) advanced to %d («%s»)\n", left_margin, jushop, column, trcstr (cur_line->text, poi)));
  if (column < left_margin) {
	move_address (poi, y);
	while (column < left_margin) {
		if (justi_tabs && tab (column) <= left_margin) {
			S0 ('\t');
			column = tab (column);
		} else {
			S0 (' ');
			column ++;
		}
	}
	poi = cur_line->text;	/* old text pointer may be invalid */
	column = 0;		/* so start again */
  }
  trace_justi ((" -> advanced to %d («%s»)\n", column, trcstr (cur_line->text, poi)));
  last_blank = NIL_PTR;
  very_last_blank = NIL_PTR;
  while (column < right_margin && * poi != '\n') {
	if (column > left_margin &&
	    white_space (* poi) && ! white_space (* (poi + 1))) {
		trace_justi (("  last blank (%02X) at %d\n", * poi, column));
		if (space_entered) {
			last_blank = very_last_blank;
		} else {
			last_blank = poi;
		}
		very_last_blank = poi;
	}
	advance_char_scr (& poi, & column, cur_line->text);
  }
  if (! auto_jus) {
	last_blank = very_last_blank;
  }

  trace_justi ((" -> advanced to %d («%s»)\n", column, trcstr (cur_line->text, poi)));
  if (* poi != '\n') {
	if (very_last_blank != NIL_PTR) {
		poi = very_last_blank;
		advance_char (& poi);
		move_address (poi, y);
		trace_justi ((" entering newline after last blank\n"));
		S0 ('\n');
		justi_line (next_left_margin, jushop, False, justi_tabs, space_entered, auto_jus);
	} else {	/* no wrapping point found */
		while (! white_space (* poi) && * poi != '\n') {
			advance_char_scr (& poi, & column, cur_line->text);
		}
		if (* poi == '\n') {
			move_address (poi, y);
			trace_justi ((" moving right beyond newline\n"));
			MOVRT ();
		} else {
			advance_char (& poi);
			move_address (poi, y);
			if (* poi != '\n') {
				trace_justi ((" entering newline\n"));
				S0 ('\n');
			} else {
				trace_justi ((" moving right\n"));
				MOVRT ();
			}
			justi_line (next_left_margin, jushop, False, justi_tabs, space_entered, auto_jus);
		}
	}
  } else if (poi - last_blank == 1 ||
	   (jushop > 0 && nonblank_line_follows ()))
  {
  /* continue with next line */
	move_address (poi, y);
	if (poi - last_blank != 1) {
		trace_justi ((" inserting a blank\n"));
		S0 (' ');
	}
	trace_justi ((" concatenating lines\n"));
	/* concatenate lines */
	if (cur_line->next != tail) {
		DCC0 ();
	}
	while (white_space (* cur_text)) {
		/* remove leading space */
		DCC0 ();
	}

	if (* cur_text != '\n') {
		justi_line (next_left_margin, jushop, False, justi_tabs, space_entered, auto_jus);
	} else {
		trace_justi ((" removing trailing space\n"));
		poi = cur_text;
		precede_char (& poi, cur_line->text);
		while (white_space (* poi)) {
			DPC0 ();
			poi = cur_text;
			precede_char (& poi, cur_line->text);
		}
		MOVRT ();
	}
  } else {
	trace_justi ((" moving right finally\n"));
	move_address (poi, y);
	MOVRT ();
  }
}

static
void
justi (left_margin, jushop, justi_tabs, space_entered, auto_jus)
  int left_margin;
  int jushop;
  FLAG justi_tabs;
  int space_entered;	/* was last appended character a space? */
  int auto_jus;		/* is this a case of justify while typing? */
{
  justi_line (left_margin, jushop, True, justi_tabs, space_entered, auto_jus);
}

static
void
justify (justi_tabs)
  FLAG justi_tabs;
{
  int space_entered = white_space (* (cur_text - 1));

  trace_justi (("\njustify ------------------------\n"));
  if (hop_flag > 0) {
	hop_flag = 0;
	justi (first_left_margin, 1 - JUSmode, justi_tabs, space_entered, 0);
  } else {
	justi (first_left_margin, JUSmode, justi_tabs, space_entered, 0);
  }
}

void
JUS ()
{
  justify (False);
}

void
JUSclever ()
{
  char * poi = cur_line->text;
  int column = 0;
  char * ppoi;
  char lastc = '\n';
  int save_first_left = first_left_margin;
  int save_next_left = next_left_margin;
  int save_right = right_margin;
  FLAG javadoc = False;
  FLAG justi_tabs = False;

  /* set left margins to first non-blank in current line */
  while (white_space (* poi)) {
	advance_char_scr (& poi, & column, cur_line->text);
  }
  first_left_margin = column;
  next_left_margin = first_left_margin;
  if (first_left_margin > right_margin - 2) {
	right_margin = first_left_margin + 2;
  }

  /* check end of previous line for blank to see if current line 
     begins a paragraph */
  ppoi = cur_line->prev->text;
  if (ppoi != NIL_PTR) {
	lastc = * ppoi;
	if (lastc != '\n') {
		while (* ppoi != '\n') {
			lastc = * ppoi;
			advance_char (& ppoi);
		}
	}
  }
  if (lastc != ' ') {
	/* current line begins a paragraph */
	FLAG is_itemchar = False;
	FLAG withdigits = False;
	unsigned long unichar = unicodevalue (poi);

	trace_jc (("skip letters from %d\n", column));
	if (  unichar == '@'
	   || (unichar == '/' && * (poi + 1) == '*')
	   || (unichar == 'o' && white_space (* (poi + 1)))
	   || unichar == '*'
	   || unichar == '+'
	   || unichar == '-'
	   || (is_itemchar = is_bullet_or_dash (unichar))
	   )
	{
		javadoc = True;
	}

	while (! is_itemchar &&
		(  (* poi >= '/' && * poi <= '9')
		|| isLetter (unichar)
		)
	      )
	{
		if (* poi >= '0' && * poi <= '9') {
			withdigits = True;
		}
		/* skip letters ... */
		advance_char_scr (& poi, & column, cur_line->text);
		unichar = unicodevalue (poi);
	}

	trace_jc (("check marker at %d\n", column));
	if (javadoc || * poi == ')' || (* poi == '.' && withdigits)) {
		trace_jc (("skip non-space from %d\n", column));
		/* numbered text / listing */
		if (is_itemchar) {
			advance_char_scr (& poi, & column, cur_line->text);
		} else {
			while (! white_space (* poi)) {
				advance_char_scr (& poi, & column, cur_line->text);
			}
		}
		if (* poi == '\t') {
			justi_tabs = True;
		}
		trace_jc (("skip space from %d\n", column));
		while (white_space (* poi)) {
			advance_char_scr (& poi, & column, cur_line->text);
		}
		trace_jc (("arrive at %d\n", column));
		next_left_margin = column;
		if (next_left_margin > right_margin - 2) {
			right_margin = next_left_margin + 2;
		}
	} else {
		/* normal text */
		next_left_margin = first_left_margin;
	}
  }
  trace_jc (("margins %d-%d.\n", first_left_margin, next_left_margin));

  justify (justi_tabs);

  first_left_margin = save_first_left;
  next_left_margin = save_next_left;
  right_margin = save_right;
}

/* JUSandreturn is called by character insertion S* () */
void
JUSandreturn ()
{
  LINE * prevline = cur_line->prev;
  int prev_offset;
  int i;
#ifdef garble
  int not_at_lineend = * cur_text != '\n';
#endif

  char * poi;
  int column;	/* effective visual column of current position */
  int space_entered = white_space (* (cur_text - 1));

  trace_justi (("\nJUSandreturn ------------------------\n"));
  poi = cur_line->text;
  column = 0;
  prev_offset = 0;
  while (poi < cur_text) {
	advance_char_scr (& poi, & column, cur_line->text);
	prev_offset ++;
  }
  if (JUSlevel < 2) {
	if (column <= right_margin) {
		return;
	}
  }

  justi (first_left_margin, JUSmode, False, space_entered, 1);
  move_address (prevline->next->text, find_y (prevline->next));
  for (i = 0; i < prev_offset; i ++) {
	if (* cur_text == '\n') {
		MOVRT ();
		/* skip indentation */
		column = 0;
		poi = cur_text;
		while (white_space (* poi) && column < next_left_margin) {
			advance_char_scr (& poi, & column, cur_line->text);
		}
		if (column > 0) {
			move_address (poi, y);
		}
	}
	MOVRT ();
  }
#ifdef garble
  if (not_at_lineend && * cur_text == '\n') {
	printf (" moving right 1 more\n");
	MOVRT ();
	while (white_space (* cur_text)) {
		printf (" moving right 1 more\n");
		MOVRT ();
	}
  }
#endif
}

static
void
modify_int (name, var, min, max)
  char * name;
  int * var;
  int min;
  int max;
{
  int number;

  build_string (text_buffer, "%s (%d), new value:", name, * var);
  if (get_number (text_buffer, '\0', & number) == ERRORS) {
	return;
  }
  if (number < min) {
	error ("Value too small");
	return;
  }
  if (number > max) {
	error ("Value too large");
	return;
  }
  * var = number;
}

static
void
modify_col (name, var, min, max)
  char * name;
  int * var;
  int min;
  int max;
{
  int number;

  build_string (text_buffer, "%s (%d), new value (Enter for current column):", name, * var);
  if (get_number (text_buffer, '0', & number) == ERRORS) {
	return;
  }
  if (number == 0) {
	number = cur_line->shift_count * SHIFT_SIZE + x + 1;
  }
  if (number < min) {
	error ("Value too small");
	return;
  }
  if (number > max) {
	error ("Value too large");
	return;
  }
  * var = number;
}

void
ADJLM ()
{
	next_left_margin ++;
	modify_col ("both left margins", & next_left_margin, 1, right_margin - 2);
	next_left_margin --;
	first_left_margin = next_left_margin;
}

void
ADJFLM ()
{
	first_left_margin ++;
	modify_col ("first left margin", & first_left_margin, 1, right_margin - 2);
	first_left_margin --;
}

void
ADJNLM ()
{
	next_left_margin ++;
	modify_col ("next left margin", & next_left_margin, 1, right_margin - 2);
	next_left_margin --;
}

void
ADJRM ()
{
	int left = next_left_margin;
	if (first_left_margin > left) {
		left = first_left_margin;
	}

	right_margin --;
	modify_col ("right margin", & right_margin, left + 2, 999);
	right_margin ++;
}

void
ADJPAGELEN ()
{
	modify_int ("assumed lines per page", & lines_per_page, 0, 999);
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
