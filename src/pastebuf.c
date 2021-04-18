/*======================================================================*\
|*		Editor mined						*|
|*		Paste buffer handling					*|
\*======================================================================*/

#include "mined.h"
#include "io.h"		/* for flush, set_cursor, highlight_selection */
#include "textfile.h"
#include "charprop.h"

#include <errno.h>


/*======================================================================*\
|*			System file link behaviour			*|
\*======================================================================*/

#ifdef unix
#define linkyank
#endif

#ifdef __pcgcc__
#undef linkyank
#endif
#ifdef __MINGW32__
#undef linkyank
#endif
#ifdef __CYGWIN__
#define linkyank
#endif


/*======================================================================*\
|*			Global variables				*|
\*======================================================================*/

FLAG yank_status = NOT_VALID;	/* Status of yank_file */
static FLAG html_status = NOT_VALID;	/* Status of html_file */
static int yank_buf_no = 0;	/* Buffer # for trials and multiple buffers */
static int max_yank_buf_no = 0;	/* Max Buffer # used */

/* pasted area markers for "Paste previous" function */
static LINE * pasted_start_line = NIL_LINE;
static char * pasted_start_textp = NIL_PTR;
static LINE * pasted_end_line = NIL_LINE;
static char * pasted_end_textp = NIL_PTR;

int buffer_open_flag = 0;	/* Counter flag for the collective buffer */

static FLAG highlight_selection = False;
static FLAG alt_rectangular_mode = False;
#define rectangular_paste_mode (rectangular_paste_flag != alt_rectangular_mode)

static int last_sel_x = 0;	/* Last selection mouse column */


/*======================================================================*\
|*			Marker stack					*|
\*======================================================================*/

/* default marker */
static LINE * mark_line = NIL_LINE;		/* For marking position. */
static char * mark_text = NIL_PTR;

/* explicit markers */
static struct {
	LINE * line;
	char * text;
} marker_n [] = {
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
	{NIL_LINE, NIL_PTR},
};
#define maxmarkers arrlen (marker_n)

/* implicit marker stack */
static struct {
	LINE * line;
	char * text;
	char * file;
	int lineno;
	int pos;
	FLAG detectCR;
} mark_stack [] = {
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
	{NIL_LINE, NIL_PTR, NIL_PTR, -1, -1, False},
};
#define markstacklen arrlen (mark_stack)
static int mark_stack_poi = 0;
static int mark_stack_top = 0;
static int mark_stack_bottom = 0;
static int mark_stack_count_top = 0;
static int mark_stack_count_poi = 0;

#define dont_debug_mark_stack

#ifdef debug_mark_stack
#define printf_mark_stack(s)	printf ("%s - mark_stack [%d..%d(%d)] @ %d(%d)\n", s, mark_stack_bottom, mark_stack_top, mark_stack_count_top, mark_stack_poi, mark_stack_count_poi)
#define printf_debug_mark_stack(s)	printf (s)
#else
#define printf_mark_stack(s)	
#define printf_debug_mark_stack(s)	
#endif


static
char *
copied_file_name ()
{
  int i;
  char * dup;
  char * filei;

  /* check if file name already in stack */
  for (i = 0; i < markstacklen; i ++) {
	filei = mark_stack [i].file;
	if (filei != NIL_PTR && streq (filei, file_name)) {
		/* reuse the copy; note: it's not being freed ! */
		return filei;
	}
  }

  /* make a new copy of file name string */
  dup = alloc (strlen (file_name) + 1);
  if (dup != NIL_PTR) {
	strcpy (dup, file_name);
  }
  return dup;
}

/*
   Setmark sets the current position into the current slot of the mark stack.
 */
static
FLAG
Setmark ()
{
  char * fn = copied_file_name ();
  if (fn) {
	mark_stack [mark_stack_poi].line = cur_line;
	mark_stack [mark_stack_poi].text = cur_text;
	mark_stack [mark_stack_poi].file = fn;
	mark_stack [mark_stack_poi].lineno = line_number;
	mark_stack [mark_stack_poi].pos = get_cur_pos ();
	mark_stack [mark_stack_poi].detectCR = lineends_detectCR;

	printf_mark_stack ("Filled");
	return True;
  } else {
	return False;
  }
}

/*
   Pushmark pushes the current position to the mark stack.
 */
void
Pushmark ()
{
  /* truncate stack to current position */
  mark_stack_top = mark_stack_poi;
  mark_stack_count_top = mark_stack_count_poi;

  /* fill data */
  if (Setmark ()) {

	/* increase top of stack */
	mark_stack_top = (mark_stack_top + 1) % markstacklen;
	/* leave 1 slot empty for Setmark called in Popmark */
	if (mark_stack_count_top < markstacklen - 1) {
		mark_stack_count_top ++;
	} else {
		mark_stack_bottom = (mark_stack_bottom + 1) % markstacklen;
	}

	/* set current position to new top of stack */
	mark_stack_poi = mark_stack_top;
	mark_stack_count_poi = mark_stack_count_top;

	printf_mark_stack ("Pushed");
  } else {
	ring_bell ();

	printf_mark_stack ("Not Pushed - no mem");
  }
}

/*
   Upmark moves one position up the stack (toward top).
 */
void
Upmark ()
{
  hop_flag = 1;
  Popmark ();
}

/*
   Popmark moves one position down the stack (towards bottom).
 */
void
Popmark ()
{
  FLAG switch_files;

  if (hop_flag > 0) {
	/* climb up stack towards top */
	printf_mark_stack ("HOP Pop");
	if (mark_stack_count_poi == mark_stack_count_top) {
		printf_debug_mark_stack ("HOP Pop no more\n");
		error ("No more stacked positions");
		return;
	}
	/* adjust current position */
	if (Setmark ()) {
		mark_stack_poi = (mark_stack_poi + 1) % markstacklen;
		mark_stack_count_poi ++;

		printf_mark_stack ("Up:");
	} else {
		ring_bell ();

		printf_mark_stack ("Not Up: - no mem");
	}
  } else {
	/* climb down stack towards bottom */
	if (mark_stack_count_poi == 0) {
		printf_debug_mark_stack ("Pop no more\n");
		error ("No more stacked positions");
		return;
	}
	/* adjust current position */
	if (Setmark ()) {
		if (mark_stack_poi == 0) {
			mark_stack_poi = markstacklen;
		}
		mark_stack_poi --;
		mark_stack_count_poi --;

		printf_mark_stack ("Dn:");
	} else {
		ring_bell ();

		printf_mark_stack ("Not Dn: - no mem");
	}
  }

  if (mark_stack [mark_stack_poi].file == NIL_PTR) {
	printf_debug_mark_stack ("not valid\n");
	error ("Stacked position not valid");
	return;
  }
  switch_files = ! streq (mark_stack [mark_stack_poi].file, file_name);
  if (switch_files ||
	checkmark (mark_stack [mark_stack_poi].line, 
		mark_stack [mark_stack_poi].text) == NOT_VALID)
  {
	int mark_lineno;
	LINE * open_line;

	if (switch_files) {
		lineends_detectCR = mark_stack [mark_stack_poi].detectCR;
		if (save_text_load_file (mark_stack [mark_stack_poi].file) == ERRORS) {
			return;
		}
	}

	mark_lineno = mark_stack [mark_stack_poi].lineno - 1;
	open_line = proceed (header->next, mark_lineno);
	if (open_line == tail) {
		EFILE ();
		error ("Stacked position not present anymore");
	} else {
		int mark_col = mark_stack [mark_stack_poi].pos;
		int cur_pos = 0;
		char * cpoi;

		move_to (0, find_y (open_line));
		cpoi = cur_line->text;
		while (* cpoi != '\n' && cur_pos < mark_col) {
			advance_char (& cpoi);
			cur_pos ++;
		}
		move_address (cpoi, y);
	}
  } else {
	clear_highlight_selection ();

	move_address (mark_stack [mark_stack_poi].text, 
			find_y (mark_stack [mark_stack_poi].line));
  }
}


/*======================================================================*\
|*			Basic Paste operations				*|
\*======================================================================*/

/*
 * Legal () checks if mark_text is still a valid pointer.
 */
