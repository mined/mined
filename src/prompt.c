/*======================================================================*\
|*		Editor mined						*|
|*		dialog prompt functions					*|
\*======================================================================*/

#include "mined.h"


#ifndef VAXC
# ifndef msdos_native_readdir
#  ifndef __USE_BSD	/* avoid warning [enabled by default] on Raspberry */
#  define __USE_BSD	/* enable file types for d_type (DT_DIR, DT_REG) */
#  endif
#  include <dirent.h>
# endif
#include <sys/stat.h>
#endif


#include "charprop.h"
#include "io.h"
#include "termprop.h"


FLAG char_on_status_line = False; /* is output active on status line ? */
FLAG input_active = False;


/*======================================================================*\
|*			Status and prompt buffer			*|
\*======================================================================*/

/* bottom status contents, kept static for redraw */
static char * bottom_inbuf = NIL_PTR;
static FLAG bottom_attrib;
static char bottom_status_buf [2 * maxPROMPTlen + maxFILENAMElen];
static int bottom_begin_fmt;
static FLAG bottom_utf8_text;

static char * mnemo = NIL_PTR;


/*======================================================================*\
|*			Output functions for status line and prompt	*|
\*======================================================================*/

/* current position on status line */
int lpos = 0;	/* extern only for keyboard mapping menu */
/* Did output on the bottom line wrap and scroll? */
static FLAG pagewrapped = False;
static int bottom_lines = 0;


/**
   Print line-end mark.
 */
static
void
print_mark ()
{
	reverse_off ();
	mark_on ();
	if (cjk_term) {
		put_cjkchar (encodedchar (0x300A));	/* 《 */
		if (cjk_lineend_width == 2) {
			lpos ++;
		}
	} else {
		put_unichar ((character) RET_marker);
	}
	lpos ++;
	mark_off ();
	reverse_on ();
}

/**
   Print a byte to status line.
   Account for column position and possible line-wrap.
 */
static
void
print_byte (c)
  character c;
{
  char_on_status_line = True;

  if (lpos > XBREAK_bottom - 1) {
	putstring ("\r\n");
	lpos = 0;
	pagewrapped = True;
	bottom_lines ++;
  }

  if (c == '\n') {
	print_mark ();
  } else if (iscontrol (c)) {
	putchar ('^');
	lpos ++;
	put_unichar (controlchar (c));
	lpos ++;
  } else {
	putcharacter (c);
	lpos ++;
  }

  char_on_status_line = False;
}

/**
   Print a mapped character to status line.
   Account for column position and possible line-wrap.
   Only called if (mapped_text && ! combining_mode && encoding_has_combining ())
 */
static
void
print_mapped (c, char_begin, line_begin)
  character c;
  char * char_begin;
  char * line_begin;
{
  int width;
  unsigned long unichar = lookup_encodedchar (c);

  char_on_status_line = True;

/*
  This would be generic; for combining_mode == False it's just width = 1; ...

  if (iscombined (unichar, char_begin, line_begin)) {
	width = 0;
  } else {
	width = 1;
  }
*/
  width = 1;

  if (lpos > XBREAK_bottom - 2 + width) {
	putstring ("\r\n");
	lpos = 0;
	pagewrapped = True;
	bottom_lines ++;
  }

  if (c == '\n') {
	print_mark ();
  } else if (iscontrol (c)) {
	putchar ('^');
	lpos ++;
	put_unichar (controlchar (c));
	lpos ++;
  } else {
	if (! combining_mode && combining_screen && iscombined (unichar, char_begin, line_begin)) {
		/* enforce separated display */
		putcharacter (' ');
		putcharacter (c);
	} else {
		putcharacter (c);
	}

	lpos ++;
  }

  char_on_status_line = False;
}

/**
   Print a CJK character to status line.
   Line position and wrapping, as well as status line flag, are handled 
   in input_cjk.
 */
static
void
print_cjk (c, width, char_begin, line_begin)
  unsigned long c;
  int width;
  char * char_begin;
  char * line_begin;
{
  unsigned long unichar = lookup_encodedchar (c);

  if (encoding_has_combining () && iscombined (unichar, char_begin, line_begin)) {
	if (! combining_mode && combining_screen) {
		/* enforce separated display */
	/*	disp_separate_combining = True;	*/
	/*	combiningdisp_on ();		*/

		if (width == 2) {
			put_cjkchar (encodedchar (0x3000));
			put_cjkchar (c);
		} else {
			if (isjoined (unichar, char_begin, line_begin)) {
				/* prevent terminal ligature by 
				   substituting joining character 
				   with its ISOLATED FORM */
				unichar = isolated_alef (unichar);
				c = encodedchar (unichar);
				if (no_char (c)) {
					put_cjkchar (encodedchar (0x2135));
				} else {
					put_cjkchar (c);
				}
			} else {
				put_cjkchar (' ');
				put_cjkchar (c);
			}
		}
	} else if (! combining_screen) {
	/*	disp_separate_combining = True;	*/
	/*	combiningdisp_on ();		*/
		put_cjkchar (c);
	} else {
		put_cjkchar (c);
	}
  } else {
	put_cjkchar (c);
  }
}

/**
   Print a Unicode character to status line.
   Handle line position and wrapping, as well as status line flag.
 */
static
void
print_unichar (unichar, width, char_begin, line_begin)
  unsigned long unichar;
  int width;
  char * char_begin;
  char * line_begin;
{
	char_on_status_line = True;

	if (width < 0) {
		width = uniscrwidth (unichar, char_begin, line_begin);
	}

	if (lpos > XBREAK_bottom - 2 + width) {
		putstring ("\r\n");
		lpos = 0;
		pagewrapped = True;
		bottom_lines ++;
	}

	lpos += width;

	if (iscombined (unichar, char_begin, line_begin)) {
		if (! combining_mode && combining_screen) {
		/* enforce separated display */
		/*	disp_separate_combining = True;	*/
		/*	combiningdisp_on ();		*/

			if (iswide (unichar)) {
				put_unichar (0x3000);
				put_unichar (unichar);
			} else {
				if (isjoined (unichar, char_begin, line_begin)) {
					/* prevent terminal ligature by 
					   substituting joining character 
					   with its ISOLATED FORM */
					put_unichar (isolated_alef (unichar));
				} else {
					put_unichar (' ');
					put_unichar (unichar);
				}
			}
		} else if (! combining_screen) {
		/*	disp_separate_combining = True;	*/
		/*	combiningdisp_on ();		*/
			put_unichar (unichar);
		} else if (apply_joining
			&& isjoined (unichar, char_begin, line_begin)) {
			/* apply ligature joining in mined */
			/* clear previously displayed LAM first */
			putchar ('');
			put_unichar (ligature_lam_alef (unichar));
		} else {
			put_unichar (unichar);
		}
	} else {
		put_unichar (unichar);
	}

	char_on_status_line = False;
}

static
int
prompt_col_count (string, middle)
  char * string;
  int middle;
{
  int count = 0;
  char * start = string;

  while (* string != '\0') {
	if ((character) (* string) < (character) ' ') {
		string ++;
		if (count < middle) {
			count += 2;
		}
	} else {
		advance_char_scr (& string, & count, start);
	}
  }
  return count;
}

/**
   Print a string to status line, limited by screen width if limit > 0.
   int limit: either 0 or XBREAK_bottom
   FLAG attributes = VALID enables formatting controls in text string:
			unicode indicator (cyan background)
			highlight (red background)
			reverse
			plain (normal)
			(unused) drop initial formatting (for filename)
	attributes = BIGGER enables right alignment if needed
 */
