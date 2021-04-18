/*======================================================================*\
|*		Editor mined						*|
|*		search functions					*|
\*======================================================================*/

#include "mined.h"
#include "charprop.h"
#include "io.h"
#include "textfile.h"	/* dont_modify () */


/*======================================================================*\
|*		Local constants and types				*|
\*======================================================================*/

#define NO_MATCH	0
#define MATCH		1
#define REG_ERROR	2

#define BEGIN_LINE	(2 * REG_ERROR)
#define END_LINE	(2 * BEGIN_LINE)

/**
   The regex structure. Status can be any of 0, BEGIN_LINE or REG_ERROR.
   In the last case, the result.err_mess field is assigned.
   Start_ptr and end_ptr point to the match found.
   For more details see the documentation file.
 */
struct regex {
  union {
	char * err_mess;
	int * expression;
  } result;
  char status;
  LINE * start_line;
  char * start_ptr;
  LINE * end_line;
  char * end_ptr;
};

typedef struct regex REGEX;

#define NIL_REG		((REGEX *) 0)
#define NIL_INT		((int *) 0)


/*======================================================================*\
|*		Local function declarations and variables		*|
\*======================================================================*/

static int compile _((character * pat_start, REGEX * program, FLAG case_ignore));
static REGEX * make_expression _((char * expr, FLAG case_ignore));

static char * substitute _((LINE * line, LINE * * line_poi, REGEX * program, 
	int * old_lines_deleted, char * replacement, int * new_lines_inserted));
static void search _((char * message, FLAG method));
static void re_search _((void));
static void prev_search _((void));
static FLAG do_search _((REGEX * program, FLAG method));
static LINE * match _((REGEX * program, character * string, FLAG method));
static int line_check _((REGEX * program, char * str, LINE * from, FLAG method));
static int check_string _((REGEX * program, character * str, LINE * from, int * expression));
static int in_exprlist _((int * exprlist, character * s, int exprlist_length, int opcode));
static int star _((REGEX * program, char * end_position, char * str, LINE * from, int * expression));
static LINE * search_it _((REGEX * program, FLAG method));


static FLAG wrapped_search;		/* temporary flag of wrapped search */

static REGEX * lastprogram = NIL_REG;
static FLAG lastmethod = NOT_VALID;	/* FORWARD, REVERSE */

static char prevexpr [maxPROMPTlen];	/* Buffer for previous expr. */
static FLAG prevmethod = NOT_VALID;	/* FORWARD, REVERSE */


/*======================================================================*\
|*			Search commands					*|
\*======================================================================*/

/*
 * A regular expression consists of a sequence of:
 *	1. A normal character matching that character.
 *	2. A . matching any character.
 *	3. A ^ matching the begin of a line.
 *	4. A $ (as last character of the pattern) mathing the end of a line.
 *	5. A \<character> matching <character>.
 *	6. A number of characters enclosed in [] pairs matching any of these
 *	   characters. A list of characters can be indicated by a '-'. So
 *	   [a-z] matches any letter of the alphabet. If the first character
 *	   after the '[' is a '^' then the set is negated (matching none of
 *	   the characters).
 *	   A ']', '^' or '-' can be escaped by putting a '\' in front of it.
 *	7. If one of the expressions as described in 1-6 is followed by a
 *	   '*' than that expressions matches a sequence of 0 or more of
 *	   that expression.
 */

static char typed_expression [maxPROMPTlen];	/* Holds previous search expression */

/*
 * SIDFW searches forward for the current identifier.
 */
void
SIDFW ()
{
  SIDF (FORWARD);
}

/*
 * SIDRV searches backward for the current identifier.
 */
void
SIDRV ()
{
  SIDF (REVERSE);
}

void
SCURCHARFW ()
{
  SCURCHAR (FORWARD);
}

void
SCURCHARRV ()
{
  SCURCHAR (REVERSE);
}

void
SCURCHAR (method)
  FLAG method;
{
  char cbuf [7];
  char * cp = cur_text;

  Pushmark ();

  advance_char (& cp);
  cbuf [0] = '\0';
  if (cp - cur_text > 6) {
	ring_bell ();
	return;
  }
  strncat (cbuf, cur_text, cp - cur_text);

  search_for (cbuf, method, True);
}

/*
 * SFW searches forward for an expression.
 */
void
SFW ()
{
  if (hop_flag > 0) {
	SIDFW ();
  } else {
	search ("Search forward:", FORWARD);
  }
}

/*
 * SRV searches backward for an expression.
 */
void
SRV ()
{
  if (hop_flag > 0) {
	SIDRV ();
  } else {
	search ("Search backward:", REVERSE);
  }
}

/**
   reverse_method maps search directions FORWARD / REVERSE to each other,
   leaving NOT_VALID as is
 */
static
FLAG
reverse_method (dir)
  FLAG dir;
{
  if (dir == REVERSE) {
	return FORWARD;
  } else if (dir == FORWARD) {
	return REVERSE;
  } else {
	return dir;
  }
}

/*
 * RES searches using the last search direction and expression.
 */
void
RES ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	prev_search ();
  } else {
	re_search ();
  }
}

/*
 * RESreverse searches using the last search expression and reverse direction.
 */
void
RESreverse ()
{
  if (hop_flag > 0) {
	hop_flag = 0;
	search_for (prevexpr, reverse_method (prevmethod), True);
  } else {
	(void) do_search (lastprogram, reverse_method (lastmethod));
  }
}

/*
 * Get_expression () prompts for an expression. If just a return is typed, the
 * old expression is used. If the expression changed, compile () is called and
 * the returning REGEX structure is returned. It returns NIL_REG upon error.
 * The save flag indicates whether the expression should be appended at the
 * message pointer.
 */
static char exp_buf [maxPROMPTlen];	/* Buffer for new expr. */
static REGEX program;			/* Program of expression */

static
REGEX *
get_expression (message)
  char * message;
{
  if (get_string (message, exp_buf, False, "") == ERRORS) {
	return NIL_REG;
  }

  if (exp_buf [0] == '\0' && typed_expression [0] == '\0') {
	error ("No previous search expression");
	return NIL_REG;
  }

  if (exp_buf [0] != '\0') {			/* A new expr. was given */
	copy_string (typed_expression, exp_buf);	/* Save expr. */
	/* Compile new expression: */
	if (compile (exp_buf, & program, True) == ERRORS) {
		return NIL_REG;
	}
  }

  if (program.status == REG_ERROR) {	/* Error during compiling */
	error (program.result.err_mess);
	return NIL_REG;
  }

  return & program;
}