static
int
legal (mark_line, mark_text)
  register LINE * mark_line;
  register char * mark_text;
{
  register char * textp = mark_line->text;

/* Locate mark_text on mark_line */
  while (textp != mark_text && * textp != '\0') {
	textp ++;
  }
  return (* textp == '\0') ? ERRORS : FINE;
}

/*
 * Check_mark () checks if mark_line and mark_text are still valid pointers.
 * If they are it returns
 * SMALLER if the marked position is before the current,
 * BIGGER if it isn't or SAME if somebody didn't get the point.
 * NOT_VALID is returned when mark_line and/or mark_text are no longer valid.
 * Legal () checks if mark_text is valid on the mark_line.
 */
FLAG
checkmark (mark_line, mark_text)
  LINE * mark_line;
  char * mark_text;
{
  LINE * lineup;
  LINE * linedown;
  FLAG do_continue;

  if (mark_line == NIL_LINE || legal (mark_line, mark_text) == ERRORS) {
	/* mark_text dangling (not pointing into mark_line) */
	return NOT_VALID;
  }

  if (mark_line == cur_line) {
	if (mark_text == cur_text) {
		return SAME;
	} else if (mark_text < cur_text) {
		return SMALLER;
	} else {
		return BIGGER;
	}
  }

  /* search for mark_line;
     proceed from cur_line in both directions;
     in case a large file is partially swapped out, this is more efficient
  */
  lineup = cur_line;
  linedown = cur_line;
  do {
	do_continue = False;
	if (lineup != header) {
		lineup = lineup->prev;
		if (lineup == mark_line) {
			return SMALLER;
		} else {
			do_continue = True;
		}
	}
	if (linedown != tail) {
		linedown = linedown->next;
		if (linedown == mark_line) {
			return BIGGER;
		} else {
			do_continue = True;
		}
	}
  } while (do_continue);
  /* mark_line not found */
  return NOT_VALID;

#ifdef old_impl
  LINE * line;
  FLAG cur_seen = False;

/* Special case: check is mark_line and cur_line are the same. */
  if (mark_line == cur_line) {
	if (mark_text == cur_text) {
		/* Even same place */
		return SAME;
	}
	if (legal (mark_line, mark_text) == ERRORS) {
		/* mark_text dangling (not pointing into mark_line) */
		return NOT_VALID;
	}
	if (mark_text < cur_text) {
		return SMALLER;
	} else {
		return BIGGER;
	}
  }

/* Start looking for mark_line in the line structure */
  for (line = header->next; line != tail; line = line->next) {
	if (line == cur_line) {
		cur_seen = True;
	} else if (line == mark_line) {
		break;
	}
  }

/* If we found mark_line (line != tail) check for legality of mark_text */
  if (line == tail || legal (mark_line, mark_text) == ERRORS) {
	return NOT_VALID;
  }

/* cur_seen is True if cur_line is before mark_line */
  if (cur_seen) {
	return BIGGER;
  } else {
	return SMALLER;
  }
#endif
}


/*======================================================================*\
|*			Cumulative buffer handling			*|
\*======================================================================*/

static
void
set_buffer_open (appending)
  FLAG appending;
{
#ifdef debug_ring_buffer
  printf ("set_buffer_open %d\n", buffer_open_flag);
#endif
  if (buffer_open_flag == 0 && (appending == False || yank_buf_no == 0)) {
	yank_buf_no ++;
	if (yank_buf_no > max_yank_buf_no) {
		max_yank_buf_no = yank_buf_no;
	}
	yank_status = NOT_VALID;
#ifdef debug_ring_buffer
	flags_changed = True;
#endif
  }
  buffer_open_flag = 2;
}

static
void
close_buffer ()
{
#ifdef debug_ring_buffer
  if (buffer_open_flag > 0) {
	flags_changed = True;
  }
#endif
  buffer_open_flag = 0;
}

static
void
revert_yank_buf ()
{
  yank_buf_no --;
  if (yank_buf_no <= 0) {
	yank_buf_no = max_yank_buf_no;
  }
}


/*======================================================================*\
|*			Yank text handling				*|
\*======================================================================*/

#define dont_debug_rectangular_paste_area
#define dont_debug_sel
#define dont_debug_sel_text
#define dont_debug_rectangular_paste_insert

/*
   Return text column position within line.
 */
static
int
get_text_col (line, textp, with_shift)
  LINE * line;
  char * textp;
  FLAG with_shift;
{
  char * tp = line->text;
  int col = 0;
  if (with_shift) {
	col = line->shift_count * SHIFT_SIZE;
  }

  while (tp != textp && * tp != '\n' && * tp != '\0') {
	advance_char_scr (& tp, & col, line->text);
  }
  return col;
}

static
char *
text_at (line, colpoi, targcol)
  LINE * line;
  int * colpoi;
  int targcol;
{
  char * cpoi = line->text;
  char * prev_cpoi = cpoi;
  int col = 0;
  int prev_col = 0;
  while (col <= targcol) {
	prev_cpoi = cpoi;
	prev_col = col;
	if (* cpoi == '\n') {
		break;
	}
	advance_char_scr (& cpoi, & col, line->text);
  }
  * colpoi = prev_col;
  return prev_cpoi;
}

#ifdef debug_rectangular_paste_area
#define trace_cols(tag)	printf ("(%s) %s->%d, %d\n", tag, line->text, start_col, end_col);
#else
#define trace_cols(tag)	
#endif

/*
   yank_text () puts copies text between start position and end position 
   into the buffer.
   Parameters start_line/start_textp and end_line/end_textp 
   must be in the correct order!
   Options:
   	- do_remove: cut
   	- do_rectangular_paste: rectangular area
   The caller must check that the arguments to yank_text () are valid 
   and in the right order.
 */