static
void
printlim_string (text, limit, attributes)
  char * text;
  int limit;
  FLAG attributes;
{
  int utfcount;
  char * start = text;
  int charwidth;
  unsigned long unichar;
  /* if ! transformed && cjk_text: (avoid -Wmaybe-uninitialized) */
  unsigned long cjkchar = 0;

  char_on_status_line = True;

  if (attributes == BIGGER) {
	int scols = prompt_col_count (text, bottom_begin_fmt);
	int colsok = XMAX - 2;

	if (scols > colsok) {
		int dropped = 0;

		/* print indicator */
		emph_on ();
		menuheader_on ();
		putstring ("...");
		disp_normal ();	
		diagdisp_on ();
		lpos += 3;
		/* adjust available space */
		colsok -= 3;

		while (scols - dropped > colsok && * text) {
			if ((character) (* text) < (character) ' ') {
				text ++;
				if (dropped < bottom_begin_fmt) {
					dropped += 2;
				}
			} else {
				advance_char_scr (& text, & dropped, start);
			}
		}
	}

	attributes = VALID;
  }

  while (* text != '\0') {
	FLAG transformed = False;
	if (text == mnemo) {
		diagdisp_off ();
	}

	if (* text == '-' && text >= start + bottom_begin_fmt && text > start
			&& (character) * (text - 1) <= (character) ' '
			&& (character) * (text + 1) <= (character) ' '
			&& bottom_inbuf == NIL_PTR
			&& utf8_screen) {
		transformed = True;
		utfcount = 1;
		unichar = 0x2013;	/* – */
		charwidth = uniscrwidth (unichar, text, start);
	} else if (utf8_text) {
		utf8_info (text, & utfcount, & unichar);
		charwidth = uniscrwidth (unichar, text, start);
	} else if (iscontrol ((character) * text)) {
		if (attributes == VALID && text >= start + bottom_begin_fmt && * text != '\n') {
			charwidth = 0;
		} else {
			charwidth = 2;
		}
	} else if (cjk_text) {
		cjkchar = charvalue (text);
		charwidth = cjkscrwidth (cjkchar, text, start);
	} else {
		charwidth = 1;
	}

	if (limit > 0 && lpos > limit - charwidth) {
		putmark (SHIFT_marker, UTF_SHIFT_marker);
		break;
	}
	if (lpos > XBREAK_bottom - charwidth) {
		putstring ("\r\n");
		lpos = 0;
		pagewrapped = True;
		bottom_lines ++;
	}

	if (* text == '\n') {
		print_mark ();	/* also incrementing lpos */
		text ++;
	} else if (iscontrol ((character) * text)) {
		if (attributes == BIGGER && * text == '') {
			text ++;
			attributes = VALID;
		} else if (attributes == BIGGER ||
			   (attributes == VALID && text >= start + bottom_begin_fmt)) {
			switch (* text) {
			case '':	unidisp_on ();
					break;
			case '':	emph_on ();
					menuheader_on ();
					break;
			case '':	disp_normal ();
					break;
			case '':	disp_normal ();
					diagdisp_on ();
					break;
			}
			text ++;
		} else {
			lpos += 2;
			putchar ('^');
			put_unichar (controlchar (* text ++));
		}
	} else if (transformed) {
		/* print and increment lpos */
		print_unichar (unichar, charwidth, text, start);
		text += utfcount;
	} else if (utf8_text) {
		if (unichar == 0xA0 && attributes == VALID && text >= start + bottom_begin_fmt) {
			/* skip NO-BREAK SPACE separator */
		} else {
			/* print and increment lpos */
			print_unichar (unichar, charwidth, text, start);
		}
		text += utfcount;
	} else if (cjk_text) {
		/*int prev_lpos = lpos;*/
		advance_char_scr (& text, & lpos, start);
		/*put_cjkchar (cjkchar, lpos - prev_lpos);*/
		put_cjkchar (cjkchar);
	} else {
		lpos ++;
		putcharacter (* text ++);
	}
  }
  char_on_status_line = False;
}

/**
   Print a string to status line.
 */
static
void
print_string (text)
  register char * text;
{
  printlim_string (text, 0, False);
}


/*======================================================================*\
|*			Terminal dialog					*|
\*======================================================================*/

/*
 * prompt reads in a 'y' or 'n' or other character.
 */
character
prompt (term)
  char * term;
{
  character c;
  while ((c = readcharacter_unicode ()) != '\033' && ! strchr (term, c) && ! quit) {
	ring_bell ();
	flush ();
  }
  if (c == '\033') {
	quit = True;
  }
  return c;
}

#ifdef UNUSED
/*
 * In case of a QUIT signal, swallow the dummy char generated by catchquit ()
 * called by re_search () and change ()
 */
void
swallow_dummy_quit_char ()
{
  /* Swallow away a quit character delivered by QUIT;
     not needed because this character is 
     ignored by being the CANCEL command
   */
  (void) _readchar_nokeymap ();
}
#endif


/*======================================================================*\
|*			Status line dialog and input			*|
\*======================================================================*/

/*
   Read numeric character value within prompt line.
 */
static
unsigned long
get_num_char (result, maxvalue)
  unsigned long * result;
  unsigned long maxvalue;
{
  unsigned long c;
  unsigned long unichar = 0;
  int base = 16;
  int i = 0;
  unsigned long maxfill = 0;
  FLAG uniinput = False;

  diagdisp_off ();

  while (maxfill < maxvalue && ! quit) {
	flush ();
	c = readcharacter_unicode ();

	if (i == 0 && c == '#') {
		base = 8;
	} else if (i == 0 && c == '=') {
		base = 10;
	} else if (i == 0 && (c == 'u' || c == 'U' || c == '+')) {
		uniinput = True;
		maxvalue = 0x10FFFF;
	} else if ((c == '\b' || c == '\177') && i > 0) {
		i --;
		unichar /= base;
		maxfill /= base;
		putstring ("\b \b");
	} else if (c >= '0' && c <= '9' && (base > 8 || c <= '7')) {
		i ++;
		unichar *= base;
		unichar += c - '0';
		maxfill = maxfill * base + base - 1;
		print_byte (c);
	} else if (base == 16 && c >= 'A' && c <= 'F') {
		i ++;
		unichar *= base;
		unichar += c - 'A' + 10;
		maxfill = maxfill * base + base - 1;
		print_byte (c);
	} else if (base == 16 && c >= 'a' && c <= 'f') {
		i ++;
		unichar *= base;
		unichar += c - 'a' + 10;
		maxfill = maxfill * base + base - 1;
		print_byte (c);
	} else if (c == ' ' || c == '\n' || c == '\r') {
		break;
	} else {
		quit = True;
		break;
	}
  }
  if (quit) {
	ring_bell ();
  }
  while (i > 0) {
	putstring ("\b \b");
	i --;
  }

  diagdisp_on ();
  if (uniinput && (cjk_text || mapped_text)) {
	* result = encodedchar (unichar);
  } else {
	* result = unichar;
  }
  return c;
}


/**
   Various auxiliary functions for input ().
 */
static
char *
input_byte (inbuf, ptr, c)
  char * inbuf;
  char * ptr;
  character c;
{
  if (c > '\0' && (ptr - inbuf) < maxPROMPTlen) {
	if (mapped_text && ! combining_mode && encoding_has_combining ()) {
		print_mapped (c, ptr, inbuf);
	} else {
		print_byte (c);
	}
	putstring (" \b");
	* ptr ++ = c;
	* ptr = '\0';
  } else {
	ring_bell ();
  }

  if (mapped_text && combining_mode && encoding_has_combining ()) {
	if (iscombining (lookup_encodedchar (c))) {
		/* refresh display from base char, needed on xterm */
		unsigned long uc;
		char * refreshptr = ptr;
		do {
			precede_char (& refreshptr, inbuf);
			c = charvalue (refreshptr);
			uc = lookup_encodedchar (c);
		} while (refreshptr != inbuf && iscombining (uc));
		if (! iscombining (uc)) {
			putstring ("\b");
		}
		/* Now refresh the whole combined character */
		while (refreshptr < ptr) {
			c = charvalue (refreshptr ++);
			put_unichar (lookup_encodedchar (c));
		}
	}
  }

  return ptr;
}

static
char *
input_cjk (inbuf, ptr, c)
  char * inbuf;
  char * ptr;
  unsigned long c;
{
  character cjkbytes [5];
  int len;
  character * cp;

	len = cjkencode (c, cjkbytes);
	if (len > 0 && (ptr - inbuf - 1 + len) < maxPROMPTlen) {
		int scrlen = cjkscrwidth (c, ptr, inbuf);

		char_on_status_line = True;

		if (lpos > XBREAK_bottom - scrlen) {
			putstring ("\r\n");
			lpos = 0;
			pagewrapped = True;
			bottom_lines ++;
		}

		print_cjk (c, scrlen, ptr, inbuf);

		cp = cjkbytes;
		while (* cp != '\0') {
			* ptr ++ = * cp;
			cp ++;
		}
		* ptr = '\0';

		lpos += scrlen;

		char_on_status_line = False;
		putstring (" \b");
	} else {
		ring_bell ();
	}

	if (combining_mode && encoding_has_combining ()) {
		if (iscombining (lookup_encodedchar (c))) {
			/* refresh display from base char, needed on xterm */
			unsigned long uc;
			char * refreshptr = ptr;
			do {
				precede_char (& refreshptr, inbuf);
				c = charvalue (refreshptr);
				uc = lookup_encodedchar (c);
			} while (refreshptr != inbuf && iscombining (uc));
			if (! iscombining (uc)) {
				if (cjkscrwidth (c, refreshptr, inbuf) == 2) {
					putstring ("\b\b");
				} else {
					putstring ("\b");
				}
			}
			/* Now refresh the whole combined character */
			while (refreshptr < ptr) {
				c = charvalue (refreshptr);
				put_cjkchar (c);
				advance_char (& refreshptr);
			}
		}
	}

	return ptr;
}

static
char *
add_input_buf (ptr, c)
  char * ptr;
  character c;
{
  * ptr ++ = c;
  return ptr;
}