/*
 * make_expression is only called by search_for and maintains its own, 
 * independant search program buffer
 */
static
REGEX *
make_expression_this (expr, case_ignore, programpoi)
  char * expr;
  FLAG case_ignore;
  REGEX * programpoi;
{
  if (compile (expr, programpoi, case_ignore) == ERRORS) {
	/* Compile new expression */
	return NIL_REG;
  }

  if (programpoi->status == REG_ERROR) {	/* Error during compiling */
	error (programpoi->result.err_mess);
	return NIL_REG;
  }
  return programpoi;
}

static
REGEX *
make_expression (expr, case_ignore)
  char * expr;
  FLAG case_ignore;
{
  static REGEX program;		/* Program of expression */

  return make_expression_this (expr, case_ignore, & program);
}

/*
 * Change () prompts for an expression and a substitution pattern and changes
 * all matches of the expression into the substitution.
 * change () starts looking for expressions at the current line and
 * continues until the end of the file if the FLAG 'global' is VALID.
 * It prompts for each change if the FLAG 'confirm' is True.
 * For a call graph of search procedures see search ().
 */
static
void
change (message, global, confirm)
  char * message;		/* Message to prompt for expression */
  FLAG global;
  FLAG confirm;
{
  char mess_buf [maxPROMPTlen];		/* Buffer to hold message */
  char replacement [maxPROMPTlen];	/* Buffer to hold subst. pattern */
  REGEX * program;			/* Program resulting from compilation */
  LINE * line = cur_line;
  char * searchfrom = cur_text;
  int text_offset = cur_text - cur_line->text;
  char * textp;
  long subs = 0L;		/* Nr of substitutions made */
  long lines = 0L;		/* Nr of lines on which subs occurred */
  int ly = y;			/* Index to check if line is on screen */
  int previousy = y;
  FLAG quit_change;
  int lines_changed = 0;

  if (dont_modify ()) {
	return;
  }

/* Save message and get expression */
  if ((program = get_expression (message)) == NIL_REG) {
	return;
  }

  lastprogram = program;
  lastmethod = FORWARD;

/* Get substitution pattern */
  build_string (mess_buf, "%s %s by:", message, typed_expression);
  if (get_string (mess_buf, replacement, False, "") == ERRORS) {
	return;
  }

  if (confirm) {
	Pushmark ();
  }

  set_cursor (0, YMAX);
  flush ();
/* Substitute until end of file */
  do {
	int old_lines_deleted = 0;
	int new_lines_inserted;
	if (line_check (program, searchfrom, line, FORWARD)) {
	    lines ++;
	    /* Repeat sub. on this line as long as we find a match */
	    do {
		if (confirm) {
			char c;
			ly = find_y (line);
			textp = program->start_ptr;
			move_address (program->start_ptr, ly);
			display_flush ();
			set_cursor_xy ();
			status_uni ("Replace ? (y/n/ESC)");
			c = prompt ("yn");
			clear_status ();
			if (c == 'y') {
			    int prev_last_y = last_y;
			    subs ++;	/* Increment # substitutions made */
			    textp = substitute (line, & line, 
					program, & old_lines_deleted, 
					replacement, & new_lines_inserted);
			    if (old_lines_deleted > 0 || new_lines_inserted > 0) {
				/* reset bot_line and last_y */
				reset (top_line, y);
				/* update display */
				if (prev_last_y > last_y) {
					display (y, cur_line, prev_last_y - y, y);
				} else {
					display (y, cur_line, last_y - y, y);
				}
			    } else {
				print_line (ly, line);
			    }
			    if (textp == NIL_PTR) {
				/* substitution failed (too long or no mem) */
				    return;
			    }
			} else {
			    advance_char (& textp);
			}
		} else {
		    subs ++;	/* Increment # substitutions made */
		    line->shift_count = 0;
			/* in case line would get completely shifted out */
		    textp = substitute (line, & line, 
					program, & old_lines_deleted, 
					replacement, & new_lines_inserted);
#ifdef debug_repl_eol
			printf ("subst(%d) |%s         ^%s", 
				old_lines_deleted, line->text, textp);
#endif
		    if (textp == NIL_PTR) {
			/* substitution failed (line too long or no memory) */
			if (ly > SCREENMAX) {
				ly = find_y (line);
				x = 0;
			} else if (ly != y) {
				x = 0;
			}
			if (old_lines_deleted > 0 || new_lines_inserted > 0) {
				display (y, cur_line, last_y - y, y);
				reset (top_line, y);	/* reset bot_line */
			} else {
				print_line (ly, line);
			}
			move_to (x, ly);
			return;
		    }
		    if (old_lines_deleted > 0 || new_lines_inserted > 0) {
			lines_changed = 1;
		    }
		}
	    } while (  (program->status & BEGIN_LINE) != BEGIN_LINE
		    && (program->status & END_LINE) != END_LINE
		    && line_check (program, textp, line, FORWARD)
		    && * textp != '\n'
		    && quit == False);

	    /* Check to see if we can print the result */
	    if (confirm == False && ly <= SCREENMAX) {
		if (lines_changed) {
#ifdef doesnt_work
			display (y, cur_line, last_y - y, y);
			reset (top_line, y);	/* reset bot_line */
#endif
		} else {
			print_line (ly, line);
		}
	    }
	}
	if (ly <= SCREENMAX) {
		ly ++;
	}
	if (old_lines_deleted > 0) {
		searchfrom = textp;
	} else {
		line = line->next;
		searchfrom = line->text;
	}
  } while (line != tail && global == VALID && quit == False);
  if (confirm == False && lines_changed) {
	RDwin ();
  }

  quit_change = quit;
/* Fix the status line */
  if (subs == 0L && quit == False) {
	error ("No substitution");
  } else if (lines >= REPORT_CHANGED_LINES || quit) {
	build_string (mess_buf, "%s%ld substitution%s on %ld line%s",
		quit_change ? "(Replace aborted) " : "",
		subs, subs > 1 ? "s" : "",
		lines, lines > 1 ? "s" : "");
	status_msg (mess_buf);
  } else if (global == NOT_VALID && subs >= REPORT_CHANGED_LINES) {
	status_line (dec_out (subs),
			subs > 1 ? " substitutions" :  " substitution");
  } else {
	clear_status ();
  }

  if (confirm) {
	move_to (x, y);
  } else {
/*	move_to (LINE_START, previousy);	*/
	move_address (cur_line->text + text_offset, previousy);
  }

/*  if (quit) swallow_dummy_quit_char (); */
  quit = False;
}