static
FLAG
yank_text (fd, buf_status, 
	start_line, start_textp, end_line, end_textp, 
	do_remove, appending, do_rectangular_paste, start_end_reversed)
  int fd;
  FLAG * buf_status;
  LINE * start_line;
  char * start_textp;
  LINE * end_line;
  char * end_textp;
  FLAG do_remove;	/* == DELETE if text should be deleted */
  FLAG appending;	/* == True if text should is appended to yank buffer */
  FLAG do_rectangular_paste;
  FLAG start_end_reversed;
{
  LINE * line = start_line;
  char * textp = start_textp;
  FLAG do_continue;
  FLAG at_eol = False;	/* end of rectangular area in line */
  long chars_written = 0L;	/* chars written to buffer this time */
  long bytes_written = 0L;	/* bytes written to buffer this time */
  int lines_written = 0;	/* lines written to buffer this time */
  int return_len;

  /* only used if do_rectangular_paste: (avoid -Wmaybe-uninitialized) */
  char * line_startp = NIL_PTR;	/* start of line text for area to delete */
  int start_col = 0;	/* left border of rectangular area */
  int end_col = 0;	/* right border of rectangular area */
  int col = 0;		/* measure rectangular area */

/* Check file to hold buffer */
  if (fd == ERRORS) {
	return ERRORS;
  }

/* Inform about running operation (to be seen in case it takes long) */
  if (appending) {
	status_msg ("Appending text ...");
  } else {
	status_msg ("Saving text ...");
	chars_saved = 0L;
	bytes_saved = 0L;
	lines_saved = 0;
  }

/* Prepare */
  if (do_rectangular_paste) {
	/* check left and right block boundaries */
	start_col = get_text_col (start_line, start_textp, False);
	end_col = get_text_col (end_line, end_textp, False);
	trace_cols ("...");

	/* adapt column to actual mouse position */
	if (start_end_reversed == REVERSE) {
		if (last_sel_x > start_col) {
			start_col = last_sel_x;
		}
	} else {
		if (last_sel_x > end_col) {
			end_col = last_sel_x;
		}
	}
	/* fix right-to-left selection */
	if (start_col > end_col) {
		int dum = end_col;
		end_col = start_col;
		start_col = dum;
	}
	trace_cols ("r-to-l fix");
	/* reject empty copy */
	if (start_col == end_col) {
		error ("Rectangular area empty");
		return ERRORS;
	}

	line_startp = textp = text_at (line, & col, start_col);
	do_continue = True;
  } else {
	chars_written = char_count (textp) - 1;
	do_continue = textp != end_textp;
  }

/* Keep writing chars until the end_location is reached. */
  while (do_continue) {
	if (* textp == '\n' || at_eol) {
		if (! do_rectangular_paste && line == end_line) {
			error ("Internal error: passed end of text to be copied");
			(void) close (fd);
			return ERRORS;
		}

		/* fill rectangular area */
		if (do_rectangular_paste) {
			if (col < start_col) {
				col = start_col;
			}
			while (col < end_col) {
				col ++;
				if (writechar (fd, ' ') == ERRORS) {
					error2 ("Write to buffer failed: ", serror ());
					(void) close (fd);
					return ERRORS;
				}
			}
		}

		/* handle different line ends */
		pasting = True;
		return_len = write_lineend (fd, line->return_type, False);
		pasting = False;
		if (return_len == ERRORS) {
			error2 ("Write to buffer failed: ", serror ());
			(void) close (fd);
			return ERRORS;
		}
		lines_written ++;
		if (line->return_type != lineend_NONE) {
			chars_written ++;
		}
		bytes_written += return_len;

		if (do_rectangular_paste) {
			/* delete rectangular area part of line */
			if (do_remove == DELETE && line_startp != textp) {
			    if (ERRORS == delete_text_only (line, line_startp, line, textp)) {
				/* give time to read allocation error msg */
				sleep (2);
			    }
			    /* refresh cursor position to keep it valid */
			    move_to (x, y);
			}

			/* move to the next line */
			if (line == end_line) {
				do_continue = False;
			} else {
				line = line->next;
				line_startp = textp = text_at (line, & col, start_col);
				at_eol = False;
			}
		} else {
			line = line->next;
			textp = line->text;

			chars_written += char_count (textp) - 1;
		}
	} else {
		if (pastebuf_utf8 && ! utf8_text) {	/* write UTF-8 */
			character unibuf [13];
			char * up = (char *) unibuf;

			/* get Unicode character */
			unsigned long unichar = charvalue (textp);
			if (cjk_text || mapped_text) {
				unichar = lookup_encodedchar (unichar);
				if (no_unichar (unichar)) {
					unichar = '';
				}
			}

			/* handle 2 char special case */
			if (unichar >= 0x80000000) {
				/* special encoding of 2 Unicode chars, 
				   mapped from 1 CJK character */
				up += utfencode (unichar & 0xFFFF, up);
				unichar = (unichar >> 16) & 0x7FFF;
				if (do_rectangular_paste) {
					chars_written ++;
				}
			}
			(void) utfencode (unichar, up);

			/* write UTF-8 */
			/* don't use write_line which might write UTF-16 ! */
			up = (char *) unibuf;
			while (* up != '\0') {
				if (writechar (fd, * up) == ERRORS) {
					error2 ("Write to buffer failed: ", serror ());
					(void) close (fd);
					return ERRORS;
				}
				up ++;
				bytes_written ++;
			}

			/* move to the next character */
			if (do_rectangular_paste) {
				chars_written ++;
				advance_char_scr (& textp, & col, line->text);
			} else {
				advance_char (& textp);
			}
		} else if (do_rectangular_paste) {	/* write native char */
			unsigned long thischar = charvalue (textp);
			char * buf = encode_char (thischar);

			/* write char code */
			while (* buf) {
				if (writechar (fd, * buf) == ERRORS) {
					error2 ("Write to buffer failed: ", serror ());
					(void) close (fd);
					return ERRORS;
				}
				buf ++;
				bytes_written ++;
			}
			/* move to the next character */
			chars_written ++;
			advance_char_scr (& textp, & col, line->text);
		} else {	/* write bytes transparently */
			if (writechar (fd, * textp) == ERRORS) {
				error2 ("Write to buffer failed: ", serror ());
				(void) close (fd);
				return ERRORS;
			}
			bytes_written ++;

			/* move to the next byte */
			textp ++;
		}

		if (do_rectangular_paste && col >= end_col) {
			at_eol = True;
		}
	}
	if (! do_rectangular_paste && textp == end_textp) {
		do_continue = False;
	}
  }

/* Final fix */
  if (! do_rectangular_paste) {
	chars_written -= char_count (end_textp) - 1;
  }

/* After rectangular cut: update screen */
  if (do_remove == DELETE && do_rectangular_paste) {
	int top_y, bottom_y;
	if (end_line == cur_line) {
		LINE * line;
		bottom_y = y;
		top_y = y - lines_written + 1;
		if (top_y < 0) {
			top_y = 0;
		}
		line = proceed (top_line, top_y);
		display (top_y, line, bottom_y - top_y, y);
	} else {
		top_y = y;
		bottom_y = y + lines_written - 1;
		if (bottom_y > last_y) {
			bottom_y = last_y;
		}
		display (y, cur_line, bottom_y - top_y, y);
	}
	/* Fix position:
	   avoid leaving the cursor aside the delection area;
	   go to upper left corner of deleted block */
	move_to (LINE_START, find_y (start_line));	/* unshift line */
	move_to (start_col, y);
  }

/* Flush the I/O buffer and close file */
  if (flush_filebuf (fd) == ERRORS) {
	error2 ("Write to buffer failed: ", serror ());
	(void) close (fd);
	return ERRORS;
  }
  if (close (fd) < 0) {
	error2 ("Write to buffer failed: ", serror ());
	return ERRORS;
  }
  * buf_status = VALID;


  /*
   * Check if the text should be deleted as well. In case it should,
   * the following hack is used to save a lot of code.
   * First move back to the start_position (this might be the current
   * location) and then delete the text.
   * delete_text () will fix the screen.
   */
  if (do_remove == DELETE && ! do_rectangular_paste) {
	move_to (find_x (start_line, start_textp), find_y (start_line));
	if (delete_text (start_line, start_textp, end_line, end_textp)
		== ERRORS) {
		sleep (2) /* give time to read allocation error msg */;
	}
	mark_line = cur_line;
	mark_text = cur_text;
  }

  bytes_saved += bytes_written;
  chars_saved += chars_written;
  lines_saved += lines_written;

  build_string (text_buffer, "%s %s: lines %d chars %ld (bytes %ld) - Paste with %s/Insert", 
	(do_remove == DELETE) ?
		appending ? "Cut/appended" : "Cut/moved"
		: appending ? "Appended" : "Copied",
	do_rectangular_paste ? "rectangular area" : "paste buffer",
	lines_written,
	chars_written,
	bytes_written,
	emulation == 'e' ? "^Y"		/* emacs yank */
	: emulation == 's' ? "^K^C"	/* WordStar block copy */
	: emulation == 'p' ? "^U"	/* pico uncut */
	: emulation == 'w' ? "^V"	/* Windows paste */
	: "^P"				/* mined paste */
	);
  status_uni (text_buffer);
  return FINE;
}


/**
   Variation of delete_text ().
   Optionally handles the emacs ring buffer.
   Parameters start_line/start_textp and end_line/end_textp 
   must be in the correct order!
 */
void
delete_text_buf (start_line, start_textp, end_line, end_textp)
  LINE * start_line;
  char * start_textp;
  LINE * end_line;
  char * end_textp;
{
  if (emacs_buffer) {
	set_buffer_open (False);
	(void)
	yank_text (yankfile (WRITE, True), & yank_status, 
			start_line, start_textp, end_line, end_textp, 
			DELETE, True, False, FORWARD);
  } else {
	(void) delete_text (start_line, start_textp, end_line, end_textp);
  }
}


/*======================================================================*\
|*			Yank file reading				*|
\*======================================================================*/

/**
   get_pasteline calls get_line and converts from Unicode if desired
 */
