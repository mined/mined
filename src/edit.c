/*======================================================================*\
|*		Editor mined						*|
|*		Text editing functions					*|
\*======================================================================*/

#include "mined.h"
#include "charprop.h"
#include "textfile.h"	/* default_lineend, code_NL, code_LF */
#include "io.h"
#include "termprop.h"	/* utf8_input */


/*======================================================================*\
|*		Language detection					*|
\*======================================================================*/

#define Turkish (language_tag == 't')		/* locale begins with tr, az */
#define Lithuanian (language_tag == 'l')	/* locale begins with lt */
#define Dutch (language_tag == 'n')		/* locale begins with nl */


/*======================================================================*\
|*		Local function forward declarations			*|
\*======================================================================*/

static void S _((character newchar));


/*======================================================================*\
|*		Global variables					*|
\*======================================================================*/

FLAG redraw_pending = False;	/* was a redraw suppressed in find_y ? */
FLAG suppress_pasting_double_LF = False;	/* filter multiple LF (from Exceed) */


/*======================================================================*\
|*		Local variables						*|
\*======================================================================*/

static character lastchar = '\0';
static FLAG lastspace_smart = False;


/*======================================================================*\
|*		Aux							*|
\*======================================================================*/

int
ishex (c)
  character c;
{
  if (ebcdic_text) {
	return     (c >= 0xF0 && c <= 0xF9)
		|| (c >= 0x81 && c <= 0x86)
		|| (c >= 0xC1 && c <= 0xC6);
  }
  return (c >= '0' && c <= '9')
	|| (c >= 'A' && c <= 'F')
	|| (c >= 'a' && c <= 'f');
}

unsigned int
hexval (c)
  character c;
{
  if (ebcdic_text) {
	if (c >= 0xF0 && c <= 0xF9) {
		return c - 0xF0;
	} else if (c >= 0x81 && c <= 0x86) {
		return c - 0x81 + 10;
	} else if (c >= 0xC1 && c <= 0xC6) {
		return c - 0xC1 + 10;
	} else {
		return 0;
	}
  }
  if (c >= '0' && c <= '9') {
	return c - '0';
  } else if (c >= 'A' && c <= 'F') {
	return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
	return c - 'a' + 10;
  } else {
	return 0;
  }
}

static
FLAG
idfchar (cpoi)
  character * cpoi;
{
  unsigned long unichar = unicodevalue (cpoi);

  if ('0' <= unichar && unichar <= '9') {
	return True;
  } else if (unichar == '_' || unichar == '$') {
	return True;
  } else {
	char * cat = category (unichar);
	return streq (cat, "Letter") || streq (cat, "Mark");
  }
}


/**
   charcode () returns the encoded character value of a Unicode character
 */
static
unsigned long
charcode (code)
  unsigned long code;
{
  if (cjk_text || mapped_text) {
	return encodedchar (code);
  } else if (utf8_text || code < 0x100) {
	return code;
  } else {
	return CHAR_INVALID;
  }
}


/**
   Check whether character is a item bullet.
 */
static
FLAG
isitemchar (unich)
  unsigned long unich;
{
  return is_bullet_or_dash (unich);
}


/**
   Check whether strings starts with a numbering 1.2.3.
 */

static char * lastsubnumberpoi;
static int lastsubnumber;
static char * last2subnumberpoi;
static int last2subnumber;

static
int
numbering (cpoi)
  char * cpoi;
{
  char * cp = cpoi;
  /*int found = 0;*/
  FLAG ok = False;
  /*char post = '\0';*/
  lastsubnumberpoi = NIL_PTR;
  do {
	if (* cp >= '0' && * cp <= '9') {
		int number;
		char * lastnum = cp;
		cp = scan_int (cp, & number);
		/*post = * cp;*/
		ok = False;
		if (* cp == '.') {
			cp ++;
			/*found ++;*/
			ok = True;

			last2subnumberpoi = lastsubnumberpoi;
			last2subnumber = lastsubnumber;
			lastsubnumberpoi = lastnum;
			lastsubnumber = number;
#ifdef with_parenthesized_numbering
#warning auto-increment / undent not implemented for parenthesized numbering
		} else if (* cp == ')') {
			cp ++;
			ok = True;
			break;
#endif
		} else {
			break;
		}
	} else {
		break;
	}
  } while (1);

  if (/*found*/ ok) {
	return cp - cpoi;
  } else {
	return 0;
  }
}


static
FLAG
onlywhitespacespacebefore (l, s)
  LINE * l;
  char * s;
{
  char * cpoi = l->text;
  while (cpoi < s && iswhitespace (unicodevalue (cpoi))) {
	advance_char (& cpoi);
  }
  if (cpoi >= s) {
	return True;
  } else {
	return False;
  }
}

static
FLAG
paragraphending (l, le)
  LINE * l;
  lineend_type le;
{
  if (l == header) {
	return True;
  } else if (le == lineend_PS) {
	return True;
  } else if (le == lineend_LS) {
	return False;
  } else if (JUSmode == 1) {	/* paragraphs end at empty line */
	char * cpoi = l->text;
	while (iswhitespace (unicodevalue (cpoi))) {
		advance_char (& cpoi);
	}
	if (* cpoi == '\n' || * cpoi == '\0') {
		return True;
	} else {
		return False;
	}
  } else {
	if (strstr (l->text, " \n")) {
		return False;
	} else {
		return True;
	}
  }
}


/*======================================================================*\
|*			Scrolling					*|
\*======================================================================*/

/*
 * Perform a forward scroll. It returns ERRORS if we're at the last line of
 * the file.
 */
static
int
forward_scroll (update)
  FLAG update;
{
  if (bot_line->next == tail) {		/* Last line of file. No dice */
	return ERRORS;
  }
  top_line = top_line->next;
  bot_line = bot_line->next;
  cur_line = cur_line->next;
  line_number ++;

/* Perform the scroll on screen */
  if (update) {
	clean_menus ();

	if (MENU) {
		if (can_delete_line) {
			delete_line (0);
		} else {
			scroll_forward ();
			displaymenuline (True);
		}
	} else {
		scroll_forward ();
	}

	scrollbar_scroll_up (0);

	print_line (SCREENMAX, bot_line);
  }

  return FINE;
}

/*
 * Perform a backwards scroll. It returns ERRORS if we're at the first line 
 * of the file. It updates the display completely if update is True. 
 * Otherwise it leaves that to the caller (page up function).
 */
static
int
reverse_scroll (update)
  FLAG update;
{
  if (top_line->prev == header) {
	/* Top of file. Can't scroll */
	return ERRORS;
  }

  if (last_y != SCREENMAX) {	/* Reset last_y if necessary */
	last_y ++;
  } else {
	bot_line = bot_line->prev;	/* Else adjust bot_line */
  }
  top_line = top_line->prev;
  cur_line = cur_line->prev;
  line_number --;

/* Perform the scroll on screen */
  if (update) {
    if (can_add_line || can_scroll_reverse) {
	clean_menus ();
	if (MENU && can_add_line) {
		add_line (0);
	} else {
		set_cursor (0, - MENU);
		scroll_reverse ();
	}
	scrollbar_scroll_down (0);
	set_cursor (0, YMAX);	/* Erase very bottom line */
	clear_lastline ();
	if (MENU && ! can_add_line) {
		displaymenuline (True);
	}
	print_line (0, top_line);
    } else {
	display (0, top_line, last_y, y);
    }
  }

  return FINE;
}


/*======================================================================*\
|*			Text/line insertion and deletion		*|
\*======================================================================*/

static
int
chars_in_line (line)
  LINE * line;
{
  return char_count (line->text) - (line->return_type == lineend_NONE);
}


/*
 * text_bytes_of () returns the number of bytes in the string 'string'
 * up to and excluding the first '\n'.
 */
static
int
text_bytes_of (string)
  register char * string;
{
  register int count = 0;

  if (string != NIL_PTR) {
	while (* string != '\0' && * string != '\n') {
		string ++;
		count ++;
	}
  }
  return count;
}


#define dont_debug_syntax_state

#ifdef debug_syntax_state
#define trace_state(tag)	if (next_state_on || strcmp (#tag, "cancel")) \
			printf (#tag" {%d: %02X} %02X%c->%02X @%s", \
			state_delay, next_state, prev, \
			value_term ? value_term : '.', \
			ret, * s ? s : "NUL\n")
#else
#define trace_state(tag)	
#endif

#define future_state(st, n)	next_state = st; state_delay = n; next_state_at = s + n; next_state_on = beg
static char next_state = 0;
static short state_delay = 0;
static char * next_state_at = NIL_PTR;
static char * next_state_on = NIL_PTR;
static char * next_state_from = NIL_PTR;

/*
 * Determine new syntax status, given previous status and current 
 * text pointer.
 */
char
syntax_state (prev, old, s, beg)
  char prev;
  char old;
  char * s;
  char * beg;
{
  static short prev_space = 1;
  static char value_term = ' ';
  char was_space = prev_space;
  char is_space = white_space (* s);
  char ret = prev;

  /* cancel future state if either of
	– terminating line display ("")
	– displaying different line than before
	– displaying line again, starting before previous position
   */
  if (! * s
	|| beg != next_state_on
	|| s < next_state_from
     ) {
	prev_space = 1;
	trace_state (cancel);
	state_delay = 0;
	next_state_on = NIL_PTR;
	value_term = ' ';
  }
  next_state_from = s;	/* detect repeated display of same line */

  if (state_delay > 0) {
	if (state_delay == 1) {
		state_delay = 0;
		ret = next_state;
		prev = ret;
		trace_state (delayed:);
	} else {
		state_delay --;
		trace_state (delayed--);
		return prev;
	}
  }

#ifdef state_machine
	none	html	comment	jsp	attrib	value
<%=	jsp+val	+j+v	+j+v	%	+j+v	+j+v
<%	jsp	+jsp	+jsp	%	+jsp	+jsp
<!--	comment	%	%	%	%	%
<	html	%	%	%	%	%
%>	%	(%)	%	—jsp	%	(jsp)!—val
-->	%	(%)	—comm	%	%	%
>	%	—h—v—a	%	%	—h!—at	q?%:–h—v—a
q	%	%	%	%	+val	q?—v—a
=	%	+value	%	%	+val—a?	%
 	%	!—at	%	%	!—at	q?%:!–v—a
 ^ 	%	+at	%	%	%	%
#endif

  prev_space = is_space;

  if (is_space) {
	if ((prev & (syntax_scripting | syntax_value)) == syntax_value) {
		if (value_term <= ' ') {
			ret = prev & ~ (syntax_attrib | syntax_value);
		}
	} else if (prev & syntax_HTML) {
		ret = prev & ~ syntax_attrib;
	}
  } else if (* s == '<') {
	if (strncmp (s, "<!--", 4) == 0) {
		if (! prev) {
			ret = prev | syntax_comment;
		}
	} else if (strncmp (s, "<%=", 3) == 0 || strncmp (s, "<%:", 3) == 0) {
		if (! (prev & syntax_scripting)) {
			future_state (prev | syntax_JSP | syntax_value, 3);
			ret = prev | syntax_JSP;
		}
	} else if (strncmp (s, "<%", 2) == 0) {
		if (! (prev & syntax_scripting)) {
			ret = prev | syntax_JSP;
		}
	} else if (strncmp (s, "<?", 2) == 0) {
		if (! (prev & syntax_scripting)) {
			ret = prev | syntax_PHP;
		}
	} else {
		if (! prev && (* (s + 1) > '@' || * (s + 1) == '/')) {
			ret = syntax_HTML;
		}
	}
#ifdef state_machine
	none	html	comment	jsp	attrib	value
%>	%	(%)	%	—jsp	%	(jsp)!—val
-->	%	(%)	—comm	%	%	%
>	%	—h—v—a	%	%	—h!—at	q?%:–h—v—a
q	%	%	%	%	+v?	q?—v—a
=	%	+value	%	%	+v!—a?	%
#endif
  } else if (strncmp (s, "-->", 3) == 0) {
	if (prev & syntax_comment) {
		future_state (prev & ~ syntax_comment, 3);
	}
  } else if (strncmp (s, "%>", 2) == 0) {
	if ((prev & (syntax_JSP | syntax_value)) == (syntax_JSP | syntax_value)) {
		future_state (prev & ~ (syntax_JSP | syntax_value), 2);
		ret = prev & ~ syntax_value;
	} else if (prev & syntax_JSP) {
		future_state (prev & ~ syntax_JSP, 2);
	}
  } else if (strncmp (s, "?>", 2) == 0) {
	if (prev & syntax_PHP) {
		future_state (prev & ~ syntax_PHP, 2);
	}
  } else if (* s == '>') {
	if (prev & syntax_scripting) {
		/* skip */
	} else if (prev & syntax_value) {
		if (value_term <= ' ') {
			future_state (prev & ~ (syntax_HTML | syntax_attrib | syntax_value), 1);
			ret = prev & ~ (syntax_attrib | syntax_value);
		}
	} else if (prev & syntax_attrib) {
		future_state (prev & ~ (syntax_HTML | syntax_attrib), 1);
		ret = prev & ~ syntax_attrib;
	} else if (prev & syntax_HTML) {
		future_state (prev & ~ syntax_HTML, 1);
	}
  } else if (* s == '"' || * s == '\'') {
	if (prev & syntax_value) {
		if (! value_term) {
			value_term = * s;
		} else if (* s == value_term) {
			future_state (prev & ~ (syntax_attrib | syntax_value), 1);
		}
	}
  } else if ((prev & syntax_attrib) && * s == '=') {
	if (! (prev & syntax_value)) {
		future_state ((prev & ~ syntax_attrib) | syntax_value, 1);
		value_term = '\0';
		ret = prev & ~ syntax_attrib;
	}
  } else if (was_space) {
	if (prev & syntax_scripting) {
		/* skip */
	} else if ((prev & syntax_HTML) && ! (prev & syntax_value)) {
		ret = prev | syntax_attrib;
	}
  } else if ((prev & syntax_value) && ! value_term) {
	value_term = ' ';
  }

  trace_state (state);
  return ret;
}

/*
 * Determine status of HTML on this line:
 *	line->syntax_mask is a bitmask indicating in which kinds of 
 *	syntax constructs the line ends
 * If changing, continue with subsequent lines.
 */
void
update_syntax_state (line)
  LINE * line;
{
  char * lpoi = line->text;
  character syntax_mask = line->prev->syntax_mask;	/* state at line begin */
  character prev_syntax_mask = line->syntax_mask;	/* previous state of line */
  character syntax_mask_old = syntax_none;
  character syntax_mask_new = syntax_mask;

  if (mark_HTML == False &&
	prev_syntax_mask == syntax_unknown && syntax_mask == syntax_unknown) {
	return;
  }

  while (* lpoi != '\0') {
	syntax_mask_new = syntax_state (syntax_mask_new, syntax_mask_old, lpoi, line->text);
	syntax_mask_old = syntax_mask;
	syntax_mask = syntax_mask_new;
	advance_char (& lpoi);
  }
  line->syntax_mask = syntax_mask;
#ifdef debug_syntax_state
  printf ("update_syntax_state %02X..%02X (was %02X) @%s", line->prev->syntax_mask, syntax_mask, prev_syntax_mask, line->text);
#endif
  line->dirty = True;
  if (syntax_mask != prev_syntax_mask && line->next != tail) {
#ifdef debug_syntax_state
  printf ("...update_syntax_state @%s", line->next->text);
#endif
	update_syntax_state (line->next);
  }
}

#define dont_debug_out_of_memory 5

/*
 * make_line installs the buffer into a LINE structure.
 * It returns a pointer to the allocated structure.
   If length < 0, it returns a static emergency line (to be used after 
   out of memory).
 */
static
LINE *
make_line (buffer, length, return_type)
  char * buffer;
  int length;
  lineend_type return_type;
{
  LINE * new_line;
  static LINE emergency_line;
  static char emergency_text [2];

#ifdef debug_out_of_memory
  static int count_make_line = 0;
  if (length >= 0 && ++ count_make_line > debug_out_of_memory) {
	return NIL_LINE;
  }
#endif

  if (length >= 0) {
	new_line = alloc_header ();
  } else {
	new_line = & emergency_line;
  }

  if (new_line == NIL_LINE) {
	ring_bell ();
	error ("Cannot allocate more memory for new line header");
	return NIL_LINE;
  } else {
	if (length >= 0) {
		new_line->text = alloc (length + 1);
	} else {
		new_line->text = emergency_text;
	}
	if (new_line->text == NIL_PTR) {
		ring_bell ();
		error ("Cannot allocate more memory for new line");
		return NIL_LINE;
	} else {
		new_line->shift_count = 0;
		new_line->return_type = return_type;
		if (length >= 0) {
			new_line->allocated = True;
			strncpy (new_line->text, buffer, length);
			new_line->text [length] = '\0';
		} else {
			new_line->allocated = False;
			strncpy (new_line->text, "\n", 2);
		}
		new_line->syntax_mask = syntax_unknown;
		new_line->sel_begin = NIL_PTR;
		new_line->sel_end = NIL_PTR;
		new_line->dirty = False;
		return new_line;
	}
  }
}

/*
 * Line_insert_after () inserts a new line with text pointed to by 'string',
   after the given line.
   It returns the address of the new line which is appended to the previous.
 */
LINE *
line_insert_after (line, string, len, return_type)
  register LINE * line;
  char * string;
  int len;
  lineend_type return_type;
{
  register LINE * new_line;

/* Allocate space for LINE structure and text */
  new_line = make_line (string, len, return_type);

  if (new_line != NIL_LINE) {
/* Install the line into the double linked list */
	new_line->prev = line;
	new_line->next = line->next;
	line->next = new_line;
	new_line->next->prev = new_line;

/* Adjust information about text attribute state (HTML marker) */
	update_syntax_state (new_line);

/* Increment total_lines */
	total_lines ++;
	if (total_chars >= 0) {
		total_chars += chars_in_line (new_line);
	}
  }
  return new_line;
}

/*
 * Insert_text () inserts the string 'string' at the given line and location.
   Do not pass a string with an embedded (non-terminating) newline!
   Make sure cur_text is properly reset afterwards! (may be left undefined)
 */
int
insert_text (line, location, string)
  register LINE * line;
  char * location;
  char * string;
{
  register char * bufp = text_buffer;	/* Buffer for building line */
  register char * textp = line->text;
  char * newbuf;
  int old_line_chars = chars_in_line (line);

  if (dont_modify ()) {
	return ERRORS;
  }

  if (length_of (textp) + text_bytes_of (string) >= maxLINElen) {
	error ("Cannot insert properly: Line too long");
	return ERRORS;
  }

/* Copy part of line until 'location' has been reached */
  while (textp != location) {
	* bufp ++ = * textp ++;
  }

/* Insert string at this location */
  while (* string != '\0') {
	* bufp ++ = * string ++;
  }
  * bufp = '\0';

/* First, allocate memory for next line contents to make sure the */
/* operation succeeds or fails as a whole */
  newbuf = alloc (length_of (text_buffer) + length_of (location) + 1);
  if (newbuf == NIL_PTR) {
	ring_bell ();
	status_fmt2 ("Out of memory - ", "Insertion failed");
	return ERRORS;
  } else { /* Install the new text in this line */
	if (* (string - 1) == '\n') {		/* Insert a new line */
		lineend_type new_return_type;
		lineend_type old_return_type = line->return_type;
		if ((keyshift & altctrl_mask) == altctrl_mask) {
			if (keyshift & shift_mask) {
				line->return_type = lineend_CR;
			} else if (default_lineend == lineend_CRLF) {
				line->return_type = lineend_LF;
			} else {
				line->return_type = lineend_CRLF;
			}
			new_return_type = old_return_type;
		} else if (utf8_lineends
			   && ((keyshift & ctrlshift_mask) || (hop_flag > 0)))
		{
			if (ebcdic_text || ebcdic_file) {
				if (keyshift & ctrl_mask) {
					line->return_type = lineend_LF;
				} else {
					line->return_type = lineend_NL1;
				}
			} else if (utf8_text
#ifdef support_gb18030_all_line_ends
				   || (cjk_text && text_encoding_tag == 'G')
#endif
				  ) {
				if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
					/* ISO 8859 NEXT LINE */
					line->return_type = lineend_NL2;
				} else if (keyshift & ctrl_mask) {
					/* Unicode line separator */
					line->return_type = lineend_LS;
				} else {
					/* Unicode paragraph separator */
					line->return_type = lineend_PS;
				}
			} else if (! cjk_text && ! no_char (encodedchar (0x0085))) {
				if (keyshift & ctrl_mask) {
					/* ISO 8859 NEXT LINE */
					line->return_type = lineend_NL1;
				}
			}
			new_return_type = old_return_type;
		} else if (old_return_type == lineend_NUL || 
		    old_return_type == lineend_NONE)
		{
		/*	line->return_type = top_line->return_type;	*/
			line->return_type = default_lineend;
			new_return_type = old_return_type;
		} else if (old_return_type == lineend_LS ||
			   old_return_type == lineend_PS)
		{
			if (hop_flag > 0) {
				line->return_type = lineend_PS;
			} else {
				line->return_type = lineend_LS;
			}
			new_return_type = old_return_type;
		} else {
			new_return_type = old_return_type;
		}

		clear_highlight_selection ();	/* before insertion! */
		/* this will initially cause update_syntax_state to 
		   proceed based on old contents;
		   we could try to avoid that here but it will be 
		   fixed soon after anyway
		 */
		if (line_insert_after (line, location, length_of (location), 
					new_return_type) == NIL_LINE)
		{
			/* restore return type of line */
			line->return_type = old_return_type;

			ring_bell ();
			status_fmt2 ("Out of memory for new line - ", "Insertion failed");
			return ERRORS;
		}
		set_modified ();	/* after successful insertion! */
	} else {
		/* Append last part of line to text_buffer */
		copy_string (bufp, location);
	}

	free_space (line->text);
	set_modified ();
	line->text = newbuf;
	copy_string (line->text, text_buffer);
	update_syntax_state (line);
	if (total_chars >= 0) {
		total_chars = total_chars + chars_in_line (line) - old_line_chars;
	}
	return FINE;
  }
}