static
char *
input_unicode (inbuf, ptr, unichar)
  char * inbuf;
  char * ptr;
  unsigned long unichar;
{
  int utfcount;
  char * inputptr = ptr;

	if (unichar < 0x80) {
	    return input_byte (inbuf, ptr, unichar);
	} else if (unichar < 0x800) {
	    if ((ptr - inbuf) + 2 > maxPROMPTlen) {
		ring_bell ();
	    } else {
		print_unichar (unichar, -1, inputptr, inbuf);
		ptr = add_input_buf (ptr, 0xC0 | (unichar >> 6));
		ptr = add_input_buf (ptr, 0x80 | (unichar & 0x3F));
		* ptr = '\0';
	    }
	} else if (unichar < 0x10000) {
	    if ((ptr - inbuf) + 3 > maxPROMPTlen) {
		ring_bell ();
	    } else {
		print_unichar (unichar, -1, inputptr, inbuf);
		ptr = add_input_buf (ptr, 0xE0 | (unichar >> 12));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 6) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | (unichar & 0x3F));
		* ptr = '\0';
	    }
	} else if (unichar < 0x200000) {
	    if ((ptr - inbuf) + 4 > maxPROMPTlen) {
		ring_bell ();
	    } else {
		print_unichar (unichar, -1, inputptr, inbuf);
		ptr = add_input_buf (ptr, 0xF0 | (unichar >> 18));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 12) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 6) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | (unichar & 0x3F));
		* ptr = '\0';
	    }
	} else if (unichar < 0x4000000) {
	    if ((ptr - inbuf) + 5 > maxPROMPTlen) {
		ring_bell ();
	    } else {
		print_unichar (unichar, -1, inputptr, inbuf);
		ptr = add_input_buf (ptr, 0xF8 | (unichar >> 24));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 18) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 12) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 6) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | (unichar & 0x3F));
		* ptr = '\0';
	    }
	} else if (unichar < 0x80000000) {
	    if ((ptr - inbuf) + 6 > maxPROMPTlen) {
		ring_bell ();
	    } else {
		print_unichar (unichar, -1, inputptr, inbuf);
		ptr = add_input_buf (ptr, 0xFC | (unichar >> 30));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 24) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 18) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 12) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | ((unichar >> 6) & 0x3F));
		ptr = add_input_buf (ptr, 0x80 | (unichar & 0x3F));
		* ptr = '\0';
	    }
	} else {
		ring_bell ();
	}

	if (combining_mode) {
		if (iscombining (unichar)) {
			/* refresh display from base char, needed on xterm */
			do {
				precede_char (& inputptr, inbuf);
				utf8_info (inputptr, & utfcount, & unichar);
			} while (inputptr != inbuf && iscombining (unichar));
			if (! iscombining (unichar)) {
				if (iswide (unichar)) {
					putstring ("\b\b");
				} else {
					putstring ("\b");
				}
			}
			/* This also assumes utf8_screen == True 
			   because otherwise combining_mode would be False
			 */
			putstring (inputptr);
		}
	}

	putstring (" \b");
	return ptr;
}

static
char *
input_character (inbuf, ptr, c)
  char * inbuf;
  char * ptr;
  unsigned long c;
{
	if (no_char (c)) {
		ring_bell ();
		return ptr;
	} else if (c < 0x80) {
		return input_byte (inbuf, ptr, c);
	} else if (utf8_text) {
		return input_unicode (inbuf, ptr, c);
	} else if (cjk_text) {
		return input_cjk (inbuf, ptr, c);
	} else if (c < 0x100) {
		return input_byte (inbuf, ptr, c);
	} else {
		ring_bell ();
		return ptr;
	}
}

static
char *
input_hexdig (inbuf, ptr, c)
  char * inbuf;
  char * ptr;
  unsigned long c;
{
	return input_character (inbuf, ptr, hexdig (c));
}

static
char *
input_hex (inbuf, ptr, c)
  char * inbuf;
  char * ptr;
  unsigned long c;
{
	ptr = input_hexdig (inbuf, ptr, (c >> 4) & 0xF);
	return input_hexdig (inbuf, ptr, c & 0xF);
}

void
redraw_prompt ()
{
	set_cursor (0, - MENU);
	while (bottom_lines > 0) {
		scroll_reverse ();
		bottom_lines --;
	}
	if (MENU && ! can_add_line) {
		displaymenuline (True);
	}
	rd_bottom_line ();
	if (can_clear_eol) {
		clear_eol ();
	}
}

static
char *
erase1 (inbuf, ptr, all_combined)
  char * inbuf;
  char * ptr;
  FLAG all_combined;
{
  FLAG need_refresh = False;

	diagdisp_off ();
	precede_char (& ptr, inbuf);
	if (iscontrol ((character) * ptr)) {
	    if (* ptr == '\n' && (cjk_lineend_width == 1 || ! cjk_term)) {
		/* single-width newline indication */
		putstring (" \b\b \b");
		lpos = lpos - 1;
	    } else {
		putstring (" \b\b\b  \b\b");
		lpos = lpos - 2;
	    }
	} else {
	    unsigned long charval = charvalue (ptr);
	    int charwidth;

	    if (combining_mode && encoding_has_combining ()) {
		if (all_combined) {
			while (ptr != inbuf && iscombined (unicode (charval), ptr, inbuf)) {
				precede_char (& ptr, inbuf);
				charval = charvalue (ptr);
			}
		} else if (iscombined (unicode (charval), ptr, inbuf)) {
			/* workaround for refresh of base character 
			   without or with fewer combining accents */
			need_refresh = True;
		}
	    }

	    if (utf8_text) {
		charwidth = uniscrwidth (charval, ptr, inbuf);
	    } else if (cjk_text) {
		charwidth = cjkscrwidth (charval, ptr, inbuf);
	    } else if (mapped_text && combining_mode && encoding_has_combining ()
			&& iscombined (lookup_encodedchar (charval), ptr, inbuf)) {
		charwidth = 0;
	    } else {
		charwidth = 1;
	    }

	    if (charwidth == 2) {
		putstring (" \b\b\b  \b\b");
		lpos = lpos - 2;
	    } else if (charwidth == 1) {
		putstring (" \b\b \b");
		lpos = lpos - 1;
	    }
	}
	diagdisp_on ();
	putstring (" \b");
	* ptr = '\0';

	if (need_refresh) {
		redraw_prompt ();
	}

	return ptr;
}

static
char *
input_prefix (inbuf, ptr, ps)
  char * inbuf;
  char * ptr;
  struct prefixspec * ps;
{
  unsigned long c = readcharacter_unicode_mapped ();

  if (command (c) == DPC) {
	if (mnemo == NIL_PTR ? ptr > inbuf : (char *) ptr > mnemo) {
		return erase1 (inbuf, ptr, False);
	} else {
		ring_bell ();
		return ptr;
	}
  } else if ((command (c) == CTRLINS && (c = readcharacter_unicode_mapped ()) == FUNcmd && (keyshift |= ctrl_mask))
	     || c == FUNcmd) {
	struct prefixspec * ps2 = lookup_prefix (keyproc, keyshift);
	if (ps2) {
		c = readcharacter_unicode_mapped ();
		if ((command (c) == CTRLINS && (c = readcharacter_unicode_mapped ()) == FUNcmd && (keyshift |= ctrl_mask))
		    || c == FUNcmd) {
			struct prefixspec * ps3 = lookup_prefix (keyproc, keyshift);
			if (ps3) {
				c = readcharacter_unicode_mapped ();
				c = compose_patterns (c, ps, ps2, ps3);
				return input_character (inbuf, ptr, c);
			} else {
				ring_bell ();
				return ptr;
			}
		} else {
			c = compose_patterns (c, ps, ps2, 0);
			return input_character (inbuf, ptr, c);
		}
	} else {
		ring_bell ();
		return ptr;
	}
  } else {
	c = compose_patterns (c, ps, 0, 0);
	return input_character (inbuf, ptr, c);
  }
}

static
voidfunc
command1 (c)
  unsigned long c;
{
  voidfunc c0 = command (c);
  if (c0 == F12 && (keyshift & altshift_mask) == alt_mask) {
	return toggleKEYMAP;
  } else if (c0 == F12 && (keyshift & ctrlshift_mask) == ctrl_mask) {
	return setupKEYMAP;
  } else {
	return c0;
  }
}

/**
   Read prompt input.
 * Input () reads a string and echoes it on the prompt line.
 * Return values:
 *	when QUIT character typed => ERRORS
 *	when empty input and clearfl == True: NO_INPUT
 *	otherwise: FINE
 */
