/*======================================================================*\
|*		Editor mined						*|
|*		text buffer functions					*|
\*======================================================================*/

#include "mined.h"
#include "charprop.h"


/*======================================================================*\
|*			Text buffer routines				*|
\*======================================================================*/

#define dont_debug_move

#define dont_debug_adjustlineno


/*
 * Proceed returns the count'th line after 'line'. When count is negative
 * it returns the count'th line before 'line'. When the next (previous)
 * line is the tail (header) indicating EOF (TOF) it stops.
 */
LINE *
proceed (line, count)
  register LINE * line;
  register int count;
{
  if (count < 0) {
	while (count ++ < 0 && line != header) {
		line = line->prev;
	}
  } else {
	while (count -- > 0 && line != tail) {
		line = line->next;
	}
  }
  return line;
}

/*
 * Reset assigns bot_line, top_line and cur_line according to 'head_line'
 * which must be the first line of the screen, and a y-coordinate,
 * which will be the current y-coordinate (if it isn't larger than last_y)
 */
void
reset (head_line, screen_y)
  LINE * head_line;
  int screen_y;
{
  register LINE * line;
  LINE * new_cur_line;
  LINE * line_up;
  LINE * line_dn;
  int up = 0;
  int dn = 0;

  top_line = line = head_line;

/* Search for bot_line (might be last line in file) */
  for (last_y = 0; 
	last_y < total_lines - 1 && last_y < SCREENMAX && line->next != tail; 
	last_y ++
      ) {
	line = line->next;
  }

  bot_line = line;
  y = (screen_y > last_y) ? last_y : screen_y;

/* Set cur_line according to the new y value */
  new_cur_line = proceed (top_line, y);

/* Adjust current line number, avoiding to scan the whole buffer */
  if (head_line == header->next) {
	cur_line = head_line;
	line_number = 1;
#ifdef debug_adjustlineno
	printf ("line_number = 1\n");
#endif
  } else if (bot_line == tail->prev) {
	cur_line = bot_line;
	line_number = total_lines;
#ifdef debug_adjustlineno
	printf ("line_number = %d\n", line_number);
#endif
  }

  line_up = line_dn = cur_line;
  do {
	if (line_up == new_cur_line) {
		line_number += up;
#ifdef debug_adjustlineno
		printf ("line_number += %d\n", up);
#endif
		break;
	} else if (line_up != tail->prev) {
		line_up = line_up->next;
		up ++;
	}
	if (line_dn == new_cur_line) {
		line_number -= dn;
#ifdef debug_adjustlineno
		printf ("line_number -= %d\n", dn);
#endif
		break;
	} else if (line_dn != header->next) {
		line_dn = line_dn->prev;
		dn ++;
	}
  } while (True);

  cur_line = new_cur_line;
}


static
int
iscombined_display (unichar, charpos, linebegin)
  unsigned long unichar;
  char * charpos;
  char * linebegin;
{
  return iscombined (unichar, charpos, linebegin) &&
	 ! (separate_isolated_combinings && (charpos == linebegin || * (charpos - 1) == '\t'));
}

/*
 * Find_x () returns the x coordinate belonging to address.
 * (Tabs are expanded).
 */
int
find_x (line, address)
  LINE * line;
  char * address;
{
  char * textp = line->text;
  register int x_left = line->shift_count * - SHIFT_SIZE;
  int x_in_line = 0;	/* must start from 0 to calculate correct 
			tab positions (since SHIFT_SIZE is not guaranteed 
			to be a multiple of tabsize (usually 8)) */
	/* Alright, SHIFT_SIZE is now guaranteed to be a multiple of 8 
	   due to lots of display problems related to that matter.
	   Leave this code anyway. */
  unsigned long unichar;
  int x_before = 0;
  int prevx;

  while (textp < address && * textp != '\n' && * textp != '\0') {
	/* use "<" rather than "!=" because address may point amidst 
	   a UTF-8 character sequence after mode switching */
	prevx = x_in_line;
	advance_char_scr (& textp, & x_in_line, line->text);
	/* determine screen pos. to stay on if moving on a combining char. */
	if (x_in_line > prevx) {
		x_before = prevx;
	}
  }
#ifdef debug_move
  if (textp < address && * textp == '\n') {
	printf ("move to address stopped at eol\n");
  }
#endif

  /* if on combining character, skip to base character */
  if (combining_mode && encoding_has_combining ()) {
	unichar = unicodevalue (textp);
	if (iscombined_display (unichar, textp, line->text)) {
		x_in_line = x_before;
	}
  }

#ifdef debug_move
  printf ("find_x %08X -> %d\n", address, x_in_line + x_left);
#endif
  return x_in_line + x_left;
}

static int old_x = 0;	/* previous x position */