/*
 * Line_delete () deletes the argument line out of the line list.
 * The pointer to the next line is returned.
 */
static
LINE *
line_delete (line)
  register LINE * line;
{
  register LINE * next_line = line->next;

/* Decrement total_lines */
  total_lines --;
  if (total_chars >= 0) {
	total_chars -= chars_in_line (line);
  }

  line->prev->return_type = line->return_type;

/* Delete the line */
  line->prev->next = line->next;
  line->next->prev = line->prev;

/* Free allocated space */
  free_space (line->text);
  free_header (line);

  return next_line;
}


/*
 * delete_text () deletes all the characters (including newlines) between 
   startposition and endposition and fixes the screen accordingly;
   it displays the number of lines deleted.
 * delete_text_only () deletes but does not update the screen.
   Parameters start_line/start_textp and end_line/end_textp 
   must be in the correct order!
 */
static
int
do_delete_text (start_line, start_textp, end_line, end_textp, update_screen)
  LINE * start_line;
  char * start_textp;
  LINE * end_line;
  char * end_textp;
  FLAG update_screen;
{
  register char * textp = start_line->text;
  register char * bufp = text_buffer;	/* Storage for new line->text */
  LINE * line;
  LINE * after_end = end_line->next;
  int line_cnt = 0;			/* Nr of lines deleted */
  int count = 0;
  int old_last_y;
  int nx = x;
  int ret = FINE;
  char * newbuf;
  int newpos_offset = start_textp - textp;
  FLAG isdeleting_lastcombining = False;
  int redraw_cols = 0;
  int old_line_chars = chars_in_line (start_line);

  if (dont_modify ()) {
	return ret;
  }

  set_modified ();	/* File will have been modified */

  if (combining_mode && encoding_has_combining ()) {
	unsigned long unichar = unicodevalue (start_textp);
	if (iscombined (unichar, start_textp, start_line->text)) {
		char * cp = start_textp;
		advance_char (& cp);
		unichar = unicodevalue (cp);
		if (! iscombining_unichar (unichar)) {
			isdeleting_lastcombining = True;
			cp = start_textp;
			do {
				precede_char (& cp, start_line->text);
				unichar = unicodevalue (cp);
			} while (cp != start_line->text && iscombining_unichar (unichar));
			if (unichar == '\t') {
				redraw_cols = 0;
			} else if (iswide (unichar)) {
				redraw_cols = 2;
			} else {
				redraw_cols = 1;
			}
		}
	}
  }

/* Set up new line. Copy first part of start line until start_position. */
  while (textp < start_textp) {
	* bufp ++ = * textp ++;
	count ++;
  }

/* Check if line doesn't exceed maxLINElen */
  if (count + length_of (end_textp) >= maxLINElen) {
	error ("Cannot delete properly: Remaining line too long");
	return ret;
  }

/* Copy last part of end_line if end_line is not tail */
  copy_string (bufp, (end_textp != NIL_PTR) ? end_textp : "\n");

/* Delete all lines between start and end_position (including end_line) */
  line = start_line->next;
  while (line != after_end && line != tail) {
	/* Here, the original mined compared with end_line->next which has 
	   already been discarded when the comparison should become true.
	   This severe error remained undetected until I ported to MSDOS */
	line = line_delete (line);
	line_cnt ++;
  }

/* Check if last line of file should be deleted */
  if (end_textp == NIL_PTR && length_of (start_line->text) == 1 && total_lines > 1) {
	start_line = start_line->prev;
	(void) line_delete (start_line->next);
	line_cnt ++;
  } else {	/* Install new text */
	newbuf = alloc (length_of (text_buffer) + 1);
	if (newbuf == NIL_PTR) {
		ring_bell ();
		error ("No more memory after deletion");
		ret = ERRORS;
	} else {
		free_space (start_line->text);
		start_line->text = newbuf;
		copy_string (start_line->text, text_buffer);
		update_syntax_state (start_line);
	}
  }

  if (total_chars >= 0) {
	total_chars = total_chars + chars_in_line (start_line) - old_line_chars;
  }

  if (! update_screen) {
	return ret;
  }

/* Update screen */
  if (line_cnt == 0) {		/* Check if only one line changed */
	/* fix position */
	move_address (cur_line->text + newpos_offset, y);
	/* display last part of line */
	if (isdeleting_lastcombining) {
		if (redraw_cols == 0 || proportional) {
			print_line (y, start_line);
		} else {
			set_cursor (x - redraw_cols, y);
			put_line (y, start_line, x - redraw_cols, True, False);
		}
	} else {
		put_line (y, start_line, x, True, False);
	}

	set_cursor_xy ();
	return ret;
  }

  old_last_y = last_y;
  reset (top_line, y);
  if ((line_cnt <= SCREENMAX - y) && can_delete_line) {
	clear_status ();
	display (y, start_line, 0, y);
	line = proceed (start_line, SCREENMAX - y - line_cnt + 1);
	while (line_cnt -- > 0) {
		delete_line (y + 1);
		scrollbar_scroll_up (y + 1);
		if (line != tail) {
			print_line (SCREENMAX, line);
			line = line->next;
		}
	}
  } else {
	display (y, start_line, old_last_y - y, y);
  }
  move_to (nx, y);
  return ret;
}

int
delete_text (start_line, start_textp, end_line, end_textp)
  LINE * start_line;
  char * start_textp;
  LINE * end_line;
  char * end_textp;
{
  return do_delete_text (start_line, start_textp, end_line, end_textp, True);
}

int
delete_text_only (start_line, start_textp, end_line, end_textp)
  LINE * start_line;
  char * start_textp;
  LINE * end_line;
  char * end_textp;
{
  return do_delete_text (start_line, start_textp, end_line, end_textp, False);
}


/*======================================================================*\
|*			Move commands					*|
\*======================================================================*/

/*
 * Move left one Unicode character (may enter into combined character).
 */
static
void
ctrl_MLF ()
{
  char * curpoi;

  if (hop_flag > 0) {
	MOVLBEG ();
  } else if (cur_text == cur_line->text) {/* Begin of line */
	if (cur_line->prev != header) {
		MOVUP ();			/* Move one line up */
		move_to (LINE_END, y);
	}
  } else {
	curpoi = cur_text;
	precede_char (& curpoi, cur_line->text);
	move_address (curpoi, y);
  }
}

/*
 * Move left one position.
 */
void
MOVLF ()
{
  if (x == 0 && cur_line->shift_count == 0) {
	/* Begin of line */
	if (cur_line->prev != header) {
		MOVUP ();			/* Move one line up */
		move_to (LINE_END, y);
	}
  } else {
	move_to (x - 1, y);
  }
}

void
LFkey ()
{
  if (apply_shift_selection) {
	trigger_highlight_selection ();

	if (keyshift & ctrl_mask) {
		keyshift = 0;
		MPW ();
		return;
	}
  }

  if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
	keyshift = 0;
	BLINEORUP ();
	return;
  }
  if (keyshift & ctrl_mask) {
	keyshift = 0;
	ctrl_MLF ();
	return;
  }
  if (keyshift & shift_mask) {
	keyshift = 0;
	MPW ();
	return;
  }

  if (hop_flag > 0) {
	MOVLBEG ();
  } else {
	MOVLF ();
  }
}

/*
 * Move right one Unicode character (may enter into combined character).
 */
static
void
ctrl_MRT ()
{
  char * curpoi;

  if (hop_flag > 0) {
	MOVLEND ();
  } else if (* cur_text == '\n') {
	if (cur_line->next != tail) {		/* Last char of file */
		MOVDN ();			/* Move one line down */
		move_to (LINE_START, y);
	}
  } else {
	curpoi = cur_text;
	advance_char (& curpoi);
	move_address (curpoi, y);
  }
}

/*
 * Move right one position.
 */
void
MOVRT ()
{
  if (* cur_text == '\n') {
	if (cur_line->next != tail) {		/* Last char of file */
		MOVDN ();			/* Move one line down */
		move_to (LINE_START, y);
	}
  } else {
	move_to (x + 1, y);
  }
}

void
RTkey ()
{
  if (apply_shift_selection) {
	trigger_highlight_selection ();

	if (keyshift & ctrl_mask) {
		keyshift = 0;
		MNW ();
		return;
	}
  }

  if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
	keyshift = 0;
	ELINEORDN ();
	return;
  }
  if (keyshift & ctrl_mask) {
	keyshift = 0;
	ctrl_MRT ();
	return;
  }
  if (keyshift & shift_mask) {
	keyshift = 0;
	MNW ();
	return;
  }

  if (hop_flag > 0) {
	MOVLEND ();
  } else {
	MOVRT ();
  }
}

/*
 * Move one line up.
 */
void
MOVUP ()
{
  if (y == 0) {			/* Top line of screen. Scroll one line */
	if (reverse_scroll (True) != ERRORS) {
		move_y (y);
	}
  } else {			/* Move to previous line */
	move_y (y - 1);
  }
}

void
UPkey ()
{
  if (apply_shift_selection) {
	trigger_highlight_selection ();

	if (keyshift & ctrl_mask) {
		keyshift = 0 | shift_mask;
		MPPARA ();	/* was BLINEORUP (); */
		return;
	}
  }

  if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
	keyshift = 0;
	MPPARA ();
	return;
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	HIGH ();
	return;
  }

  if (hop_flag > 0) {
	HIGH ();
  } else {
	MOVUP ();
  }
}

/*
 * Move one line down.
 */
void
MOVDN ()
{
  if (y == last_y) {		/* Last line of screen. Scroll one line */
	if (bot_line->next == tail && bot_line->text [0] != '\n') {
		return;
	} else {
		(void) forward_scroll (True);
		move_y (y);
	}
  } else {			/* Move to next line */
	move_y (y + 1);
  }
}

void
DNkey ()
{
  if (apply_shift_selection) {
	trigger_highlight_selection ();

	if (keyshift & ctrl_mask) {
		keyshift = 0 | shift_mask;
		MNPARA ();	/* was ELINEORDN (); MS Word: MOVDN and MOVLBEG */
		return;
	}
  }

  if ((keyshift & ctrlshift_mask) == ctrlshift_mask) {
	keyshift = 0;
	MNPARA ();
	return;
  } else if (keyshift & shift_mask) {
	keyshift = 0;
	LOW ();
	return;
  }

  if (hop_flag > 0) {
	LOW ();
  } else {
	MOVDN ();
  }
}

/*
 * Move to top of screen
 */
void
HIGH ()
{
  move_y (0);
}

/*
 * Move to bottom of screen
 */
void
LOW ()
{
  move_y (last_y);
}

/*
 * Move to begin of current line.
 */
void
MOVLBEG ()
{
  move_to (LINE_START, y);
}

/*
 * Move to begin of (current or previous) line.
 */
void
BLINEORUP ()
{
  if (cur_text == cur_line->text) {
	/* already at line beginning, move to previous line */
	MOVUP ();
  }
  MOVLBEG ();
}

/*
 * Move to end of current line.
 */
void
MOVLEND ()
{
  move_to (LINE_END, y);
}

/*
 * Move to end of (current or next) line.
 */
void
ELINEORDN ()
{
  if (* cur_text == '\n') {
	/* already at line end, move to end of next line */
	MOVDN ();
  }
  MOVLEND ();
}

/*
 * GOTO () prompts for a linenumber and moves to that line.
 * GOHOP () prompts (for number or command) with a delay.
 */
void
goline (number)
  int number;
{
  LINE * line;
  if (number <= 0 || (line = proceed (header->next, number - 1)) == tail) {
	error2 ("Invalid line number: ", dec_out ((long) number));
  } else {
	Pushmark ();
	clear_status ();
	move_y (find_y (line));
  }
}

void
goproz (number)
  int number;
{
  goline ((long) (total_lines - 1) * number / 100 + 1);
}

static
void
go_or_hop (always_prompt)
  FLAG always_prompt;
{
  unsigned long c;
  int number;

  if (MENU) {
	hop_flag = 1;
	displayflags ();
	set_cursor_xy ();
	flush ();
	hop_flag = 0;
  }
  if (always_prompt || ! char_ready_within (500, NIL_PTR)) {
	status_uni ("HOP/Go: type command (to amplify/expand) or number (to go to ...) ...");
  }
  if (quit) {
	return;
  }
  c = readcharacter_unicode ();
  if (quit) { /* don't quit on ESC, allow ^G ESC ... */
	clear_status ();
	return;
  }

  if ('0' <= c && c <= '9') {
	int end;
	if (lines_per_page > 0) {
		end = get_number ("...number [% | p(age | m(ark | g(o marker | f(ile #]", c, & number);
	} else {
		end = get_number ("...number [% | m(ark | g(o marker | f(ile #]", c, & number);
	}

	if (! visselect_keeponsearch) {
		clear_highlight_selection ();
	}

	if (end == '%') {
		goproz (number);
	} else if (end == 'm' || end == 'M' || end == ',') {
		MARKn (number);
	} else if (end == '\'' || end == '.' || end == 'g' || end == 'G') {
		GOMAn (number);
	} else if (end == 'f' || end == 'F' || end == '#') {
		edit_nth_file (number);
	} else if (lines_per_page > 0 && (end == 'p' || end == 'P') && number > 0) {
		goline (number * lines_per_page - lines_per_page + 1);
	} else if (end != ERRORS) {
		goline (number);
	}
	return;
  } else {
	clear_status ();
	hop_flag = 1;
	invoke_key_function (c);
	return;
  }
}

void
GOTO ()
{
  go_or_hop (True);
}

void
GOHOP ()
{
  go_or_hop (False);
}


/*
 * Scroll forward one page or to eof, whatever comes first. (Bot_line becomes
 * top_line of display.) Try to leave the cursor on the same line. If this is
 * not possible, leave cursor on the line halfway the page.
 */
void
MOVPD ()
{
  int i;
  int new_y;

  for (i = 0; i < SCREENMAX; i ++) {
	if (forward_scroll (page_scroll) == ERRORS) {
		break;			/* EOF reached */
	}
  }

  if (y - i < 0) {			/* Line no longer on screen */
	new_y = page_stay ? 0 : SCREENMAX >> 1;
  } else {
	new_y = y - i;
  }

  if (page_scroll == False) {
	display (0, top_line, last_y, new_y);
  } else {
	(void) display_scrollbar (False);
	if (MENU && ! can_delete_line) {
		displaymenuline (True);
	}
  }

  move_y (new_y);
}

void
PGDNkey ()
{
  if (apply_shift_selection) {
	trigger_highlight_selection ();
  }

  if (keyshift & ctrl_mask) {
	keyshift = 0;
	SD ();
	return;
  }

  if (hop_flag > 0) {
	hop_flag = 0;
	EFILE ();
  } else {
	MOVPD ();
  }
}

/*
 * Scroll backwards one page or to top of file, whatever comes first.
 * (Top_line becomes bot_line of display).
 * The very bottom line (YMAX) is always blank.
 * Try to leave the cursor on the same line.
 * If this is not possible, leave cursor on the line halfway the page.
 */
void
MOVPU ()
{
  int i;
  int new_y;

  for (i = 0; i < SCREENMAX; i ++) {
	if (reverse_scroll (page_scroll) == ERRORS) {
		/* should also flag reverse_scroll that clearing of 
		   bottom line is not desired */
		break;			/* Top of file reached */
	}
  }

  if (y + i > SCREENMAX) {		/* line no longer on screen */
	new_y = page_stay ? last_y : SCREENMAX >> 1;
  } else {
	new_y = y + i;
  }

  if (can_scroll_reverse && page_scroll) {
	set_cursor (0, YMAX);	/* Erase very bottom line */
	clear_lastline ();
	(void) display_scrollbar (False);
  } else {
	display (0, top_line, last_y, new_y);
  }

  move_y (new_y);
}

void
PGUPkey ()
{
  if (apply_shift_selection) {
	trigger_highlight_selection ();
  }

  if (keyshift & ctrl_mask) {
	keyshift = 0;
	SU ();
	return;
  }

  if (hop_flag > 0) {
	hop_flag = 0;
	BFILE ();
  } else {
	MOVPU ();
  }
}

/*
 * Go to top of file, scrolling if possible, else redrawing screen.
 */
void
BFILE ()
{
  Pushmark ();

  if (proceed (top_line, - SCREENMAX) == header) {
	MOVPU ();			/* It fits; let MOVPU do it */
  } else {
	reset (header->next, 0);	/* Reset top_line, etc. */
	RD_y (0);			/* Display full page */
  }
  move_to (LINE_START, 0);
}

/*
 * Go to last position of text, scrolling if possible, else redrawing screen
 */
void
EFILE ()
{
  Pushmark ();

  if (proceed (bot_line, SCREENMAX) == tail) {
	MOVPD ();		/* It fits; let MOVPD do it */
  } else {
	reset (proceed (tail->prev, - SCREENMAX), SCREENMAX);
	RD_y (last_y);		/* Display full page */
  }
  move_to (LINE_END, last_y);
}

/*
 * Scroll one line up. Leave the cursor on the same line (if possible).
 */
void
SU ()
{
  register int i;

  if (hop_flag > 0) {
	hop_flag = 0;
	for (i = 0; i < (SCREENMAX >> 1); i ++) {
		if (i > 0 && disp_scrollbar) {
			(void) display_scrollbar (True);
		}
		SU ();
	}
	return;
  }

  if (reverse_scroll (True) != ERRORS) {	/* else we are at top of file */
	move_y ((y == SCREENMAX) ? SCREENMAX : y + 1);
  }
}

/*
 * Scroll one line down. Leave the cursor on the same line (if possible).
 */
void
SD ()
{
  register int i;

  if (hop_flag > 0) {
	hop_flag = 0;
	for (i = 0; i < (SCREENMAX >> 1); i ++) {
		if (i > 0 && disp_scrollbar) {
			(void) display_scrollbar (True);
		}
		SD ();
	}
	return;
  }

  if (forward_scroll (True) != ERRORS) {
	move_y ((y == 0) ? 0 : y - 1);
  }
}


/*----------------------------------------------------------------------*\
	Contents-dependent moves
\*----------------------------------------------------------------------*/

/*
 * A word was previously defined as a number of non-blank characters
 * separated by tabs, spaces or linefeeds.
 * By consulting idfchar (), sequences of real letters only or digits
 * or underlines are recognized as words.
 */

/*
 * BSEN () and ESEN () look for the beginning or end of the current sentence.
 */
void
BSEN ()
{
  search_for ("[;.]", REVERSE, False);
}

void
ESEN ()
{
  search_for ("[;.]", FORWARD, False);
}

/*
 * MNPARA () and MPPARA () look for end or beginning of current paragraph.
 */
void
MNPARA ()
{
  do {
	if (cur_line->next == tail) {
		MOVLEND ();
		break;
	}
	if (JUSmode == 0 && cur_line->text [strlen (cur_line->text) - 2] != ' ') {
		MOVDN ();
		MOVLBEG ();
		break;
	}
	MOVDN ();
	if (JUSmode == 1 && * (cur_line->text) == '\n') {
		break;
	}
  } while (True);
}

void
MPPARA ()
{
  if (JUSmode == 0 && cur_text == cur_line->text) {
	/* prevent sticking if already at paragraph beginning */
	MOVUP ();
  }
  do {
	if (cur_line->prev == header) {
		MOVLBEG ();
		break;
	}
	if (JUSmode == 0 && cur_line->prev->text [strlen (cur_line->prev->text) - 2] != ' ') {
		MOVLBEG ();
		break;
	}
	MOVUP ();
	if (JUSmode == 1 && * (cur_line->text) == '\n') {
		break;
	}
  } while (True);
}

static
void
search_tag (poi)
  char * poi;
{
  char pat [maxPROMPTlen];
  FLAG direction = FORWARD;
  char * patpoi = & pat [3];

  strcpy (pat, "</*");
  poi ++;
  if (* poi == '/') {
	direction = REVERSE;
	poi ++;
  }
  while (! white_space (* poi) && * poi != '\n' && * poi != '>' && * poi != '\0') {
	* patpoi = * poi;
	patpoi ++;
	poi ++;
  }
  * patpoi = '\0';

  search_corresponding (pat, direction, "/");
}

/**
  SCORR () looks for a corresponding bracket, HTML tag, 
  or looks for the next/previous MIME separator or mail header
 */