/*
 * Substitute () replaces the match on this line by the substitute pattern
 * as indicated by the program. Every '&' in the replacement is replaced by
 * the original match. A \ in the replacement escapes the next character.
 */
static
char *
substitute (line, line_poi, 
		program, old_lines_deleted, 
		replacement, new_lines_inserted)
  LINE * line;
  LINE * * line_poi;
  REGEX * program;
  int * old_lines_deleted;
  char * replacement;	/* Contains replacement pattern */
  int * new_lines_inserted;
{
  char text_buffer [maxLINElen];
  register char * textp = text_buffer;
  register char * subp = replacement;
  char * linep = line->text;
  char * amp;
  LINE * ampl;
  char * newtext;
  char * repl_start;
  char * repl_end;
  int repl_len;
  lineend_type end_return_type = program->end_line->return_type;

  set_modified ();

/* Copy part of line until the beginning of the match */
  while (linep != program->start_ptr) {
	* textp ++ = * linep ++;
  }

/*
 * Replace the match by the substitution pattern. Each occurrence of '&' is
 * replaced by the original match. A \ escapes the next character.
 */
  while (* subp != '\0' && textp < & text_buffer [maxLINElen]) {
	if (* subp == '&') {		/* Replace the original match */
		amp = program->start_ptr;
		ampl = program->start_line;
		/* Do not report an error here if aborting */
		while (textp < & text_buffer [maxLINElen] &&
			(ampl != program->end_line || amp < program->end_ptr)
		) {
			if (* amp == '\n') {
				/* include original newline */
				if (ampl->return_type != lineend_NONE) {
					* textp ++ = '\n';
				}
				ampl = ampl->next;
				amp = ampl->text;
			} else {
				* textp ++ = * amp ++;
			}
		}
		subp ++;
	} else {
		if (* subp == '\\' && * (subp + 1) != '\0') {
			subp ++;
			if (* subp == 'n') {
				* textp = '\n';
			} else if (* subp == 'r') {
				* textp = '\r';
			} else {
				* textp = * subp;
			}
			textp ++;
			subp ++;
		} else {
			* textp ++ = * subp ++;
		}
	}
  }
  * textp = '\0';

/* Check for line length not exceeding maxLINElen */
  if (length_of (text_buffer) + length_of (program->end_ptr) >= maxLINElen) {
	if (program->end_line != program->start_line) {
		error ("Substitution failed: replacement buffer exceeded");
	} else {
		error ("Substitution failed: resulted line too long");
	}
	return NIL_PTR;
  }

/* Append last part of line to the newly built line */
  copy_string (textp, program->end_ptr);

/* Check if substituting in more than 1 line; delete lines then */
  if (program->end_line != program->start_line) {
	(void) delete_text_only (line->next, line->next->text, 
		     program->end_line->next, program->end_line->next->text);
	* old_lines_deleted = 9;
  } else {
	* old_lines_deleted = 0;
  }


/* Insert replacement string; may be multiple lines */

  * new_lines_inserted = 0;

  repl_end = strchr (text_buffer, '\n');
  if (repl_end == NIL_PTR) {
	repl_end = strchr (text_buffer, '\0');
  } else {
	repl_end ++;
  }
  repl_len = (int) (repl_end - text_buffer);

/* Free old line and install new one */
/*  newtext = alloc (length_of (text_buffer) + 1); */
  newtext = alloc (repl_len + 1);
  if (newtext == NIL_PTR) {
	ring_bell ();
	error ("Substitution failed: cannot allocate more memory");
	return NIL_PTR;
  } else {
	free_space (line->text);
	line->text = newtext;
/*	copy_string (line->text, text_buffer);	*/
	strncpy (line->text, text_buffer, repl_len);
	line->text [repl_len] = '\0';
	update_syntax_state (line);
	linep = line->text + (int) (textp - text_buffer);
  }

  while (* repl_end != '\0') {
	lineend_type old_return_type = line->return_type;

	(* new_lines_inserted) ++;
	repl_start = repl_end;
	repl_end = strchr (repl_start, '\n');
	if (repl_end == NIL_PTR) {
		repl_end = strchr (repl_start, '\0');
	} else {
		repl_end ++;
	}
	repl_len = (int) (repl_end - repl_start);

	if (old_return_type == lineend_NONE || old_return_type == lineend_NUL) {
		line->return_type = default_lineend;
	}
	line = line_insert_after (line, repl_start, repl_len, old_return_type);
	if (line == NIL_LINE) {
		ring_bell ();
		status_fmt2 ("Out of memory - ", "Substitution failed");
		return NIL_PTR;
	}
	if (* repl_end == '\0') {
		line->return_type = end_return_type;
	}
	* line_poi = line;
	linep = line->text + (int) (textp - repl_start);
  }
  return linep;
}

/*
 * GR () a replaces all matches from the current position until the end
 * of the file.
 */
void
GR ()
{
  change ("Global replace:", VALID, False);
}

/*
 * Replace is substitute with confirmation dialogue.
 */
void
REPL ()
{
  change ("Global replace (with confirm):", VALID, True);
}

/*
 * LR () replaces all matches on the current line.
 */
void
LR ()
{
  change ("Line replace:", NOT_VALID, False);
}

/*
 * Search () calls get_expression to fetch the expression. If this went well,
 * the function match () is called which returns the line with the next match.
 * If this line is the NIL_LINE, it means that a match could not be found.
 * Find_x () and find_y () display the right page on the screen, and return
 * the right coordinates for x and y.
 * These coordinates are passed to move_to ().
 * Re_search () searches using the last search program and direction.
   Call graph of search procedures:
RES ------------\
SFW -\		> re_search > match -----------\
SRV --> search <				\
	        \				 \
	         > get_expression > compile	  > line_check > check_string
GR  \	        /				 /
REPL > change  <--------------------------------/
LR  /	        \
		 \ substitute

 */
static
void
re_search ()
{
  (void) do_search (lastprogram, lastmethod);
}

static
void
prev_search ()
{
  search_for (prevexpr, prevmethod, True);
}

static
void
search (message, method)
  char * message;
  FLAG method;
{
  register REGEX * program;
  char prevexp_buf [maxPROMPTlen];	/* Buffer for previous expr. */

  if (lastmethod != NOT_VALID) {
	copy_string (prevexp_buf, exp_buf);
  }

/* Get the expression */
  if ((program = get_expression (message)) == NIL_REG) {
	return;
  }

  if (program != NIL_REG && lastmethod != NOT_VALID) {
	copy_string (prevexpr, prevexp_buf);
	prevmethod = lastmethod;
  }
  lastprogram = program;
  lastmethod = method;

  Pushmark ();

  re_search ();
}