static
int
get_pasteline (fd, buffer, len)
  int fd;
  char buffer [maxLINElen];
  int * len;
{
  int ret;
  pasting = True;
  ret = get_line (fd, buffer, len, False);
  pasting = False;

  if (ret == NO_INPUT || ret == ERRORS) {
	return ret;
  }

  if (utf8_text || ! pastebuf_utf8) {
	return ret;
  } else {
	char nativebuf [2 * maxLINElen];
	char * poi = buffer;
	char * npoi = nativebuf;
	unsigned long prev_uc = 0;
	char * prev_npoi = npoi;

	while (* poi) {
		int ulen = UTF8_len (* poi);
		unsigned long uc = utf8value (poi);
		char * ppoi = poi;

		advance_utf8 (& poi);
		if (ppoi + ulen != poi || (* ppoi & 0xC0) == 0x80) {
			/* illegal UTF-8 value */
			* npoi ++ = '';
			prev_uc = 0;
		} else if (cjk_text || mapped_text) {
			unsigned long nc = encodedchar2 (prev_uc, uc);
			if (no_char (nc)) {
				nc = encodedchar (uc);
			} else {
				npoi = prev_npoi;
			}

			prev_uc = uc;
			prev_npoi = npoi;

			if (no_char (nc)) {
				/* character not known in current encoding */
				* npoi ++ = '';
			} else if (cjk_text) {
				int cjklen = cjkencode (nc, npoi);
				npoi += cjklen;
			} else {
				* npoi ++ = (character) nc;
			}
		} else {
			if (uc >= 0x100) {
				/* character not known in current encoding */
				* npoi ++ = '';
			} else {
				* npoi ++ = (character) uc;
			}
		}
	}
	* npoi = '\0';

	* len = strlen (nativebuf);
	if (* len >= maxLINElen) {
		error ("Line too long in current encoding");
		return ERRORS;
	} else {
		strcpy (buffer, nativebuf);
		return ret;
	}
  }
}


#ifdef debug_rectangular_paste_insert
static
void
print_str (s)
  char * s;
{
  if (! s) {
	printf ("(null)");
	return;
  }
  printf ("\"");
  while (* s) {
	if (* s == '\n') {
		printf ("\\n");
	} else if (* s == '"') {
		printf ("\\\"");
	} else {
		printf ("%c", * s);
	}
	s ++;
  }
  printf ("\"");
}

static
char *
line_spec (line)
  LINE * line;
{
  if (line == tail) {
	return "tail";
  } else if (line == header) {
	return "head";
  } else {
	return "    ";
  }
}

static
void
trace_line (tag, line)
  char * tag;
  LINE * line;
{
  printf ("[%s-1] %s ", tag, line_spec (line->prev)); print_str (line->prev->text); printf ("\n");
  printf ("[%s  ] %s [1m", tag, line_spec (line)); print_str (line->text); printf ("[0m\n");
  printf ("[%s+1] %s ", tag, line_spec (line->next)); print_str (line->next->text); printf ("\n");
}
#define trace_rectangular_paste(args)	printf args
#else
#define trace_line(tag, line)	
#define trace_rectangular_paste(args)	
#endif

/*
 * paste_file () inserts the contents of an opened file (as given by
   filedescriptor fd) at the current location.
   Call insert_file () only via paste_file ().
   If the rectangular_paste_mode is True, a rectangular area will 
   be inserted, proceeding line by line.
   After the insertion, if stay_old_pos is True, the cursor remains at the
   start of the inserted text, if stay_old_pos is False, it is placed to
   its end.
 */
static
void
insert_file (fd, stay_old_pos, from_text_file)
  int fd;
  FLAG stay_old_pos;
  FLAG from_text_file;	/* consider UTF-16 ? */
{
  char line_buffer [maxLINElen];	/* buffer for next line to insert */
  LINE * line = cur_line;
  int line_count = total_lines;		/* determine # lines inserted */
  LINE * page = cur_line;
  int ret;
  int len;
  lineend_type return_type;
  /* copy rectangular option; this working flag will be reset at end-of-file */
  FLAG do_rectangular_paste = rectangular_paste_mode;
  int paste_col = get_cur_col ();	/* for rectangular paste */

  reset_get_line (from_text_file);
  trace_rectangular_paste (("-------------------------------------------\n"));

  pasted_end_line = NIL_LINE;

/* Get the first line of text (might be ended with a '\n') */
  ret = get_pasteline (fd, line_buffer, & len);
  if (ret == NO_INPUT) {
	/* empty file */
	return;
  } else if (ret == ERRORS) {
	/* sleep; "0 lines" error message? - no, nothing inserted */
	return;
  }

/* Adjust line end type if present */
  if (ret == SPLIT_LINE) {
	return_type = lineend_NONE;
  } else if (ret == NUL_LINE) {
	return_type = lineend_NUL;
  } else if (ret != NO_LINE) {
	return_type = got_lineend;
  } else {
	return_type = cur_line->return_type;
  }

/* Insert this text at the current location */
  len = cur_text - cur_line->text;
  if (do_rectangular_paste) {
	/* strip final newline */
	char * nl = strchr (line_buffer, '\n');
	if (nl) {
		* nl = '\0';
	}
  }
  trace_line ("first<", line);
  trace_rectangular_paste (("insert <%s>\n", line_buffer));
  if (insert_text (line, cur_text, line_buffer) == ERRORS) {
/*	"Out of memory" error already shown, nothing inserted
	sleep (2);
	ring_bell ();
	error ("Partial line inserted");
*/
	return;
  }
  trace_line ("first>", line);
  if (! do_rectangular_paste) {
	cur_line->return_type = return_type;
  }

  pasted_end_line = line;
  pasted_end_textp = line->text + len + length_of (line_buffer);


/* Repeat getting lines (and inserting lines) until EOF is reached */
  while (ret != ERRORS /*line != NIL_LINE*/
	 && (ret = get_pasteline (fd, line_buffer, & len)) != ERRORS
	 /* NO_LINE (line without newline) is handled separately
	    unless in rectangular paste mode */
	 && ret != NO_INPUT
	 && (ret != NO_LINE || do_rectangular_paste)
	)
  {
	/* at end of text, switch to normal paste mode */
	if (line != NIL_LINE && line->next == tail) {
#ifdef switch_to_normal_at_eof
		trace_rectangular_paste (("switching to normal\n"));
		do_rectangular_paste = False;
#endif
#ifdef handle_last_line_separately
		/* NO_LINE (line without newline) is handled separately */
		if (ret == NO_LINE) {
			trace_rectangular_paste (("NO_LINE -> break\n"));
			break;
		}
#endif
	}

	if (do_rectangular_paste) {
		char * inspoi;
		int inscol = 0;
		char * nl;

		if (line->next == tail) {
			LINE * new_line = line_insert_after (line, "\n", 1, line->return_type);
			if (new_line == NIL_LINE) {
				ring_bell ();
				status_fmt2 ("Out of memory - ", "Insertion failed");
				ret = ERRORS;
				break;
			} else {
				line = new_line;
			}
			line = line->prev;
			trace_rectangular_paste (("EOF: append line\n"));
			trace_line ("eof++", line);
		}

		line = line->next;
		trace_rectangular_paste (("line = line->next\n"));
		trace_line ("next", line);
		if (line == tail) {
			break;	/* cannot occur; prevented above */
		}
		/* strip final newline */
		nl = strchr (line_buffer, '\n');
		if (nl) {
			* nl = '\0';
		}
		inspoi = line->text;
		while (inscol < paste_col && * inspoi != '\n' && * inspoi != '\0') {
			advance_char_scr (& inspoi, & inscol, line->text);
		}

		if (inscol < paste_col) {
			/* fill short line up to paste column */
			char fill_buffer [2 * maxLINElen];
			char * fill_poi = fill_buffer;

			char * reftext = line->prev->text;
			int refcol = 0;
			char * refprev = "";

			/* append space, take from ref line if possible */
			while (inscol < paste_col) {
				unsigned long ch;
				unsigned long unich;

				/* look up end of insert position in ref line */
				while (refcol <= inscol) {
					refprev = reftext;
					advance_char_scr (& reftext, & refcol, line->prev->text);
				}
				/* check ref char advancing position */
				ch = charvalue (refprev);
				unich = unicode (ch);
				/* use ideographic space for double width */
				if (unich != 0x3000 && iswide (unich)
				   && (utf8_text || cjk_text) 
				   ) {
					unich = 0x3000;
					ch = encodedchar (unich);
				}

				if (unich == '\t') {
					if (tab (inscol) <= paste_col) {
						* fill_poi ++ = '\t';
						inscol = tab (inscol);
					} else {
						* fill_poi ++ = ' ';
						inscol ++;
					}
				} else if (iswhitespace (unich)) {
					int w = 1;
					if (iswide (unich)) {
						w = 2;
					} else if (iscombining (unich)) {
						w = 0;
					}
					if (w > 0 && inscol + 2 <= paste_col) {
						/* clone space character */
						char * ec = encode_char (ch);
						while (* ec) {
							* fill_poi ++ = * ec ++;
						}
						inscol += w;
					} else {
						* fill_poi ++ = ' ';
						inscol ++;
					}
				} else {
					* fill_poi ++ = ' ';
					inscol ++;
				}
				* fill_poi = '\0';
			}

			/* append fill buffer */
			trace_rectangular_paste (("fill <%s>\n", fill_buffer));
			if (insert_text (line, inspoi, fill_buffer) == ERRORS) {
				ret = ERRORS;
				break;
			}
			trace_line ("fill>", line);
			/* adjust insert pointer */
			inscol = 0;
			inspoi = line->text;
			while (inscol < paste_col && * inspoi != '\n' && * inspoi != '\0') {
				advance_char_scr (& inspoi, & inscol, line->text);
			}
		}

		len = inspoi - line->text;
		trace_rectangular_paste (("insert_text <%s>\n", line_buffer));
		if (insert_text (line, inspoi, line_buffer) == ERRORS) {
			ret = ERRORS;
			break;
		}
		trace_line ("insert", line);
		pasted_end_line = line;
		pasted_end_textp = line->text + len + length_of (line_buffer);
	} else {
		LINE * new_line;
		if (ret == SPLIT_LINE) {
			return_type = lineend_NONE;
		} else if (ret == NUL_LINE) {
			return_type = lineend_NUL;
		} else {
			return_type = got_lineend;
		}
		trace_rectangular_paste (("line_insert <%s>\n", line_buffer));
		new_line = line_insert_after (line, line_buffer, len, return_type);
		if (new_line == NIL_LINE) {
			ring_bell ();
			status_fmt2 ("Out of memory - ", "Insertion failed");
			ret = ERRORS;
			break;
		} else {
			line = new_line;
		}
		trace_line ("insert", line);
	}
  }

  (void) close (fd);

/* Calculate nr of lines added */
  line_count = total_lines - line_count;

/* finish pasting, check last line */
  if (ret == ERRORS /* || line == NIL_LINE*/) {
	pasted_end_line = NIL_LINE;
	/* show memory allocation error msg */
	sleep (2);
  } else if (ret == NO_LINE && ! do_rectangular_paste) {
	/* Last line read not ended by a '\n' */
	if (line->next == tail) {	/* after do_rectangular_paste */
		LINE * new_line = line_insert_after (line, "\n", 1, line->return_type);
		if (new_line == NIL_LINE) {
			/* cannot append empty line; ignore */
		}
		trace_rectangular_paste (("NO_LINE: append line\n"));
		trace_line ("eof++", line);
	}
	line = line->next;
	trace_rectangular_paste (("NO_LINE: line = line->next\n"));
	trace_line ("next(eof)", line);
	trace_rectangular_paste (("insert_text <%s>\n", line_buffer));
	if (insert_text (line, line->text, line_buffer) == ERRORS) {
		pasted_end_line = NIL_LINE;
		/* give time to read error msg */
		sleep (2);
	} else {
		pasted_end_line = line;
		pasted_end_textp = line->text + length_of (line_buffer);
	}
  } else if (line_count > 0 && ! rectangular_paste_mode) {
	pasted_end_line = line->next;
	pasted_end_textp = line->next->text;
	trace_line ("pasted_end", pasted_end_line);
  }

/* report get_line errors */
  show_get_l_errors ();

/* rescue post-error situation */
  if (ret == ERRORS) {
	pasted_end_textp = line->text;
  }

/* Update the screen */
  if (line_count == 0 && ! rectangular_paste_mode) {
	/* Only one line changed */
	print_line (y, line);

	move_to (x, y);
	pasted_start_line = cur_line;
	pasted_start_textp = cur_text;
	if (stay_old_pos == False) {
		move_address (pasted_end_textp, y);
	}
  } else {
	/* Several lines changed, or rectangular paste */
	reset (top_line, y);	/* Reset pointers */
	while (page != line && page != bot_line->next) {
		page = page->next;
	}
	if (page != bot_line->next || stay_old_pos) {
		display (y, cur_line, SCREENMAX - y, y);
		/* screen display style parameter (last) may be inaccurate */
	}

	move_to (x, y);
	pasted_start_line = cur_line;
	pasted_start_textp = cur_text;
	if (stay_old_pos == False) {
		if (ret == NO_LINE) {
			move_address (pasted_end_textp, find_y (line));
		} else {
			if (do_rectangular_paste) {
				trace_line ("end", line);
				trace_line ("cur", cur_line);
				move_address (pasted_end_textp, find_y (line));
				trace_line ("fin", cur_line);
			} else {
				move_to (0, find_y (line->next));
			}
		}
	}
  }

/* If number of added lines >= REPORT_CHANGED_LINES, print the count */
  if (ret == ERRORS || line_count >= REPORT_CHANGED_LINES) {
	if (ret == ERRORS) {
		ring_bell ();
	}
	status_line (dec_out ((long) line_count), " lines added");
  }
}