void
SCORR (pref_direction)
  FLAG pref_direction;
{
  char * poi;
  unsigned int cv = charvalue (cur_text);
  char * errmsg;

  if (hop_flag > 0) {
	hop_flag = 0;
	search_wrong_enc ();
	return;
  }

  switch (cv) {
    case '(':	search_corresponding ("[()]", FORWARD, ")");
		return;
    case ')':	search_corresponding ("[()]", REVERSE, "(");
		return;
    case '[':	search_corresponding ("[\\[\\]]", FORWARD, "]");
		return;
    case ']':	search_corresponding ("[\\[\\]]", REVERSE, "[");
		return;
    case '{':	search_corresponding ("[{}]", FORWARD, "}");
		return;
    case '}':	search_corresponding ("[{}]", REVERSE, "{");
		return;
    case (character) '�': /* « LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
		if (utf8_text) {
			search_corresponding ("[«»]", FORWARD, "»");
		} else {
			search_corresponding ("[��]", FORWARD, "�");
		}
		return;
    case (character) '�': /* » RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
		if (utf8_text) {
			search_corresponding ("[«»]", REVERSE, "«");
		} else {
			search_corresponding ("[��]", REVERSE, "�");
		}
		return;
    case 0x2039:	/* ‹ SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
		search_corresponding ("[‹›]", FORWARD, "›");
		return;
    case 0x203A:	/* › SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
		search_corresponding ("[‹›]", REVERSE, "‹");
		return;
    case 0x2045:	/* ⁅ LEFT SQUARE BRACKET WITH QUILL */
		search_corresponding ("[⁅⁆]", FORWARD, "⁆");
		return;
    case 0x2046:	/* ⁆ RIGHT SQUARE BRACKET WITH QUILL */
		search_corresponding ("[⁅⁆]", REVERSE, "⁅");
		return;
    case 0x207D:	/* ⁽ SUPERSCRIPT LEFT PARENTHESIS */
		search_corresponding ("[⁽⁾]", FORWARD, "⁾");
		return;
    case 0x207E:	/* ⁾ SUPERSCRIPT RIGHT PARENTHESIS */
		search_corresponding ("[⁽⁾]", REVERSE, "⁽");
		return;
    case 0x208D:	/* ₍ SUBSCRIPT LEFT PARENTHESIS */
		search_corresponding ("[₍₎]", FORWARD, "₎");
		return;
    case 0x208E:	/* ₎ SUBSCRIPT RIGHT PARENTHESIS */
		search_corresponding ("[₍₎]", REVERSE, "₍");
		return;
    default:	if (mark_HTML) {
			poi = cur_text;
			if (* poi == '>') {
				MOVLF ();
			}
			while (poi != cur_line->text && * poi != '<') {
				precede_char (& poi, cur_line->text);
			}
			if (* poi == '<') {
				search_tag (poi);
				return;
			} else {
				errmsg = "No bracket or tag to match";
			}
		} else {
			if (* cur_text == '<') {
				search_corresponding ("[<>]", FORWARD, ">");
				return;
			} else if (* cur_text == '>') {
				search_corresponding ("[<>]", REVERSE, "<");
				return;
			} else {
				errmsg = "No bracket to match";
			}
		}
  }

  if (pref_direction != REVERSE) {
	pref_direction = FORWARD;
  }

  if ((cv == '/' && * (cur_text + 1) == '*')
      ||
      (cv == '*' && cur_text != cur_line->text && * (cur_text - 1) == '/')
     ) {
	search_for ("\\*/", FORWARD, False);
  } else if ((cv == '*' && * (cur_text + 1) == '/')
      ||
      (cv == '/' && cur_text != cur_line->text && * (cur_text - 1) == '*')
     ) {
	search_for ("/\\*", REVERSE, False);
  } else if (* cur_line->text == '#') {
	/* #if #else #endif */
	char * cp = cur_line->text;
	cp ++;
	while (white_space (* cp)) {
		cp ++;
	}
	/* #if/#elif/#else/#endif matching */
	if (strisprefix ("if", cp)) {
		search_corresponding ("^#[ 	]*[ie][fln]", FORWARD, "#1");
	} else if (strisprefix ("end", cp)) {
		search_corresponding ("^#[ 	]*[ie][fln]", REVERSE, "#3");
	} else if (strisprefix ("el", cp)) {
	    if (pref_direction == FORWARD) {
		search_corresponding ("^#[ 	]*[ie][fln]", FORWARD, "#2");
	    } else {
		search_corresponding ("^#[ 	]*[ie][fln]", REVERSE, "#2");
	    }
	} else {
		/* nothing to match */
		error (errmsg);
	}
  } else if (strisprefix ("--", cur_line->text)) {
	/* search for next/previous MIME separator */
	char pattern [maxPROMPTlen + 1];
	int len = strlen (cur_line->text);
	MOVLBEG ();
	pattern [0] = '^';
	strcpy (& pattern [1], cur_line->text);
	if (pattern [len] == '\n') {
		pattern [len] = '\0';
		len --;
	}
	if (streq ("--", & pattern [len - 1])) {
		pattern [len - 1] = '\0';
	}
	search_for (pattern, pref_direction, False);
  } else {
	/* try to find mail header and search for next/previous mail */
	LINE * line = cur_line;
	char * text;
	while (white_space (* (line->text)) && line->prev != header) {
		line = line->prev;
	}
	text = line->text;
	while (* text != ':' && * text != '\0' && ! white_space (* text)) {
		advance_char (& text);
	}
	if ((* text == ':' && text != line->text) || strisprefix ("From ", line->text)) {
		/* mail header found */
		if (pref_direction == REVERSE) {
			/* go to beginning of current mail message */
			MOVDN ();
			search_for ("^From ", REVERSE, True);
		}
		search_for ("^From ", pref_direction, True);
	} else {
		/* nothing to match */
		error (errmsg);
	}
  }
}

/*
   get_idf extracts the identifier at the current position (text) 
   into idf_buf.
   start points to the beginning of the current line.
 */
int
get_idf (idf_buf, text, start)
  char * idf_buf;
  char * text;
  char * start;
{
  char * idf_buf_poi = idf_buf;
  char * idf_poi;
  char * copy_poi;

  if (! idfchar (text)) {
	error ("No identifier");
	return ERRORS;
  } else {
	idf_poi = text;
	while (idfchar (idf_poi) && idf_poi != start) {
		precede_char (& idf_poi, start);
	}
	if (! idfchar (idf_poi)) {
		advance_char (& idf_poi);
	}
	while (idfchar (idf_poi)) {
		copy_poi = idf_poi;
		advance_char (& idf_poi);
		while (copy_poi != idf_poi) {
			* idf_buf_poi ++ = * copy_poi ++;
		}
	}
	* idf_buf_poi = '\0';
	return FINE;
  }
}

/*
 * SIDF () searches for the identifier at the current position
 */
void
SIDF (method)
  FLAG method;
{
  char idf_buf [maxLINElen];	/* identifier to search for */

  int ret = get_idf (idf_buf, cur_text, cur_line->text);
  if (ret == ERRORS) {
	return;
  }

  search_expr (idf_buf, method, False);
}

/*
 * MPW () moves to the start of the previous word. A word is defined as a
 * number of non-blank characters separated by tabs spaces or linefeeds.
 */
static
void
move_previous_word (remove)
  FLAG remove;
{
  register char * begin_line;
  char * textp;
  char start_char = * cur_text;
  char * start_pos = cur_text;
  FLAG idfsearch = False;	/* avoid -Wmaybe-uninitialized */

  if (remove == DELETE && dont_modify ()) {
	return;
  }

/* First check if we're at the beginning of line. */
  if (cur_text == cur_line->text) {
	if (cur_line->prev == header) {
		return;
	}
	start_char = '\0';
  }

  MOVLF ();

  begin_line = cur_line->text;
  textp = cur_text;

  if (* textp == '\n' && textp != begin_line && ! white_space (* (textp - 1))) {
	/* stop at line-end (unless it ends with a space) */
  } else {
	/* Check if we're in the middle of a word. */
	if (! alpha (* textp) || ! alpha (start_char)) {
		while (textp != begin_line 
			&& (white_space (* textp) || * textp == '\n'))
		{
			precede_char (& textp, begin_line);
		}
	}

	/* Now we're at the end of previous word. Skip non-blanks until a blank comes */
	if (wordnonblank) {
		while (textp != begin_line && alpha (* textp)) {
			precede_char (& textp, begin_line);
		}
	} else {
		if (idfchar (textp)) {
			idfsearch = True;
			while (textp != begin_line && idfchar (textp)) {
				precede_char (& textp, begin_line);
			}
		} else {
			idfsearch = False;
			while (textp != begin_line 
				&& alpha (* textp) && ! idfchar (textp))
			{
				precede_char (& textp, begin_line);
			}
		}
	}

	/* Go to the next char if we're not at the beginning of the line */
	/* At the beginning of the line, check whether to stay or to go to the word */
	if (textp != begin_line && * textp != '\n') {
		advance_char (& textp);
	} else if (textp == begin_line && * textp != '\n' &&
		   (wordnonblank
			? * textp == ' '
			: (idfsearch
				? ! idfchar (textp)
				: (! alpha (* textp) || idfchar (textp)))))
	{
		advance_char (& textp);
		if (white_space (* textp) || textp == start_pos) {
			/* no word there or not moved, so go back */
			precede_char (& textp, begin_line);
		}
	}
  }

/* Find the x-coordinate of this address, and move to it */
  move_address (textp, y);
  if (remove == DELETE) {
	(void) delete_text (cur_line, textp, cur_line, start_pos);
  }
}

void
MPW ()
{
  if (hop_flag > 0) {
	BSEN ();
  } else {
	move_previous_word (NO_DELETE);
  }
}

/*
 * MNW () moves to the start of the next word. A word is defined as a number of
 * non-blank characters separated by tabs spaces or linefeeds. Always keep in
 * mind that the pointer shouldn't pass the '\n'.
 */
static
void
move_next_word (remove)
  FLAG remove;
{
  char * textp = cur_text;

  if (remove == DELETE && dont_modify ()) {
	return;
  }

/* Move to the end of the current word. */
  if (wordnonblank) {
	if (* textp != '\n') {
		advance_char (& textp);
	}
	while (alpha (* textp)) {
		advance_char (& textp);
	}
  } else {
	if (idfchar (textp)) {
		while (* textp != '\n' && idfchar (textp)) {
			advance_char (& textp);
		}
	} else {
		while (alpha (* textp) && ! idfchar (textp)) {
			advance_char (& textp);
		}
	}
  }

/* Skip all white spaces */
  while (* textp != '\n' && white_space (* textp)) {
	textp ++;
  }
/* If we're deleting, delete the text in between */
  if (remove == DELETE) {
	delete_text_buf (cur_line, cur_text, cur_line, textp);
	return;
  }

/* If we're at end of line, move to the beginning of (first word on) the next line */
  if (* textp == '\n' && cur_line->next != tail
	/* stop at line-end (unless it ends with a space) */
	&& (textp == cur_text || white_space (* (textp - 1)))
  ) {
	MOVDN ();
	move_to (LINE_START, y);
	textp = cur_text;
/*
	while (* textp != '\n' && white_space (* textp)) {
		textp ++;
	}
*/
  }
  move_address (textp, y);
}

void
MNW ()
{
  if (hop_flag > 0) {
	ESEN ();
  } else {
	move_next_word (NO_DELETE);
  }
}


/*======================================================================*\
|*			Modify commands: delete				*|
\*======================================================================*/

/*
 * DCC deletes the character under the cursor. If this character is a '\n' the
 * current line is joined with the next one.
 * If this character is the only character of the line, the current line will
 * be deleted.
 * DCC0 deletes without justification.
 */
static
void
delete_char (with_combinings)
  FLAG with_combinings;
{
  if (* cur_text == '\n') {
	if (cur_line->next == tail) {
		if (cur_line->return_type != lineend_NONE) {
			if (dont_modify ()) {
				return;
			}
			set_modified ();
			cur_line->return_type = lineend_NONE;
			if (total_chars >= 0) {
				total_chars --;
			}
			set_cursor_xy ();
			put_line (y, cur_line, x, True, False);
			status_msg ("Trailing line-end deleted");
		}
	} else {
		(void) delete_text (cur_line, cur_text, cur_line->next, cur_line->next->text);
	}
  } else {
	char * after_char = cur_text;
	advance_char (& after_char);

	if (with_combinings && combining_mode && encoding_has_combining ()) {
		/* check subsequent characters whether they are 
		   actually combined (or joined);
		   mind: doesn't work with poor man's bidi
		*/
		unsigned long unichar = unicodevalue (cur_text);

		/* skip this if already positioned within a combined char */
		if (! iscombined_unichar (unichar, cur_text, cur_line->text)) {
			/* delete combining accents together with base char */
			unichar = unicodevalue (after_char);
			while (iscombined_unichar (unichar, after_char, cur_line->text)) {
				advance_char (& after_char);
				unichar = unicodevalue (after_char);
			}
		}
	}

	(void) delete_text (cur_line, cur_text, cur_line, after_char);
  }
}

void
DCC ()
{
  if (hop_flag > 0 && (keyshift & ctrl_mask)) {
	/* Delete line right, to end of line */
	hop_flag = 0;
	DLN ();
  } else if ((shift_selection == True) && (keyshift & ctrl_mask)) {
	keyshift = 0;
	/* Delete word right */
	DNW ();
  } else if (keyshift & ctrl_mask) {
	keyshift = 0;
	delete_char (False);
  } else {
	delete_char (True);
  }
}

void
DCC0 ()
{
  DCC ();
}

/*
   "Plain BS":
   DPC0 deletes the character on the left side of the cursor.
   If the cursor is at the beginning of the line, the line end 
   is deleted, merging the two lines.
   With hop flag, delete left part of line from current point.
 */
void
DPC0 ()
{
  char * delete_pos;

  if (x == 0 && cur_line->prev == header) {
	/* Top of file; if line is shifted, pos is not 0 (shift symbol!) */
	return;
  }

  if (dont_modify ()) {
	return;
  }

  if (hop_flag > 0) {
	hop_flag = 0;
	if (emulation == 'e') {	/* emacs mode */
		DPW ();
	} else if (cur_text != cur_line->text) {
	  delete_pos = cur_text;
	  MOVLBEG ();
	  (void) delete_text (cur_line, cur_line->text, cur_line, delete_pos);
	}
  } else {
	/*FLAG was_on_comb = iscombining (unicodevalue (cur_text));*/
	FLAG was_on_comb = iscombined_unichar (unicodevalue (cur_text), cur_text, cur_line->text);
	if (keyshift & ctrl_mask) {
		ctrl_MLF ();
	} else {
		unsigned long unichar;
		MOVLF ();
		/* handle spacing combining characters */
		unichar = unicodevalue (cur_text);
		if (isspacingcombining_unichar (unichar)
		    || /* handle ARABIC TAIL FRAGMENT */
		    (unichar == 0xFE73
		     && iscombined_unichar (unichar, cur_text, cur_line->text))
		   ) {
			MOVLF ();
		}
	}
	if (was_on_comb) {
		delete_char (False);
	} else {
		delete_char (True);
	}
  }
}

/*
   DPC normally deletes the character left just as DPC0 does.
   However, unless the hop flag is set, it first checks if there is 
   anything but white space on the current line left of the current position.
   If there is only white space, it tries to perform a "backtab" function, 
   reverting the indentation to the previous amount above in the text 
   (unless if it's in the line immediately above the current line).
 */
void
DPC ()
{
  if (keyshift & alt_mask) {
	DCC ();
  } else if (hop_flag > 0 && (keyshift & ctrl_mask)) {
	/* Delete line left, to beginning of line */
	if (dont_modify ()) {
		return;
	}
	if (cur_text != cur_line->text) {
		char * delete_pos = cur_text;
		MOVLBEG ();
		(void) delete_text (cur_line, cur_line->text, cur_line, delete_pos);
	}
  } else if ((shift_selection == True) && (keyshift & ctrlshift_mask)) {
	keyshift = 0;
	/* Delete word left */
	DPW ();
  } else if ((keyshift & ctrlshift_mask) == ctrl_mask) {
	/* with Control and without Shift -> Plain BS */
	DPC0 ();
  } else if (hop_flag > 0) {	/* deprecated? */
	DPC0 ();	/* delete line left; emacs mode: delete word left */
  } else if (has_active_selection () && ! (keyshift & ctrl_mask)) {
	CUT ();
  } else if (plain_BS && ! (keyshift & shift_mask)) {
	/* unless Shifted: Plain BS preference configured -> Plain BS */
	keyshift |= ctrl_mask;
	DPC0 ();
  } else {
	/* "Smart BS": with auto-undent and combined character handling */
	char * cp = cur_line->text;
	int column = 0;
	int numberlen = 0;
	int stop_col = 0;

	if (autoindent) {
		FLAG afterpara = paragraphending (cur_line->prev, cur_line->prev->return_type);
		FLAG is_itemchar = False;
		/* check white space left of cursor, bullet or list numbering */
		while (* cp != '\0' && cp != cur_text &&
			(white_space (* cp) ||
			 (afterpara &&
			  (   (is_itemchar = isitemchar (unicodevalue (cp)))
			   || (autonumber && backnumber && numberlen == 0 && (numberlen = numbering (cp)) > 0)
			  )
			 )
			)
		      ) {
			if (numberlen > 0) {
				cp += numberlen;
				column += numberlen;
			} else {
				if (is_itemchar) {
					stop_col = column;
					is_itemchar = False;
				}
				advance_char_scr (& cp, & column, cur_line->text);
			}
		}
	}

	if (cp == cur_text) {
		/* only white space/bullet/numbering left of position */
		int previous_col = column;
		LINE * lp = cur_line->prev;

		if (autonumber && backnumber && numberlen > 0) {
		    if (last2subnumberpoi != NIL_PTR) {
			char numbuf [22];
			char * numpoi = numbuf;
			if (backincrem) {
				(void) delete_text (cur_line, last2subnumberpoi, cur_line, cur_text);
				/* insert incremented subnumber, e.g. 4. */
				sprintf (numbuf, "%d", last2subnumber + 1);
				while (* numpoi) {
					insert_unichar (* numpoi ++);
				}
				insert_unichar ('.');
			} else {
				(void) delete_text (cur_line, lastsubnumberpoi, cur_line, cur_text);
			}
		    } else {
			(void) delete_text (cur_line, lastsubnumberpoi, cur_line, cur_text);
		    }
		    return;
		}

		while (previous_col >= column && lp != header) {
			/* count white space on line lp */
			cp = lp->text;
			previous_col = 0;
			while (* cp != '\0' && previous_col < column && white_space (* cp)) {
				advance_char_scr (& cp, & previous_col, lp->text);
			}
			if (* cp == '\n' || * cp == '\0') {
				/* don't count space lines */
				previous_col = column;
			}

			lp = lp->prev;
		}

		/* Undent: if less indented previous line was found, 
		   (* and this was not on the immediately preceeding line *)
		   perform the back TAB function */
		if (previous_col < column /* && cur_line->prev != lp->next */) {
			/* important check, or we may hang */
			if (dont_modify ()) {
				return;
			}

			while (column > previous_col && column > stop_col) {
				DPC0 ();
				column = 0;
				cp = cur_line->text;
				while (* cp != '\0' && cp != cur_text) {
					advance_char_scr (& cp, & column, cur_line->text);
				}
			}
			while (column < previous_col) {
				S (' ');
				column ++;
			}
		} else {
			DPC0 ();
		}
	} else {
		/* check whether in combined character
			b/B is current position, b, c are combining:
			AB	delete A	(whole char)
			AcB	delete Ac	(whole char)
			Ab	delete A	(partial char)
			Acb	delete c	(partial char)
		 */
		unsigned long unichar = unicodevalue (cur_text);
		if (iscombined_unichar (unichar, cur_text, cur_line->text)) {
			/* if in combined char, delete at most base char, not 1 more left */
			keyshift |= ctrl_mask;
		}

		DPC0 ();
	}
  }
}

/*
 * DLINE delete the whole current line.
 */
void
DLINE ()
{
  if (dont_modify ()) {
	return;
  }

  if (hop_flag > 0) {
    hop_flag = 0;
    if (* cur_text != '\n') {
	delete_text_buf (cur_line, cur_text, cur_line, cur_text + length_of (cur_text) - 1);
    }
  } else {
    MOVLBEG ();
    if (* cur_text != '\n') {
	DLN ();
    }
    DCC ();
  }
}

/*
 * DLN deletes all characters until the end of the line. If the current
 * character is a '\n', then delete that char.
 */
void
DLN ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	DLINE ();
  } else if (* cur_text == '\n') {
/*	DCC ();	*/
	if (cur_line->next != tail) {
		delete_text_buf (cur_line, cur_text, cur_line->next, cur_line->next->text);
	}
  } else {
	delete_text_buf (cur_line, cur_text, cur_line, cur_text + length_of (cur_text) - 1);
  }
}

/*
 * DNW () deletes the next word (as defined in MNW ())
 */
void
DNW ()
{
  if (* cur_text == '\n') {
	DCC ();
  } else {
	move_next_word (DELETE);
  }
}

/*
 * DPW () deletes the previous word (as defined in MPW ())
 */
void
DPW ()
{
  if (cur_text == cur_line->text) {
	DPC0 ();
  } else {
	move_previous_word (DELETE);
  }
}


/*======================================================================*\
|*			Modify commands: insert				*|
\*======================================================================*/

static
void
enterNUL ()
{
  if (dont_modify ()) {
	return;
  }

  S ('\n');				/* Insert a new line */
  MOVUP ();				/* Move one line up */
  move_to (LINE_END, y);		/* Move to end of this line */
  cur_line->return_type = lineend_NUL;
  put_line (y, cur_line, x, True, False);
  MOVRT ();				/* move behind inserted NUL */
}


/*
 * Functions to insert character at current location.
 * S0 inserts without justification.
 */