/*
 * Find_address () returns the pointer in the line with given offset.
 * (Tabs are expanded).
 * find_address is only called by move_it ()
tab (cnt)		is	(((cnt) + tabsize) & ~07)
 */
static
char *
find_address (line, new_x, cur_x)
  LINE * line;
  int new_x;
  int * cur_x;
{
  char * textp = line->text;
  int tx = line->shift_count * - SHIFT_SIZE;
  char * prev_poi;
  int prev_x;

#define navigate_tab_to_closer_side

#ifdef debug_move
  printf ("find_address (old %d) new %d (from %d)\n", old_x, new_x, * cur_x);
#endif
  while (tx < new_x && * textp != '\n') {
	if (ebcdic_text ? * textp == code_TAB : * textp == '\t') {
		if (new_x == old_x /* (* cur_x) */ - 1 && tab (tx) > new_x) {
			/* Moving left over tab */
#ifdef debug_move
			printf ("tab break 1\n");
#endif
			break;
		} else if (tab_left && new_x == old_x && tab (tx) > new_x) {
			/* Moving up/down on tab; stay left */
#ifdef debug_move
			printf ("tab break 2\n");
#endif
			break;
		} else if (! tab_right && new_x == old_x
#ifdef navigate_tab_to_closer_side
			   && tab (tx) - * cur_x > * cur_x - tx
#else
			   && tab (tx) - new_x > tabsize / 2
#endif
			   && tab (tx) > old_x /* prevent sticking left */
			  ) {
			/* Moving up/down on tab; stay left */
#ifdef debug_move
			printf ("tab break 3\n");
#endif
			break;
		} else {
			tx = tab (tx);
		}
		textp ++;
	} else {
		prev_poi = textp;
		prev_x = tx;

		advance_char_scr (& textp, & tx, line->text);

		if (combining_mode) {
			/* skip over combining characters */
			unsigned long unichar = unicodevalue (textp);
			while ((encoding_has_combining ()
				&& iscombined_display (unichar, textp, line->text)
			       )
			       || isspacingcombining_unichar (unichar)
			       || /* handle ARABIC TAIL FRAGMENT */
			       (unichar == 0xFE73
				&& iscombined_unichar (unichar, textp, line->text)
			       )
			      ) {
				advance_char_scr (& textp, & tx, line->text);
				unichar = unicodevalue (textp);
			}
		}

		if (tx > new_x && new_x < * cur_x && new_x < old_x) {
			/* moving left into wide character; stay before it */
			textp = prev_poi;
			tx = prev_x;
			new_x = tx;
		} else if (tab_left && tx > new_x && new_x == old_x) {
			/* moving up/down on wide character; stay left */
			textp = prev_poi;
			tx = prev_x;
			new_x = tx;
		}
	}
  }
#ifdef debug_move
  printf ("find_address: (old %d) new %d (from %d) to %d -> %08X\n", 
	  old_x, new_x, * cur_x, tx, textp);
#endif
  * cur_x = tx;
  return textp;
}

#define LINE_ANYWHERE LINE_START - 1

/*
 * move_to: move to given coordinates on screen.
 * move_y: move to given line on screen, staying in last explicit column.
 * move_address: move to given line at given text position.
 * The caller must check that scrolling is not needed.
 * If new x-position is < 0 or > XBREAK, move_it () will check if
 * the line can be shifted. If it can it sets (or resets) the shift_count
 * field of the current line accordingly. By this mechanism, the
 * pseudo-x-positions LINE_START / LINE_END (a very small / big value)
 * perform the appropriate positioning actions.
 * Move also sets cur_text to the right char.
 * "If we're moving to the same x coordinate, try to move to the x-coordinate
 * used on the other previous call." -- This worked erroneously and was 
 * replaced by an explicit old_x variable and move_y call.
 * move_address is directly called by move_next/previous_word(), 
 * re_search(), RDwin(), load_file(), justi(), JUSandreturn()
 */