static
void
paste_file (fd, stay_old_pos, from_text_file)
  int fd;
  FLAG stay_old_pos;
  FLAG from_text_file;	/* consider UTF-16 ? */
{
  clear_highlight_selection ();

  /* workaround to avoid side effects of reset_get_line:
     avoid spoiling UTF-16 information while pasting
   */
  save_text_info ();

  insert_file (fd, stay_old_pos, from_text_file);

  /* restore side effects of reset_get_line */
  restore_text_info ();
}


/**
   Insert the buffer at the current location.
   @param old_pos
   	True: cursor stays @ insert position before inserted text
   	False: cursor moves behind inserted text
 */
static
void
paste_buffer (old_pos, use_clipboard)
  FLAG old_pos;
  FLAG use_clipboard;
{
  register int fd;		/* File descriptor for buffer */
  FLAG save_lineends_CRLFtoLF = lineends_CRLFtoLF;

  if (dont_modify ()) {
	return;
  }

#ifdef __CYGWIN__
  if (use_clipboard) {
	if ((fd = open ("/dev/clipboard", O_RDONLY | O_BINARY, 0)) < 0) {
		error ("Cannot access clipboard");
		return;
	}
	status_uni ("Pasting from Windows clipboard");
	if (cur_line->return_type == lineend_LF) {
		lineends_CRLFtoLF = True;	/* temporary, reset below */
	}
  } else
#endif
  if (use_clipboard || hop_flag > 0) {
	if ((fd = open (yankie_file, O_RDONLY | O_BINARY, 0)) < 0) {
		error ("No inter window buffer present");
		return;
	}
	status_uni ("Pasting from cross-session buffer");
  } else {
	if ((fd = yankfile (READ, False)) == ERRORS) {
		int e = geterrno ();
		if (e == 0 || e == ENOENT /* cygwin */) {
			status_uni ("Buffer is empty - type F1 k for help on copy/paste");
		} else {
			error2 ("Cannot read paste buffer: ", serror ());
		}
		return;
	}
	status_uni ("Pasting");
	if (append_flag) {
		close_buffer ();
	}
  }
  /* Insert the buffer */
  paste_file (fd, old_pos, False);

  lineends_CRLFtoLF = save_lineends_CRLFtoLF;
}


/*======================================================================*\
|*			Paste buffer file setup				*|
\*======================================================================*/

/*
 * scratchfile/yankfile () tries to create a unique file in a temporary directory.
 * It tries several different filenames until one can be created
 * or MAXTRIALS attempts have been made.
 * After MAXTRIALS times, an error message is given and ERRORS is returned.
 */

#define MAXTRIALS 99

static
void
set_yank_file_name (buf_name, which, no)
  char * buf_name;
  char * which;
  int no;
{
#ifdef msdos
  build_string (buf_name, "%s-%s.%d", yankie_file, which, no);
#else
  build_string (buf_name, "%s_%s.%d_%d", yankie_file, which, (int) getpid (), no);
#endif
}

/*
 * Delete yank file if there is one.
 */