static unsigned long previous_unichar = 0;

/*
 * S1byte: enter a byte of a character; collect bytes for multi-byte characters
 */
static
int
S1byte (newchar, JUSlvl, utf8_transform)
  register character newchar;
  int JUSlvl;
  FLAG utf8_transform;
{
  static character buffer [7];
  static character * utfpoi = buffer;
  static int utfcount = 1;
  static int cjkremaining = 0;
  static character firstbyte = '\0';
  static unsigned long unichar;
  int offset;

  int width = 1;

  if (newchar == '\0') {
	if (firstbyte != '\0') {
		firstbyte = '\0';
		ring_bell ();
	} else {
		enterNUL ();
	}
	return FINE;
  }

  if (utf8_text) {
	if (utf8_transform) {
	/* UTF-8 input for UTF-8 text */
		if (newchar < 0x80) {
			unichar = newchar;
			* utfpoi = newchar;
			utfpoi ++;
			* utfpoi = '\0';
			utfpoi = buffer;
		} else if ((newchar & 0xC0) == 0x80) {
		/* UTF-8 sequence byte */
			unichar = (unichar << 6) | (newchar & 0x3F);
			* utfpoi = newchar;
			utfpoi ++;
			utfcount --;
			if (utfcount == 0) {
				* utfpoi = '\0';
				utfpoi = buffer;
				width = uniscrwidth (unichar, cur_text, cur_line->text);
			} else {
				return FINE;
			}
		} else { /* first UTF-8 byte */
			utfpoi = buffer;
			* utfpoi = newchar;

			if ((newchar & 0xE0) == 0xC0) {
				utfcount = 2;
				unichar = newchar & 0x1F;
			} else if ((newchar & 0xF0) == 0xE0) {
				utfcount = 3;
				unichar = newchar & 0x0F;
			} else if ((newchar & 0xF8) == 0xF0) {
				utfcount = 4;
				unichar = newchar & 0x07;
			} else if ((newchar & 0xFC) == 0xF8) {
				utfcount = 5;
				unichar = newchar & 0x03;
			} else if ((newchar & 0xFE) == 0xFC) {
				utfcount = 6;
				unichar = newchar & 0x01;
			} else /* ignore illegal UTF-8 code */
				return FINE;

			utfpoi ++;
			utfcount --;
			return FINE;
		}
	} else {
	/* 8-bit input for UTF-8 text */
		unichar = newchar;
		if (newchar < 0x80) {
			buffer [0] = newchar;
			buffer [1] = '\0';
		} else {
			buffer [0] = (newchar >> 6) | 0xC0;
			buffer [1] = (newchar & 0x3F) | 0x80;
			buffer [2] = '\0';
		}
	}
  } else if (cjk_text) {
	/* 8/16-bit (CJK) input for CJK text */
	if (cjkremaining > 0) {
		* utfpoi ++ = newchar;
		* utfpoi = '\0';
		cjkremaining --;
		if (cjkremaining > 0) {
			return FINE;
		}
		utfpoi = buffer;
	} else if (firstbyte != '\0') {
		buffer [0] = firstbyte;
		buffer [1] = newchar;
		buffer [2] = '\0';
		cjkremaining = CJK_len (buffer) - 2;
		if (cjkremaining > 0) {
			firstbyte = '\0';
			utfpoi = & buffer [2];
			return FINE;
		}
	} else if (multichar (newchar)) {
		firstbyte = newchar;
		return FINE;
	} else {
		buffer [0] = newchar;
		buffer [1] = '\0';
	}
	if (* buffer == '\t') {
		width = 1;
	} else {
		width = col_count (buffer);
	}
	firstbyte = '\0';
  } else if (utf8_transform) {
	/* UTF-8 input for 8-bit text */
	buffer [1] = '\0';
	if (newchar < 0x80) {
		buffer [0] = newchar;
	} else if ((newchar & 0xC0) == 0x80) {
		/* UTF-8 sequence byte; not handled here anymore */
		unichar = (unichar << 6) | (newchar & 0x3F);
		utfcount --;
		if (utfcount == 0) {
			if ((unichar & 0xFF) == unichar) {
				buffer [0] = unichar & 0xFF;
			} else {
				buffer [0] = '';
			}
		} else {
			return FINE;
		}
	} else {
		/* first UTF-8 byte; not handled here anymore */
		if ((newchar & 0xE0) == 0xC0) {
			utfcount = 2;
			unichar = newchar & 0x1F;
		} else if ((newchar & 0xF0) == 0xE0) {
			utfcount = 3;
			unichar = newchar & 0x0F;
		} else if ((newchar & 0xF8) == 0xF0) {
			utfcount = 4;
			unichar = newchar & 0x07;
		} else if ((newchar & 0xFC) == 0xF8) {
			utfcount = 5;
			unichar = newchar & 0x03;
		} else if ((newchar & 0xFE) == 0xFC) {
			utfcount = 6;
			unichar = newchar & 0x01;
		} else { /* ignore illegal UTF-8 code */
			return FINE;
		}
		utfcount --;
		return FINE;
	}
  } else {
	/* 8-bit input for 8-bit text */
	buffer [0] = newchar;
	buffer [1] = '\0';
  }

/* right-to-left support */
  if (poormansbidi) {
	if (! utf8_text) {
		unichar = unicodevalue (buffer);
	}
	if (is_right_to_left (previous_unichar)) {
		if (newchar == '\n') {
			MOVLEND ();
		} else if (iscombining (unichar) && * cur_text != '\n') {
			MOVRT ();
		} else if (unichar != ' ' && unichar != '\t' && ! is_right_to_left (unichar)) {
			unsigned long rc = unicodevalue (cur_text);
			while (rc == ' ' || rc == '\t' || is_right_to_left (rc) || iscombining (rc)) {
				MOVRT ();
				rc = unicodevalue (cur_text);
			}
		}
	}
  }


/* Insert the new character */
  offset = cur_text - cur_line->text + length_of (buffer);
  if (insert_text (cur_line, cur_text, buffer) == ERRORS) {
	return ERRORS;
  }

/* Fix screen */
  if (newchar == '\n') {
	if (y == SCREENMAX) {		/* Can't use display () */
		print_line (y, cur_line);
		(void) forward_scroll (True);
		move_to (0, y);
	} else {
		reset (top_line, y);	/* Reset pointers */
		if (can_add_line) {
			clean_menus ();
			add_line (y + 1);
			scrollbar_scroll_down (y + 1);
			clear_status ();
			display (y, cur_line, 1, y + 1);
		} else {
			display (y, cur_line, last_y - y, y + 1);
		}
		move_to (0, y + 1);
	}
  } else if (x + width == XBREAK) { /* If line must be shifted, just call move_to */
	move_to (x + width, y);
	/* adjust in case of combining character position */
	if (cur_line->text + offset != cur_text) {
		move_address (cur_line->text + offset, y);
	}
  } else {			/* else display rest of line */
/*	also redraw previous char if it was incomplete ...
	just always redraw it!
	if (width != 0) {
		put_line (y, cur_line, x, False, False);
		move_to (x + width, y);
	} else
*/
	{
		/* take care of combining char added to left char */
		int newx = x + width;
		move_to (newx, y);
		if (iswide (unicodevalue (buffer))) {
			move_to (x - 3, y);
		} else {
			move_to (x - 1, y);
		}
		put_line (y, cur_line, x, False, False);
		/* adjust to new position */
		move_address (cur_line->text + offset, y);
	}
  }

/* right-to-left support */
  if (poormansbidi) {
	if (unichar == ' ' || unichar == '\t') {
		if (is_right_to_left (previous_unichar)) {
			move_to (x - 1, y);
		}
	} else if (unichar != '\n') {
		if (iscombining (unichar)) {
			if (is_right_to_left (previous_unichar)) {
				move_to (x - 1, y);
			}
		} else {
			if (is_right_to_left (unichar)) {
				move_to (x - 1, y);
			}
			previous_unichar = unichar;
		}
	}
  }

  if (JUSlvl > 0) {
	JUSandreturn ();
  }
  return FINE;
}

void
S0 (newchar)
  register character newchar;
{
  S1byte (newchar, 0, utf8_input);
}

static
int
Sbyte (newchar)
  register character newchar;
{
  return S1byte (newchar, JUSlevel, False);
}


static
FLAG
Sutf8char (newchar)
  unsigned long newchar;
{
  if (newchar < 0x80) {
	S1byte ((character) newchar, JUSlevel, True);
  } else if (newchar < 0x800) {
	S1byte ((character) (0xC0 | (newchar >> 6)), JUSlevel, True);
	S1byte ((character) (0x80 | (newchar & 0x3F)), JUSlevel, True);
  } else if (newchar < 0x10000) {
	S1byte ((character) (0xE0 | (newchar >> 12)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 6) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | (newchar & 0x3F)), JUSlevel, True);
  } else if (newchar < 0x200000) {
	S1byte ((character) (0xF0 | (newchar >> 18)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 12) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 6) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | (newchar & 0x3F)), JUSlevel, True);
  } else if (newchar < 0x4000000) {
	S1byte ((character) (0xF8 | (newchar >> 24)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 18) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 12) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 6) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | (newchar & 0x3F)), JUSlevel, True);
  } else if (newchar < 0x80000000) {
	S1byte ((character) (0xFC | (newchar >> 30)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 24) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 18) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 12) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | ((newchar >> 6) & 0x3F)), JUSlevel, True);
	S1byte ((character) (0x80 | (newchar & 0x3F)), JUSlevel, True);
  } else {
	error ("Invalid Unicode value");
	return False;
  }
  return True;
}

static
void
Sutf8 (newchar)
  unsigned long newchar;
{
  (void) Sutf8char (newchar);
}

static
FLAG
Scjk (code)
  unsigned long code;
{
  character cjkbytes [5];
  character * cp;

  if (no_char (code)) {
	ring_bell ();
	error ("Invalid character");
	return False;
  } else {
	(void) cjkencode (code, cjkbytes);
	if (* cjkbytes != '\0') {
		cp = cjkbytes;
		while (* cp != '\0') {
			S1byte (* cp ++, JUSlevel, False);
		}
	} else {
		ring_bell ();
		error ("Invalid CJK character code");
		return False;
	}
  }
  return True;
}

static FLAG deliver_dont_insert = False;
static unsigned long delivered_character;

/*
 * insert_character inserts character literally
 */
static
FLAG
insert_character (code)
  unsigned long code;
{
  if (deliver_dont_insert) {
	delivered_character = code;
	return True;
  }

  if (code == CHAR_UNKNOWN) {
	ring_bell ();
	error ("Unknown character mnemonic");
	return False;
  } else if (code == CHAR_INVALID) {
	ring_bell ();
	error ("Invalid character");
	return False;
  } else if (utf8_text) {
	return Sutf8char (code);
  } else if (cjk_text) {
	return Scjk (code);
  } else if (code < 0x100) {
	Sbyte (code);
	return True;
  } else {
	ring_bell ();
	error ("Invalid character");
	return False;
  }
}

FLAG
insert_unichar (uc)
  unsigned long uc;
{
  return insert_character (charcode (uc));
}


/*
 * Insert newline with auto indentation.
 */
static
void
SNLindent (do_autonumber)
  FLAG do_autonumber;
{
  char * coptext;
  unsigned long ch;
  unsigned long unich;
  FLAG afterpara;
  lineend_type prev_ret_type = cur_line->return_type;
  int numberlen = 0;

  if (Sbyte ('\n') == ERRORS) {
	return;
  }
  coptext = cur_line->prev->text;
  afterpara = paragraphending (cur_line->prev, prev_ret_type);

  /* find beginning of ended paragraph */
  if (afterpara) {
	LINE * paraline = cur_line->prev;
	char * s;
	/* should the following be a preference option?
	   meaning an item starts with a bullet or numbering 
	   even "within a paragraph"
	 */
	FLAG stop_at_item = True;	/* regardless of paragraphending? */

	while (paraline->prev != header) {
		LINE * prevline = paraline->prev;
		if (paragraphending (prevline, prevline->return_type)) {
			stop_at_item = False;	/* trigger final item check */
			break;
		}

		if (stop_at_item) {
			s = paraline->text;
			while (iswhitespace (unicodevalue (s))) {
				advance_char (& s);
			}
			numberlen = numbering (s);
			if (isitemchar (unicodevalue (s)) || numberlen > 0) {
				coptext = paraline->text;
				break;
			}
		}

		paraline = prevline;
	}

	if (! stop_at_item || paraline->prev == header) {
		s = paraline->text;
		while (iswhitespace (unicodevalue (s))) {
			advance_char (& s);
		}
		numberlen = numbering (s);
		if (isitemchar (unicodevalue (s)) || numberlen > 0) {
			coptext = paraline->text;
		}
	}
  }

  ch = charvalue (coptext);
  unich = unicode (ch);
  if (unich == 0xFEFF) {
	/* skip initial ZERO WIDTH NO-BREAK SPACE; BYTE ORDER MARK */
	advance_char (& coptext);
	ch = charvalue (coptext);
	unich = unicode (ch);
  }
  /* clone white space indentation */
  while (iswhitespace (unich)) {
	insert_character (ch);
	advance_char (& coptext);
	ch = charvalue (coptext);
	unich = unicode (ch);
  }
  numberlen = numbering (coptext);

  /* list and auto-numbering support */

  /* bullet list: check whether there is a bullet or dash */
  if (isitemchar (unich)) {
	/* cancel right-to-left memory for proper positioning */
	previous_unichar = 0;

	if (afterpara) {
		/* clone bullet from previous line */
		insert_character (ch);
	} else {
		/* indent over bullet (within list item) */
		if (is_wideunichar (unich)) {
			insert_unichar (0x3000);	/* wide space */
		} else {
			insert_character (' ');
		}
	}

	advance_char (& coptext);
	/* clone white space separator */
	ch = charvalue (coptext);
	unich = unicode (ch);
	while (iswhitespace (unich)) {
		insert_character (ch);
		advance_char (& coptext);
		ch = charvalue (coptext);
		unich = unicode (ch);
	}

	if (prev_ret_type == lineend_PS) {
		cur_line->prev->return_type = lineend_PS;
	}
	return;
  }

  /* auto-numbering: check whether there is numbering (1. 2. ...) */
  if (do_autonumber && numberlen > 0) {
	char numbuf [22];
	char * numpoi = numbuf;
	char * afternumber = coptext + numberlen;

	/* cancel right-to-left memory for proper positioning */
	previous_unichar = 0;

	if (afterpara) {
	    /* copy numbering prefix, e.g. 1.2. of 1.2.3. */
	    while (coptext < lastsubnumberpoi && * coptext != '\n') {
		ch = charvalue (coptext);
		insert_character (ch);
		advance_char (& coptext);
	    }
	    /* insert incremented subnumber, e.g. 4. */
	    sprintf (numbuf, "%d", lastsubnumber + 1);
	    while (* numpoi) {
		insert_character (* numpoi ++);
		/* align source and insertion for remaining whitespace */
		if (* coptext >= '.' && * coptext <= '9') {
			coptext ++;
		}
	    }
	    insert_character ('.');
	} else {
	    while (numberlen -- > 0) {
		insert_character (' ');
	    }
	}
	coptext = afternumber;

	/* clone white space separator */
	ch = charvalue (coptext);
	unich = unicode (ch);
	while (iswhitespace (unich)) {
		insert_character (ch);
		advance_char (& coptext);
		ch = charvalue (coptext);
		unich = unicode (ch);
	}

	if (prev_ret_type == lineend_PS) {
		cur_line->prev->return_type = lineend_PS;
	}
	return;
  }
}

/*
 * Insert new line at current location. Triggered by Enter key / CR.
 */
void
SNL ()
{
  trace_keytrack ("SNL", 0);
  if ((keyshift & altctrlshift_mask) == alt_mask) {
	keyshift = 0;
	Popmark ();
	return;
  }

  if (dont_modify ()) {
	return;
  }

  if (utf8_lineends == False && (keyshift & ctrl_mask)) {
	keyshift = 0;
	/* new line within same paragraph */
	if (JUSmode == 0) {
		if (* (cur_text -1) != ' ') {
			S (' ');
		}
	}
  }

  if (autoindent == False 
      || last_delta_readchar < 10 || average_delta_readchar < 10) {
	if (suppress_pasting_double_LF) {
		/* try to compensate an Exceed bug pasting LF twice */
		if (lastchar != '\r') {
			S ('\n');
		} else {
			/* skipping LF, should prevent skipping multiple */
			lastchar = ' ';	/* does not work */
		}
	} else {
		S ('\n');
	}
  } else {
	SNLindent (autonumber);
  }
  lastchar = '\r';
}

static
void
Spair (l, r)
  character l;
  character r;
{
  S1byte (l, JUSlevel, utf8_input);
  SNLindent (False);
  S1byte (r, JUSlevel, utf8_input);

  MOVUP ();
  MOVLEND ();
}

static
void
S (newchar)
  register character newchar;
{
  FLAG prev_lastspace_smart = lastspace_smart;
  lastspace_smart = False;

  if (hop_flag > 0) {
	lastchar = newchar;
	if (newchar == '\n') {
		S1byte (newchar, JUSlevel, utf8_input);
		return;
	}
	hop_flag = 0;
	flags_changed = True;
	if (newchar == '(') {
		Spair ('(', ')');
		return;
	} else if (newchar == '[') {
		Spair ('[', ']');
		return;
	} else if (newchar == '{') {
		Spair ('{', '}');
		return;
	} else if (newchar == '<') {
		Spair ('<', '>');
		return;
	} else if (newchar == '/') {
		if (* cur_text != '\n') {
			SNLindent (False);
			MOVUP ();
		}
		S1byte ('/', JUSlevel, utf8_input);
		S1byte ('*', JUSlevel, utf8_input);
		S1byte ('*', JUSlevel, utf8_input);
		SNLindent (False);
		S1byte (' ', JUSlevel, utf8_input);
		SNLindent (False);
		S1byte ('*', JUSlevel, utf8_input);
		S1byte ('/', JUSlevel, utf8_input);
		MOVUP ();
		S1byte (' ', JUSlevel, utf8_input);
		S1byte (' ', JUSlevel, utf8_input);
		return;
	}
  } else if (expand_tabs && newchar == '\t') {
	do {
		S1byte (' ', JUSlevel, utf8_input);
	} while (x % tabsize != 0);
	return;
  } else if (newchar == ' ' && (keyshift & ctrlshift_mask) == ctrlshift_mask) {
	(void) insert_unichar (0xA0);	/*   NO-BREAK SPACE */
	return;
  } else if (quote_type != 0 && newchar == '-' 
		&& lastchar == '-'
		&& cur_text != cur_line->text && * (cur_text - 1) == '-'
		&& (utf8_text || ! no_char (charcode (0x2013)))
		)
  {	/* handle smart dashes */
	lastchar = ' ';
	DPC0 ();
	if (cur_text != cur_line->text && * (cur_text - 1) == ' ') {
		Scharacter (charcode (0x2013));	/* – EN DASH */

		S1byte (' ', JUSlevel, utf8_input);
		lastspace_smart = True;
	} else {
		Scharacter (charcode (0x2014));	/* – EM DASH */
	}
	return;
  } else if (quote_type != 0 && lastchar == '-'
		&& (newchar == ' ' || newchar == '\t')
		&& cur_text != cur_line->text && * (cur_text - 1) == '-'
		&& onlywhitespacespacebefore (cur_line, cur_text - 1)
		&& (utf8_text || ! no_char (charcode (0x2013)))
		)
  {	/* handle leading smart dash */
	lastchar = ' ';
	DPC0 ();
	Scharacter (charcode (0x2013));	/* – EN DASH */
	S1byte (newchar, JUSlevel, utf8_input);
	return;
  } else if (quote_type != 0 && newchar == '-'
		&& (* cur_text == '\n' || iswhitespace (unicodevalue (cur_text)))
		&& onlywhitespacespacebefore (cur_line, cur_text)
		&& (utf8_text || ! no_char (charcode (0x2013)))
		)
  {	/* insert leading smart dash */
	lastchar = '�';	/* use as marker */
	Scharacter (charcode (0x2013));	/* – EN DASH */
	return;
  } else if (quote_type != 0 && newchar == '-' && lastchar == (character) '�'
		&& (utf8_text || ! no_char (charcode (0x2013)))
		)
  {	/* swallow second '-' for already inserted leading smart dash */
	lastchar = ' ';
	return;
  } else if (quote_type != 0 && newchar == ' ' 
		&& lastchar == '-'
		&& cur_text - cur_line->text >= 2
		&& * (cur_text - 1) == '-'
		&& * (cur_text - 2) == ' '
		&& (utf8_text || ! no_char (charcode (0x2212)))
		)
  {	/* handle smart minus */
	lastchar = ' ';
	DPC0 ();
	Scharacter (charcode (0x2212));	/* − MINUS SIGN */
	S1byte (' ', JUSlevel, utf8_input);
	return;
  } else if (utf8_text && quote_type != 0 && newchar == '-' 
		&& lastchar == '<'
		&& cur_text != cur_line->text && * (cur_text - 1) == '<')
  {	/* handle smart arrows */
	lastchar = ' ';
	DPC0 ();
	Sutf8 (0x2190);	/* ← LEFTWARDS ARROW */
	return;
  } else if (utf8_text && quote_type != 0 && newchar == '>' 
		&& cur_text != cur_line->text
		&& ((lastchar == '-' && * (cur_text - 1) == '-')
		    || (lastchar == (character) '�' /*&& prev U+2013*/)
		   )
	    )
  {	/* handle smart arrows */
	lastchar = ' ';
	DPC0 ();
	Sutf8 (0x2192);	/* → RIGHTWARDS ARROW */
	return;
  } else if (utf8_text && quote_type != 0 && newchar == '>' 
		&& lastchar == '<'
		&& cur_text != cur_line->text && * (cur_text - 1) == '<')
  {	/* handle smart arrows */
	lastchar = ' ';
	DPC0 ();
	Sutf8 (0x2194);	/* ↔ LEFT RIGHT ARROW */
	return;
  } else if (quote_type != 0 && newchar == '-' 
		&& (streq (script (charvalue (cur_text)), "Hebrew")
		    || streq (script (precedingchar (cur_text, cur_line->text)), "Hebrew")
		   )
	    )
  {	/* handle Maqaf */
	lastchar = ' ';
	(void) insert_unichar (0x05BE);	/* ־ MAQAF */
	return;
  } else if (newchar == ' ' && prev_lastspace_smart) {
	return;
  }

  lastchar = newchar;
  S1byte (newchar, JUSlevel, utf8_input);
}