static
void
move_it (new_x, new_address, new_y)
  register int new_x;
  int new_y;
  char * new_address;
{
  register LINE * line = cur_line;	/* For building new cur_line */
  register int lineno = line_number;
  int shift = 0;			/* How many shifts to make */
  char * utf_search;
  char * utf_prev;
  char * char_begin;
  int new_x_requested = new_x;		/* for rectangular selection */

/*  static int rel_x = 0;	*/	/* Remember relative x position */
/*	This was used as a trick to stay virtually in the previous column 
	even when moving across shorter lines; but it had >= 2 errors.
	Renamed to old_x, made globally accessible and explicitly used 
	by appropriate calls to avoid these problems. TW */
  int tx = x;

  if (new_x == LINE_ANYWHERE) {
  /* fix move_address indicator */
	new_x = 0;
  }

/* Check for illegal values */
  if (new_y < 0 || new_y > last_y) {
	return;
  }

/* Adjust y-coordinate and cur_line */
  if (new_y < y) {
	while (y != new_y) {
		y --;
		line = line->prev;
		lineno --;
	}
  } else {
	while (y != new_y) {
		y ++;
		line = line->next;
		lineno ++;
	}
  }

/* Set or unset relative x-coordinate */
  if (new_address == NIL_PTR) {
	new_address = find_address (line, new_x, & tx);
	new_x = tx;
  } else {
	/* rel_x = */ new_x = find_x (line, new_address);
  }

/* Adjust on character boundary */
  if (cjk_text) {
	char_begin = charbegin (line->text, new_address);
	if (char_begin != new_address) {
		/* adjust position which is currently within a 
		   CJK double-width, multi-byte character */
		if (new_x >= x) {
			if (* new_address != '\n') {
				new_x ++;
				new_address = char_begin;
				advance_char (& new_address);
			} else {
				/* incomplete CJK half character */
			}
		} else {
			new_x --;
			new_address = char_begin;
		}
	}
  } else if (utf8_text) {
	if ((* new_address & 0xC0) == 0x80) {	/* UTF-8 sequence byte? */
	/* adjust position which is currently within a UTF-8 character */
	/* adjust new_x for multi-width characters ? */
		utf_search = line->text;
		utf_prev = utf_search;
		while (utf_search < new_address) {
			utf_prev = utf_search;
			advance_utf8 (& utf_search);
		}
		if (utf_search != new_address) {
			if (new_x >= x) {
				new_address = utf_search;
			} else {
				new_address = utf_prev;
			}
		}
	}
  }

/* Adjust shift_count if new_x lower than 0 or higher than XBREAK */
/* Allow adjustment also if new_x == 0 to enable left shift mark */
  if (new_x <= 0
      || new_x >= XBREAK
      || (new_x == XBREAK - 1 && iswide (unicodevalue (new_address)))
     ) {
	if (new_x > XBREAK
	    || (new_x == XBREAK && * new_address != '\n')
	    || new_x == XBREAK - 1
	   ) {
		shift = (new_x - XBREAK) / SHIFT_SIZE + 1;
	} else {
		shift = new_x / SHIFT_SIZE;
		if (new_x % SHIFT_SIZE) {
			shift --;
		}
		if (new_x == 0 && line->shift_count != 0 && marker_defined (SHIFT_BEG_marker, UTF_SHIFT_BEG_marker)) {
			/* do not stay on left shift marker; adjust line */
			shift --;
		}
	}

	if (shift != 0) {
		line->shift_count += shift;
		new_x = find_x (line, new_address);
		if (new_x == 0 && line->shift_count != 0 && marker_defined (SHIFT_BEG_marker, UTF_SHIFT_BEG_marker)) {
			/* do not stay on left shift marker; adjust line */
			line->shift_count --;
			new_x = find_x (line, new_address);
		}
		print_line (y, line);
		/* rel_x = new_x; */
	}
  }

/* Assign and position cursor */
  x = new_x;
  cur_text = new_address;
  cur_line = line;
  line_number = lineno;
#ifdef debug_adjustlineno
  printf ("line_number = %d\n", line_number);
#endif

/* Update selection highlighting */
  if (new_x_requested < 0 || new_x_requested == LINE_END) {
	update_selection_marks (x);
  } else {
	update_selection_marks (new_x_requested);
  }

  /* don't display_flush () here */

  set_cursor_xy ();
}

void
move_y (ny)
  register int ny;
{
  move_it (old_x, NIL_PTR, ny);
}

void
move_to (nx, ny)
  register int nx;
  register int ny;
{
  old_x = x;
  move_it (nx, NIL_PTR, ny);
  old_x = x;
}

void
move_address (nadd, ny)
  register char * nadd;
  register int ny;
{
  old_x = x;
  move_it (LINE_ANYWHERE, nadd, ny);
  old_x = x;
}

void
move_address_w_o_RD (nadd, ny)
  register char * nadd;
  register int ny;
{
  old_x = x;
  move_it (LINE_ANYWHERE, nadd, ny);
  old_x = x;
}


/*
 * find_y () checks if the matched line is on the current page. If it is, it
 * returns the new y coordinate, else it displays the correct page with the
 * matched line in the middle (unless redrawflag is set to False) 
 * and returns the new y value.
 */
static
int
find_y_RD (match_line, redrawflag)
  LINE * match_line;
  FLAG redrawflag;
{
  register LINE * line;
  register int count = 0;

/* Check if match_line is on the same page as currently displayed. */
  for (line = top_line; line != match_line && line != bot_line->next;
						      line = line->next) {
	count ++;
  }
  if (line != bot_line->next) {
	return count;
  }

/* Display new page, with match_line in center. */
  if ((line = proceed (match_line, - (SCREENMAX >> 1))) == header) {
  /* Can't display in the middle. Make first line of file top_line */
	count = 0;
	for (line = header->next; line != match_line; line = line->next) {
		count ++;
	}
	line = header->next;
  } else {	/* New page is displayed. Set cursor to middle of page */
	count = SCREENMAX >> 1;
  }

/* Reset pointers and redraw the screen */
  reset (line, 0);
  if (redrawflag) {
	RD_y (count);
	redraw_pending = False;
  } else {
	redraw_pending = True;
  }

  return count;
}