void
delete_yank_files ()
{
/*  if (yank_status == VALID) {
	(void) delete_file (yank_file);
  }
*/
  while (max_yank_buf_no > 0) {
	set_yank_file_name (yank_file, "buf", max_yank_buf_no);
	(void) delete_file (yank_file);
	max_yank_buf_no --;
  }

  if (html_status == VALID) {
	(void) delete_file (html_file);
  }
}

static
int
scratchfile (mode, append, buf_name, which, buf_status)
  FLAG mode;	/* Can be READ or WRITE permission */
  FLAG append;	/* == True if text should only be appended to yank buffer */
  char * buf_name;
  char * which;
  FLAG * buf_status;
{
  int fd = 0;			/* Filedescriptor to buffer */

  set_yank_file_name (buf_name, which, yank_buf_no);

/* If * buf_status == NOT_VALID, scratchfile is called for the first time */
  if (* buf_status == NOT_VALID && mode == WRITE) { /* Create new file */
	/* Generate file name. */
	/*set_yank_file_name (buf_name, which, yank_buf_no);*/
	/* Check file existence */
	if (access (buf_name, F_OK) == 0
	    || (
		fd = open (buf_name, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, bufprot)
		) < 0)
	{
		if (++ yank_buf_no >= MAXTRIALS) {
		    if (fd == 0) {
			error2 ("Cannot create buffer file: " /* , buf_name */, "File exists");
		    } else {
			error2 ("Cannot create buffer file: " /* , buf_name */, serror ());
		    }
		    return ERRORS;
		} else {	/* try again */
		    return scratchfile (mode, append, buf_name, which, buf_status);
		}
	}
  }
  else if (* buf_status == NOT_VALID && mode == READ) {
	errno = 0;
	return ERRORS;
  }
  else /* * buf_status == VALID */
	if (  (mode == READ && (fd = open (buf_name, O_RDONLY | O_BINARY, 0)) < 0)
	   || (mode == WRITE &&
		(fd = open (buf_name, O_WRONLY | O_CREAT
				| (append ? O_APPEND : O_TRUNC)
				| O_BINARY
				, bufprot)) < 0))
  {
	* buf_status = NOT_VALID;
	return ERRORS;
  }

  clear_filebuf ();
  return fd;
}

int
yankfile (mode, append)
  FLAG mode;	/* Can be READ or WRITE permission */
  FLAG append;	/* == True if text should only be appended to yank buffer */
{
  return scratchfile (mode, append, yank_file, "buf", & yank_status);
}

static
int
htmlfile (mode, append)
  FLAG mode;	/* Can be READ or WRITE permission */
  FLAG append;	/* == True if text should only be appended to yank buffer */
{
  return scratchfile (mode, append, html_file, "tag", & html_status);
}


/*======================================================================*\
|*			Copy/Paste and Marker handling			*|
\*======================================================================*/

void
PASTEEXT ()
{
  if (hop_flag) {	/* paste from clipboard */
	hop_flag = 0;
	paste_buffer (paste_stay_left, True);
  } else {		/* paste from other mined session */
	hop_flag = 1;
	paste_buffer (paste_stay_left, False);
  }
}

void
PASTE ()
{
  FLAG use_clipboard = (keyshift & shift_mask) || (hop_flag && (keyshift & alt_mask));
  paste_buffer (paste_stay_left, use_clipboard);
}

void
PASTEstay ()
{
  FLAG use_clipboard = (keyshift & shift_mask) || (hop_flag && (keyshift & alt_mask));
  paste_buffer (True, use_clipboard);
}

void
YANKRING ()
{
  FLAG check = checkmark (pasted_start_line, pasted_start_textp);

  if (cur_line == pasted_end_line && cur_text == pasted_end_textp 
      && check == SMALLER)
  {
	move_address (pasted_start_textp, find_y (pasted_start_line));
	if (delete_text (pasted_start_line, pasted_start_textp, pasted_end_line, pasted_end_textp)
		== ERRORS) {
		sleep (2) /* give time to read allocation error msg */;
	} else {
		/* for some mysterious reason, 
		   this is needed to fix the display: */
		clear_status ();

		revert_yank_buf ();
		paste_buffer (False, False);
	}
  } else if (cur_line == pasted_start_line && cur_text == pasted_start_textp 
	     && check == SAME)
  {
	if (delete_text (pasted_start_line, pasted_start_textp, pasted_end_line, pasted_end_textp)
		== ERRORS) {
		sleep (2) /* give time to read allocation error msg */;
	} else {
		/* for some mysterious reason, 
		   this is needed to fix the display: */
		clear_status ();

		revert_yank_buf ();
		paste_buffer (True, False);
	}
  } else {
	error ("No previous paste to exchange");
  }
}

/*
 * paste_HTML () inserts the HTML embedding buffer at the current location.
 */
void
paste_HTML ()
{
  int fd;		/* File descriptor for buffer */

  if (dont_modify ()) {
	return;
  }

  if ((fd = open (html_file, O_RDONLY | O_BINARY, 0)) < 0) {
	error ("HTML paste buffer vanished");
	return;
  }
  paste_file (fd, False, False);
}

/*
 * INSFILE () prompts for a filename and inserts the file at the current
 * location in the file.
 */
void
INSFILE ()
{
  register int fd;		/* File descriptor of file */
  char name [maxFILENAMElen];	/* Buffer for file name */

  if (restricted) {
	restrictederr ();
	return;
  }

  if (dont_modify ()) {
	return;
  }

/* Get the file name */
  if (get_filename ("Insert file:", name, False) != FINE) {
	return;
  }
  clear_status ();

  status_line ("Inserting ", name);
  if ((fd = open (name, O_RDONLY | O_BINARY, 0)) < 0) {
	error2 ("Cannot open file: " /*, name */, serror ());
  } else {	/* Insert the file */
	paste_file (fd, True, True);	/* leave cursor at begin of insertion */
  }
}

/*
 * WB () writes the buffer (yank_file) into another file, which
 * is prompted for.
 */
void
WB ()
{
  register int new_fd;		/* Filedescriptor to copy file */
  int yank_fd;			/* Filedescriptor to buffer */
  register int cnt;		/* Count check for read/write */
  int ret = FINE;		/* Error check for write */
  char wfile_name [maxFILENAMElen];	/* Output file name */
  char * msg_doing; char * msg_done;

  if (restricted) {
	restrictederr ();
	return;
  }

/* Checkout the buffer */
  if ((yank_fd = yankfile (READ, False)) == ERRORS) {
	int e = geterrno ();
	if (e == 0 || e == ENOENT /* cygwin */) {
		error ("Paste buffer is empty");
	} else {
		error2 ("Cannot read paste buffer: ", serror ());
	}
	return;
  }

/* Get file name */
  if (get_filename ((hop_flag > 0) 
		? "Append buffer to file:"
		: "Write buffer to file (use with HOP to append):", 
	wfile_name, False) != FINE) {
	return;
  }

/* Create the new file or open previous file for appending */
  if (hop_flag > 0) {
    status_line ("Opening ", wfile_name);
    if ((new_fd = open (wfile_name, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, fprot0)) < 0) {
	error2 ("Cannot append to file: " /* , wfile_name */, serror ());
	return;
    }
    msg_doing = "Appending buffer to ";
    msg_done = "Appended buffer to";
  } else {
    FLAG ovw = checkoverwrite (wfile_name);
    if (ovw == False) {
	return;
    } else {
	if (ovw == True && backup_mode) {
#ifndef backup_only_edited_file
		(void) do_backup (wfile_name);
#endif
	}

	status_line ("Opening ", wfile_name);
	if ((new_fd = open (wfile_name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, fprot0)) < 0) {
		error2 ("Cannot create file: " /* , wfile_name */, serror ());
		return;
	}
    }
    msg_doing = "Writing buffer to ";
    msg_done = "Wrote buffer to";
  }

  status_line (msg_doing, wfile_name);
  flush ();

/* Copy buffer into file */
  while ((cnt = read (yank_fd, text_buffer, sizeof (text_buffer))) > 0) {
	if (write (new_fd, text_buffer, (unsigned int) cnt) != cnt) {
		error2 ("Writing buffer to file failed: ", serror ());
		ret = ERRORS;
		break;
	}
  }
  if (cnt < 0) {
	error2 ("Reading paste buffer failed: ", serror ());
	ret = ERRORS;
  }

/* Clean up open files and status line */
  (void) close (yank_fd);
  if (close (new_fd) < 0) {
	if (ret != ERRORS) {
		error2 ("Writing buffer to file failed: ", serror ());
		ret = ERRORS;
	}
  }

  if (ret != ERRORS) {
	file_status (msg_done, bytes_saved, chars_saved, 
			wfile_name, lines_saved, 
			False, True, False, False);
  }
}