/**
   Insert character by function
 */
void
STAB ()
{
  S ('\t');
}

void
SSPACE ()
{
  S (' ');
}

/**
   Underline preceding header
 */
void
Underline ()
{
  FLAG at_end = * cur_text == '\n';
  int cols;

  hop_flag = 0;

  if (cur_text == cur_line->text) {
	MOVLF ();
  }
  MOVLEND ();
  cols = col_count (cur_line->text);

  SNLindent (False);
  cols -= col_count (cur_line->text);
  while (cols > 0) {
	S0 ('-');
	cols --;
  }

  if (! at_end) {
	MOVRT ();
	if (white_space (* cur_text)) {
		MNW ();
	}
  }
}

void
reset_smart_replacement ()
{
  lastchar = '\0';
  lastspace_smart = False;
}


static
unsigned long
stropped (ch, cpoi, begin_line)
  unsigned long ch;
  char * cpoi;
  char * begin_line;
{
  unsigned long unichar = unicode (ch);
  unsigned long chup = case_convert (unichar, 1);
  if (chup == unichar) {
	/* wasn't a small letter */
	return ch;
  } else {
	/* small letter to be inserted, check whether to capitalize */
	FLAG strop = False;
	while (cpoi != begin_line) {
		unsigned long pre;
		precede_char (& cpoi, begin_line);
		pre = unicodevalue (cpoi);
		if (isLetter (pre)) {
			unsigned long small = case_convert (pre, -1);
			if (small == pre) {
				/* preceding small letter */
				return ch;
			} else {
				/* minimal condition found */
				strop = True;
			}
		} else {
			/* arrived before identifier */
			break;
		}
	}
	if (strop) {
		/* check escaping context (string) */
		int qn = 0;
		char prevc = '\0';
		while (begin_line <= cpoi) {
			unsigned long qc = charvalue (begin_line);
			if ((qc == '"' || qc == '\'')
			    && (qn % 2 == 0 || prevc != '\\')) {
				qn ++;
			}
			prevc = qc;
			advance_char (& begin_line);
		}
		if (qn % 2) {	/* within string */
			return ch;
		} else {	/* return capital letter to insert */
			return encodedchar (chup);
		}
	} else {
		return ch;
	}
  }
}

void
Scharacter (code)
  unsigned long code;
{
  if (no_char (code)) {
	ring_bell ();
	error ("Invalid character");
	return;
  }

  if (lowcapstrop) {
	code = stropped (code, cur_text, cur_line->text);
  }

  if (code < 0x80) {
#ifdef suppress_pasting_CRLF
	if (code == '\n' && lastchar == '\r'
	 && (last_delta_readchar < 1000 || average_delta_readchar < 1000)) {
		/* skip on pasting CRLF */
		lastchar = '\n';
	} else {
		S (code);	/* go through HOP handling */
	}
#else
	S (code);	/* go through HOP handling */
#endif
  } else if (utf8_text) {
	Sutf8 (code);
  } else if (cjk_text) {
	(void) Scjk (code);
  } else if (code < 0x100) {
	Sbyte (code & 0xFF);
  } else {
	ring_bell ();
	error ("Invalid character");
  }
}


/*
   APPNL inserts a newline at the current position;
   stays in front of it (on current line);
   keeps the line end type of the current line.
 */
void
APPNL ()
{
  lineend_type return_type;

  if (dont_modify ()) {
	return;
  }

  if (hop_flag > 0) {
	return_type = lineend_NONE;
	hop_flag = 0;
  } else if (cur_line->return_type == lineend_NONE
	     || cur_line->return_type == lineend_NUL) {
	return_type = default_lineend;
  } else if (! utf8_text &&
	(cur_line->return_type == lineend_PS
	 || cur_line->return_type == lineend_LS)) {
	return_type = default_lineend;
  } else if (cur_line->return_type == lineend_PS) {
	return_type = lineend_LS;
  } else {
	return_type = cur_line->return_type;
  }

  if (Sbyte ('\n') == ERRORS) {
	return;
  }
  MOVUP ();				/* Move one line up */
  move_to (LINE_END, y);		/* Move to end of this line */
  if (cur_line->return_type != return_type) {
	cur_line->return_type = return_type;
	if (return_type == lineend_NONE) {
		if (total_chars >= 0) {
			total_chars --;
		}
	}
	put_line (y, cur_line, x, True, False);
  }
}


/*======================================================================*\
|*			Convert lineend type				*|
\*======================================================================*/

static
void
convlineend_cur (ret)
  lineend_type ret;
{
  if (dont_modify ()) {
	return;
  }

  if (cur_line->return_type != ret) {
	if (cur_line->return_type == lineend_NONE) {
		if (total_chars >= 0) {
			total_chars ++;
		}
	}
	cur_line->return_type = ret;
	set_modified ();
  }
}

static
void
convlineend_all (ret)
  lineend_type ret;
{
  LINE * line;
  FLAG modified = False;

  if (dont_modify ()) {
	return;
  }

  for (line = header->next; line != tail; line = line->next) {
	if (line->return_type == lineend_LF
	 || line->return_type == lineend_CRLF
	 || line->return_type == lineend_CR) {
		if (line->return_type != ret) {
			modified = True;
			line->return_type = ret;
		}
	}
  }
  if (modified) {
	set_modified ();
  }
}


void
convlineend_cur_LF ()
{
  convlineend_cur (lineend_LF);
}

void
convlineend_cur_CRLF ()
{
  convlineend_cur (lineend_CRLF);
}

void
convlineend_all_LF ()
{
  convlineend_all (lineend_LF);
}

void
convlineend_all_CRLF ()
{
  convlineend_all (lineend_CRLF);
}


/*======================================================================*\
|*			Smart quotes					*|
\*======================================================================*/

static
unsigned long
quote_mark_value (qt, pos)
	int qt;
	quoteposition_type pos;
{
	unsigned long unichar;
	int utflen;
	char * q = quote_mark (qt, pos);
	utf8_info (q, & utflen, & unichar);
	return unichar;
}

int quote_type = 0;
int prev_quote_type = 0;
int default_quote_type = 0;
static FLAG quote_open [2] = {False, False};

static
void
reset_quote_state ()
{
	quote_open [False] = False;
	quote_open [True] = False;
}

void
set_quote_type (qt)
  int qt;
{
  if (qt >= 0 && qt < count_quote_types ()) {
	quote_type = qt;
	/*spacing_quotes = spacing_quote_type (qt);*/
  }
  reset_quote_state ();
  set_quote_menu ();
}

void
set_quote_style (q)
  char * q;
{
  set_quote_type (lookup_quotes (q));
}


static
void
Sapostrophe ()
{
  unsigned long apostrophe = 0x2019;
  /* check whether apostrophe is available in text encoding */
  if (! utf8_text) {
	apostrophe = encodedchar (apostrophe);
	if (no_char (apostrophe)) {
		error ("Apostrophe not available in current encoding");
		return;
	}
  }
  /* insert apostrophe */
  Scharacter (apostrophe);
}

static
void
Squote (doublequote)
  FLAG doublequote;
{
  int alternate = keyshift & alt_mask;
  int qt = alternate ? prev_quote_type : quote_type;
  FLAG spacing_quotes = spacing_quote_type (qt);

  if (qt == 0) {
	if (doublequote) {
		S ('"');
	} else {
		S ('\'');
	}
  } else {
	unsigned long leftquote;
	unsigned long rightquote;
	FLAG insert_left;
	unsigned long prevchar = unicode (precedingchar (cur_text, cur_line->text));
	char * cp = cur_line->text;

	/* check white space left of cursor */
	while (* cp != '\0' && cp != cur_text && white_space (* cp)) {
		advance_char (& cp);
	}

	/* determine mode (left/right) and state */
	if (cp == cur_text) {
		/* only white space left of current position */
		insert_left = True;
		quote_open [doublequote] = True;
	} else if (quote_open [doublequote]) {
		insert_left = False;
		quote_open [doublequote] = False;
	} else if (prevchar == 0x0A) {
		insert_left = True;
		quote_open [doublequote] = UNSURE;
	} else if (! doublequote && isLetter (prevchar)) {
		Sapostrophe ();
		return;
	} else if (is_wideunichar (prevchar)) {
		if (quote_open [doublequote] == UNSURE
		 || quote_open [doublequote] == OPEN) {
			insert_left = False;
			quote_open [doublequote] = False;
		} else {
			insert_left = True;
			quote_open [doublequote] = True;
		}
	} else {
		/* ! quote_open [doublequote] */
		insert_left = opensquote (prevchar)
			|| prevchar == quote_mark_value (qt, LEFTDOUBLE)
			|| prevchar == quote_mark_value (qt, LEFTSINGLE)
			;
		if (insert_left) {
			quote_open [doublequote] = OPEN;
		} else {
			quote_open [doublequote] = False;
		}
	}

	/* determine quotemarks of selected quote style */
	if (doublequote) {
		leftquote = quote_mark_value (qt, LEFTDOUBLE);
		rightquote = quote_mark_value (qt, RIGHTDOUBLE);
	} else {
		leftquote = quote_mark_value (qt, LEFTSINGLE);
		rightquote = quote_mark_value (qt, RIGHTSINGLE);
	}

	/* check whether quote marks are available in text encoding */
	if (! utf8_text) {
		leftquote = encodedchar (leftquote);
		rightquote = encodedchar (rightquote);
		if (no_char (leftquote) || no_char (rightquote)) {
			error ("Quote marks style not available in current encoding");
			return;
		}
	}

	/* insert quote mark */
	if (insert_left) {
		Scharacter (leftquote);
		if (spacing_quotes) {
			if (no_char (encodedchar (0x00A0))) {
				Scharacter (0x0020);
			} else {
				Scharacter (0x00A0);
			}
			lastspace_smart = True;
		}
	} else {
		if (spacing_quotes) {
			if (lastchar == ' ' && cur_text != cur_line->text
				&& * (cur_text - 1) == ' ')
			{
				DPC0 ();
			}
			if (no_char (encodedchar (0x00A0))) {
				Scharacter (0x0020);
			} else {
				Scharacter (0x00A0);
			}
		}
		Scharacter (rightquote);
	}
  }
}

void
Sdoublequote ()
{
  if ((keyshift & altshift_mask) == altshift_mask) {
	S ('"');
  } else {
	Squote (True);
  }
}

void
Ssinglequote ()
{
  if ((keyshift & altshift_mask) == altshift_mask) {
	S ('\'');
  } else if (hop_flag > 0 && ! (keyshift & alt_mask)) {
	/* HOP (not Alt): enter apostrophe */
	Sapostrophe ();
  } else {
	Squote (False);
  }
}

void
Sacute ()
{
  if (quote_type == 0 && hop_flag == 0) {
	insert_unichar ((character) '�');
  } else {
	Sapostrophe ();
  }
}

void
Sgrave ()
{
  if (quote_type == 0 && hop_flag == 0) {
	S ('`');
  } else {
	unsigned long okina = 0x02BB;	/* ʻ MODIFIER LETTER TURNED COMMA */
	/* check whether okina is available in text encoding */
	if (! utf8_text) {
		okina = encodedchar (okina);
		if (no_char (okina)) {
			status_uni ("Glottal stop/ʻokina not available in current encoding");
			return;
		}
	}
	/* insert okina */
	Scharacter (okina);
  }
}

void
Sdash ()
{
  if (hop_flag > 0) {
	Underline ();
  } else {
	S ('-');
  }
}


/*======================================================================*\
|*			Web marker insertion				*|
\*======================================================================*/

/*
   Strip string from first blank.
 */
static
void
strip (s)
  char * s;
{
  while (* s != '\0' && (ebcdic_text ? * s != code_SPACE : * s != ' ')) {
	s ++;
  }
  * s = '\0';
}

/*
   Turn string single-line (replace line-ends with space).
 */
static
void
unwrap (s)
  char * s;
{
  while (* s != '\0') {
	if (* s == '\n') {
		* s = ' ';
	}
	s ++;
  }
}

/*
   Insert string buffer.
 */
static
void
Sbuf (s)
  char * s;
{
  while (* s) {
	Scharacter (charvalue (s));
	advance_char (& s);
  }
}

static
void
Sunibuf (s)
  char * s;
{
  while (* s) {
	Scharacter (encodedchar (utf8value (s)));
	advance_utf8 (& s);
  }
}

static
void
embed_HTML ()
{
  char marker [maxPROMPTlen];
  char tag [maxPROMPTlen];

  if (dont_modify ()) {
	return;
  }
  if (get_string_nokeymap ("Embed text in HTML marker:", marker, True, "") != FINE) {
	return;
  }
  unwrap (marker);

  yank_HTML (DELETE);

  Sunibuf ("<");
  if ((marker [0] == 'A' || marker [0] == 'a') && marker [1] == '\0') {
	Sbuf (marker);
	Sunibuf (" href=");
  } else {
	Sbuf (marker);
  }
  Sunibuf (">");

  paste_HTML ();

  Sunibuf ("</");
  strcpy (tag, marker);
  strip (tag);
  Sbuf (tag);
  Sunibuf (">");
}

static char HTMLmarker [maxPROMPTlen];
static FLAG HTMLmarking = False;

void
HTML ()
{
  if (dont_modify ()) {
	return;
  }

  if (hop_flag > 0) {
	hop_flag = 0;
	embed_HTML ();
  } else {
	keyshift = 0;
	if (HTMLmarking == False) {
		if (FINE != get_string_nokeymap ("Begin HTML marker:", HTMLmarker, True, "")) {
			return;
		}
		unwrap (HTMLmarker);
		clear_status ();
		Sunibuf ("<");
		Sbuf (HTMLmarker);
		Sunibuf (">");
		HTMLmarking = True;
	} else {
		Sunibuf ("</");
		strip (HTMLmarker);
		Sbuf (HTMLmarker);
		Sunibuf (">");
		HTMLmarking = False;
	}
  }
}


/*======================================================================*\
|*			Character code handling				*|
\*======================================================================*/

/*
 * Replace current character with its hex/octal/decimal representation.
 */
static
character
hexdiguni (c)
  unsigned int c;
{
  if (c < 10) {
	return c + '0';
  } else {
	return c - 10 + 'A';
  }
}

character
hexdig (c)
  unsigned int c;
{
  if (ebcdic_text) {
	if (c < 10) {
		return c + 0xF0;
	} else {
		return c - 10 + 0xC1;
	}
  }
  return hexdiguni (c);
}

#ifdef gcc_O_bug_fixed
static
#else
/* workaround for gcc -O bug */
extern void insertcode _((character c, int radix));
#endif
void
insertcode (c, radix)
  character c;
  int radix;
{
  if (radix == 8) {
	S (hexdig ((c >> 6) & 007));
	S (hexdig ((c >> 3) & 007));
	S (hexdig ((c) & 007));
  } else if (radix == 16) {
	S (hexdig ((c >> 4) & 017));
	S (hexdig ((c) & 017));
  } else {	/* assume radix = 10 or, at least, three digits suffice */
	unsigned int radix2 = radix * radix;
	S (hexdig (c / radix2));
	S (hexdig ((c % radix2) / radix));
	S (hexdig (c % radix));
  }
}

static
void
insertunicode (unichar)
  unsigned long unichar;
{
  if (no_unichar (unichar)) {
	error ("No Unicode value");
  } else {
	if (unichar > 0xFFFF) {
		if (unichar > 0x10FFFF) {
			insertcode (unichar >> 24, 16);
		}
		insertcode (unichar >> 16, 16);
	}
	insertcode (unichar >> 8, 16);
	insertcode (unichar, 16);
  }
}

static
void
insertvalue (v, radix)
  unsigned long v;
  int radix;
{
  char buffer [12];
  char * bufpoi = & buffer [11];

  if (radix == 16) {
	insertunicode (v);
  } else {
	* bufpoi = '\0';
	while (v > 0) {
		bufpoi --;
		* bufpoi = hexdig (v % radix);
		v = v / radix;
	}
	while (* bufpoi != '\0') {
		S (* bufpoi ++);
	}
  }
}

static
void
changetocode (radix, univalue)
  int radix;
  FLAG univalue;
{
  character c = * cur_text;
  int utfcount = 1;
  unsigned long unichar;
#ifdef endlessloop
  int utflen;
#endif
  char buffer [7];
  char * textpoi;
  char * utfpoi;

  if (c == '\n') {
	switch (cur_line->return_type) {
	    case lineend_NONE:	unichar = CHAR_INVALID;
				break;
	    case lineend_NUL:	unichar = 0x00;
				break;
	    case lineend_LF:	unichar = 0x0A;
				break;
	    case lineend_CRLF:	unichar = 0x0D0A; /* transforms like 2 bytes */
				break;
	    case lineend_CR:	unichar = 0x0D;
				break;
	    case lineend_NL1:
	    case lineend_NL2:
				unichar = 0x85;
				break;
	    case lineend_LS:	unichar = 0x2028;
				break;
	    case lineend_PS:	unichar = 0x2029;
				break;
	    default:		unichar = CHAR_INVALID;
				break;
	}
	if (unichar < 0x100) {
		insertcode (unichar, 16);
	} else if (! no_unichar (unichar)) {
		insertunicode (unichar);
	}
  } else if (utf8_text) {
	if ((c & 0x80) == 0x00) {
		utfcount = 1;
		unichar = c;
	} else if ((c & 0xE0) == 0xC0) {
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
	} else /* illegal UTF-8 code */ {
		if (! univalue) {
		/*	DCC ();	*/
			insertcode (c, radix);
		}
		error ("Invalid UTF-8 sequence");
		return;
	}
	utfcount --;
	utfpoi = buffer;
	* utfpoi ++ = c;
	textpoi = cur_text;
	textpoi ++;
	while (utfcount > 0 && (* textpoi & 0xC0) == 0x80) {
		c = * textpoi ++;
		* utfpoi ++ = c;
		unichar = (unichar << 6) | (c & 0x3F);
		utfcount --;
	}
	* utfpoi = '\0';
/*	delete_char (False);	*/
	if (univalue) {
		insertvalue (unichar, radix);
	} else {
		utfpoi = buffer;
		while (* utfpoi != '\0') {
			insertcode (* utfpoi ++, radix);
		}
	}

#ifdef endlessloop
	utf8_info (cur_text, & utflen, & unichar);
	if (iscombining_unichar (unichar)) {
		changetocode (radix, univalue);
	}
#endif

	if (utfcount > 0) {
		error ("Invalid UTF-8 sequence");
	}
  } else if (cjk_text) {
	if (univalue) {
		insertvalue (lookup_encodedchar (charvalue (cur_text)), radix);
	} else {
		(void) cjkencode (charvalue (cur_text), buffer);
	/*	DCC ();	*/
		textpoi = buffer;
		while (* textpoi != '\0') {
			insertcode (* textpoi ++, radix);
		}
	}
  } else if (mapped_text && univalue) {
/*	DCC ();	*/
	insertvalue (lookup_encodedchar (c), radix);
  } else {
/*	DCC ();	*/
	insertcode (c, radix);
  }
}

static
void
changefromcode (format, univalue)
  char * format;
  FLAG univalue;
{
  long scancode;
  unsigned long code;

  if (sscanf (cur_text, format, & scancode) > 0) {
	if (scancode == -1) {
		ring_bell ();
		error ("Character code too long to scan");
		return;
	}
	code = scancode;
	if (univalue && (cjk_text || mapped_text)) {
		code = encodedchar (code);
		if (no_char (code)) {
			ring_bell ();
			error ("Invalid character");
			return;
		}
	}
	if (utf8_text && ! univalue) {
		unsigned char buffer [9];
		int i = 9;
		int utfcount;
		buffer [-- i] = '\0';
		while (code) {
			buffer [-- i] = code & 0xFF;
			code = code >> 8;
		}
		utf8_info (& buffer [i], & utfcount, & code);
		if (utfcount != sizeof (buffer) - 1 - i
		    || utfcount != UTF8_len (buffer [i])
		    || (buffer [i] & 0xC0) == 0x80) {
			ring_bell ();
			error ("Illegal UTF-8 sequence");
			return;
		}
	}
	(void) insert_character (code);
  } else {
	ring_bell ();
	error ("No character code at text position");
	hop_flag = 0;
	MOVRT ();
  }
}