# ifdef __GNUC__
# pragma GCC diagnostic push
# ifndef __clang__
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
# endif
# endif
static
int
input (inbuf, clearfl, term_input)
  char * inbuf;
  FLAG clearfl;
  char * term_input;
{
  char * ptr;
  unsigned long rch = 0;	/* (avoid -Wmaybe-uninitialized) */
  unsigned long unich;
  unsigned long mnemoc;
  char * term_chars;
  int ret = ERRORS;

  /* save text encoding */
  FLAG save_utf8_text = utf8_text;
  FLAG save_cjk_text = cjk_text;
  FLAG save_mapped_text = mapped_text;
  if (ebcdic_text) {
	/* temporarily set text encoding to UTF-8 */
	utf8_text = False;
	cjk_text = False;
	mapped_text = True;
  }

  ptr = inbuf;
  * ptr = '\0';
  bottom_lines = 0;

  while (! quit) {
     if (lpos >= XBREAK_bottom) {
	pagewrapped = True;
     }
     if (lpos < 0) {
	redraw_prompt ();
     }

     while (True) {
	if (mnemo != NIL_PTR) {
		diagdisp_off ();
		putstring (" \b");
		flush ();
		rch = readcharacter ();
	} else {
		char * prev_bottom_inbuf = bottom_inbuf;
		FLAG prev_bottom_attrib = bottom_attrib;
		int prev_bottom_begin_fmt = bottom_begin_fmt;
		FLAG prev_bottom_utf8_text = bottom_utf8_text;
		char prev_bottom_status_buf [sizeof (bottom_status_buf)];
		strncpy (prev_bottom_status_buf, bottom_status_buf, sizeof (bottom_status_buf));

		flush ();
		rch = readcharacter_mapped ();

		if (prev_bottom_inbuf != bottom_inbuf) {
			/* after nested status line (overlayed by pick list),
			   restore bottom inbuf information,
			   and redraw prompt line
			 */
			bottom_inbuf = prev_bottom_inbuf;
			bottom_attrib = prev_bottom_attrib;
			bottom_begin_fmt = prev_bottom_begin_fmt;
			bottom_utf8_text = prev_bottom_utf8_text;
			strncpy (bottom_status_buf, prev_bottom_status_buf, sizeof (bottom_status_buf));

			redraw_prompt ();
		}
	}
	if (no_char (rch)) {
		if (rch == CHAR_INVALID) {
			ring_bell ();
		} else {	/* CHAR_UNKNOWN, aborted pick list */
			/* redraw_prompt () already called */
		}
	} else {
		if (rch == FUNcmd) {
			unich = FUNcmd;
		} else {	/* EBCDIC kludge */
			unich = lookup_encodedchar (rch);
		}
		break;
	}
     }

     /* special mapping */
     if (unich == ' ' && (keyshift & ctrlshift_mask) == ctrlshift_mask) {
	unich = 0xA0;	/* NO-BREAK SPACE */
	rch = encodedchar (unich);
     }

     if (command (unich) == SNL || command (unich) == Popmark) {
	unich = '\r';
	rch = encodedchar (unich);
     }

     for (term_chars = term_input; * term_chars != '\0'; term_chars ++) {
	if (unich == * term_chars) {
		unich = '\n';
		rch = encodedchar (unich);
		break;
	}
     }

     if (mnemo != NIL_PTR &&
         (unich == ' ' || unich == '\n' || unich == '\r' || unich == quit_char || unich == '\033')) {
	mnemoc = compose_mnemonic (mnemo);

	diagdisp_on ();

	/* restore prompt display, remove mnemonic */
	while ((char *) ptr > mnemo) {
		ptr = erase1 (inbuf, ptr, True);
	}

	mnemo = NIL_PTR;

	if (unich != quit_char && unich != '\033') {
		ptr = input_character (inbuf, ptr, mnemoc);
	}
     } else if (rch == quit_char) {
	ring_bell ();
	quit = True;
     } else if (allow_keymap && command1 (rch) == toggleKEYMAP) {
	reverse_off ();
	toggleKEYMAP ();
	if (flags_changed) {
		displayflags ();
	}
	redraw_prompt ();
     } else if (allow_keymap && command1 (rch) == setupKEYMAP) {
	reverse_off ();
	setupKEYMAP ();
	if (flags_changed) {
		displayflags ();
	}
	redraw_prompt ();
     } else if (command (unich) == DPC) {
	if (mnemo == NIL_PTR ? ptr > inbuf : (char *) ptr > mnemo) {
		ptr = erase1 (inbuf, ptr, ! (keyshift & ctrl_mask));
	} else {
		ring_bell ();
	}
     } else if (command (unich) == AltX) {
	char * cp = ptr;
	char * pp = cp;
	char * pp1;
	int n = 0;
	unsigned long altc = 0;
	precede_char (& cp, inbuf);
	pp1 = cp;
	while (n < 6 && cp != pp && ishex (* cp)) {
		n ++;
		pp = cp;
		precede_char (& cp, inbuf);
	}
	if (pp != ptr && n >= 2) {
		/* hex value found */
		char * hp = pp;
		while (hp != ptr) {
			altc = ((altc << 4) + hexval (* hp));
			hp ++;
		}
		if (altc > (unsigned long) 0x10FFFF) {
			n = 1;
		}
	}
	if (n >= 2) {
		/* hex -> uni */
		while (n > 0) {
			ptr = erase1 (inbuf, ptr, False);
			n --;
		}
		ptr = input_character (inbuf, ptr, altc);
	} else if (mnemo == NIL_PTR ? ptr > inbuf : (char *) ptr > mnemo) {
		/* uni -> hex */
		unsigned long cv = unicodevalue (pp1);
		if (! no_char (cv)) {
			ptr = erase1 (inbuf, ptr, False);

			if (cv > 0xFFFF) {
				if (cv > 0x10FFFF) {
					ptr = input_hex (inbuf, ptr, cv >> 24);
				}
				ptr = input_hex (inbuf, ptr, cv >> 16);
			}
			ptr = input_hex (inbuf, ptr, cv >> 8);
			ptr = input_hex (inbuf, ptr, cv);
		}
	}
     } else if (command (unich) == CTRLINS) {
	unich = readcharacter_unicode ();
	rch = encodedchar (unich);
	if (command (unich) == COMPOSE && keyshift >= '@' && keyshift <= '_') {
		/* allow input of control char, esp ^^ */
		ptr = input_character (inbuf, ptr, keyshift & 0x1F);
	} else if (unich == FUNcmd) {
		struct prefixspec * ps;
		keyshift |= ctrl_mask;
		ps = lookup_prefix (keyproc, keyshift);
		if (ps) {
			ptr = input_prefix (inbuf, ptr, ps);
		} else {
			ring_bell ();
		}
	} else if (unich == '#') {
	    unsigned long cx;
	    do {
		cx = get_num_char (& rch, max_char_value ());
		if (quit) {
			quit = False;
		} else {
			ptr = input_character (inbuf, ptr, rch);
		}
	    } while (cx == ' ');
	} else if (unich == ' ') {
		/* mark mnemonic start position 
		   in input buffer */
		mnemo = (char *) ptr;
		diagdisp_off ();
	} else if (unich > ' ' && unich != 0x7F) {
		rch = compose_chars (unich, readcharacter_unicode ());
		if (no_char (rch)) {
			ring_bell ();
		} else {
			ptr = input_character (inbuf, ptr, rch);
		}
	} else {
		ptr = input_character (inbuf, ptr, rch);
	}
     } else if (unich == '\033'
		|| (command (unich) == MOUSEfunction
			/* workaround for mintty shift-paste */
			&& mouse_button != releasebutton
			/* consider fix_mouse_release_event () */
			&& mouse_button != movebutton)) {
	quit = True;
     } else if (unich == '\n' || unich == '\r') {
	/* if inbuf is empty clear status line */
	if (ptr == inbuf && clearfl) {
		ret = NO_INPUT;
	} else {
		ret = FINE;
	}
	break;
     } else if (unich == FUNcmd) {
	struct prefixspec * ps = lookup_prefix (keyproc, keyshift);
	if (ps) {
		ptr = input_prefix (inbuf, ptr, ps);
	} else {
		ring_bell ();
	}
     } else {
	ptr = input_character (inbuf, ptr, rch);
     }
  }

  if (ret == ERRORS) {
	quit = False;
  }

  if (ebcdic_text) {
	/* pop text encoding */
	utf8_text = save_utf8_text;
	cjk_text = save_cjk_text;
	mapped_text = save_mapped_text;
  }

  return ret;
}
# ifdef __GNUC__
# pragma GCC diagnostic pop
# endif

/*
 * Show concatenation of s1 and s2 on the status line (bottom of screen)
   Set stat_visible only if bottom_line is visible.
   The return value is FINE except for get_string, where it is taken 
   from the call to input ().
status_line (str1, str2) is (void) bottom_line (True, (str1), (str2), NIL_PTR, False, "")
status_msg (str)	 is status_line (str, NIL_PTR)
status_beg (str)	 is (void) bottom_line (True, (str), NIL_PTR, NIL_PTR, True, "")
status_fmt2(str1, str2)	 is (void) bottom_line (VALID, str1, str2, NIL_PTR, False, "")
error (str1)		 is (void) bottom_line (True, (str1), NIL_PTR, NIL_PTR, False, "")
error2 (str1, str2)	 is (void) bottom_line (True, (str1), (str2), NIL_PTR, False, "")
clear_status ()		 is (void) bottom_line (False, NIL_PTR, NIL_PTR, NIL_PTR, False, "")
get_string (str1, str2, fl, term_chars)
			 is bottom_line (True, (str1), NIL_PTR, (str2), fl, term_chars)
 */
static
void
status2_uni (msg1, msg2)
  char * msg1;
  char * msg2;
{
  /* push text encoding, temporarily set to UTF-8 */
  FLAG save_utf8_text = utf8_text;
  FLAG save_cjk_text = cjk_text;
  FLAG save_mapped_text = mapped_text;
  utf8_text = True;
  cjk_text = False;
  mapped_text = False;
  /* don't need to save and set utf16_file = False; */

  bottom_line (VALID, msg1, msg2, NIL_PTR, False, "");

  /* pop text encoding */
  utf8_text = save_utf8_text;
  cjk_text = save_cjk_text;
  mapped_text = save_mapped_text;
}