/*
   Search for expression, do not remember it for next search.
 */
void
search_for (expr, method, case_ignore)
  char * expr;
  FLAG method;
  FLAG case_ignore;
{
  register REGEX * program;

/* make the expression */
  if ((program = make_expression (expr, case_ignore)) == NIL_REG) {
	return;
  }
  (void) do_search (program, method);
}

/*
   Search for expression, remember it for next search.
 */
FLAG
search_ini (expr, method, case_ignore)
  char * expr;
  FLAG method;
  FLAG case_ignore;
{
  register REGEX * program;

/* make the expression */
  if ((program = make_expression (expr, case_ignore)) == NIL_REG) {
	return False;
  }
  lastprogram = program;
  lastmethod = method;
  return do_search (program, method);
}

/*
   Search for expression, remember it for next search.
 */
void
search_expr (expr, method, case_ignore)
  char * expr;
  FLAG method;
  FLAG case_ignore;
{
  register REGEX * program;

/* make the expression */
  if ((program = make_expression (expr, case_ignore)) == NIL_REG) {
	return;
  }

  Pushmark ();

  lastprogram = program;
  lastmethod = method;
  (void) do_search (program, method);
}

/*
 * search_corresponding searches for a corresponding bracket of a 
 * bracket pair expression.
 * The parameter 'method' must be valid.
 * Special handling is triggered by special values of 'corresponding':
   "/": XML tag matching
   "#": #if/#elif/#else/#endif matching
 */
void
search_corresponding (expr, method, corresponding)
  char * expr;
  FLAG method;
  char * corresponding;
{
  register REGEX * program;
  LINE * match_line;
  int bracount = 0;
  LINE * old_line = cur_line;
  char * old_text = cur_text;
  FLAG was_redraw_pending;
#ifdef debug_corresponding_redraw
  char pflags [9999];
#endif
  static REGEX myprogram;

/* make the expression */
  if ((program = make_expression_this (expr, True, & myprogram)) == NIL_REG) {
	return;
  }

  redraw_pending = False;
#ifdef debug_corresponding_redraw
  strcpy (pflags, redraw_pending ? "T" : "F");
#endif
  wrapped_search = False;
  do {
	int matched;
	/* only used if * corresponding == '#': (avoid -Wmaybe-uninitialized) */
	int nestingweight = 0;

	do {
		match_line = search_it (program, method);
		if (match_line == NIL_LINE) {
			return;
		}
		if (wrapped_search) {
			status_msg ("No corresponding bracket");
			move_address (old_text, find_y (old_line));
			return;
		}
		/* now check if this is really a match */
		if (* corresponding == '/') {
			/* XML tag matching */
			matched = white_space (* program->end_ptr)
				|| * program->end_ptr == '\n'
				|| * program->end_ptr == '>'
				|| * program->end_ptr == '\0';
		} else if (* corresponding == '#') {
			/* #if/#elif/#else/#endif matching */
			char * poi = program->start_ptr;
			poi ++;
			while (white_space (* poi) && * poi != '\n' && * poi != '\0') {
				poi ++;
			}
			if (strisprefix ("if", poi)) {
				matched = 1;
				nestingweight = 1;
			} else if (strisprefix ("el", poi)) {
				matched = 1;
				nestingweight = 0;
			} else if (strisprefix ("end", poi)) {
				matched = 1;
				nestingweight = -1;
			} else {
				matched = 0;
			}
		} else {
			matched = 1;
		}

		if (quit == False && ! matched) {
			move_address_w_o_RD (old_text, find_y_w_o_RD (old_line));
			move_address (program->start_ptr, find_y (match_line));
		}
	} while (quit == False && ! matched);

	if (* corresponding == '/') {
		/* XML tag matching */
		if ((method == FORWARD) == (* (program->start_ptr + 1) == '/')) {
			bracount --;
		} else {
			bracount ++;
		}
	} else if (* corresponding == '#') {
		/* #if/#elif/#else/#endif matching */
		if (bracount == 0 && nestingweight == 0) {
			bracount = -1;
		} else if (method == FORWARD) {
			bracount += nestingweight;
		} else {
			bracount -= nestingweight;
		}
	} else {
		if (strncmp (program->start_ptr, corresponding, strlen (corresponding)) == 0) {
			bracount --;
		} else {
			bracount ++;
		}
	}

	if (bracount >= 0) {
	/* set position to match for next search: */
		move_address_w_o_RD (program->start_ptr, find_y_w_o_RD (match_line));
#ifdef debug_corresponding_redraw
		strcat (pflags, redraw_pending ? "T" : "F");
#endif
	}

	if (quit) {
		status_msg ("Bracket search aborted");
		quit = False;
		move_address (old_text, find_y (old_line));
		return;
	}
  } while (bracount >= 0);

/* set position to old to enable correct function of find_y: */
  move_address_w_o_RD (old_text, find_y_w_o_RD (old_line));
#ifdef debug_corresponding_redraw
  strcat (pflags, redraw_pending ? "T" : "F");
#endif

/* position and re-display if necessary: */
  was_redraw_pending = redraw_pending;
  move_address (program->start_ptr, find_y (match_line));
#ifdef debug_corresponding_redraw
  strcat (pflags, redraw_pending ? "T" : "F");
#endif
  if (was_redraw_pending && redraw_pending) {
	RD ();
#ifdef debug_corresponding_redraw
	status_line ("pending redraw performed ", pflags);
#endif
  }
}

/*
 * do_search checks its parameters, does the search, and positions
 * For search_it, arguments must be valid, and it does not position
 */
static
LINE *
search_it (program, method)
  register REGEX * program;
  FLAG method;
{
  register LINE * match_line;

  set_cursor (0, YMAX);
  clear_status ();
  flush ();
/* Find the match */
  if ((match_line = match (program, cur_text, method)) == NIL_LINE) {
	if (quit) {
		status_msg ("Search aborted");
/*		swallow_dummy_quit_char (); */
		quit = False;
	} else if (program->status == REG_ERROR) {
		status_msg ("Search program corrupted");
	} else {
		status_msg ("Pattern not found");
	}
  }
  return match_line;
}