void
changehex ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	if (utf8_text || cjk_text) {
		changefromcode ("%lx", False);
	} else {
		changefromcode ("%2lx", False);
	}
  } else {
	changetocode (16, False);
  }
}

void
changeuni ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	changefromcode ("%lx", True);
  } else {
	changetocode (16, True);
  }
}

void
changeoct ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	changefromcode ("%lo", True);
  } else {
	changetocode (8, True);
  }
}

void
changedec ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	changefromcode ("%lu", True);
  } else {
	changetocode (10, True);
  }
}


/*======================================================================*\
|*			Character information display			*|
\*======================================================================*/

static char title [maxXMAX];	/* buffer for description menu title */

/**
   Han character information display:
   Han info popup menu structure;
   must be static because it may be refreshed later.
 */
static menuitemtype descr_menu [27];

/**
   display_Han shows information about Han characters
   (pronunciations and definition from Unihan database)
 */
void
display_Han (cpoi, force_utf8)
  char * cpoi;
  FLAG force_utf8;
{
  int utfcount;
  unsigned long hanchar = 0;	/* avoid -Wmaybe-uninitialized */
  unsigned long unichar;

  struct hanentry * entry;
  char * flag_Mandarin = "";
  char * value_Mandarin = "";
  char * flag_Cantonese = "";
  char * value_Cantonese = "";
  char * flag_Japanese = "";
  char * value_Japanese = "";
  char * flag_Sino_Japanese = "";
  char * value_Sino_Japanese = "";
  char * flag_Hangul = "";
  char * value_Hangul = "";
  char * flag_Korean = "";
  char * value_Korean = "";
  char * flag_Vietnamese = "";
  char * value_Vietnamese = "";
  char * flag_HanyuPinlu = "";
  char * value_HanyuPinlu = "";
  char * flag_HanyuPinyin = "";
  char * value_HanyuPinyin = "";
  char * flag_XHCHanyuPinyin = "";
  char * value_XHCHanyuPinyin = "";
  char * flag_Tang = "";
  char * value_Tang = "";
  char * flag_description = "";
  char * value_description = "";

  char s [27] [maxXMAX];	/* buffer for description menu */

  if (force_utf8 || utf8_text) {
	utf8_info (cpoi, & utfcount, & unichar);
	if (cjk_text) {
		hanchar = encodedchar (unichar);
	}
  } else if (cjk_text) {
	hanchar = charvalue (cpoi);
	unichar = lookup_encodedchar (hanchar);
  } else {
	return;
  }

  entry = lookup_handescr (unichar);
  if (! entry) {
	/* ignore missing entry if valid Unicode character is 
	   not assigned to Han character range */
	if (! no_char (unichar) && ! streq (script (unichar), "Han")) {
		return;
	}
  }

  if (disp_Han_full && ! force_utf8) {
	int i = 0;
	if (cjk_text && ! no_unichar (unichar)) {
		build_string (title, "%04lX U+%04lX", hanchar, unichar);
	} else if (cjk_text) {
		if (valid_cjk (hanchar, NIL_PTR)) {
			build_string (title, "%04lX U? unknown", hanchar);
		} else if (no_char (hanchar)) {
			build_string (title, "Invalid");
		} else {
			build_string (title, "%04lX invalid", hanchar);
		}
	} else {
		build_string (title, "U+%04lX", unichar);
	}
	if (entry && disp_Han_Mandarin && * entry->Mandarin) {
		strcpy (s [i], "Mandarin: ");
		strcat (s [i], entry->Mandarin);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_Cantonese && * entry->Cantonese) {
		strcpy (s [i], "Cantonese: ");
		strcat (s [i], entry->Cantonese);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_Japanese && * entry->Japanese) {
		strcpy (s [i], "Japanese: ");
		strcat (s [i], entry->Japanese);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_Sino_Japanese && * entry->Sino_Japanese) {
		strcpy (s [i], "Sino-Japanese: ");
		strcat (s [i], entry->Sino_Japanese);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_Hangul && * entry->Hangul) {
		strcpy (s [i], "Hangul: ");
		strcat (s [i], entry->Hangul);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_Korean && * entry->Korean) {
		strcpy (s [i], "Korean: ");
		strcat (s [i], entry->Korean);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_Vietnamese && * entry->Vietnamese) {
		strcpy (s [i], "Vietnamese: ");
		strcat (s [i], entry->Vietnamese);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_HanyuPinlu && * entry->HanyuPinlu) {
		strcpy (s [i], "HanyuPinlu: ");
		strcat (s [i], entry->HanyuPinlu);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_HanyuPinyin && * entry->HanyuPinyin) {
		strcpy (s [i], "HanyuPinyin: ");
		strcat (s [i], entry->HanyuPinyin);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_XHCHanyuPinyin && * entry->XHCHanyuPinyin) {
		strcpy (s [i], "XHC Hànyǔ pīnyīn: ");
		strcat (s [i], entry->XHCHanyuPinyin);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}
	if (entry && disp_Han_Tang && * entry->Tang) {
		strcpy (s [i], "Tang: ");
		strcat (s [i], entry->Tang);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
	}

	/* append separator */
	fill_menuitem (& descr_menu [i], NIL_PTR, NIL_PTR);
	i ++;

	if (entry && disp_Han_description && * entry->Definition) {
#define wrap_description
#ifdef wrap_description
		char * descrpoi = entry->Definition;
		char * descrline;
		char * lastcomma;
		char * lastsemicolon;
		char * lastblank;
		char * lastcharacter;
		char * cut;
		char * bufpoi;
		int maxwidth = XMAX - 2;
		int col;

		while (* descrpoi != '\0') {
			descrline = descrpoi;
			col = 0;
			lastcomma = NIL_PTR;
			lastsemicolon = NIL_PTR;
			lastblank = NIL_PTR;
			lastcharacter = NIL_PTR;

			/* find max string length to fit in line */
			while (col < maxwidth && * descrpoi != '\0') {
				if (* descrpoi == ' ') {
					lastblank = descrpoi;
				} else if (* descrpoi == ',') {
					lastcomma = descrpoi;
				} else if (* descrpoi == ';') {
					lastsemicolon = descrpoi;
				} else {
					lastcharacter = descrpoi;
				}
				advance_char_scr (& descrpoi, & col, descrline);
			}

			/* determine cut at last separator */
			if (* descrpoi == '\0') {
				cut = descrpoi;
			} else if (lastsemicolon != NIL_PTR) {
				cut = lastsemicolon + 1;
			} else if (lastcomma != NIL_PTR) {
				cut = lastcomma + 1;
			} else if (lastblank != NIL_PTR) {
				cut = lastblank;
			} else if (lastcharacter != NIL_PTR) {
				cut = lastcharacter;
			} else {
				cut = descrpoi;
			}

			/* add line to menu, adjust poi to cut */
			descrpoi = descrline;
			bufpoi = s [i];
			while (descrpoi != cut) {
				* bufpoi ++ = * descrpoi ++;
			}
			* bufpoi = '\0';

			fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
			i ++;

			/* skip white space */
			while (* descrpoi == ' ') {
				descrpoi ++;
			}
		}
#else
		strcpy (s [i], entry->Definition);
		fill_menuitem (& descr_menu [i], s [i], NIL_PTR);
		i ++;
#endif
	}

	{	/* determine menu position; adjust if too far down */
		int descr_col = x;
		int descr_line = y + 1;
		if (descr_line + i + 2 > YMAX) {
			descr_line = y - i - 2;
			if (descr_line < 0) {
				descr_line = 0;
				descr_col ++;
			}
		}

		(void) popup_menu (descr_menu, i, descr_col, descr_line, title, False, True, NIL_PTR);
	}
  } else {
	if (entry && disp_Han_Mandarin && * entry->Mandarin) {
		flag_Mandarin = " M: ";
		value_Mandarin = entry->Mandarin;
	}
	if (entry && disp_Han_Cantonese && * entry->Cantonese) {
		flag_Cantonese = " C: ";
		value_Cantonese = entry->Cantonese;
	}
	if (entry && disp_Han_Japanese && * entry->Japanese) {
		flag_Japanese = " J: ";
		value_Japanese = entry->Japanese;
	}
	if (entry && disp_Han_Sino_Japanese && * entry->Sino_Japanese) {
		flag_Sino_Japanese = " S: ";
		value_Sino_Japanese = entry->Sino_Japanese;
	}
	if (entry && disp_Han_Hangul && * entry->Hangul) {
		flag_Hangul = " H: ";
		value_Hangul = entry->Hangul;
	}
	if (entry && disp_Han_Korean && * entry->Korean) {
		flag_Korean = " K: ";
		value_Korean = entry->Korean;
	}
	if (entry && disp_Han_Vietnamese && * entry->Vietnamese) {
		flag_Vietnamese = " V: ";
		value_Vietnamese = entry->Vietnamese;
	}
	if (entry && disp_Han_HanyuPinlu && * entry->HanyuPinlu) {
		flag_HanyuPinlu = " P: ";
		value_HanyuPinlu = entry->HanyuPinlu;
	}
	if (entry && disp_Han_HanyuPinyin && * entry->HanyuPinyin) {
		flag_HanyuPinyin = " X: ";
		value_HanyuPinyin = entry->HanyuPinyin;
	}
	if (entry && disp_Han_XHCHanyuPinyin && * entry->XHCHanyuPinyin) {
		flag_XHCHanyuPinyin = " X: ";
		value_XHCHanyuPinyin = entry->XHCHanyuPinyin;
	}
	if (entry && disp_Han_Tang && * entry->Tang) {
		flag_Tang = " T: ";
		value_Tang = entry->Tang;
	}
	if (entry && disp_Han_description && * entry->Definition) {
		flag_description = " D: ";
		value_description = entry->Definition;
	}

	/* MIND! When adding new pronunciation tags (Unihan update), 
	   be sure to add %s%s tags each in all three formats!
	 */
	if (cjk_text && ! no_unichar (unichar)) {
	    if (no_char (hanchar)) {
		build_string (text_buffer, 
			"Unmapped Han (U+%04lX)%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", 
			unichar,
			flag_Mandarin, value_Mandarin,
			flag_Cantonese, value_Cantonese,
			flag_Japanese, value_Japanese,
			flag_Sino_Japanese, value_Sino_Japanese,
			flag_Hangul, value_Hangul,
			flag_Korean, value_Korean,
			flag_Vietnamese, value_Vietnamese,
			flag_HanyuPinlu, value_HanyuPinlu,
			flag_HanyuPinyin, value_HanyuPinyin,
			flag_XHCHanyuPinyin, value_XHCHanyuPinyin,
			flag_Tang, value_Tang,
			flag_description, value_description
		);
	    } else {
		build_string (text_buffer, 
			"%04lX (U+%04lX)%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", 
			hanchar,
			unichar,
			flag_Mandarin, value_Mandarin,
			flag_Cantonese, value_Cantonese,
			flag_Japanese, value_Japanese,
			flag_Sino_Japanese, value_Sino_Japanese,
			flag_Hangul, value_Hangul,
			flag_Korean, value_Korean,
			flag_Vietnamese, value_Vietnamese,
			flag_HanyuPinlu, value_HanyuPinlu,
			flag_HanyuPinyin, value_HanyuPinyin,
			flag_XHCHanyuPinyin, value_XHCHanyuPinyin,
			flag_Tang, value_Tang,
			flag_description, value_description
		);
	    }
	} else if (cjk_text) {
	    if (valid_cjk (hanchar, NIL_PTR)) {
		build_string (text_buffer, "%04lX (U? unknown)", hanchar);
	    } else if (no_char (hanchar)) {
		build_string (text_buffer, "Invalid");
	    } else {
		build_string (text_buffer, "%04lX invalid", hanchar);
	    }
	} else {
		build_string (text_buffer, 
			"U+%04lX%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", 
			unichar,
			flag_Mandarin, value_Mandarin,
			flag_Cantonese, value_Cantonese,
			flag_Japanese, value_Japanese,
			flag_Sino_Japanese, value_Sino_Japanese,
			flag_Hangul, value_Hangul,
			flag_Korean, value_Korean,
			flag_Vietnamese, value_Vietnamese,
			flag_HanyuPinlu, value_HanyuPinlu,
			flag_HanyuPinyin, value_HanyuPinyin,
			flag_XHCHanyuPinyin, value_XHCHanyuPinyin,
			flag_Tang, value_Tang,
			flag_description, value_description
		);
	}

	status_uni (text_buffer);
  }
}


/*
 * display_code shows UTF-8 code sequence and Unicode value on the status line
 */
char hexbuf [20];
char * hexbufpoi = hexbuf;

static
void
appendbyte (c)
  unsigned int c;	/* 'char' leads to spurious bugs when shifting */
{
  * hexbufpoi ++ = hexdiguni ((c >> 4) & 0x0F);
  * hexbufpoi ++ = hexdiguni (c & 0x0F);
}

static
void
appendword (c)
  unsigned long c;
{
  if (utf16_little_endian) {
	appendbyte (c & 0xFF);
	appendbyte (c >> 8);
  } else {
	appendbyte (c >> 8);
	appendbyte (c & 0xFF);
  }
}

static char * scriptmsg;
static char * categorymsg;
static char * scriptsep;
static char * chardescr;
static char * charsep;
static int namedseqlen;
static unsigned long * namedseq;

static
void
setup_charinfo (unichar, follow)
  unsigned long unichar;
  char * follow;
{
  if (disp_charseqname && combining_mode) {
	chardescr = charseqname (unichar, follow, & namedseqlen, & namedseq);
	if (chardescr != NIL_PTR) {
		charsep = " ";
		scriptmsg = "";
		categorymsg = "";
		scriptsep = "";
		return;
	}
  }
  namedseqlen = 0;

  /* prepare script info */
  if (disp_scriptname) {
	struct scriptentry * script_info = scriptinfo (unichar);
	if (script_info) {
		scriptmsg = category_names [script_info->scriptname];
		categorymsg = category_names [script_info->categoryname];
		scriptsep = " ";
	} else {
		scriptmsg = "Not Assigned ";
		categorymsg = "";
		scriptsep = "";
	}
  } else {
	scriptmsg = "";
	categorymsg = "";
	scriptsep = "";
  }

  /* prepare character description (name, decomposition, mnemos) */
  if (disp_charname) {
	chardescr = charname (unichar);
	charsep = " ";
  } else if (disp_mnemos) {
	chardescr = mnemos (unichar);
	charsep = " mnemos:";
  } else if (disp_decomposition) {
	chardescr = decomposition_string (unichar);
	charsep = " decompose:";
  } else {
	chardescr = 0;
  }
  if (! chardescr || ! * chardescr) {
	chardescr = "";
	charsep = "";
  }
}

static
void
build_namedseq (text_buffer)
  char * text_buffer;
{
  if (namedseqlen == 2) {
	build_string (text_buffer, "Named Seq U+%04lX U+%04lX %s",
			namedseq [0], namedseq [1],
			chardescr);
  } else if (namedseqlen == 3) {
	build_string (text_buffer, "Named Seq U+%04lX U+%04lX U+%04lX %s",
			namedseq [0], namedseq [1], namedseq [2],
			chardescr);
  } else if (namedseqlen == 4) {
	build_string (text_buffer, "Named Seq U+%04lX U+%04lX U+%04lX U+%04lX %s",
			namedseq [0], namedseq [1], namedseq [2], namedseq [3],
			chardescr);
  }
}