void
status_uni (msg)
  char * msg;
{
  status2_uni (msg, NIL_PTR);
}

character
status2_prompt (term, msg1, msg2)
  char * term;
  char * msg1;
  char * msg2;
{
  (void) bottom_line (BIGGER, msg1, msg2, NIL_PTR, False, term);
  return prompt (term);
}

/**
   get_string_uni prompts a string like get_string_nokeymap
   but uses Unicode input
   BEWARE: conflicts with RDwin!
 */
int
get_string_uni (prompt, inbuf, statfl, term_input)
  char * prompt;
  char * inbuf;
  FLAG statfl;
  char * term_input;
{
  int ret;

  /* push text encoding, temporarily set to UTF-8 */
  FLAG save_utf8_text = utf8_text;
  FLAG save_cjk_text = cjk_text;
  FLAG save_mapped_text = mapped_text;
  utf8_text = True;
  cjk_text = False;
  mapped_text = False;

  ret = get_string_nokeymap (prompt, inbuf, statfl, term_input);

  /* pop text encoding */
  utf8_text = save_utf8_text;
  cjk_text = save_cjk_text;
  mapped_text = save_mapped_text;

  return ret;
}

void
rd_bottom_line ()
{
  set_cursor (0, YMAX);
  diagdisp_on ();
  if (bottom_inbuf == NIL_PTR) {
	lpos = 0;
	if (bottom_utf8_text && ! utf8_text) {
		utf8_text = True;
		printlim_string (bottom_status_buf, XBREAK_bottom, bottom_attrib);
		utf8_text = False;
	} else {
		printlim_string (bottom_status_buf, XBREAK_bottom, bottom_attrib);
	}
  } else {
	lpos = 0;
	print_string (bottom_status_buf);
	print_string (bottom_inbuf);
  }
  if (! input_active) {
	diagdisp_off ();
	set_cursor_xy ();	/* Set cursor back to old position */
  }
  flush ();	/* Perform the actual screen output */
}

/**
   Clear or display status line, read prompt if desired.
   s1, s2: strings to display
   inbuf: buffer for prompt to read
   attrib:
	True: turn on reverse video on both strings
	False: don't (clear_status)
	SAME: interpret control chars in both string for attributes
	VALID: interpret control chars in string s2 for attributes
			(at beginning) suppress embedding spaces
		plus printlim_string controls:
			unicode indicator (cyan background)
			highlight (red background)
			reverse
			plain (normal)
	BIGGER: like VALID, and align right (cutting left if needed)
 */
int
bottom_line (attrib, s1, s2, inbuf, statfl, term_input)
  FLAG attrib;
  char * s1;
  char * s2;
  char * inbuf;
  FLAG statfl;
  char * term_input;
{
  int ret = FINE;

  /* save text encoding for EBCDIC kludge */
  FLAG save_utf8_text = utf8_text;
  FLAG save_cjk_text = cjk_text;
  FLAG save_mapped_text = mapped_text;

#ifdef redraw_info_stack_workaround_doesnt_work
  char * prev_bottom_inbuf = bottom_inbuf;
  FLAG prev_bottom_attrib = bottom_attrib;
  int prev_bottom_begin_fmt = bottom_begin_fmt;
  FLAG prev_bottom_utf8_text = bottom_utf8_text;
  char prev_bottom_status_buf [sizeof (bottom_status_buf)];
  strncpy (prev_bottom_status_buf, bottom_status_buf, sizeof (bottom_status_buf));
#endif

  if (debug_mined && (s1 || s2)) {
	debuglog ("prompt", s1, s2);
  }

  if (inbuf != NIL_PTR) {
	* inbuf = '\0';
  }

  if (pagewrapped) {
	bottom_status_buf [0] = '\0';
	RD ();
	display_flush ();
	pagewrapped = False;
  }

  /* set static status buffer (for redrawing):
     - input buffer
     - markup attributre
     - UTF-8 flag
     - buffer contents
     - begin of formatting marker
   */
  bottom_inbuf = inbuf;
  bottom_attrib = attrib;
  bottom_utf8_text = utf8_text;
  bottom_begin_fmt = 0;

  if (ebcdic_text) {
	/* temporarily set text encoding to UTF-8 */
	utf8_text = True;
	cjk_text = False;
	mapped_text = False;
  }

  if ((attrib == VALID || attrib == SAME) && s1 && * s1 == '') {
	build_string (bottom_status_buf, "%s%s", unnull (s1), unnull (s2));
	if (s2 != NIL_PTR) {
		bottom_begin_fmt = strlen (unnull (s1));
	}
  } else {
	build_string (bottom_status_buf, " %s%s ", unnull (s1), unnull (s2));
	if (s2 != NIL_PTR) {
		bottom_begin_fmt = strlen (unnull (s1)) + 1;
	}
  }
  if (attrib == SAME) {
	attrib = VALID;
	bottom_begin_fmt = 0;
  }

  if (attrib != False && stat_visible) {
	set_cursor (0, YMAX);
	clear_lastline ();
  }

  set_cursor (0, YMAX);
  if (attrib != False) {	/* Print rev. start sequence */
	diagdisp_on ();
	stat_visible = True;
  } else {			/* Used as clear_status () */
	diagdisp_off ();
	stat_visible = False;
  }
  if (inbuf == NIL_PTR) {
	lpos = 0;
	printlim_string (bottom_status_buf, XBREAK_bottom, attrib);
  } else {
	lpos = 0;
	print_string (bottom_status_buf);

	if (ebcdic_text) {
		/* pop text encoding */
		utf8_text = save_utf8_text;
		cjk_text = save_cjk_text;
		mapped_text = save_mapped_text;
	}
	input_active = True;
	ret = input (inbuf, statfl, term_input);
	input_active = False;
	if (ebcdic_text) {
		/* restore to temporary UTF-8 */
		utf8_text = True;
		cjk_text = False;
		mapped_text = False;
	}
  }

  /* Print normal video */
  diagdisp_off ();
  /* set_cursor (lpos, YMAX); not needed; wouldn't work proportional */
  if (can_clear_eol) {
	clear_eol ();
  } else {
	put_blanks (XMAX - 1 - lpos);
	if (proportional) {
		set_cursor (0, YMAX);
		diagdisp_on ();
		if (bottom_inbuf == NIL_PTR) {
			lpos = 0;
			printlim_string (bottom_status_buf, XBREAK_bottom, attrib);
		} else {
			lpos = 0;
			print_string (bottom_status_buf);
			print_string (bottom_inbuf);
		}
		/* pagewrapped ? diagdisp_off ()? */
	} else {
		set_cursor (lpos, YMAX);
	}
  }

  if (inbuf != NIL_PTR) {
	set_cursor (0, YMAX);
  } else if (statfl) {
	diagdisp_on ();
  } else if (term_input && * term_input) {
	/* leave cursor for prompt () */
  } else {
	set_cursor_xy ();	/* Set cursor back to old position */
  }

  flush ();	/* Perform the actual screen output */
  if (ret != FINE) {
	clear_status ();
  }

#ifdef redraw_info_stack_workaround_doesnt_work
  if (prev_bottom_inbuf != NIL_PTR) {
	/* restore bottom line info for redrawing after nested status lines,
	   or rather status line overlayed by pick list
	 */
	bottom_inbuf = prev_bottom_inbuf;
	bottom_attrib = prev_bottom_attrib;
	bottom_begin_fmt = prev_bottom_begin_fmt;
	bottom_utf8_text = prev_bottom_utf8_text;
	strncpy (bottom_status_buf, prev_bottom_status_buf, sizeof (bottom_status_buf));
  }
#endif

  if (ebcdic_text) {
	/* pop text encoding */
	utf8_text = save_utf8_text;
	cjk_text = save_cjk_text;
	mapped_text = save_mapped_text;
  }

  return ret;
}


/*======================================================================*\
|*			Number input					*|
\*======================================================================*/

/*
 * get_number () reads a number from the terminal.
 * prompt_num_char () prompts for a numeric character from the terminal.
 * If the parameter firstdigit is a valid digit, empty input (return) 
 * will yield a valid number; if it is '\0', it won't.
 * The last character typed in is returned.
 * ERRORS is returned on a bad number or on interrupted input.
 * The resulting number is put into the integer the argument points to.
 */