static
FLAG
do_search (program, method)
  register REGEX * program;
  FLAG method;
{
  register LINE * match_line;

  if (method == NOT_VALID) {
	error ("No previous search");
	return False;
  }
  if (program == NIL_REG) {
	error ("No previous search expression");
	return False;
  }

  if (! visselect_keeponsearch) {
	clear_highlight_selection ();
  }

  match_line = search_it (program, method);
  if (match_line == NIL_LINE) {
	return False;
  }
  move_address (program->start_ptr, find_y (match_line));
  return True;
}


/*======================================================================*\
|*			Search/replace utility functions		*|
\*======================================================================*/

/* Opcodes for characters */
#define NORMAL		0x0200
#define DOT		0x0400
#define ENDOFLN		0x0800
#define STAR		0x8000
#define NEGATE		0x4000
#define BRACKET		0x2000
#define DONE		0x1000

/* Mask for opcodes and characters */
#define LOW_BYTE	0x00FF
#define OPCODE_MASK	0xFE00
#define LENGTH_MASK	0x01FF

/* Previous is the content of the previous address (ptr) points to */
#define previous(ptr)		(* ((ptr) - 1))

/* Buffer to store outcome of compilation */
#define MAX_EXP_SIZE	LENGTH_MASK	/* 511 */
static int exp_buffer [MAX_EXP_SIZE];

/* Errors often used */
static char * too_long = "Regular expression too long";

/*
 * reg_error () is called by compile () if something went wrong. It sets the
 * status of the structure to error, and assigns the error field of the union.
 */
#define reg_error(str)	program->status = REG_ERROR, \
					program->result.err_mess = (str)

/*
 * Finished () is called when everything went right during compilation. It
 * allocates space for the expression, and copies the expression buffer into
 * this field.
 */
static
int
finished (program, last_exp)
  register REGEX * program;
  int * last_exp;
{
  register unsigned int length = (last_exp - exp_buffer) * sizeof (int);
/* Allocate space */
  program->result.expression = alloc (length);
  if (program->result.expression == NIL_INT) {
	ring_bell ();
	error ("Cannot allocate memory for search expression");
	return ERRORS;
  } else {	/* Copy expression. (expression consists of ints!) */
	memcpy (program->result.expression, exp_buffer, length);
	return FINE;
  }
}

#define dont_debug_expression
#ifdef debug_expression
#define trace_expression(params)	indent_trace (); print_expression params
#define trace_search(params)	indent_trace (); printf params
#define trace_indent()	trace_indentation ++;
#define trace_undent()	trace_indentation --;

static int trace_indentation = 0;

static
void
indent_trace ()
{
  int i;
  for (i = 0; i < trace_indentation; i ++) {
	printf (" ");
  }
}

static
void
print_expression (expr)
  int * expr;
{
  char s [22];
  int ei = 0;

  while (ei < MAX_EXP_SIZE) {
	s [0] = '\0';
	if (* expr & NORMAL)	strcat (s, "=");
	if (* expr & NEGATE)	strcat (s, "!");
	if (* expr & DOT)	strcat (s, ".");
	if (* expr & ENDOFLN)	strcat (s, "\\n");
	if (* expr & STAR)	strcat (s, "*");
	if (* expr & BRACKET)	strcat (s, "[");
	if (* expr & DONE)	strcat (s, "DONE");
	printf ("%s%04X ", s, * expr);

	ei ++;
	if (* expr & DONE) break;
	if (* expr == 0) break;
	expr ++;
  }
  printf (" [exp size %d]\n", ei);
}
#else
#define trace_expression(params)	
#define trace_search(params)	
#define trace_indent()	
#define trace_undent()	
#endif


/*======================================================================*\
|*			Compile search expression			*|
\*======================================================================*/

/*
 * Compile compiles the pattern into a more comprehensible form and returns a
 * REGEX structure. If something went wrong, the status field of the structure
 * is set to REG_ERROR and an error message is set into the err_mess field of
 * the union. If all went well the expression is saved and the expression
 * pointer is set to the saved (and compiled) expression.
 */