void
display_the_code ()
{
  character c = * cur_text;
  int utfcount = 1;
  unsigned long unichar;
  int utfcount2;
  unsigned long unichar2;
  char * textpoi;
  FLAG invalid = False;
  char * lengthmsg;
  char * widemsg;
  char * combinedmsg;

  hexbufpoi = hexbuf;

  if (c == '\n') {
	switch (cur_line->return_type) {
	    case lineend_NONE:	textpoi = "no line end / split line";
				break;
	    case lineend_NUL:	appendbyte ('\0');
				textpoi = "U+00 NUL";
				break;
	    case lineend_LF:	appendbyte (code_LF);
				textpoi = "U+0A (Unix) LINE FEED";
				break;
	    case lineend_CRLF:	appendbyte ('\r');
				appendbyte (code_LF);
				textpoi = "U+0D U+0A (DOS) CRLF";
				break;
	    case lineend_CR:	appendbyte ('\r');
				textpoi = "U+0D (Mac) CARRIAGE RETURN";
				break;
	    case lineend_NL1:	appendbyte (code_NL);
				textpoi = "U+85 (ISO 8859) NEXT LINE";
				break;
	    case lineend_NL2:	appendbyte ('\302');
				appendbyte ('\205');
				textpoi = "U+85 NEXT LINE";
				break;
	    case lineend_LS:	appendbyte ('\342');
				appendbyte ('\200');
				appendbyte ('\250');
				textpoi = "U+2028 LINE SEPARATOR";
				break;
	    case lineend_PS:	appendbyte ('\342');
				appendbyte ('\200');
				appendbyte ('\251');
				textpoi = "U+2029 PARAGRAPH SEPARATOR";
				break;
	    default:		appendbyte (code_LF);
				textpoi = "unknown line end";
				break;
	}
	* hexbufpoi = '\0';
	build_string (text_buffer, "Line end: %s - %s", hexbuf, textpoi);
	status_uni (text_buffer);
  } else if (utf8_text) {
	int utf_utf_len;
	int uni_utf_len;

	if ((c & 0x80) == 0x00) {
		utfcount = 1;
		unichar = c;
	} else if ((c & 0xE0) == 0xC0) {
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
	} else /* illegal UTF-8 code */ {
		appendbyte (c);
		invalid = True;
		unichar = 0;
	}

	utf_utf_len = utfcount;

	textpoi = cur_text;
	if (invalid == False) {
		utfcount --;
		appendbyte (c);
		textpoi ++;
		while (utfcount > 0 && (* textpoi & 0xC0) == 0x80) {
			c = * textpoi ++;
			appendbyte (c);
			unichar = (unichar << 6) | (c & 0x3F);
			utfcount --;
		}
		if (utfcount > 0) {
			invalid = True;
		}
	} else {
		textpoi ++;
	}

	if (unichar < 0x80) {
		uni_utf_len = 1;
	} else if (unichar < 0x800) {
		uni_utf_len = 2;
	} else if (unichar < 0x10000) {
		uni_utf_len = 3;
	} else if (unichar < 0x200000) {
		uni_utf_len = 4;
	} else if (unichar < 0x4000000) {
		uni_utf_len = 5;
	} else if (unichar < 0x80000000) {
		uni_utf_len = 6;
	} else {
		uni_utf_len = 0;
	}
	if (utf_utf_len == uni_utf_len) {
		lengthmsg = "";
	} else {
		lengthmsg = " (too long)";
	}

	utf8_info (textpoi, & utfcount2, & unichar2);
	if (isspacingcombining_unichar (unichar2)) {
		combinedmsg = " - with ...";
	} else if (iscombining (unichar2)) {
		combinedmsg = " - combined ...";
	} else if (isjoined (unichar2, textpoi, cur_line->text)) {
		combinedmsg = " - joined ...";
	} else {
		combinedmsg = "";
	}

	* hexbufpoi = '\0';
	if (invalid == False) {
	    char * combmsg;

	    /* prepare character information */
	    setup_charinfo (unichar, textpoi);

	    if (iscombining_unichar (unichar)) {
		if (isspacingcombining_unichar (unichar)) {
			combmsg = "spacing combining ";
		} else {
			combmsg = "combining ";
		}
	    } else if (iscombined (unichar, cur_text, cur_line->text)) {
		if (iscombining (unichar)) {
			/* catch cases missed above; to be fixed? */
			combmsg = "combining ";
		} else {
			combmsg = "joining ";
		}
	    } else {
		combmsg = "";
	    }

	    if (is_wideunichar (unichar)) {
		widemsg = "wide ";
	    } else if ((unichar & 0x7FFFFC00) == 0xD800) {
		widemsg = "single high surrogate ";
	    } else if ((unichar & 0x7FFFFC00) == 0xDC00) {
		widemsg = "single low surrogate ";
	    } else if ((unichar & 0x7FFFFFFE) == 0xFFFE) {
		widemsg = "reserved ";
	    } else {
		widemsg = "";
	    }

	    if (namedseqlen) {
		build_namedseq (text_buffer);
	    } else if (utf16_file) {
	      hexbufpoi = hexbuf;
	      if (unichar > 0x10FFFF) {
		build_string (text_buffer, 
			"Illegal UTF-16 %s - %s%s%s%s%s%sU+%08lX%s%s%s", 
			lengthmsg, widemsg, combmsg, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, charsep, chardescr, combinedmsg);
	      } else if (unichar > 0xFFFF) {
		appendword (0xD800 | ((unichar - 0x10000) >> 10));
		appendword (0xDC00 | ((unichar - 0x10000) & 0x03FF));
		* hexbufpoi = '\0';
		build_string (text_buffer, 
			"UTF-16: %s%s - %s%s%s%s%s%sU+%06lX%s%s%s", 
			hexbuf, lengthmsg, widemsg, combmsg, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, charsep, chardescr, combinedmsg);
	      } else {
		appendword (unichar);
		* hexbufpoi = '\0';
		build_string (text_buffer, 
			"UTF-16: %s%s - %s%s%s%s%s%sU+%04lX%s%s%s", 
			hexbuf, lengthmsg, widemsg, combmsg, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, charsep, chardescr, combinedmsg);
	      }
	    } else {
	      if (unichar > 0x10FFFF) {
		build_string (text_buffer, 
			"Non-Unicode UTF-8: %s%s - %s%s%s%s%s%sU+%08lX%s%s%s", 
			hexbuf, lengthmsg, widemsg, combmsg, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, charsep, chardescr, combinedmsg);
	      } else if (unichar > 0xFFFF) {
		build_string (text_buffer, 
			"UTF-8: %s%s - %s%s%s%s%s%sU+%06lX%s%s%s", 
			hexbuf, lengthmsg, widemsg, combmsg, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, charsep, chardescr, combinedmsg);
	      } else {
		build_string (text_buffer, 
			"UTF-8: %s%s - %s%s%s%s%s%sU+%04lX%s%s%s", 
			hexbuf, lengthmsg, widemsg, combmsg, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, charsep, chardescr, combinedmsg);
	      }
	    }
	} else {
	    build_string (text_buffer, "Invalid UTF-8 sequence: %s%s", hexbuf, combinedmsg);
	}
	status_uni (text_buffer);
  } else if (cjk_text) {
	int len = CJK_len (cur_text);
	unsigned long cjkchar = charvalue (cur_text);
	character cjkbytes [5];
	int charlen = cjkencode (cjkchar, cjkbytes);

	textpoi = cur_text;
	while (len > 0 && * textpoi != '\0' && * textpoi != '\n') {
		appendbyte (* textpoi);
		textpoi ++;
		len --;
		charlen --;
	}
	* hexbufpoi = '\0';

	combinedmsg = "";
	if (text_encoding_tag == 'G' || text_encoding_tag == 'X' || text_encoding_tag == 'x') {
		unichar2 = lookup_encodedchar (charvalue (textpoi));
		if (isspacingcombining_unichar (unichar2)) {
			combinedmsg = " - with ...";
		} else if (iscombining (unichar2)) {
			combinedmsg = " - combined ...";
		} else if (text_encoding_tag == 'G' && isjoined (unichar2, textpoi, cur_line->text)) {
			combinedmsg = " - joined ...";
		}
	}

	if (len != 0 || charlen != 0) {
		build_string (text_buffer, "Incomplete CJK character code: %s%s", hexbuf, combinedmsg);
	} else {
	    unichar = lookup_encodedchar (cjkchar);
	    if (c > 0x7F || unichar != (character) c || * combinedmsg) {
		if (valid_cjk (cjkchar, NIL_PTR)) {
		    if (no_unichar (unichar)) {
			build_string (text_buffer, "CJK character code: %s (U? unknown)%s", hexbuf, combinedmsg);
		    } else {
			char * format;

			unichar2 = 0;
			if (unichar >= 0x80000000) {
				unichar2 = (unichar >> 16) & 0x7FFF;
				unichar = unichar & 0xFFFF;
				format = "CJK %scharacter: %s - %s%s%s%sU+%04lX (with U+%04lX)%s%s%s";
			} else if (unichar > 0x10FFFF) {
				format = "CJK %scharacter: %s - %s%s%s%sU+%08lX%s%s%s";
			} else if (unichar > 0xFFFF) {
				format = "CJK %scharacter: %s - %s%s%s%sU+%06lX%s%s%s";
			} else {
				format = "CJK %scharacter: %s - %s%s%s%sU+%04lX%s%s%s";
			}

			/* prepare character information */
			setup_charinfo (unichar, textpoi);

			/* determine combining status */
			widemsg = "";
			if (text_encoding_tag == 'G' || text_encoding_tag == 'X' || text_encoding_tag == 'x') {
				if (iscombining_unichar (unichar)) {
					if (isspacingcombining_unichar (unichar)) {
						widemsg = "spacing combining ";
					} else {
						widemsg = "combining ";
					}
				}
			}

			if (namedseqlen) {
				build_namedseq (text_buffer);
			} else if (unichar2 == 0) {
				build_string (text_buffer, format, widemsg, hexbuf, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, charsep, chardescr, combinedmsg);
			} else {
				build_string (text_buffer, format, widemsg, hexbuf, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, unichar2, charsep, chardescr, combinedmsg);
			}
		    }
		} else {
			build_string (text_buffer, "Invalid CJK character code: %s%s", hexbuf, combinedmsg);
		}
	    } else {
		/* prepare character information */
		setup_charinfo (unichar, textpoi);

		if (namedseqlen) {
			build_namedseq (text_buffer);
		} else {
			build_string (text_buffer, "Character: %s%s%s%sU+%02X%s%s", scriptmsg, scriptsep, categorymsg, scriptsep, c, charsep, chardescr);
		}
	    }
	}
	status_uni (text_buffer);
  } else {	/* 8 bit text */
	appendbyte (c);
	* hexbufpoi = '\0';

	if (mapped_text) {
		unichar2 = lookup_encodedchar ((character) * (cur_text + 1));
		combinedmsg = "";
		if (isspacingcombining_unichar (unichar2)) {
			combinedmsg = " - with ...";
		} else if (iscombining (unichar2)) {
			combinedmsg = " - combined ...";
		} else if (isjoined (unichar2, cur_text + 1, cur_line->text)) {
			combinedmsg = " - joined ...";
		}

		unichar = lookup_encodedchar (c);
		if (no_unichar (unichar)) {
			build_string (text_buffer, "Character code: %s (U? unknown)%s", hexbuf, combinedmsg);
		} else {
			widemsg = "C";
			if (iscombining_unichar (unichar)) {
				if (isspacingcombining_unichar (unichar)) {
					widemsg = "Spacing combining c";
				} else {
					widemsg = "Combining c";
				}
			}

			/* prepare character information */
			setup_charinfo (unichar, cur_text + 1);

			if (namedseqlen) {
				build_namedseq (text_buffer);
			} else {
				build_string (text_buffer, "%sharacter: %s - %s%s%s%sU+%04lX%s%s%s", widemsg, hexbuf, scriptmsg, scriptsep, categorymsg, scriptsep, unichar, charsep, chardescr, combinedmsg);
			}
		}
	} else {
		/* prepare character information */
		setup_charinfo (c, cur_text + 1);

		if (ebcdic_file) {
			character e;
			mapped_text = True;
			e = encodedchar (c);
			mapped_text = False;
			build_string (text_buffer, "Character: %02X - %s%s%s%sU+%02X%s%s", e, scriptmsg, scriptsep, categorymsg, scriptsep, c, charsep, chardescr);
		} else {
			build_string (text_buffer, "Character: %s%s%s%sU+%02X%s%s", scriptmsg, scriptsep, categorymsg, scriptsep, c, charsep, chardescr);
		}
	}
	status_uni (text_buffer);
  }
}

void
display_code ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	if (always_disp_code) {
		always_disp_code = False;
	} else {
		always_disp_code = True;
		always_disp_fstat = False;
		if (! disp_Han_full) {
			always_disp_Han = False;
		}
	}
  } else {
	display_the_code ();
  }
}


/*======================================================================*\
|*			Character composition				*|
\*======================================================================*/

/**
   do_insert_accented combines and inserts accented character, 
   handles multiple accent keys
 */
static
void
do_insert_accented (accentnames, ps, ps2, ps3)
  char * accentnames;
  struct prefixspec * ps;
  struct prefixspec * ps2;
  struct prefixspec * ps3;
{
  unsigned long base;
  struct prefixspec * newps = 0;
  unsigned char prefix_keyshift = keyshift;

  if (* accentnames == '\0') {
	return;
  }

  build_string (text_buffer, "Compose %s with:", accentnames);
  status_uni (text_buffer);
  base = readcharacter_unicode_mapped ();

  if (command (base) == DPC) {
	clear_status ();
	keyshift = prefix_keyshift | ctrl_mask;
	if (keyshift & shift_mask) {
		DPC ();
	} else {
		DPC0 ();
	}
	return;
  }

  if (command (base) == CTRLINS) {
    unsigned long ctrl;
    status_uni ("Enter compose char / <blank> mnemonic ...");
    ctrl = readcharacter_unicode ();

    newps = lookup_prefix_char (ctrl);
    if (newps) {
	/* continue below */
    } else if (ctrl == FUNcmd) {
	keyshift |= ctrl_mask;
	newps = lookup_prefix (keyproc, keyshift);
	if (newps) {
		/* continue below */
	} else {
		error ("Mnemonic input or accent prefix expected");
		return;
	}
    } else if (ctrl == ' ') {
	char mnemonic [maxPROMPTlen];
	build_string (text_buffer, "Compose %s with character mnemonic:", accentnames);
	if (get_string_uni (text_buffer, mnemonic, False, " ") == ERRORS) {
		return;
	}
	base = compose_mnemonic (mnemonic);
	/* final compose and insert below */
    } else if (ctrl > ' ' && ctrl != '#' && ctrl != 0x7F) {
	static character buf [7];
	unsigned long ch;
	(void) utfencode (ctrl, buf);
	build_string (text_buffer, "Compose %s with %s..", accentnames, buf);
	status_uni (text_buffer);
	ch = readcharacter_unicode ();
	if (ch == '\033' || ch == FUNcmd) {
		clear_status ();
		return;
	}
	base = compose_chars (ctrl, ch);
	/* final compose and insert below */
    } else {
	error ("Mnemonic input expected");
	return;
    }
  } else if (base == FUNcmd) {
	newps = lookup_prefix (keyproc, keyshift);
	if (newps) {
		/* continue below */
	} else {
		clear_status ();
		return;
	}
  } else if (no_char (base) || base == '\033') {
	clear_status ();
	return;
  }

  if (newps) {
	/* handle multiple accent prefixes */
	if (ps3) {
		error ("Max. 3 accent prefix keys anticipated");
	} else {
		char newaccentnames [maxPROMPTlen];
		if (ps2) {
			strcpy (newaccentnames, accentnames);
		} else {
			strcpy (newaccentnames, ps->accentsymbol);
		}
		strcat (newaccentnames, " and ");
		strcat (newaccentnames, newps->accentsymbol);
		if (ps2) {
			do_insert_accented (newaccentnames, ps, ps2, newps);
		} else {
			do_insert_accented (newaccentnames, ps, newps, 0);
		}
	}
  } else {
	clear_status ();
	(void) insert_character (compose_patterns (base, ps, ps2, ps3));
  }
}

/**
   insert_accented combines and inserts accented character
 */
static
void
insert_accented (ps)
  struct prefixspec * ps;
{
  do_insert_accented (ps->accentname, ps, 0, 0);
}

/*
 * CTRLGET reads a control-char or encoded or mnemonic character
   used by mousemen
 */
unsigned long
CTRLGET (check_prefix)
  FLAG check_prefix;
{
  FLAG save_utf8_text = utf8_text;
  utf8_text = True;
  /* temporarily switch insert_character to not enter into the text */
  deliver_dont_insert = True;
  delivered_character = CHAR_UNKNOWN;

  if (check_prefix) {
	struct prefixspec * ps = lookup_prefix (keyproc, keyshift);
	if (ps) {
		insert_accented (ps);
	}
  } else {
	CTRLINS ();
  }

  deliver_dont_insert = False;
  utf8_text = save_utf8_text;
  return delivered_character;
}

/*
 * CTRLINS inserts a control-char or encoded or mnemonic character
 */
void
CTRLINS ()
{
  unsigned long ctrl;
  unsigned long ch;

  status_uni ("Enter control char / # hex/octal/decimal / compose char / <blank> mnemonic ...");
  ctrl = readcharacter_unicode ();
  trace_keytrack ("CTRLINS", ctrl);

  if (command (ctrl) == COMPOSE && keyshift >= '@' && keyshift <= '_') {
	/* allow input of control char, esp ^^ */
	clear_status ();
	(void) insert_character (keyshift & 0x1F);
	return;
  } else {
	struct prefixspec * ps = lookup_prefix_char (ctrl);
	if (ps) {
		insert_accented (ps);
		return;
	}
  }

  if (ctrl == FUNcmd) {
	struct prefixspec * ps;
	keyshift |= ctrl_mask;
	ps = lookup_prefix (keyproc, keyshift);
	if (ps) {
		insert_accented (ps);
	} else {
		clear_status ();
		(* keyproc) ('\0');
	}
  } else if (ctrl < ' ' || ctrl == 0x7F) {
	clear_status ();
	if (ctrl != '\177') {
		ctrl = ctrl & '\237';
	}
	(void) insert_character (ctrl);
  } else if (ctrl == '#') {
	int finich;
	do {
		if (utf8_text) {
			finich = prompt_num_char (& ch, 0x7FFFFFFF);
			if (finich != ERRORS) {
				(void) insert_character (ch);
			}
		} else if (cjk_text || mapped_text) {
			finich = prompt_num_char (& ch, max_char_value ());
			if (finich != ERRORS) {
				if ((cjk_text && valid_cjk (ch, NIL_PTR))
				    || ch < 0x100) {
					(void) insert_character (ch);
				} else {
					ring_bell ();
					error ("Invalid character value");
				}
			}
		} else {
			finich = prompt_num_char (& ch, 0xFF);
			if (finich != ERRORS) {
				if (ch < 0x100) {
					(void) insert_character (ch);
				} else {
					ring_bell ();
					error ("Invalid character value");
				}
			}
		}
	} while (finich == ' ' && ! deliver_dont_insert);
  } else if (ctrl == ' ') {
	char mnemonic [maxPROMPTlen];
	if (get_string_uni ("Enter character mnemonic:", mnemonic, False, " ") 
		== ERRORS) {
		return;
	}
	ch = compose_mnemonic (mnemonic);
	clear_status ();
	(void) insert_character (ch);
  } else {
	static character buf [7];
	(void) utfencode (ctrl, buf);
	build_string (text_buffer, "Enter second composing character: %s..", buf);
	status_uni (text_buffer);
	ch = readcharacter_unicode ();
	if (ch == '\033' || ch == FUNcmd) {
		clear_status ();
		return;
	}
	ch = compose_chars (ctrl, ch);
	clear_status ();
	(void) insert_character (ch);
  }
}


/*
   Insert next char with defined accent composition pattern
   (invoked by function key).
 */
static
void
insert_prefix (prefunc)
  voidfunc prefunc;
{
  struct prefixspec * ps = lookup_prefix (prefunc, keyshift);
  if (ps) {
	insert_accented (ps);
  } else {
	status_msg ("Accent prefix with this shift state not assigned");
  }
}

void
COMPOSE ()
{
  insert_prefix (COMPOSE);
}

void
F5 ()
{
  insert_prefix (F5);
}

void
F6 ()
{
  insert_prefix (F6);
}

void
key_0 ()
{
  insert_prefix (key_0);
}

void
key_1 ()
{
  insert_prefix (key_1);
}

void
key_2 ()
{
  insert_prefix (key_2);
}

void
key_3 ()
{
  insert_prefix (key_3);
}

void
key_4 ()
{
  insert_prefix (key_4);
}

void
key_5 ()
{
  insert_prefix (key_5);
}

void
key_6 ()
{
  insert_prefix (key_6);
}

void
key_7 ()
{
  insert_prefix (key_7);
}

void
key_8 ()
{
  insert_prefix (key_8);
}

void
key_9 ()
{
  insert_prefix (key_9);
}


/*======================================================================*\
|*			Character transformation			*|
\*======================================================================*/

/*
   Search for wrongly encoded character.
   Searches for UTF-8 character in Latin-1 text or vice versa.
 */
void
search_wrong_enc ()
{
  LINE * prev_line;
  char * prev_text;
  LINE * lpoi;
  char * cpoi;
  int utfcount;
  unsigned long unichar;

  if (hop_flag > 0) {
	int text_offset = cur_text - cur_line->text;
	(void) CONV ();
	move_address (cur_line->text + text_offset, y);
  }

  prev_line = cur_line;
  prev_text = cur_text;
  lpoi = cur_line;
  cpoi = cur_text;
  while (True) {
	/* advance character pointer */
	if (* cpoi == '\n') {
		lpoi = lpoi->next;
		if (lpoi == tail) {
			lpoi = header->next;
			status_uni (">>>>>>>> Search wrapped around end of file >>>>>>>>");
		}
		cpoi = lpoi->text;
	} else {
		advance_char (& cpoi);
	}

	/* check for wrongly encoded character */
	if ((* cpoi & 0x80) != 0) {
		FLAG isutf;
		if ((* cpoi & 0xC0) == 0xC0) {
			utf8_info (cpoi, & utfcount, & unichar);
			isutf = UTF8_len (* cpoi) == utfcount
				&& (* cpoi & 0xFE) != 0xFE;
		} else {
			isutf = False;
		}
		if (isutf != utf8_text) {
			break;
		}
	}

	/* check search wrap-around */
	if (lpoi == prev_line && cpoi == prev_text) {
		status_msg ("No more wrong encoding found");
		return;
	}
  }
  move_address (cpoi, find_y (lpoi));
}

/*
   Replace character mnemonic with character.
   Prefer national characters according to parameter:
   g (German):	ae -> ä/�, oe -> ö/�, ue -> ü/�
   d (Danish)	ae -> æ/�, oe -> ø/�
   f (French)	oe -> œ/oe ligature
 */
void
UML (lang)
  char lang;
{
  unsigned long uc;
  unsigned long c1;
  unsigned long c2;
  char * cpoi = cur_text;

  unsigned long unichar = 0;

  keyshift = 0;
  hop_flag = 0;

  c1 = unicodevalue (cpoi);

  if (c1 == '&') {
	char mnemo [maxMSGlen];
	char * mp = mnemo + 1;
	char * text = cur_text + 1;
	int del_offset;

	if (* text == '#') {
		int count = 0;
		uc = 0;

		text ++;
		if (* text == 'x' || * text == 'X') {
			/* &#x20AC; -> € */
			text ++;
			/*format = "%lx";*/
			while (ishex (* text)) {
				uc = uc * 16 + hexval (* text ++);
				count ++;
			}
		} else {
			/* &#8364; -> € */
			/*format = "%ld";*/
			while (* text >= '0' && * text <= '9') {
				uc = uc * 10 + hexval (* text ++);
				count ++;
			}
		}
		/*if (sscanf (text, format, & uc) <= 0) {...*/
		if (! count) {
			ring_bell ();
			error ("Invalid character numeric");
			return;
		}

		if (cjk_text || mapped_text) {
			uc = encodedchar (uc);
		}
		if (* text == ';') {
			text ++;
		}
	} else {
		/* &quot; -> " */
		mnemo [0] = '&';
		while ((* text >= 'a' && * text <= 'z') || (* text >= 'A' && * text <= 'Z')) {
			* mp ++ = * text ++;
		}
		* mp = '\0';
		if (* text == ';') {
			text ++;
		}
		uc = compose_mnemonic (mnemo);
	}

	del_offset = text - cur_text;
	if (insert_character (uc)) {
		(void) delete_text (cur_line, cur_text, cur_line, cur_text + del_offset);
	}
/*
	if (uc == CHAR_UNKNOWN) {
		ring_bell ();
		error ("Unknown character mnemonic");
		return;
	} else if (uc == CHAR_INVALID) {
		ring_bell ();
		error ("Invalid character");
		return;
	} else if (delete_text (cur_line, cur_text, cur_line, text) != ERRORS) {
		insert_character (uc);
	}
*/

	return;
  }

  if (c1 == '%') {
	/* HTTP replacement back-conversion, e.g. %40 -> @ */
	char * chpoi = cpoi;
	int codelen = 2;
	int bytecount = 0;
	unsigned int bytecode;
	char allbytes [7];
	char * bytepoi = allbytes;
	unsigned long code = 0;
	while (bytecount < codelen && sscanf (chpoi, "%%%02X", & bytecode) > 0) {
		code = (code << 8) + bytecode;
		* bytepoi ++ = bytecode;
		* bytepoi = 0;
		bytecount ++;
		chpoi += 3;
		if (utf8_text) {
			if (bytecount == 1) {
				codelen = UTF8_len (bytecode);
			}
		} else if (cjk_text) {
			if (bytecount == 2) {
				codelen = CJK_len (allbytes);
			}
		} else {
			codelen = 1;
		}
	}

	if (bytecount == codelen) {
		int offset = cur_text - cur_line->text;
		(void) delete_text (cur_line, cpoi, cur_line, chpoi);
		(void) insert_text (cur_line, cur_text, allbytes);
		print_line (y, cur_line);
		move_address (cur_line->text + offset, y);
		MOVRT ();
	} else {
		error ("Invalid code sequence");
	}
	return;
  }

  if (* cpoi != '\n') {
	advance_char (& cpoi);
  }
  c2 = unicodevalue (cpoi);

  /* language-specific accent preferences */
  if (lang == 'i') {
	if (c1 == '\'' || c1 == (character) '�') {
		c1 = '`';
	} else if (c2 == '\'' || c2 == (character) '�') {
		c2 = '`';
	}
  }
  if (lang == 'e') {
	if (c2 == ',' && strchr ("aeiuAEIU", c1)) {
		c2 = ';';
	} else if (c1 == ',' && strchr ("aeiuAEIU", c2)) {
		c1 = ';';
	} else if (c2 == '-' && (c1 == 'd' || c1 == 'D')) {
		c2 = '/';
	} else if (c1 == '-' && (c2 == 'd' || c2 == 'D')) {
		c1 = '/';
	}
  }

  /* first, try mnemonic / accented conversion */
  uc = compose_chars (c1, c2);
  /* result is already converted to encoding */

  /* override with language-specific conversion preferences */
  if (lang == 'd') {
	if (c1 == 'a' && c2 == 'e') {
		unichar = (character) '�';	/* æ */
	} else if (c1 == 'A' && (c2 & ~0x20) == 'E') {
		unichar = (character) '�';	/* Æ */
	} else if (c1 == 'o' && c2 == 'e') {
		unichar = (character) '�';	/* ø */
	} else if (c1 == 'O' && (c2 & ~0x20) == 'E') {
		unichar = (character) '�';	/* Ø */
	}
  } else if (lang == 'f') {
	if (c1 == 'o' && c2 == 'e') {
		unichar = 0x0153;	/* œ */
	} else if (c1 == 'O' && (c2 & ~0x20) == 'E') {
		unichar = 0x0152;	/* Œ */
	}
  } else if (lang == 'g') {
	if (c1 == 'a' && c2 == 'e') {
		unichar = (character) '�';	/* ä */
	} else if (c1 == 'o' && c2 == 'e') {
		unichar = (character) '�';	/* ö */
	} else if (c1 == 'u' && c2 == 'e') {
		unichar = (character) '�';	/* ü */
	} else if (c1 == 'A' && (c2 & ~0x20) == 'E') {
		unichar = (character) '�';	/* Ä */
	} else if (c1 == 'O' && (c2 & ~0x20) == 'E') {
		unichar = (character) '�';	/* Ö */
	} else if (c1 == 'U' && (c2 & ~0x20) == 'E') {
		unichar = (character) '�';	/* Ü */
	} else if (c1 == 's' && c2 == 's') {
		unichar = (character) '�';	/* ß */
	}
  }
  if (unichar != 0) {	/* was explicitly set above */
	uc = charcode (unichar);
  }

  if (uc == CHAR_INVALID) {
	ring_bell ();
	error ("Invalid character");

  } else if (! no_char (uc)) {
	/* apply mnemonic conversion */
	if (utf8_text) {
		DCC (); DCC ();
		Sutf8 (uc);
	} else if (cjk_text && ! mapped_text) {
		DCC (); DCC ();
		(void) Scjk (uc);
	} else {
		if (uc < 0x100) {
			DCC (); DCC ();
			Sbyte (uc);
		} else {
			ring_bell ();
			error ("Invalid character");
		}
	}

  } else if (! CONV ()) {
	ring_bell ();
	error ("Unknown character mnemonic");
  }
}