int
find_y (match_line)
  LINE * match_line;
{
  return find_y_RD (match_line, True);
}

int
find_y_w_o_RD (match_line)
  LINE * match_line;
{
  return find_y_RD (match_line, False);
}


/*
   Determine current character position within line (# characters).
 */
int
get_cur_pos ()
{
  int cur_pos = 0;
  char * cpoi = cur_line->text;
  while (* cpoi != '\0' && cpoi < cur_text) {
	advance_char (& cpoi);
	cur_pos ++;
  }
  return cur_pos;
}

/*
   Return current column position within line (# screen columns + line shift).
 */
int
get_cur_col ()
{
  return cur_line->shift_count * SHIFT_SIZE + x;
}


/*======================================================================*\
|*			Text/file information				*|
\*======================================================================*/

void
recount_chars ()
{
  LINE * line;
  total_chars = 0L;
  for (line = header->next; line != tail; line = line->next) {
	total_chars += char_count (line->text);
	if (line->return_type == lineend_NONE) {
		total_chars --;
	}
  }
}

/*
 * Display a line telling how many chars and lines the file contains. Also tell
 * whether the file is readonly and/or modified.
called via fstatus macro:
fstatus (mess, bytes, chars) is
	file_status ((mess), (bytes), (chars), file_name, \
		     total_lines, True, writable, modified, viewonly)
also called directly from WB
*/
void
file_status (message, bytecount, charcount, filename, lines, textstat, writefl, changed, viewing)
  char * message;
  register long bytecount;	/* Contains number of bytes in file */
  register long charcount;	/* Contains number of characters in file */
  char * filename;
  int lines;
  FLAG textstat;
  FLAG writefl;
  FLAG changed;
  FLAG viewing;
{
#ifdef old_approach_full_screening_every_time
  LINE * line;
  int line_num = 0;
  int my_line_number = -1;
#endif
  char fileinfo [maxPROMPTlen + maxFILENAMElen];
  char flaginfo [maxPROMPTlen];
  char textinfo [maxPROMPTlen];
  char * filesep = "";

  if (* message) {
	filesep = " ";
  }

  if (reading_pipe && * message) {
	filename = "[standard input]";
  } else if (! * filename) {
	filename = "[no file]";
  }

#ifdef old_approach_full_screening_every_time
  /* determine information */
  if (bytecount < 0) {	/* # chars not given; count them */
	bytecount = 0L;
	charcount = 0L;
	for (line = header->next; line != tail; line = line->next) {
		bytecount += length_of (line->text);
		charcount += char_count (line->text);
		if (line->return_type == lineend_NONE) {
			charcount --;
			bytecount --;
		} else if (line->return_type == lineend_CRLF
			|| line->return_type == lineend_NL2) {
			bytecount ++;
		} else if (line->return_type == lineend_LS
			|| line->return_type == lineend_PS) {
			bytecount += 2;
		}
		line_num ++;
		if (line == cur_line) {
			my_line_number = line_num;
		}
	}
  } else {		/* check for current line # only */
	for (line = header->next; line != cur_line->next; line = line->next) {
		line_num ++;
		if (line == cur_line) {
			my_line_number = line_num;
		}
	}
  }
#endif

  build_string (fileinfo, "%s%s%s", message, filesep, filename);
  build_string (flaginfo, "%s -%s%s ",
		writefl ? file_is_fifo ? " (pipe)"
					: ""
			: file_is_dir ? " (directory)"
				: file_is_dev ? " (device file)"
						: " (readonly)",
		viewing ? " (view only)" : "",
		changed ? " (modified)" : "");

  if (textstat) {
	if (total_chars < 0) {
		recount_chars ();
	}
	build_string (textinfo, "%sline %d char %d (total %d/%ld)%s%s",
		flaginfo,
		line_number,
		get_cur_pos () + 1,
		total_lines, total_chars,

		(lines_per_page != 0) ? ", page " : "",
		(lines_per_page != 0)
			? num_out ((line_number - 1) / lines_per_page + 1, 10)
			: ""
		);
  } else {
	build_string (textinfo, "%slines %d chars %ld (bytes %ld)",
		flaginfo,
		lines,
		charcount,
		bytecount
		);
  }

  /* display the information; note that msg contains filename characters */
  status_fmt2 (fileinfo, textinfo);
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