static
int
compile (pat_start, program, case_ignore)
  character * pat_start;	/* Pointer to pattern */
  REGEX * program;
  FLAG case_ignore;
{
  register int * expression = exp_buffer;
  character * pattern = pat_start;
  int * prev_char;		/* Pointer to previous compiled atom */
  int * acct_field = NIL_INT;	/* Pointer to last BRACKET start */
  FLAG negate;			/* Negate flag for BRACKET */
  character c;

  unsigned long low_char;
  unsigned long high_char;
  int charlen;
  character utfbuf [7];
  character * utfpoi;

  trace_search (("compiling\n"));

/* Check for begin of line */
  if (* pattern == '^') {
	program->status = BEGIN_LINE;
	pattern ++;
  } else {
	program->status = 0;
  }
/* If the first character is a '*' we have to assign it here. */
  if (* pattern == '*') {
	* expression ++ = '*' + NORMAL;
	pattern ++;
  }

  for (; ;) {
	c = * pattern ++;
	trace_search (("	compile @%02X\n", c));
	switch (c) {
	case '.' :
		* expression ++ = DOT;
		break;
	case '$' :
		/*
		 * Only means ENDOFLN if it is the last char of the pattern
		 */
		if (* pattern == '\0') {
			* expression ++ = ENDOFLN | DONE;
			program->status |= END_LINE;
			trace_expression ((exp_buffer));
			return finished (program, expression);
		} else {
			* expression ++ = NORMAL + '$';
		}
		break;
	case '\0' :
		* expression ++ = DONE;
		trace_expression ((exp_buffer));
		return finished (program, expression);
	case '\n' :
		* expression ++ = (NORMAL | ENDOFLN) + '\n';
		break;
	case '\\' :
		/* If last char, it must! mean a normal '\' */
		if (* pattern == '\0') {
			* expression ++ = NORMAL + '\\';
		} else {
			c = * pattern ++;
			if (c == 'n') {
				* expression ++ = (NORMAL | ENDOFLN) + 'n';
			} else if (c == 'r') {
				* expression ++ = (NORMAL | ENDOFLN) + '\r';
			} else if (c == 'R') {
				* expression ++ = (NORMAL | ENDOFLN) + 'R';
			} else if (c == '0') {
				/* NUL is represented as pseudo-line-end */
				* expression ++ = (NORMAL | ENDOFLN) + '\0';
			} else {
				* expression ++ = NORMAL + c;
			}
		}
		break;
	case '*' :
		/*
		 * If the previous expression was a [] find out the
		 * begin of the list, and adjust the opcode.
		 */
		prev_char = expression - 1;
		if (* prev_char & BRACKET) {
			* (expression - (* acct_field & LOW_BYTE)) |= STAR;
		} else {
			* prev_char |= STAR;
		}
		break;
	case '[' :
		/*
		 * First field in expression gives information about
		 * the list.
		 * The opcode consists of BRACKET and if necessary
		 * NEGATE to indicate that the list should be negated
		 * and/or STAR to indicate a number of sequence of this
		 * list.
		 * The lower byte contains the length of the list.
		 */
		acct_field = expression ++;
		if (* pattern == '^') {	/* List must be negated */
			pattern ++;
			negate = True;
		} else {
			negate = False;
		}
		while (* pattern != ']') {
		    if (* pattern == '\0') {
			reg_error ("Missing ]");
			trace_expression ((exp_buffer));
			return FINE;
		    }
		    if (* pattern == '\\') {
			pattern ++;
		    }
		    if (expression >= & exp_buffer [MAX_EXP_SIZE]) {
			reg_error (too_long);
			trace_expression ((exp_buffer));
			return FINE;
		    }
		    * expression ++ = * pattern ++;
		    if (* pattern == '-') {
			/* Make list of chars */

			/* determine lower range limit from previous char */
			low_char = precedingchar (pattern, pat_start);

			pattern ++;	/* Skip '-' */

			/* determine upper range limit */
			high_char = charvalue (pattern);

			if (low_char ++ > high_char) {
				reg_error ("Bad range in [..-..] search expression");
				trace_expression ((exp_buffer));
				return FINE;
			}

			/* Build list */
			while (low_char <= high_char && low_char != 0) {
				if (utf8_text) {
					charlen = utfencode (low_char, utfbuf);
				} else if (cjk_text) {
					charlen = cjkencode (low_char, utfbuf);
				} else {
					utfbuf [0] = low_char;
					utfbuf [1] = '\0';
				}
				utfpoi = utfbuf;
				while (* utfpoi != '\0') {
					if (expression >= & exp_buffer [MAX_EXP_SIZE]) {
						reg_error (too_long);
						trace_expression ((exp_buffer));
						return FINE;
					}
					* expression ++ = * utfpoi ++;
				}

			    low_char ++;
			}

			advance_char ((char * *) & pattern);
		    }
		    if (expression >= & exp_buffer [MAX_EXP_SIZE - 1]) {
			reg_error (too_long);
			trace_expression ((exp_buffer));
			return FINE;
		    }
		}
		pattern ++;			/* Skip ']' */

		/* Assign length of list in acct field */
		if ((* acct_field = (int) (expression - acct_field)) == 1) {
			reg_error ("Empty []");
			trace_expression ((exp_buffer));
			return FINE;
		}
		/* Assign negate and bracket field */
		* acct_field |= BRACKET;
		if (negate) {
			* acct_field |= NEGATE;
		}
		/*
		 * Add BRACKET to opcode of last char in field because
		 * a '*' may be following the list.
		 */
		previous (expression) |= BRACKET;
		break;
	default : {
		  unsigned long letter;
		  unsigned long upper;
		  unsigned long title;
		  FLAG do_case_ignore;

		  pattern --;	/* reset to begin of character */

		  /* trigger case-insensitive search on small letter */
		  letter = unicodevalue (pattern);
		  upper = case_convert (letter, 1);
		  do_case_ignore = case_ignore && ! no_char (upper) && upper != letter;

		  if (do_case_ignore || ((utf8_text || cjk_text) && multichar (c))) {
			int charcount;

			/* simulate single-char [u] pattern */
			acct_field = expression ++;

			/* determine multi-byte char length */
			if (utf8_text) {
				utf8_info (pattern, & charlen, & low_char);
			} else if (cjk_text) {
				charlen = CJK_len (pattern);
			} else {
				charlen = 1;
			}

			for (charcount = 0; charcount < charlen; charcount ++) {
				if (expression >= & exp_buffer [MAX_EXP_SIZE]) {
					reg_error (too_long);
					trace_expression ((exp_buffer));
					return FINE;
				}
				* expression ++ = * pattern ++;
			}

			/* make pattern for case-insensitive search */
			if (do_case_ignore) {
				char * ec = encode_char (encodedchar (upper));
				while (* ec) {
					if (expression >= & exp_buffer [MAX_EXP_SIZE]) {
						reg_error (too_long);
						trace_expression ((exp_buffer));
						return FINE;
					}
					* expression ++ = * ec ++;
				}
				trace_expression ((exp_buffer));
				title = case_convert (letter, 2);
				if (! no_char (title) && title != upper) {
					char * ec = encode_char (encodedchar (title));
					while (* ec) {
						if (expression >= & exp_buffer [MAX_EXP_SIZE]) {
							reg_error (too_long);
							trace_expression ((exp_buffer));
							return FINE;
						}
						* expression ++ = * ec ++;
					}
					trace_expression ((exp_buffer));
				}
			}

			/* Assign length of list in acct field */
			* acct_field = (int) (expression - acct_field);
			/* Assign negate and bracket field */
			* acct_field |= BRACKET;
			/*
			 * Add BRACKET to opcode of last char in field because
			 * a '*' may be following the list.
			 */
			previous (expression) |= BRACKET;
		  } else {
			* expression ++ = c + NORMAL;
			pattern ++;
		  }
		}
	}
	if (expression == & exp_buffer [MAX_EXP_SIZE]) {
		reg_error (too_long);
		trace_search (("compiled "));
		trace_expression ((exp_buffer));
		return FINE;
	}
  }
  /* NOTREACHED */
}


/*======================================================================*\
|*			Invoke search					*|
\*======================================================================*/

/*
 * Match gets as argument the program, pointer to place in current line to
 * start from and the method to search for (either FORWARD or REVERSE).
 * Match () will look through the whole file until a match is found.
 * NIL_LINE is returned if no match could be found.
 */