/*
   Convert between UTF-8 and other encoding (from Latin-1 or to current).
 */
FLAG
CONV ()
{
  character c1 = * cur_text;
  int utfcount;
  unsigned long unichar = 0;

  if (utf8_text && ((character) * cur_text) >= 0x80) {
	/* convert Latin-1 -> UTF-8 */
	if (c1 >= 0xC0) {
		utf8_info (cur_text, & utfcount, & unichar);
		if (UTF8_len (c1) == utfcount && (c1 & 0xFE) != 0xFE) {
			ring_bell ();
			error ("Already a UTF-8 character");
			return True;
		}
	}
	if (delete_text (cur_line, cur_text, cur_line, cur_text + 1) == FINE) {
		Sutf8 (c1);
	}

	return True;

  } else if (! utf8_text && (c1 & 0xC0) == 0xC0) {
	/* convert UTF-8 -> Latin-1 / mapped / CJK */
	utf8_info (cur_text, & utfcount, & unichar);
	if (UTF8_len (c1) != utfcount || (c1 & 0xFE) == 0xFE) {
		ring_bell ();
		error ("Not a UTF-8 character");
		return True;
	}
	if (mapped_text || cjk_text) {
		unsigned long nc = encodedchar (unichar);
		if (no_char (nc)) {
			ring_bell ();
			error ("Cannot convert Unicode character");
			return True;
		} else {
			if (delete_text (cur_line, cur_text, cur_line, cur_text + utfcount) == FINE) {
				(void) insert_character (nc);
			}
			return False;
		}
	}
	if (unichar < 0x100) {
		if (delete_text (cur_line, cur_text, cur_line, cur_text + utfcount) == FINE) {
			(void) insert_character (unichar);
		}
	} else {
		ring_bell ();
		error ("Cannot encode Unicode character");
	}

	return True;

  } else {
	return False;
  }
}


/*======================================================================*\
|*			Case conversion					*|
\*======================================================================*/

/**
   Delete base character only of combined character, leave 
   combining accents. Called by case conversion functions.
 */
static
void
delete_basechar ()
{
  char * after_char = cur_text;
  int text_offset = cur_text - cur_line->text;

  advance_char (& after_char);
  (void) delete_text (cur_line, cur_text, cur_line, after_char);
  /* enforce proper placement of cursor on combining characters */
  move_address (cur_line->text + text_offset, y);
}


FLAG
iscombined_unichar (unichar, charpos, linebegin)
  unsigned long unichar;
  char * charpos;
  char * linebegin;
{
  if (isjoined (unichar, charpos, linebegin)) {
	return True;
  }
  if (iscombining_unichar (unichar)) {
	return True;
  }
  /* handle ARABIC TAIL FRAGMENT */
  if (unichar == 0xFE73 && charpos != linebegin) {
	unsigned long prev_unichar;
	char * sp;
	precede_char (& charpos, linebegin);
	prev_unichar = unicodevalue (charpos);
	sp = script (prev_unichar);
	if (streq (sp, "Arabic")) {
		return True;
	}
  }
  return False;
}


static
void
check_After (unichar)
  unsigned long unichar;
{
	if ((Turkish && unichar == 'I')			/* tr / az */
	   || (Lithuanian && soft_dotted (unichar))	/* lt */
	   ) {
		/* Handle U+0307 COMBINING DOT ABOVE 
		   After_Soft_Dotted / After_I
		   while handling the I or the soft dotted letter
		 */
		char * comb_char = cur_text;
		unsigned long unichar2;
		int utfcount;

		utf8_info (comb_char, & utfcount, & unichar2);
		while (iscombining_unichar (unichar2) && unichar2 != 0x0307) {
			advance_char (& comb_char);
			utf8_info (comb_char, & utfcount, & unichar2);
		}
		if (unichar2 == 0x0307) {
			char * after_char = comb_char;
			advance_char (& after_char);
			(void) delete_text (cur_line, comb_char, cur_line, after_char);
		}
	}
}


/**
   Convert lower and upper case letters
   dir == 0: toggle
   dir == 2: convert to title case
   dir == 1: convert to upper case
   dir == -1: convert to lower case
 */
static
void
lowcap (dir)
  int dir;
{
  short condition = 0;
  unsigned long unichar;

  if (* cur_text == '\n') {
	MOVRT ();
	return;
  }

  do {
	int tabix = 0;
	int prev_x;
	unsigned long convchar;
	unichar = unicodevalue (cur_text);

	/* initialize condition; do it inside the loop to avoid 
	   inconsistencies with Shift-F11, Shift-F3, dropping handling of 
	   language-specific cases after special cases (e.g. ßi)
	 */
	if (Turkish) {
		condition = U_cond_tr | U_cond_az;
	}
	if (Lithuanian) {
		condition = U_cond_lt;
	}

	if (dir >= 0 &&
		(	(unichar >= 0x3041 && unichar <= 0x3096)
		||	(unichar >= 0x309D && unichar <= 0x309F)
		||	unichar == 0x1B001 || unichar == 0x1F200
		)
	   ) {	/* Hiragana -> Katakana */
		if (unichar == 0x1B001) {
			convchar = 0x1B000;
		} else if (unichar == 0x1F200) {
			convchar = 0x1F201;
		} else {
			convchar = charcode (unichar + 0x60);
		}
		if (no_char (convchar)) {
			/* does not occur */
			ring_bell ();
			error ("Unencoded Katakana character");
			break;
		} else {
			prev_x = x;
			delete_basechar ();
			(void) insert_character (convchar);
			if (x == prev_x) {	/* may occur with combining chars */
				move_to (prev_x + 1, y);
			}
		}
	}
	else if (dir <= 0 &&
		(	(unichar >= 0x30A1 && unichar <= 0x30F6)
		||	(unichar >= 0x30FD && unichar <= 0x30FF)
		||	unichar == 0x1B000 || unichar == 0x1F201
		)
		) {	/* Katakana -> Hiragana */
		if (unichar == 0x1B000) {
			convchar = 0x1B001;
		} else if (unichar == 0x1F201) {
			convchar = 0x1F200;
		} else {
			convchar = charcode (unichar - 0x60);
		}
		if (no_char (convchar)) {
			/* can occur in Big5, Johab, UHC encodings
			   missing U+3094, U+3095, U+3096
			 */
			ring_bell ();
			error ("Unencoded Hiragana character");
			break;
		} else {
			prev_x = x;
			delete_basechar ();
			(void) insert_character (convchar);
			if (x == prev_x) {	/* may occur with combining chars */
				move_to (prev_x + 1, y);
			}
		}
	}
	else if ((tabix = lookup_caseconv_special (unichar, condition)) >= 0)
	{
	    char * after_char;
	    unsigned long unichar2;
	    condition = caseconv_special [tabix].condition &= ~ U_conds_lang;
	    if (condition & U_cond_Final_Sigma) {
		/** Final_Cased:
			Within the closest word boundaries containing 
			C, there is a cased letter before C, and there 
			is no cased letter after C.
			Before C [{cased=true}] [{word-Boundary≠true}]*
			After C !([{wordBoundary≠true}]* [{cased}]))
		 */
		after_char = cur_text;
		advance_char (& after_char);
		unichar2 = unicodevalue (after_char);
		while (iscombining_unichar (unichar2)) {
			advance_char (& after_char);
			unichar2 = unicodevalue (after_char);
		}
		if (unichar2 < (unsigned long) 'A'
		|| (unichar2 > (unsigned long) 'Z' && unichar2 < (unsigned long) 'a')
		|| (unichar2 > (unsigned long) 'z' && unichar2 < (unsigned long) (character) '�')
		) {	/* final position detected */
			condition &= ~ U_cond_Final_Sigma;
		}
	    }
	    if (condition & U_cond_Not_Before_Dot) {	/* tr / az */
		/** Before_Dot:
			C is followed by U+0307 COMBINING DOT ABOVE. 
			Any sequence of characters with a combining 
			class that is neither 0 nor 230 may intervene 
			between the current character and the 
			combining dot above.
			After C ([{cc≠230} & {cc≠0}])* [\u0307]
		 */
		after_char = cur_text;
		advance_char (& after_char);
		unichar2 = unicodevalue (after_char);
		while (iscombining_notabove (unichar2) && unichar2 != 0x0307) {
			advance_char (& after_char);
			unichar2 = unicodevalue (after_char);
		}
		if (unichar2 != 0x0307) {
			condition &= ~ U_cond_Not_Before_Dot;
		}
	    }
	    if (condition & U_cond_After_I) {	/* tr / az */
		/** After_I:
			The last preceding base character was an 
			uppercase I, and there is no intervening 
			Canonical_Combining_Class 230 (Above).
			Before C [I] ([{cc≠230} & {cc≠0}])*
		 */
		/* This case only works in separated display mode;
		   for combined mode see explicit handling below.
		 */
		after_char = cur_text;
		precede_char (& after_char, cur_line->text);
		unichar2 = unicodevalue (after_char);
		while (iscombining_notabove (unichar2) && after_char != cur_line->text) {
			precede_char (& after_char, cur_line->text);
			unichar2 = unicodevalue (after_char);
		}
		if (unichar2 == 'I') {
			condition &= ~ U_cond_After_I;
		}
	    }
	    if (condition & U_cond_After_Soft_Dotted) {	/* lt */
		/** After_Soft_Dotted:
			The last preceding character with a combining class 
			of zero before C was Soft_Dotted, and there is 
			no intervening Canonical_Combining_Class 230 (Above).
			Before C [{Soft_Dotted=true}] ([{cc≠230} & {cc≠0}])*
		 */
		/* This case only works in separated display mode;
		   for combined mode see explicit handling below.
		 */
		after_char = cur_text;
		precede_char (& after_char, cur_line->text);
		unichar2 = unicodevalue (after_char);
		while (iscombining_notabove (unichar2) && after_char != cur_line->text) {
			precede_char (& after_char, cur_line->text);
			unichar2 = unicodevalue (after_char);
		}
		if (soft_dotted (unichar2)) {
			condition &= ~ U_cond_After_Soft_Dotted;
		}
	    }
	    if (condition & U_cond_More_Above) {	/* lt */
		/** More_Above:
			C is followed by one or more characters of 
			Canonical_Combining_Class 230 (Above) 
			in the combining character sequence.
			After C [{cc≠0}]* [{cc=230}]
		 */
		after_char = cur_text;
		advance_char (& after_char);
		unichar2 = unicodevalue (after_char);
		while (iscombining_notabove (unichar2)) {
			advance_char (& after_char);
			unichar2 = unicodevalue (after_char);
		}
		if (iscombining_above (unichar2)) {
			condition &= ~ U_cond_More_Above;
		}
	    }

	    if (condition == 0) { /* no condition or condition resolved */
		FLAG do_convert = False;
		unsigned long convchar2;
		unsigned long convchar3;

		if (caseconv_special [tabix].base == caseconv_special [tabix].lower.u1)
		{	/* is lower, toggle or convert to upper */
		    if (dir == 2) {
			convchar = caseconv_special [tabix].title.u1;
			convchar2 = caseconv_special [tabix].title.u2;
			convchar3 = caseconv_special [tabix].title.u3;
			do_convert = True;
		    } else if (dir >= 0) {
			convchar = caseconv_special [tabix].upper.u1;
			convchar2 = caseconv_special [tabix].upper.u2;
			convchar3 = caseconv_special [tabix].upper.u3;
			do_convert = True;
		    }
		} else {	/* is upper, toggle or convert to lower */
		    if (dir <= 0) {
			convchar = caseconv_special [tabix].lower.u1;
			convchar2 = caseconv_special [tabix].lower.u2;
			convchar3 = caseconv_special [tabix].lower.u3;
			do_convert = True;
		    }
		}

		if (do_convert) {
			if (convchar != 0) {
			    convchar = charcode (convchar);
			    if (convchar2 != 0) {
				convchar2 = charcode (convchar2);
				if (convchar3 != 0) {
					convchar3 = charcode (convchar3);
				}
			    }
			}

			if (no_char (convchar) || no_char (convchar2) || no_char (convchar3)) {
				ring_bell ();
				error ("Unencoded case converted character(s)");
				break;
			} else {
				FLAG inserted_something = False;

				prev_x = x;
				delete_basechar ();
				if (convchar != 0) {
				    inserted_something = True;
				    (void) insert_character (convchar);
				    if (convchar2 != 0) {
					(void) insert_character (convchar2);
					if (convchar3 != 0) {
						(void) insert_character (convchar3);
					}
				    }
				}

				if (inserted_something) {
					check_After (unichar);

					if (x == prev_x) {
						/* may occur with combining chars */
						move_to (prev_x + 1, y);
					}
				}
			}
		} else {
			move_to (x + 1, y);
		}
	    } else {
		/* notify to try further */
		tabix = -1;
	    }
	}
	if (tabix == -1 && (tabix = lookup_caseconv (unichar)) >= 0)
	{
		convchar = unichar;
		if (dir == 2 && caseconv_table [tabix].title != 0) {
			convchar = caseconv_table [tabix].title;
		} else if (caseconv_table [tabix].toupper != 0) {
		    if (dir >= 0) {
			convchar = unichar + caseconv_table [tabix].toupper;
		    }
		} else {
		    if (dir <= 0) {
			convchar = unichar + caseconv_table [tabix].tolower;
		    }
		}

		convchar = charcode (convchar);
		if (no_char (convchar)) {
			ring_bell ();
			error ("Unencoded case converted character");
			break;
		} else {
			prev_x = x;
			delete_basechar ();
			(void) insert_character (convchar);

			if (Turkish || Lithuanian) {
				char * comb_char;
				move_to (prev_x, y);
				comb_char = cur_text;
				advance_char (& comb_char);
				move_address (comb_char, y);
			}
			check_After (unichar);

			if (x == prev_x) {	/* may occur with combining chars */
				move_to (prev_x + 1, y);
			}
		}
	}
	else if (tabix == -1)
	{
		move_to (x + 1, y);
	}
  } while ((hop_flag > 0 && idfchar (cur_text))
	  || (Dutch && (unichar == 'I' || unichar == 'i')
		    && (* cur_text == 'J' || * cur_text == 'j')
	     )
	  );
}

/**
   Toggle lower and upper case letters
 */
void
LOWCAP ()
{
  lowcap (0);
}

/**
   Convert to lower case letters
 */
void
LOWER ()
{
  lowcap (-1);
}

/**
   Convert to upper case letters
 */
void
UPPER ()
{
  lowcap (1);
}

/**
   Convert single character to upper case letter, then skip word (emacs)
 */
void
CAPWORD ()
{
  hop_flag = 0;

  lowcap (1);
  MOVLF ();
  MNW ();
}

/**
   Toggle low/cap/all cap (Windows)
 */
void
LOWCAPWORD ()
{
  char * cp = cur_line->text;
  char * first_alpha = NIL_PTR;
  FLAG found = False;
  FLAG first_cap = False;
  FLAG first_title = False;
  FLAG subseq_cap = False;
  FLAG subseq_small = False;
  int letters = 0;
#ifdef hop_title_case
  int upper_type = hop_flag > 0 ? 2 : 1;	/* HOP -> title case */
#endif
  unsigned long prev_uc = 0;

  while (* cp != '\0' && * cp != '\n') {
	unsigned long uc = unicodevalue (cp);

	if (Dutch && prev_uc == 'I' && uc == 'J') {
		advance_char (& cp);
		uc = unicodevalue (cp);
	}

	if (cp == cur_text) {
		found = True;
	}
	if (idfchar (cp) /* && * cp != '_' && * cp != '$' */) {
		/* idfchar includes categories "Letter" and "Mark" 
		   and thus all combining characters */
		/* check:
			any subsequent letter capital → make all letters small
			first letter capital → make all letters capital
			(all letters small) → make first letter capital
		   consider the following Unicode categories as upper:
			Letter, uppercase
			Letter, titlecase
		   (based on caseconv_table [...].tolower != 0)
		   and these as lower or insignificant:
			Letter, other
			Letter, lowercase
			Letter, modifier
			all others
		*/
		FLAG iscapital = False;
		FLAG issmall = False;
		int tabix = lookup_caseconv (uc);
		if (tabix >= 0) {
			iscapital = caseconv_table [tabix].tolower != 0;
			issmall = caseconv_table [tabix].toupper != 0;
		}

		if (first_alpha == NIL_PTR) {
			first_alpha = cp;
			if (iscapital) {
				first_cap = True;
			}
			first_title = caseconv_table [tabix].title == uc;
		} else {
			if (iscapital) {
				subseq_cap = True;
			}
			if (issmall) {
				subseq_small = True;
			}
		}

		letters ++;
	} else if (found) {
		/* word has been scanned */
		break;
	} else {
		/* word has not yet been passed; reset info */
		first_alpha = NIL_PTR;
		first_cap = False;
		subseq_cap = False;
		letters = 0;
	}

	prev_uc = uc;
	advance_char (& cp);
  }

  if (found && first_alpha != NIL_PTR) {
	int offset = cur_line->shift_count * SHIFT_SIZE + x;
	unsigned long uc;
	char * sn;

	move_address (first_alpha, y);
	uc = unicodevalue (cur_text);
	sn = script (uc);
	if (streq (sn, "Hiragana")) {
		hop_flag = 1;
		lowcap (1);
	} else if (streq (sn, "Katakana")) {
		hop_flag = 1;
		lowcap (-1);
	} else if (subseq_cap || (first_cap && ! first_title && ! subseq_small)) {
		hop_flag = 1;
		lowcap (-1);
	} else if (first_cap) {
		hop_flag = 1;
		lowcap (1);
	} else {
		hop_flag = 0;
		lowcap (2);
	}
	move_to (offset - cur_line->shift_count * SHIFT_SIZE, y);
  }
}


/*======================================================================*\
|*			Character/Code conversion			*|
\*======================================================================*/

/**
   AltX toggles text left of cursor between character and Unicode value
   * sequence of 2 <= n <= 6 hex digits, value <= 10FFFF -> Unicode character
   * no-digit character -> hex Unicode value, represented with >= 4 digits
 */
void
AltX ()
{
  char * cp = cur_text;
  char * pp = cp;
  char * pp1;
  int n = 0;
  unsigned long c = 0;

  if (cur_text == cur_line->text) {
	return;
  }

  precede_char (& cp, cur_line->text);
  pp1 = cp;

  while (n < 6 && cp != pp && ishex (* cp)) {
	n ++;
	pp = cp;
	precede_char (& cp, cur_line->text);
  }

  if (pp != cur_text && n >= 2) {
	/* hex value found */
	char * hp = pp;
	while (hp != cur_text) {
		c = ((c << 4) + hexval (* hp));
		hp ++;
	}
	if (c > (unsigned long) 0x10FFFF) {
		n = 1;
	}
  }
  if (n >= 2) {
	/* hex -> uni */

	(void) delete_text (cur_line, pp, cur_line, cur_text);

	/* sequence of 2 <= n <= 6 hex digits, value <= 10FFFF -> Unicode character */
	if (insert_unichar (c)) {
	}
  } else {
	/* uni -> hex */
	unsigned long cv = unicodevalue (pp1);

	/* no-digit character -> hex Unicode value, represented with >= 4 digits */
	if (! no_char (cv)) {
		move_address (pp1, y);
		delete_char (False);

		insertunicode (cv);
	}
  }
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