long
get_number (message, firstdigit, result)
  char * message;
  char firstdigit;
  int * result;
{
  long ch;
  int value;
  int i = 0;

  status_beg (message);

  if (firstdigit > '\0') {
	ch = firstdigit;
  } else {
	ch = readcharacter_unicode ();
  }

  if (ch == quit_char || ch == '\033') {
	quit = True;
  }
  if (! quit && (ch < '0' || ch > '9')) {
	error ("Bad number");
	return ERRORS;
  }

/* Convert input to a decimal number */
  value = 0;
  while (! quit && ((ch >= '0' && ch <= '9')
			   || ch == '\b' || ch == '\177')) {
	if (ch == '\b' || ch == '\177') {
		if (i > 0) {
			i --;
			value /= 10;
			putstring ("\b \b"); flush ();
			if (lpos >= XBREAK_bottom) {
				pagewrapped = True;
			}
			lpos --;
		}
	} else {
		i ++;
		print_byte (ch); flush ();
		if (lpos >= XBREAK_bottom) {
			pagewrapped = True;
		}
		value *= 10;
		value += ch - '0';
	}
	ch = readcharacter_unicode ();
	if (ch == quit_char || ch == '\033') {
		quit = True;
	}
  }

  clear_status ();
  if (quit) {
	clear_status ();
	return ERRORS;
  }
  * result = value;
  return ch;
}

/*
   Prompt for hex or octal character value.
 */
int
prompt_num_char (result, maxvalue)
  unsigned long * result;
  unsigned long maxvalue;
{
  unsigned long ch = 0;	/* (avoid -Wmaybe-uninitialized) */
  int value = 0;
  int i;
  FLAG ok = False;
  FLAG uniinput = False;
  int base = 16;
  unsigned long maxfill = 0;

  if (cjk_text || mapped_text) {
	status_beg ("Enter character code value (hex / # oct / = dec / u unicode) ...");
  } else {
	status_beg ("Enter character code value (hex / # oct / = dec) ...");
  }

  i = 0;
  while (maxfill < maxvalue && ! ok) {
	ch = readcharacter_unicode ();
	if (i == 0) {
		if (ch == '#') {
			base = 8;
			print_string ("(oct) "); flush ();
			continue;
		} else if (ch == '=') {
			base = 10;
			print_string ("(dec) "); flush ();
			continue;
		} else if (ch == 'u' || ch == 'U' || ch == '+') {
			uniinput = True;
			maxvalue = 0x10FFFF;
			print_string ("(uni) "); flush ();
			continue;
		}
	}

	if (ch >= '0' && ch <= '9' && (ch <= '7' || base > 8)) {
		i ++;
		value *= base;
		value += ch - '0';
		maxfill = maxfill * base + base - 1;
		print_byte (ch); flush ();
	} else if (base == 16 && ch >= 'A' && ch <= 'F') {
		i ++;
		value *= base;
		value += ch - 'A' + 10;
		maxfill = maxfill * base + base - 1;
		print_byte (ch); flush ();
	} else if (base == 16 && ch >= 'a' && ch <= 'f') {
		i ++;
		value *= base;
		value += ch - 'a' + 10;
		maxfill = maxfill * base + base - 1;
		ch = ch - 'a' + 'A';
		print_byte (ch); flush ();
	} else if (ch == '\b' || ch == '\177') {
		if (i > 0) {
			i --;
			value /= base;
			maxfill /= base;
			putstring ("\b \b"); flush ();
			if (lpos >= XBREAK_bottom) {
				pagewrapped = True;
			}
			lpos --;
		}
	} else if (ch == '\033' || ch == quit_char) {
		clear_status ();
		return ERRORS;
	} else if (ch == ' ' || ch == '\n' || ch == '\r') {
		if (i == 0) {
			clear_status ();
			return ERRORS;
		}
		ok = True;
	} else {
		quit = True;
		ok = True;
	}

	if (lpos >= XBREAK_bottom) {
		pagewrapped = True;
	}
  }

  clear_status ();
  if (quit) {
	if (base == 16) {
		error ("Bad hex value");
	} else if (base == 8) {
		error ("Bad octal value");
	} else {
		error ("Bad decimal value");
	}
	quit = False;
	return ERRORS;
  }

  if (uniinput && (cjk_text || mapped_text)) {
	* result = encodedchar (value);
  } else if (ebcdic_file && ! uniinput) {
	unsigned long res;
	mapped_text = True;
	res = lookup_encodedchar (value);
	mapped_text = False;
	* result = res;
  } else {
	* result = value;
  }
  return ch;
}


/*======================================================================*\
|*			Native readdir					*|
\*======================================================================*/

#ifdef msdos_native_readdir
/* doesn't speed up */

#include <dir.h>

#undef DIR
#define DIR struct ffblk
static DIR ffblk;
static int done;
#define DT_DIR 4
#define DT_REG 8
struct dirent {
	char d_name [maxFILENAMElen];
	char d_type;
};
static struct dirent dirent;

static
DIR *
opendir (char * dirname)
{
	char pattern [maxFILENAMElen];
	strcpy (pattern, dirname);
	strcat (pattern, "\\*.*");
	done = findfirst (pattern, & ffblk, 0xF7);
	return & ffblk;
}

static
struct dirent *
readdir (DIR * dir)
{
	if (done) {
		return 0;
	} else {
		strcpy (dirent.d_name, ffblk.ff_name);
		if (ffblk.ff_attrib & 0x10) {
			dirent.d_type = DT_DIR;
		} else {
			dirent.d_type = DT_REG;
		}
		done = findnext (& ffblk);
		return & dirent;
	}
}

static
int
closedir (DIR * dir)
{
	return 0;
}

#endif


/*======================================================================*\
|*			File chooser					*|
\*======================================================================*/

FLAG sort_by_extension = False;
FLAG sort_dirs_first = False;

#ifdef VAXC

/*
 * get_filename () prompts for a file name (on the prompt line)
 */
int
get_filename (message, file, directory_chooser)
  char * message;
  char * file;
  FLAG directory_chooser;
{
  char file0 [maxPROMPTlen];
  int ret = get_string (message, file0, True, "");
  char * filep = file0;

  strcpy (file, "");

  if (file0 [0] == '~' && (file0 [1] == '/' || file0 [1] == '\0')) {
	strappend (file, gethomedir (), maxFILENAMElen);
	filep ++;
	strip_trailingslash (file);
  }

  strappend (file, filep, maxFILENAMElen);
  return ret;
}

#else

#define dont_debug_file_chooser
#define dont_debug_wildcard


struct treenode {
	struct treenode * left;
	struct treenode * right;
	char * s1;
	char * s2;
};

static struct treenode * dir_root = 0;

static
char *
getsuffix (fn)
  char * fn;
{
  while (* fn == '.') {
	fn ++;
  }
  return unnull (strrchr (fn, '.'));
}

static
int
fncompare (s1, s2)
  char * s1;
  char * s2;
{
  if (sort_by_extension) {
	char * suf1 = getsuffix (s1);
	char * suf2 = getsuffix (s2);
	int cmp = strcmp (suf1, suf2);
	if (cmp != 0) {
		return cmp;
	} else {
		return strcmp (s1, s2);
	}
  } else {
	return strcmp (s1, s2);
  }
}

/**
   Insert file name and info into binary tree.
   Return:
   	True if a new entry was inserted
   	VALID if a duplicate entry was ignored
   	False if out of memory
 */
static
FLAG
tree_insert (nodepoi, s1, s2)
  struct treenode * * nodepoi;
  char * s1;
  char * s2;
{
  if (* nodepoi) {
	int cmp = fncompare (s1, (* nodepoi)->s1);
	if (cmp < 0) {
		return tree_insert (& (* nodepoi)->left, s1, s2);
	} else if (cmp > 0) {
		return tree_insert (& (* nodepoi)->right, s1, s2);
	} else {
		/* duplicate entry */
		return VALID;
	}
  } else {
	* nodepoi = alloc (sizeof (struct treenode));
	if (* nodepoi) {
		(* nodepoi)->left = 0;
		(* nodepoi)->right = 0;
		(* nodepoi)->s1 = s1;
		(* nodepoi)->s2 = s2;
		return True;
	} else {
		return False;
	}
  }
}

static menuitemtype * filemenu;
static int itemcount;
static int filecount;

static
void
tree_traverse (node, dirs)
  struct treenode * node;
  FLAG dirs;
{
  if (node) {
	tree_traverse (node->left, dirs);
	if (dirs == NOT_VALID ||
	    (dirs == False) == ! node->s2) {
		if (itemcount < filecount) {
			fill_menuitem (& filemenu [itemcount], node->s1, node->s2);
		} else if (itemcount == filecount) {
			ring_bell ();
		}
		itemcount ++;
	}
	tree_traverse (node->right, dirs);
  }
}

static
void
tree_free (node)
  struct treenode * node;
{
  if (node) {
	tree_free (node->left);
	tree_free (node->right);
	free_space (node->s1);
	free_space (node);
  }
}


static
int
isin (utf, set)
  char * utf;
  char * set;
{
  unsigned long uni = utf8value (utf);

  while (1) {
	unsigned long elem = utf8value (set);
	if (uni == elem) {
		return 1;
	}
	set = nextutf8 (set);
	if (* set == '-') {
		unsigned long elem2;
		set ++;
		elem2 = utf8value (set);
		if (uni > elem && uni <= elem2) {
			return 1;
		}
		if (elem2 && elem2 != ']') {
			set = nextutf8 (set);
		}
	}
	if (* set == ']') {
		return 0;
	}
  }
}