static
LINE *
match (program, string, method)
  REGEX * program;
  character * string;
  register FLAG method;
{
  register LINE * line = cur_line;
  char * next = (char *) string;

/* Corrupted program */
  if (program->status == REG_ERROR) {
	return NIL_LINE;
  }

  trace_search (("match %s", string));
  trace_indent ();

/* Check current line first */
  if (method == FORWARD && ! (program->status & BEGIN_LINE)) {
	advance_char (& next);
	if (line_check (program, next, line, method) == MATCH) {
		trace_search (("-> match 1 found\n"));
		trace_undent ();
		return cur_line;	/* Match found */
	}
  }
  if (method == REVERSE && ! (program->status & END_LINE)) {
	character old_charbyte = * string;	/* Save char and */
	* string = '\n';		/* Assign '\n' for line_check */
	if (line_check (program, line->text, line, method) == MATCH) {
		* string = old_charbyte; /* Restore char */
		trace_search (("-> match 2 found\n"));
		trace_undent ();
		return cur_line;    /* Found match */
	}
	* string = old_charbyte;    /* No match, but restore char */
  }

/* No match in last (or first) part of line. Check out rest of file */
  do {
	line = (method == FORWARD) ? line->next : line->prev;
	if (line->text == NIL_PTR) {	/* header/tail */
		if (method == FORWARD) {
			status_uni (">>>>>>>> Search wrapped around end of file >>>>>>>>");
		} else {
			status_uni ("<<<<<<<< Search wrapped around top of file <<<<<<<<");
		}
		wrapped_search = True;
		continue;
	}
	if (line_check (program, line->text, line, method) == MATCH) {
		trace_search (("-> match 3 found\n"));
		trace_undent ();
		return line;
	}
  } while (line != cur_line && quit == False);

/* No match found. */
  trace_undent ();
  return NIL_LINE;
}

/*
 * Line_check () checks the line (or rather string) for a match. Method
 * indicates FORWARD or REVERSE search. It scans through the whole string
 * until a match is found, or the end of the string is reached.
 */
static
int
line_check (program, string, from_line, method)
  register REGEX * program;
  char * string;
  LINE * from_line;
  FLAG method;
{
  char * textp = string;

  trace_search (("line_check [%d] %s", string - from_line->text, string));
  trace_indent ();

/* Assign start_ptr field. We might find a match right away! */
  program->start_ptr = textp;
  program->start_line = from_line;

/* If the match must be anchored, just check the string. */
  if (program->status & BEGIN_LINE) {
	if (string == from_line->text) {
		int ret = check_string (program, string, from_line, NIL_INT);
		trace_undent ();
		return ret;
	} else {
		trace_undent ();
		return NO_MATCH;

	}
  }

  if (method == REVERSE) {
	/* First move to the end of the string */
	for (textp = string; * textp != '\n'; textp ++)
		{}
	/* Start checking string until the begin of the string is met */
	while (textp >= string) {
		program->start_ptr = textp;
		if (check_string (program, textp, from_line, NIL_INT)) {
			trace_undent ();
			return MATCH;
		}
		if (textp == string) {
			textp --;	/* just flag exit */
		} else {
			precede_char (& textp, string);
		}
	}
  } else {	/* method == FORWARD */
	/* Move through the string until the end of it is found */
	while (quit == False && * textp != '\0') {
		program->start_ptr = textp;
		if (check_string (program, textp, from_line, NIL_INT)) {
			trace_undent ();
			return MATCH;
		}
		if (* textp == '\n') {
			break;
		}
		advance_char (& textp);
	}
  }

  trace_undent ();
  return NO_MATCH;
}

/*
 * Check_string () checks if a match can be found in the given string.
 * Whenever a STAR is found during matching, then the begin position of
 * the string is marked and the maximum number of matches is performed.
 * Then the function star () is called which starts to finish the match
 * from this position of the string (and expression).
 * Check () returns MATCH for a match, NO_MATCH if the string
 * couldn't be matched or REG_ERROR for an illegal opcode in expression.
 */
static
int
check_string (program, string, from_line, expression)
  REGEX * program;
  character * string;
  LINE * from_line;
  int * expression;
{
  register int opcode;		/* holds opcode of next expr. atom */
  character patternchar;	/* char that must be matched */
  int exprlist_length;
  int ret;
  int has_star;			/* pattern has a star */
  /* only used if has_star: (avoid -Wmaybe-uninitialized) */
  character * mark = (character *) NIL_PTR;	/* for marking position */

  if (expression == NIL_INT) {
	expression = program->result.expression;
  }

  trace_search (("check_string [%d] %s", (char *) string - from_line->text, string));
  trace_expression ((expression));
  trace_indent ();

/* Loop until end of string or end of expression */
  while (quit == False && ! (* expression & DONE) && * string != '\0') {
	trace_search (("check [%d] %s", (char *) string - from_line->text, string));
	trace_expression ((expression));

	patternchar = * expression & LOW_BYTE;	/* extract match char */
	opcode = * expression & OPCODE_MASK;	/* extract opcode */
	exprlist_length = * expression & LENGTH_MASK; /* extract length */

	has_star = (opcode & STAR) != 0;	/* check star occurrence */
	if (has_star) {
		/* strip opcode */
		opcode &= ~STAR;
		/* mark current character */
		mark = string;
		/* the following fix is not needed as multi-byte 
		   characters in the search pattern are compiled 
		   into single-element range search expressions
		*/
		/* mark = charbegin (from_line, string); */
	}
	expression ++;		/* Increment expr. */

	if (opcode & ENDOFLN) {
	    if (* string == '\n') {
		/* handle multi-line matching */
		lineend_type rettype = from_line->return_type;
		if ((rettype == lineend_NUL && patternchar == '\0')
		 || (rettype != lineend_NONE && rettype != lineend_NUL &&
			  (patternchar == '\n'
			|| (patternchar == '\r' && rettype == lineend_CRLF)
			|| (patternchar == 'R' && rettype == lineend_CR)
			|| (patternchar == 'n' && rettype == lineend_LF)
			  )
		    )
		   )
		{
			if (from_line->next == tail) {
				if (! (* expression & DONE)) {
					trace_undent ();
					return NO_MATCH;
				}
				/* matching at tail */
				/* however, we would need to 
				   forward from_line and string 
				   to undefined values;
				   thus not matching below...
				 */
			} else {
				ret = check_string (program, 
					from_line->next->text, 
					from_line->next, expression);
				trace_undent ();
				return ret;
			}
		} else {
			trace_undent ();
			return NO_MATCH;
		}
	    } else {
		trace_undent ();
		return NO_MATCH;
	    }
	} else
	switch (opcode & ~ENDOFLN) {
	case NORMAL :
		/* this case only applies to single-byte characters 
		   as multi-byte characters in the search pattern are 
		   compiled into single-element range search expressions
		*/
		if (has_star) {
			while (* string == patternchar) {
			/* Skip all matches */
			/*	advance_char (& string);	*/
				string ++;
			}
		/*	advance_char (& string);	*/
		/*	string ++;	???	*/
		} else {
			if (* string == patternchar) {
			/*	advance_char (& string);	*/
				string ++;
			} else {
				trace_undent ();
				return NO_MATCH;
			}
		}
		break;
	case DOT :
		if (* string != '\0' && * string != '\n') {
			advance_char ((char * *) & string);
		} else if (! has_star) {
			trace_undent ();
			return NO_MATCH;
		}
		if (has_star) {			/* Skip to eoln */
			while (* string != '\0' && * string != '\n') {
				advance_char ((char * *) & string);
			}
		}
		break;
	case NEGATE | BRACKET :
	case BRACKET :
		if (has_star) {
		    while (in_exprlist (expression, string, 
					exprlist_length, opcode) == MATCH)
		    {
			/* advance char or line end */
			if (* string == '\n') {
			/* handle multi-line matching */
				if (from_line->return_type == lineend_NONE
					|| from_line->return_type == lineend_NUL
					)
				{
					trace_undent ();
					return NO_MATCH;
				} else if (from_line->next == tail) {
					if (! (* (expression + exprlist_length - 1) & DONE)) {
						trace_undent ();
						return NO_MATCH;
					}
				} else {
					from_line = from_line->next;
					string = (character *) from_line->text;
				}
			} else {
				advance_char ((char * *) & string);
			}
		    }
		} else {
			if (in_exprlist (expression, string, 
					exprlist_length, opcode) == NO_MATCH)
			{
				trace_undent ();
				return NO_MATCH;
			}
			/* advance char or line end */
			if (* string == '\n') {
			/* handle multi-line matching */
				if (from_line->return_type == lineend_NONE
					|| from_line->return_type == lineend_NUL
					)
				{
					trace_undent ();
					return NO_MATCH;
				} else if (from_line->next == tail) {
					if (! (* (expression + exprlist_length - 1) & DONE)) {
						trace_undent ();
						return NO_MATCH;
					}
				} else {
					from_line = from_line->next;
					string = (character *) from_line->text;
				}
			} else {
				advance_char ((char * *) & string);
			}
		}
		/* Advance pattern expression to end of range list */
		expression += exprlist_length - 1;
		break;
	default :
		program->status = REG_ERROR;
		ring_bell ();
		/*
		error ("Corrupted search program");
		sleep (2);
		*/
		trace_undent ();
		return NO_MATCH;
	}
	if (has_star) {
		ret = star (program, mark, string, from_line, expression);
		trace_undent ();
		return ret;
	}
  }
  if (* expression & DONE) {
	program->end_ptr = (char *) string;	/* Match ends here */
	program->end_line = from_line;
	/*
	 * We might have found a match. The last thing to do is check
	 * whether a '$' was given at the end of the expression, or
	 * the match was found on a null string. (E.g. [a-z]* always
	 * matches) unless a ^ or $ was included in the pattern.
	   See also comments above about matching \n or \r on last line 
	   which fails here (2nd check) as replacement would not work.
	 */
	if ((* expression & ENDOFLN) && * string != '\n' && * string != '\0') {
		trace_undent ();
		return NO_MATCH;
	}
	if ((char *) string == (char *) program->start_ptr
		&& ! (program->status & BEGIN_LINE)
		&& ! (* expression & ENDOFLN))
	{
		trace_undent ();
		return NO_MATCH;
	}
	trace_undent ();
	return MATCH;
  }
  trace_undent ();
  return NO_MATCH;
}