/*
   setMARK sets position marker to current line / text pointer.
   MARK sets position marker or goes to it or (if on it) clears visible selection.
 */
void
setMARK (set_only)
  FLAG set_only;
{
  clear_highlight_selection ();

  mark_line = cur_line;
  mark_text = cur_text;
  start_highlight_selection ();
  if (! set_only) {
	status_uni ("Mark set - type F1 k for help on copy/paste - HOP Mark toggles rectangular");
  }
}

void
MARK ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	GOMA ();
  } else if (highlight_selection && mark_line == cur_line && mark_text == cur_text) {
	clear_highlight_selection ();
	status_uni ("Mark set -selection hidden- HOP Mark toggles rectangular");
  } else {
	setMARK (False);
  }
}

/*
 * toggleMARK sets / unsets mark (for pico mode).
 */
void
toggleMARK ()
{
  if (checkmark (mark_line, mark_text) == NOT_VALID) {
	setMARK (True);
  } else {
	mark_line = NIL_LINE;
	mark_text = NIL_PTR;
	status_msg ("Mark unset");
  }
}

/*
 * GOMA moves to the marked position
 */
void
GOMA ()
{
  if (checkmark (mark_line, mark_text) == NOT_VALID) {
	error ("Mark not set");
  } else if (mark_line == cur_line && mark_text == cur_text) {
	/* toggle rectangular selection */
	toggle_rectangular_paste_mode ();
	if (rectangular_paste_flag) {
		status_msg ("Rectangular selection enabled");
	} else {
		status_msg ("Rectangular selection disabled");
	}
	start_highlight_selection ();
  } else {
	Pushmark ();

	move_address (mark_text, find_y (mark_line));

	/*continue_highlight_selection ();*/
	clear_highlight_selection ();
  }
}

/*
 * MARKn sets mark n to the current line / current text pointer.
 * mark_n sets it silently.
 */
void
MARKn (n)
  int n;
{
  if (hop_flag > 0) {
	GOMAn (n);
  } else {
	if (n < 0 || n >= maxmarkers) {
		error ("Marker # out of range");
		return;
	}
	marker_n [n].line = cur_line;
	marker_n [n].text = cur_text;
	status_msg ("Marker set");
  }
}

void
mark_n (n)
  int n;
{
	if (n == -1) {	/* initial mark */
		mark_line = cur_line;
		mark_text = cur_text;
	} else if (n < 0 || n >= maxmarkers) {
		error ("Marker # out of range");
		return;
	} else {
		marker_n [n].line = cur_line;
		marker_n [n].text = cur_text;
	}
}

/*
 * GOMAn moves to the marked position n
 */
void
GOMAn (n)
  int n;
{
  Pushmark ();

  if (n < 0 || n >= maxmarkers) {
	error ("Marker # out of range");
	return;
  }

  if (checkmark (marker_n [n].line, marker_n [n].text) == NOT_VALID) {
	error ("Marker not set");
  } else {
	move_address (marker_n [n].text, find_y (marker_n [n].line));
  }
}


/*
 * Yankie () provides a reference to the last saved buffer to be read
 * by other mined invocations.
 */
static
void
yankie ()
{
#ifdef __CYGWIN__
  /* also copy to Windows clipboard */
  status_uni ("Copying to Windows clipboard");
  if (copyfile (yank_file, "/dev/clipboard") != True) {
	/* ignore error */
  }
  status_uni (text_buffer);
#endif

#ifdef linkyank
  (void) delete_file (yankie_file);
  if (link (yank_file, yankie_file) == 0) {
	return;
  }
  if (geterrno () != EPERM
#ifdef EOPNOTSUPP
   && geterrno () != EOPNOTSUPP	/* Haiku */
#endif
#ifdef ENOTSUP
   && geterrno () != ENOTSUP	/* just in case */
#endif
#ifdef ENOSYS
   && geterrno () != ENOSYS	/* just in case */
#endif
  ) {
    /*	no error handling here as a message would inappropriately 
	obscure the original paste buffer copy information message 
	or an error message to the paste buffer copy function;
    */
	return;
  }
  /*	resort to copying if the file system does not support hard links
  */
#endif
  status_uni ("Copying to cross-session buffer");
  if (copyfile (yank_file, yankie_file) != True) {
	/* no error handling here as a message would inappropriately 
	   obscure the original paste buffer copy information message 
	   or an error message to the paste buffer copy function */
  }
  status_uni (text_buffer);
}

/*
 * yank_block is an interface to the actual yank.
 * It calls checkmark () to check if the marked position is still valid.
 * If it is, yank_text is called.
 */
static
void
yank_block (remove, append)
  FLAG remove;	/* == DELETE if text should be deleted */
  FLAG append;	/* == True if text should only be appended to yank buffer */
{
  switch (checkmark (mark_line, mark_text)) {
	case NOT_VALID :
		if (remove == DELETE) {
#ifdef oldstyle_DELkey
# ifdef msdos
			status_uni ("Mark not set for Cut to paste buffer - type Ctrl-Del to delete char, F1 k for help");
# else
			if (mined_keypad) {
				status_uni ("Mark not set for Cut to paste buffer - type Alt-Del to delete char, F1 k for help");
			} else {
				status_uni ("Mark not set for Cut to paste buffer - type F1 k for help");
			}
# endif
#else
			status_uni ("Mark not set for Cut to paste buffer - type F1 k for help");
#endif
		} else {
			status_uni ("Mark not set for Copy to paste buffer - type F1 k for help");
		}
		return;
	case SMALLER :
		set_buffer_open (append);
		if (yank_text (yankfile (WRITE, append), & yank_status, 
				mark_line, mark_text, cur_line, cur_text, 
				remove, append, rectangular_paste_mode, FORWARD)
		    == FINE) {
			yankie ();
		}
		break;
	case BIGGER :
		set_buffer_open (append);
		if (yank_text (yankfile (WRITE, append), & yank_status, 
				cur_line, cur_text, mark_line, mark_text, 
				remove, append, rectangular_paste_mode, REVERSE)
		    == FINE) {
			yankie ();
		}
		break;
	case SAME :
		status_uni ("No text selected for Copy or Cut");
		break;
	default :
		error ("Internal mark error");
		return;
  }
  alt_rectangular_mode = False;
}

void
yank_HTML (remove)
  FLAG remove;	/* == DELETE if text should be deleted */
{
  switch (checkmark (mark_line, mark_text)) {
	case NOT_VALID :
		error ("HTML tag selection failed");
		return;
	case SMALLER :
		(void)
		yank_text (htmlfile (WRITE, False), & html_status, 
				mark_line, mark_text, cur_line, cur_text, 
				remove, False, False, FORWARD);
		break;
	case BIGGER :
		(void)
		yank_text (htmlfile (WRITE, False), & html_status, 
				cur_line, cur_text, mark_line, mark_text, 
				remove, False, False, REVERSE);
		break;
	case SAME :
		error ("HTML tag selection failed");
		break;
	default :
		error ("Internal mark error");
		return;
  }
}

/*
 * COPY () puts the text between the marked position and the current
 * in the buffer.
 */
void
COPY ()
{
  if (visselect_copydeselect) {
	clear_highlight_selection ();
  }

  if (append_flag) {
	yank_block (NO_DELETE, True);
  } else if (hop_flag > 0) {
	yank_block (NO_DELETE, True);
  } else {
	yank_block (NO_DELETE, False);
  }
}

/*
 * CUT () is essentially the same as COPY (), but the text is deleted.
 */