int
wildcard (pat, s)
  char * pat;
  char * s;
{
#ifdef debug_wildcard
  indent (level);
  printf ("wildcard (%s, %s)\n", pat, s);
#endif

  if (! * pat) {
	return
#ifdef vms
		* s == ';' ||
#endif
		! * s;
  } else if (* pat == '*') {
	return (* s && wildcard (pat, nextutf8 (s)))
		|| wildcard (pat + 1, s);
  } else if (* pat == '?') {
	return * s && wildcard (pat + 1, nextutf8 (s));
#ifdef vms
  } else if (* pat == '%') {
	return * s && wildcard (pat + 1, nextutf8 (s));
#else
  } else if (* pat == '[') {
	char * endset = strchr (pat, ']');
	if (endset) {
		pat ++;
		if (* pat == '^' || * pat == '!') {
			pat ++;
			return ! isin (s, pat)
				&& wildcard (endset + 1, nextutf8 (s));
		} else {
			return isin (s, pat)
				&& wildcard (endset + 1, nextutf8 (s));
		}
	} else {
		/* invalid pattern */
		return 0;
	}
#endif
  } else {
#ifdef case_sensitive
	return * pat == * s	/* transparently compare UTF-8 bytes */
		&& wildcard (pat + 1, s + 1);
#else
	unsigned long c1 = case_convert (utf8value (pat), -1);
	unsigned long c2 = case_convert (utf8value (s), -1);
	return c1 == c2
		&& wildcard (nextutf8 (pat), nextutf8 (s));
#endif
  }
}

static
char *
pathlast (path)
  char * path;
{
  char * bn = getbasename (path);
  if (bn == path) {
	return NIL_PTR;
  } else {
	return bn - 1;
  }
}

static
void
add_pathsep (dirname, maxlen)
  char * dirname;
  int maxlen;
{
  if (dirname [0] != '\0') {
	char last = dirname [strlen (dirname) - 1];
	if (last != '/'
#ifdef pc
		&& last != '\\'
#endif
#ifdef vms
		&& last != ']'
#endif
#ifndef unix
		&& last != ':'
#endif
		) {
		strappend (dirname, "/", maxlen);
	}
  }
}


/**
   display hint "checking directory" after waiting a while
 */
#if ! defined (__TURBOC__) && ! defined (VAXC)
#define use_hint_timer
#endif

#ifdef use_hint_timer
#include <sys/time.h>

static struct timeval now_start;
#endif

void
check_slow_hint ()
{
#ifdef use_hint_timer
  struct timeval now;
  gettimeofday (& now, 0);
  if (now.tv_sec > 0 &&
	(now.tv_sec - now_start.tv_sec) * 1000
	+ (now.tv_usec - now_start.tv_usec) / 1000 > 700)
  {
		status_msg ("...checking directory...");
		gettimeofday (& now_start, 0);
  }
#endif
}


/* workaround for buggy include files ... */
#if defined (linux) && defined (i386)
/* d_type is missing for Linux kernel 2.2 */
# undef _DIRENT_HAVE_D_TYPE
#endif
#if ! defined (DT_DIR) || ! defined (DT_REG)
# undef _DIRENT_HAVE_D_TYPE
#endif

/* unsuccessful attempts to speed up readdir/stat on slow network drives: */
#define dont_use_fstatat
#define dont_use_rel_stat
/* new attempt to speed up readdir/stat on slow network drives: */
#define use_lazy_stat

#define dont_debug_lazy_stat

#ifdef debug_lazy_stat
# undef _DIRENT_HAVE_D_TYPE
#endif

#ifdef use_lazy_stat
static char direcname [maxFILENAMElen];

static
int
dummyflagoff (item, i)
  menuitemtype * item;
  int i;
{
  return 0;
}
#endif

/**
   is_directory () lazy check
 */