/*
 * Star () calls check_string () to find out the longest match possible.
 * It searches backwards until the (in check_string ()) marked position
 * is reached, or a match is found.
 */
static
int
star (program, end_position, string, from_line, expression)
  REGEX * program;
  char * end_position;
  char * string;
  LINE * from_line;
  int * expression;
{
  trace_search (("star %s", string));
  trace_expression ((expression));
  trace_indent ();

  do {
	if (check_string (program, string, from_line, expression)) {
		trace_undent ();
		return MATCH;
	}
	precede_char (& string, end_position);
  } while (string != end_position);

  if (check_string (program, string, from_line, expression)) {
	trace_undent ();
	return MATCH;
  }

  trace_undent ();
  return NO_MATCH;
}

/*
 * in_exprlist () checks if the given character is in the list of [].
 * If it is it returns MATCH. If it isn't it returns NO_MATCH.
 * These returns values are reversed when the NEGATE field in the 
 * opcode is present.
 * Multi-byte characters are stored in separate successive character fields
 * of the compiled search expression list.
 */
static
int
in_exprlist (exprlist, s, exprlist_length, opcode)
  register int * exprlist;
  character * s;
  register int exprlist_length;
  int opcode;
{
  character * spoi;
  FLAG charmatch;

  if (* s == '\0' || (* s == '\n' && (opcode & NEGATE))) {
	/* End of string, never matches */
	return NO_MATCH;
  }

  if (utf8_text) {
	while (exprlist_length > 1) {	/* > 1, don't check acct_field */
		charmatch = True;
		if ((* exprlist & LOW_BYTE) != * s) {
			charmatch = False;
		}
		if ((* exprlist & 0xC0) == 0xC0) { /* multi-byte char */
			spoi = s;
			exprlist ++; exprlist_length --;
			spoi ++;
			while ((* exprlist & 0xC0) == 0x80) {
				/* UTF-8 sequence byte */
				if (charmatch) {
					if ((* exprlist & LOW_BYTE) != * spoi) {
						charmatch = False;
					}
					spoi ++;
				}
				exprlist ++; exprlist_length --;
			}
		} else {
			exprlist ++; exprlist_length --;
		}
		if (charmatch) {
			return (opcode & NEGATE) ? NO_MATCH : MATCH;
		}
	}
  } else if (cjk_text) {
	while (exprlist_length > 1) {	/* > 1, don't check acct_field */
		character cjkbuf [2];
		int cjklen;
		cjkbuf [0] = * exprlist & LOW_BYTE;
		if (multichar (cjkbuf [0])) {
			cjkbuf [1] = * (exprlist + 1) & LOW_BYTE;
			cjklen = CJK_len (cjkbuf);
		} else {
			cjklen = 1;
		}

		charmatch = True;
		spoi = s;
		while (cjklen > 0) {
			if ((* exprlist & LOW_BYTE) != * spoi) {
				charmatch = False;
			}
			exprlist ++; exprlist_length --;
			spoi ++;
			cjklen --;
		}

		if (charmatch) {
			return (opcode & NEGATE) ? NO_MATCH : MATCH;
		}
	}
  } else {
	while (exprlist_length > 1) {	/* > 1, don't check acct_field */
		if ((* exprlist & LOW_BYTE) == * s) {
			return (opcode & NEGATE) ? NO_MATCH : MATCH;
		}
		exprlist ++;
		exprlist_length --;
	}
  }

  return (opcode & NEGATE) ? MATCH : NO_MATCH;
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