void
CUT ()
{
  clear_highlight_selection ();

  if (dont_modify ()) {
	return;
  }

  if (append_flag) {
	yank_block (DELETE, True);
  } else if (hop_flag > 0) {
	yank_block (DELETE, True);
  } else {
	yank_block (DELETE, False);
  }
}


/*======================================================================*\
|*			Selection highlighting				*|
\*======================================================================*/

#ifdef debug_sel_text
static
void
trace_line_text (line, text)
  LINE * line;
  char * text;
{
  char * s = line->text;
  if (! s) {
	printf ("(null)");
	return;
  }
  printf ("\"");
  while (* s) {
	if (s == text) {
		printf ("[7m");
	} else if (s == mark_text) {
		printf ("[41m");
	} else if (s == cur_text) {
		printf ("[46m");
	}
	if (* s == '\n') {
		printf ("\\n");
	} else if (* s == '"') {
		printf ("\\\"");
	} else {
		printf ("%c", * s);
	}
	s ++;
  }
  printf ("[0m");
  printf ("\"");
  printf ("\n");
}
#else
#define trace_line_text(line, text)	
#endif

static
void
select_line (line, sel_begin, sel_end)
  LINE * line;
  char * sel_begin;
  char * sel_end;
{
  if (sel_begin != line->sel_begin) {
	line->sel_begin = sel_begin;
	line->dirty = True;
  }
  if (sel_end != line->sel_end) {
	line->sel_end = sel_end;
	line->dirty = True;
  }
#ifdef debug_sel
  printf ("%p %p %c ", sel_begin, sel_end, line->dirty ? '!' : '-');
  trace_line_text (line, 0);
#endif
}

static
void
do_update_selection_marks (select, pos_x)
  FLAG select;
  int pos_x;
{
#ifdef debug_sel
  printf ("do_update_selection_marks sel %d pos %d\n", select, pos_x);
#endif
  /*
     check which lines have changed being in the selection area or not;
     proceed lines
     - from current position until mark
     - beyond mark until previous position / end of previous selection
     - from current position in other direction until previous ...
     and mark them dirty if changed
     print_line should clear and optionally evaluate the dirty flag
   */
  if (highlight_selection) {
	/* only used if rectangular_paste_mode: (avoid -Wmaybe-uninitialized) */
	int start_col = 0;
	int end_col = 0;

	LINE * clearup;
	LINE * cleardown;
	FLAG check = NOT_VALID;
	if (select) {
		check = checkmark (mark_line, mark_text);
	}

	if (check == NOT_VALID) {
		highlight_selection = False;
	}

	if (check != NOT_VALID && rectangular_paste_mode) {
		/* check block boundaries */
		start_col = get_text_col (mark_line, mark_text, False);
		/*end_col = pos_x;*/
		end_col = get_text_col (cur_line, cur_text, False);
#ifdef debug_sel
		printf ("start: %d/%d .. end @%d(%d): %d/%d -> %d..%d",
			get_text_col (mark_line, mark_text, False),
			get_text_col (mark_line, mark_text, True),
			pos_x, last_sel_x,
			get_text_col (cur_line, cur_text, False),
			get_text_col (cur_line, cur_text, True),
			start_col, end_col);
#endif
		/* adapt column to actual mouse position */
		if (last_sel_x > end_col) {
			end_col = last_sel_x;
#ifdef debug_sel
			printf (" ->> %d..%d", start_col, end_col);
#endif
		}
		/* fix right-to-left selection */
		if (start_col > end_col) {
			int dum = end_col;
			end_col = start_col;
			start_col = dum;
#ifdef debug_sel
			printf (" -/> %d..%d", start_col, end_col);
#endif
		}
#ifdef debug_sel
		printf ("\n");
#endif
	}

	if (check == SMALLER) {
		/* blablabla MMMMM X blablabla */
		/* bla O MMMM blablabla */
		/* blablabla MM O blablabla */
		/* blablabla MMMMMMMM O blablabla */
		LINE * line = cur_line;
		char * sel_end = cur_text;

		/* select lines above until marker */
		while (line != header) {
			char * sel_begin;
			int dumcol;
			if (rectangular_paste_mode) {
				sel_begin = text_at (line, & dumcol, start_col);
				sel_end = text_at (line, & dumcol, end_col);
			} else if (line == mark_line) {
				sel_begin = mark_text;
			} else {
				sel_begin = line->text;
			}
			select_line (line, sel_begin, sel_end);
			if (line == mark_line) {
				/* end of selecting */
				break;
			}

			line = line->prev;
			/* select lines above to their end */
			sel_end = NIL_PTR;
		}

		/* deselect lines above marker */
		clearup = line->prev;

		/* deselect lines below cur_line */
		cleardown = cur_line->next;

	} else if (check == BIGGER) {
		/* blablabla X MMMMM blablabla */
		/* blablablablabla  MMMM O blabla */
		/* blablablabla O MM blablabla */
		/* blabla O MMMMMMMM blablabla */
		LINE * line = cur_line;
		char * sel_begin = cur_text;

		/* select lines below until marker */
		while (line != tail) {
			char * sel_end;
			int dumcol;
			if (rectangular_paste_mode) {
				sel_begin = text_at (line, & dumcol, start_col);
				sel_end = text_at (line, & dumcol, end_col);
			} else if (line == mark_line) {
				sel_end = mark_text;
			} else {
				sel_end = NIL_PTR; /* end of line and beyond */
			}
			select_line (line, sel_begin, sel_end);
			if (line == mark_line) {
				/* end of selecting */
				break;
			}

			line = line->next;
			/* select lines below from their start */
			sel_begin = line->text;
		}

		/* deselect lines below marker */
		cleardown = line->next;

		/* deselect lines above cur_line */
		clearup = cur_line->prev;

	} else /* (check == SAME || check == NOT_VALID) */ {
		/* deselect cur_line and lines above */
		clearup = cur_line;
		/* deselect lines below cur_line */
		cleardown = cur_line->next;
	}

	/* deselect lines above selection as long as still selected */
	while (clearup != header && clearup->sel_begin) {
		select_line (clearup, NIL_PTR, NIL_PTR);
		clearup = clearup->prev;
	}

	/* deselect lines below selection as long as still selected */
	while (cleardown != tail && cleardown->sel_begin) {
		select_line (cleardown, NIL_PTR, NIL_PTR);
		cleardown = cleardown->next;
	}
  }
}


void
start_highlight_selection ()
{
  highlight_selection = True;
}

void
clear_highlight_selection ()
{
  do_update_selection_marks (False, 0);
}


void
update_selection_marks (until_x)
  int until_x;
{
#define fix_shift_update
#ifdef fix_shift_update
  until_x -= cur_line->shift_count * SHIFT_SIZE;
#endif
  if (until_x != LINE_END) {
	last_sel_x = until_x;
  }
  do_update_selection_marks (True, until_x);
}

void
continue_highlight_selection (until_x)
  int until_x;
{
  highlight_selection = True;
  update_selection_marks (until_x);
}


FLAG
has_active_selection ()
{
  if (highlight_selection) {
	FLAG check = checkmark (mark_line, mark_text);
	if (check == BIGGER || check == SMALLER) {
		return True;
	}
  }
  return False;
}

void
trigger_highlight_selection ()
{
  if (keyshift & shift_mask) {
	if (has_active_selection ()) {
		continue_highlight_selection (last_sel_x);
	} else {
		setMARK (True);
	}
	keyshift &= ~ shift_mask;
  } else {
	clear_highlight_selection ();
  }
}

void
toggle_rectangular_paste_mode ()
{
	if (rectangular_paste_flag) {
		rectangular_paste_flag = False;
	} else {
		rectangular_paste_flag = True;
	}
	displayflags ();
	update_selection_marks (x);
/*	display_flush ();	*/
}

void
adjust_rectangular_mode (alt_mouse)
  FLAG alt_mouse;
{
  if (alt_mouse != alt_rectangular_mode) {
	alt_rectangular_mode = alt_mouse;
/*	update_selection_marks (x);	call followed by move_to anyway */
/*	display_flush ();	*/
  }
}


void
SELECTION ()
{
  if (hop_flag > 0) {
	clear_highlight_selection ();
  } else {
	continue_highlight_selection (last_sel_x);
	if (! highlight_selection) {
		error ("Mark not set");
	}
  }
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