int
is_directory (item)
  menuitemtype * item;
{
#ifdef use_lazy_stat
  if (streq (item->hopitemname, "lazy")) {
	struct stat fstat_buf;
	char pathname [maxFILENAMElen];
# if defined (use_fstatat) || defined (use_rel_stat)
	strcpy (pathname, "");
# else
	strcpy (pathname, direcname);
	add_pathsep (pathname, maxFILENAMElen);
# endif
	strappend (pathname, item->itemname, maxFILENAMElen);
# ifdef vms
	if (! strchr (pathname, ';')) {
		/* pull out dir flag for unaccessible dirs */
		strappend (pathname, ".DIR;1", maxFILENAMElen);
	}
# endif
# ifdef use_fstatat
#error fd for fstatat missing in lazy mode
	if (fstatat (dir_fd, pathname, & fstat_buf, 0) < 0) {
# else
	if (stat (pathname, & fstat_buf) < 0) {
# endif
		/* No such file or directory ... */
		item->hopitemname = "";
		item->itemon = dummyflagoff;
	} else if (S_ISDIR (fstat_buf.st_mode)) {
		item->hopitemname = "directory";
	} else {
		/* not a directory */
		item->hopitemname = "";
		item->itemon = dummyflagoff;
	}
/*printf ("is_dir: stat <%s> -> %s\n", pathname, item->hopitemname);*/
  }
#endif

  return streq (item->hopitemname, "directory");
}

/*
 * get_filename () opens a file chooser dialog to choose a file name
 */
int
get_filename (message, file, directory_chooser)
  char * message;
  char * file;
  FLAG directory_chooser;
{
#ifndef use_lazy_stat
  char direcname [maxFILENAMElen];
#endif
  char * filter = NIL_PTR;
  char dirpath [maxFILENAMElen];
  FLAG continue_chooser = True;
  int ret = ERRORS;
  FLAG memerr = False;

  strcpy (direcname, ".");

  while (continue_chooser) {
	DIR * dir;
#ifdef use_fstatat
	int dir_fd;
#endif
	struct dirent * direntry;
	int fi;

#ifdef use_hint_timer
	gettimeofday (& now_start, 0);
#else
# if defined (msdos) || defined (vms)
	status_msg ("...checking directory...");
# endif
#endif

	filecount = 0;

#ifdef use_rel_stat
	... = chdir (direcname);
	dir = opendir (".");
	if (! dir) {
		error2 ("Cannot open directory: ", serror ());
		return ERRORS;
	}
#else
	dir = opendir (direcname);
	fi = 0;
	while (! dir) {
		/* this loop is not useful initially 
		   because direcname starts as "."
		 */
		char * lastslash;
		error2 ("Cannot open directory: ", serror ());
		if (fi >= 2) {
			break;
		}
		lastslash = strrchr (direcname, '/');
		if (lastslash && lastslash != direcname) {
			* lastslash = '\0';
		} else {
			if (fi == 0) {
				strcpy (direcname, gethomedir ());
			} else {
#ifdef vms
				/* strcpy (direcname, "[000000]"); */
				/* SYS$SYSROOT:[SYSLIB] / SYS$SHARE : */
				strcpy (direcname, "/");
#else
				strcpy (direcname, "/");
#endif
			}
			fi ++;
		}
		dir = opendir (direcname);
	}
#endif
#ifdef use_fstatat
	dir_fd = open (direcname, 0);
#endif

	dir_root = 0;
#ifdef vms
	if (tree_insert (& dir_root, "..", "directory") == True) {
		filecount ++;
	}
#endif
	while ((direntry = readdir (dir)) != 0) {
#ifdef use_hint_timer
		check_slow_hint ();
#endif

		if (streq (direntry->d_name, ".")
		    /* && ! directory_chooser */
		   ) {
			/* ignore */
			/* also for change dir, if absolute cwd is listed */
		} else if (! filter || wildcard (filter, direntry->d_name)) {
		    char * dirflag = NIL_PTR;
#ifdef _DIRENT_HAVE_D_TYPE
/*printf ("d_type %02X %s\n", direntry->d_type, direntry->d_name);*/
		    if (direntry->d_type == DT_DIR) {
			dirflag = "directory";
		    } else if (direntry->d_type == DT_REG) {
			/* don't mark */
		    } else
			/* DT_UNKNOWN or DT_LNK (-> directory?) */
#endif
#ifdef use_lazy_stat
			dirflag = "lazy";
#else
		    {
			struct stat fstat_buf;
			char pathname [maxFILENAMElen];
# if defined (use_fstatat) || defined (use_rel_stat)
			strcpy (pathname, "");
# else
			strcpy (pathname, direcname);
			add_pathsep (pathname, maxFILENAMElen);
# endif
			strappend (pathname, direntry->d_name, maxFILENAMElen);
# ifdef vms
			if (! strchr (pathname, ';')) {
				/* pull out dir flag for unaccessible dirs */
				strappend (pathname, ".DIR;1", maxFILENAMElen);
			}
# endif
# ifdef use_fstatat
			if (fstatat (dir_fd, pathname, & fstat_buf, 0) < 0) {
# else
/*printf ("stat %s\n", pathname);*/
			if (stat (pathname, & fstat_buf) < 0) {
# endif
				/* No such file or directory ... */
			} else if (S_ISDIR (fstat_buf.st_mode)) {
/*printf ("S_ISDIR %s\n", pathname);*/
				dirflag = "directory";
			}
		    }
#endif

		    if (! directory_chooser || dirflag) {
				char * fn = dupstr (direntry->d_name);
				FLAG inserted = False;
				if (fn) {
					inserted = tree_insert (& dir_root, fn, dirflag);
				}
				if (inserted == True) {
					filecount ++;
				} else if (inserted == False && ! memerr) {
					error ("File menu incomplete - cannot allocate memory for checking files");
					sleep (1);
					memerr = True;
				} else if (inserted == VALID) {
					/* discarded duplicate entry */
				}
		    }
		}
	}
	closedir (dir);
#ifdef use_rel_stat
#error not implemented: revert from chdir: go to previous directory
#endif
#ifdef use_fstatat
	close (dir_fd);
#endif

#if defined (msdos) || defined (vms)
	clear_status ();
#endif

	/* reset wildcard filter */
	filter = NIL_PTR;

	/* compose pathname of selected directory for display */
	if (is_absolute_path (direcname)) {
		strcpy (dirpath, direcname);
	} else {
		char * dnpoi;
		(void) get_cwd (dirpath);
		dnpoi = direcname;
		while (strisprefix ("..", dnpoi) &&
			(dnpoi [2] == '\0'
			 || dnpoi [2] == '/'
#ifdef pc
			 || dnpoi [2] == '\\'
#endif
			)
		      ) {
			char * lastslash = pathlast (dirpath);
			if (lastslash) {
#ifdef vms
				if (* lastslash == ']') {
					* lastslash = 0;
					lastslash = strrchr (dirpath, '.');
					if (lastslash) {
						* lastslash ++ = ']';
						* lastslash = 0;
					} else {
						lastslash = strrchr (dirpath, '[');
						if (lastslash) {
							* lastslash = 0;
						} else {
							* dirpath = 0;
						}
						strappend (dirpath, "[000000]", maxFILENAMElen);
					}
				} else {
					* lastslash = 0;
				}
#else
				* lastslash = 0;
#endif
			}
			dnpoi += 2;
			if (* dnpoi) {
				dnpoi ++;
			}
		}
		if (* dnpoi && ! streq (dnpoi, ".")) {
			add_pathsep (dirpath, maxFILENAMElen);
			strappend (dirpath, dnpoi, maxFILENAMElen);
		}
		if (! * dirpath) {
			strcat (dirpath, "/");
		}
	}

	/* adjust # entries */
	filecount += 1;
	/* allocate menu structure */
	filemenu = alloc (filecount * sizeof (menuitemtype));
	memerr = False;
	while (! filemenu && filecount > 1) {
		if (! memerr) {
			memerr = True;
		}
		filecount --;
		filemenu = alloc (filecount * sizeof (menuitemtype));
	}
	if (! filemenu) {
		error ("Cannot allocate memory for file menu");
		return ERRORS;
	} else if (memerr) {
		error ("File menu incomplete - not enough memory for menu");
		sleep (1);
	}
	itemcount = 0;
	/* pre-fill current working directory */
	fill_menuitem (& filemenu [itemcount ++], dirpath, "directory");
	/* fill menu with dir/file names */
	if (sort_dirs_first) {
		tree_traverse (dir_root, True);
		tree_traverse (dir_root, False);
	} else {
		tree_traverse (dir_root, NOT_VALID);
	}

	hop_flag = 0;
	fi = popup_menu (filemenu, filecount, 2, 2, message, True, False, "*");

	if (fi < 0) {
		ret = ERRORS;
		continue_chooser = False;
	} else if (streq (filemenu [fi].itemname, "~")) {
		strcpy (direcname, "");
		strappend (direcname, gethomedir (), maxFILENAMElen);
	} else {
	    menuitemtype * chosenitem = & (filemenu [fi]);
	    char * chosenname = chosenitem->itemname;
	    char expandedname [maxFILENAMElen];
	    if (chosenname [0] == '~'
		&& (chosenname [1] == '/' || chosenname [1] == '\0')) {
		strcpy (expandedname, "");
		strappend (expandedname, gethomedir (), maxFILENAMElen);
		chosenname ++;
		strip_trailingslash (expandedname);
		strappend (expandedname, chosenname, maxFILENAMElen);
		chosenname = expandedname;
	    }

#ifdef debug_file_chooser
	    printf ("%s -> %s (%s)", direcname, chosenname, chosenitem->hopitemname);
#endif
	    if (streq (chosenitem->hopitemname, "*")) {
		/* set wildcard filter */
		filter = chosenname;
	    } else {
		/* compose changed directory/file name */
		if (streq (chosenname, "..")
#ifdef vms
			|| streq (chosenname, "[-]")
#endif
			) {
			char * lastslash = pathlast (direcname);
			if ((lastslash && streq (lastslash + 1, ".."))
			    || streq (direcname, "..")) {
				strappend (direcname, "/..", maxFILENAMElen);
			} else if (lastslash) {
#ifdef vms
				if (* lastslash == ']') {
				    if (* (lastslash + 1)) {
					* (lastslash + 1) = '\0';
				    } else {
					* lastslash = 0;
					lastslash = strrchr (direcname, '.');
					if (lastslash) {
						* lastslash ++ = ']';
						* lastslash = 0;
					} else {
						lastslash = strrchr (direcname, '[');
						if (lastslash) {
							* lastslash = 0;
						} else {
							* direcname = 0;
						}
						strappend (direcname, "[000000]", maxFILENAMElen);
					}
				    }
				} else {
					* lastslash = 0;
				}
#else
				if (lastslash == direcname) {
					lastslash ++;
				}
				* lastslash = 0;
#endif
			} else if (streq (direcname, ".")) {
				strcpy (direcname, "..");
			} else {
				strcpy (direcname, ".");
			}
#ifdef pc_better_dont
			if (direcname [1] == ':' && direcname [2] == '\0') {
				strcat (direcname, "\\");
			}
#endif
		} else if (streq (chosenname, ".")
#ifdef vms
			|| streq (chosenname, "[]")
#endif
			) {
			/* no change */
		} else {
#ifdef vms
			char * dirsuf;
			char * last;
#endif
			if (streq (direcname, ".") || is_absolute_path (chosenname)) {
				strcpy (direcname, "");
			} else {
				add_pathsep (direcname, maxFILENAMElen);
			}
#ifdef vms
			dirsuf = strstr (chosenname, ".DIR");
			last = direcname + strlen (direcname);
			if (last != direcname) {
				last --;
			}
			if (dirsuf && (! dirsuf [4] || dirsuf [4] == ';')) {
				/* dsk: + THAT.DIR -> dsk:[.THAT]
				   [dir] + THAT.DIR -> [dir.THAT]
				*/
				* dirsuf = '\0';

				if (* last == ']') {
					* last = '.';
				} else {
					strappend (direcname, "[.", maxFILENAMElen);
				}
				strappend (direcname, chosenname, maxFILENAMElen);
				strappend (direcname, "]", maxFILENAMElen);
			} else if (* last == ']' && * chosenname == '[') {
				/* [a] + [.b] -> [a.b]
				   [a] + [] -> [a]
				   [a] + [-] -> [a.-]
				*/
				chosenname ++;
				if (* chosenname == '.' || * chosenname == ']') {
					* last = '\0';
				} else {
					* last = '.';
				}
				strappend (direcname, chosenname, maxFILENAMElen);
			} else {
				strappend (direcname, chosenname, maxFILENAMElen);
			}
#else
			strappend (direcname, chosenname, maxFILENAMElen);
#endif
		}

#ifdef msdos
		if (chosenname [0] && chosenname [1] == ':' && ! chosenname [2]) {
			/* MSDOS shortcut for change disk */
			strcpy (file, chosenname);
			ret = FINE;
			continue_chooser = False;
		}
		else
#endif
		if (directory_chooser) {
			if (streq (chosenitem->hopitemname, "cd")) {
				/* navigate into directory after TAB */
			} else {
				/* exit with selected directory name */
				strcpy (file, direcname);
				ret = FINE;
				continue_chooser = False;
			}
		} else if (is_directory (chosenitem)) {
			/* navigate into selected directory */
		} else {
			/* check whether selected name 
			   is actually a directory
			*/
			struct stat fstat_buf;
#ifdef vms
			strcpy (expandedname, direcname);
			if (! strchr (expandedname, ';')) {
				char * lastslash = pathlast (dirpath);
				if (lastslash &&
				    (* lastslash == ']' || * lastslash == ':')) {
				} else {
					/* pull out dir info for unaccessible dirs */
					strappend (expandedname, ".DIR;1", maxFILENAMElen);
				}
			}
			if (stat (expandedname, & fstat_buf) == 0
#else
			if (stat (direcname, & fstat_buf) == 0
#endif
			 && S_ISDIR (fstat_buf.st_mode)) {
				/* navigate into directory */
			} else {
				/* exit with selected file name */
				strcpy (file, direcname);
				ret = FINE;
				continue_chooser = False;
			}
		}
	    }
#ifdef debug_file_chooser
	    printf (": %s\n", direcname);
#endif
	}

	free_space (filemenu);
	tree_free (dir_root);
	dir_root = 0;
  }

  return ret;
}

#endif


/*======================================================================*\
|*				End					*|
\*======================================================================*/
