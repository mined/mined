/*======================================================================*\
|*		Editor mined						*|
|*		file handling functions					*|
\*======================================================================*/

/*to get a prototype for readlink and symlink:*/
/*#define _POSIX_C_SOURCE 200112*/
/*#define _XOPEN_SOURCE 500*/
/*#define _XOPEN_SOURCE & #define _XOPEN_SOURCE_EXTENDED*/
#define _BSD_SOURCE

#ifdef __TURBOC__
#include <utime.h>
#endif

#include "mined.h"

#ifndef FORK
#define fork vfork
#endif

#ifndef vms
#include <dirent.h>
#endif
#ifndef VAXC
#include <sys/stat.h>
#endif
#include <errno.h>


#include "textfile.h"
#include "charprop.h"
#include "termprop.h"	/* utf8_screen */
#include "io.h"

#define use_locking

#if defined (msdos) || defined (vms) || defined (__MINGW32__)
#undef use_locking
#endif


#ifdef vms_without_access_fix
#define dont_check_modtime
#warning [41m obsolete, fixed: avoiding O_RDWR on VMS [0m
#endif


#ifdef __clang__
#pragma clang diagnostic ignored "-Wincompatible-pointer-types"
#endif


/*======================================================================*\
|*			Data section					*|
\*======================================================================*/

static int open_linum;		/* line # to re-position to */
static int open_col;		/* line column to re-position to */
static int open_pos;		/* character index to re-position to */

FLAG overwriteOK = False;	/* Set if current file is OK for overwrite */
static FLAG file_locked = False;	/* lock file status */
static FLAG recovery_exists = False;	/* Set if newer #file# exists for current file */
static FLAG backup_pending = True;
static struct stat filestat;	/* Properties of file read for editing */

char * default_text_encoding = "";

static FLAG save_restricted;
static char save_file_name [maxFILENAMElen];
static int save_cur_pos;
static int save_cur_line;

FLAG viewing_help = False;


/* options */
static FLAG multiexit = True;	/* Should exit command go to next file? */
char * preselect_quote_style = NIL_PTR;

FLAG lineends_LFtoCRLF = False;
FLAG lineends_CRLFtoLF = False;
FLAG lineends_CRtoLF = False;
FLAG lineends_detectCR = False;


/* state */
FLAG pasting = False;
#define pasting_encoded	(pasting && ! pastebuf_utf8)

lineend_type got_lineend;

static FLAG loaded_from_filename = False;


/*======================================================================*\
|*			File loading performance monitoring		*|
\*======================================================================*/

#define dont_debug_timing
#ifdef __TURBOC__
#undef debug_timing
#endif

#ifdef debug_timing

#include <sys/time.h>

static
long
gettime ()
{
  struct timeval now;
  gettimeofday (& now, 0);
  return ((long) now.tv_sec) * 1000000 + now.tv_usec;
}

#define mark_time(timer)	timer -= gettime ()
#define elapsed_time(timer)	timer += gettime ()
#define elapsed_mark_time(timer1, timer2)	{long t = gettime (); timer1 += t; timer2 -= t;}

#else

#define mark_time(timer)
#define elapsed_time(timer)
#define elapsed_mark_time(timer1, timer2)

#endif


/*======================================================================*\
|*			Text string routines				*|
\*======================================================================*/

int
UTF8_len (c)
  char c;
{
  if ((c & 0x80) == 0x00) {
	return 1;
  } else if ((c & 0xE0) == 0xC0) {
	return 2;
  } else if ((c & 0xF0) == 0xE0) {
	return 3;
  } else if ((c & 0xF8) == 0xF0) {
	return 4;
  } else if ((c & 0xFC) == 0xF8) {
	return 5;
  } else if ((c & 0xFE) == 0xFC) {
	return 6;
  } else { /* illegal UTF-8 code */
	return 1;
  }
}

int
CJK_len (text)
  character * text;
{
  if (multichar (* text)) {
	if (* text == 0x8E && text_encoding_tag == 'C') {
		return 4;
	} else if (* text == 0x8F &&
		   (text_encoding_tag == 'J' || text_encoding_tag == 'X')) {
		return 3;
	} else if (text_encoding_tag == 'G'
		&& * (text + 1) <= '9'
		&& * (text + 1) >= '0') {
			return 4;
	} else {
		return 2;
	}
  } else {
	return 1;
  }
}


/*
 * char_count () returns the number of characters in the string
 * excluding the '\0'.
 */
int
char_count (string)
  char * string;
{
  int count = 0;

  if (string != NIL_PTR) {
	while (* string != '\0') {
		advance_char (& string);
		count ++;
	}
  }
  return count;
}


/*
 * col_count () returns the number of screen columns in the string
 */
int
col_count (string)
  char * string;
{
  int count = 0;
  char * start = string;

  if (string != NIL_PTR) {
	while (* string != '\0') {
		advance_char_scr (& string, & count, start);
	}
  }
  return count;
}

/**
   determine Unicode information from UTF-8 character
   return parameters:
     length: the number of UTF-8 bytes in the character
     ucs: its Unicode value
 */
void
utf8_info (u, length, ucs)
  char * u;
  int * length;
  unsigned long * ucs;
{
  char * textpoi = u;
  character c = * textpoi;
  int utfcount;
  unsigned long unichar;

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
  } else if (c == 0xFE) {
	/* illegal UTF-8 code */
	utfcount = 1;
	unichar = '4';
  } else if (c == 0xFF) {
	/* illegal UTF-8 code */
	utfcount = 1;
	unichar = '5';
  } else {
	/* illegal UTF-8 sequence character */
	utfcount = 1;
	unichar = '8';
  }

  * length = utfcount;

  utfcount --;
  textpoi ++;
  while (utfcount > 0 && (* textpoi & 0xC0) == 0x80) {
	unichar = (unichar << 6) | (* textpoi & 0x3F);
	utfcount --;
	textpoi ++;
  }
  if (utfcount > 0) {
	/* too short UTF-8 sequence */
	unichar = (character) '<';	/* '«' may raise a CJK terminal width problem */
	* length -= utfcount;
  }

  * ucs = unichar;
}

/**
   Determine if a Unicode character is joined to a ligature 
   with the previous character in the string or line 
   (which may be in any encoding).
 */
int
isjoined (unichar, charpos, linebegin)
  unsigned long unichar;
  char * charpos;
  char * linebegin;
{
  if ((joining_screen || apply_joining) && encoding_has_combining ()) {
	if (unichar == 0x0622 || unichar == 0x0623 || unichar == 0x0625 || unichar == 0x0627) {
		/* ALEF may be joined to a ligature with preceding LAM */
		register unsigned long prev_unichar;

		precede_char (& charpos, linebegin);
		prev_unichar = unicodevalue (charpos);
		if (prev_unichar == 0x0644) {
			/* LAM joins to a ligature with any of the above */
			return 1;
		}
	}
  }
  return 0;
}

/**
   Determine if a Unicode character is effectively of zero width, i.e. 
   if it combines with the previous character in the string or line 
   (which may be in any encoding).
 */
int
iscombined (unichar, charpos, linebegin)
  unsigned long unichar;
  char * charpos;
  char * linebegin;
{
  if (mapped_term && no_char (mappedtermchar (unichar))) {
	return False;
  }

  return isjoined (unichar, charpos, linebegin) || iscombining (unichar);
}

int
iscombining (ucs)
  unsigned long ucs;
{
  if (mapped_term && no_char (mappedtermchar (ucs))) {
	return False;
  } else {
	return term_iscombining (ucs);
  }
}

int
iswide (ucs)
  unsigned long ucs;
{
  if (ucs & 0x80000000) {
    /* special encoding of 2 Unicode chars, mapped from 1 CJK character */
    if ((ucs & 0xFFF3) == 0x02E1) {
      /* 0x02E9 0x02E5 or 0x02E5 0x02E9 */
      return 1;
    } else {
      /* strip accent indication for width lookup */
      ucs &= 0xFFFF;
    }
  }

  return term_iswide (ucs);
}

/**
   Determine the effective screen width of a Unicode character.
 */
int
uniscrwidth (unichar, charpos, linebegin)
  unsigned long unichar;
  char * charpos;
  char * linebegin;
{
  if (combining_mode && iscombined (unichar, charpos, linebegin)) {
	if (separate_isolated_combinings) {
		if (charpos == linebegin || * (charpos - 1) == '\t') {
			if (iswide (unichar)) {
				return 2;
			} else {
				return 1;
			}
		}
	}
	return 0;
  }

  if (mapped_term || (cjk_term && ! cjk_uni_term)) {
	unsigned long cjktermchar = mappedtermchar (unichar);
	if (! no_char (cjktermchar)) {
		if (cjktermchar < 0x100
		 || /*GB18030 kludge*/ (unichar >= 0x80 && unichar <= 0x9F)) {
			return 1;
		} else if ((term_encoding_tag == 'J' || term_encoding_tag == 'X')
			&& (cjktermchar >> 8) == 0x8E) {
			return 1;
		} else {
			return 2;
		}
	}
  }

  if (iswide (unichar)) {
	return 2;
  } else {
	return 1;
  }
}

#define dont_debug_cjkscrwidth

#ifdef debug_cjkscrwidth
static unsigned long unichar = -1;
#define trace_cjk(tag, w)	if (cjkchar > 0x80) printf ("[%s] %04lX (U+%04lX): %d\n", tag, cjkchar, unichar, w);
#else
#define trace_cjk(tag, w)	
#endif

/**
   Determine the effective screen width of a CJK character.
 */
int
cjkscrwidth (cjkchar, charpos, linebegin)
  unsigned long cjkchar;
  char * charpos;
  char * linebegin;
{
  char encoding_tag;

  if (! cjk_term || cjk_uni_term) {
	unsigned long unichar = lookup_encodedchar (cjkchar);
	if (no_unichar (unichar) && ! valid_cjk (cjkchar, NIL_PTR)) {
		trace_cjk ("noval", 1);
		return 1;
	} else if (combining_mode && iscombined (unichar, charpos, linebegin)) {
		if (separate_isolated_combinings) {
			if (charpos == linebegin || * (charpos - 1) == '\t') {
				if (utf_cjk_wide_padding || iswide (unichar)) {
					trace_cjk ("no pad", 2);
					return 2;
				} else {
					trace_cjk ("comb", 1);
					return 1;
				}
			}
		}
		trace_cjk ("comb", 0);
		return 0;
	} else if (utf_cjk_wide_padding || iswide (unichar)) {
		trace_cjk ("pad", 2);
		return 2;
	} else if (no_unichar (unichar) && cjk_term) {
		trace_cjk ("nouni", 2);
		return 2;
	} else {
		trace_cjk ("ut", 1);
		return 1;
	}
  }

  encoding_tag = text_encoding_tag;
  if (mapped_term || (cjk_term && remapping_chars ())) {
	unsigned long unichar = lookup_encodedchar (cjkchar);
	if (! no_unichar (unichar)) {
		unsigned long cjktermchar = mappedtermchar (unichar);
		if (! no_char (cjktermchar)) {
			cjkchar = cjktermchar;
			encoding_tag = term_encoding_tag;
		}
	}
  }

  if (cjkchar < 0x100) {
	trace_cjk ("<", 1);
	return 1;
  } else if ((encoding_tag == 'J' || encoding_tag == 'X')
	  && (cjkchar >> 8) == 0x8E) {
	trace_cjk ("jx", 1);
	return 1;
  } else {
	trace_cjk ("2", 2);
	return 2;
  }
}

/*
 * Advance only character pointer to next character.
 * UTF-8 mode.
 */
void
advance_utf8 (poipoi)
  char * * poipoi;
{
  int follow = UTF8_len (* * poipoi) - 1;

  (* poipoi) ++;
  while (follow > 0 && (* * poipoi & 0xC0) == 0x80) {
	(* poipoi) ++;
	follow --;
  }
}

/*
 * Advance only character pointer to next character.
 * CJK mode.
 */
static
void
advance_cjk (poipoi)
  char * * poipoi;
{
  int len = CJK_len (* poipoi);

  (* poipoi) ++;
  len --;
  while (len > 0 && * * poipoi != '\0' && * * poipoi != '\n') {
	(* poipoi) ++;
	len --;
  }
}

/*
 * Advance only character pointer to next character.
 * Handle tab characters and different character encodings correctly.
 */
void
advance_char (poipoi)
  char * * poipoi;
{
  if (utf8_text) {
	advance_utf8 (poipoi);
  } else if (cjk_text) {
	advance_cjk (poipoi);
  } else {
	(* poipoi) ++;
  }
}

/*
   charbegin () determines the first byte of the character pointed to 
   in the given line
 */
char *
charbegin (line, s)
  char * line;
  char * s;
{
  char * char_search;
  char * char_prev;

  if (utf8_text || cjk_text) {
	char_search = line;
	char_prev = char_search;
	while (char_search < s) {
		char_prev = char_search;
		advance_char (& char_search);
	}
	if (char_search > s) {
		return char_prev;
	} else {
		return s;
	}
  }
  return s;
}

/*
 * precede_char () moves the character pointer within line "begin_line" 
 * left by 1 character
 */
void
precede_char (poipoi, begin_line)
  char * * poipoi;
  char * begin_line;
{
  if (utf8_text) {
	char * char_search = * poipoi;
	int l = 0;
	while (char_search != begin_line && l < 6) {
		char_search --;
		l ++;
		if ((* char_search & 0xC0) != 0x80) {
			break;
		}
	}
	if (l > 0 && l > UTF8_len (* char_search)) {
		(* poipoi) --;
	} else {
		* poipoi = char_search;
	}
  } else if (cjk_text) {
	char * char_search = begin_line;
	char * char_prev = char_search;
	while (char_search < * poipoi) {
		char_prev = char_search;
		advance_cjk (& char_search);
	}
	* poipoi = char_prev;
  } else if (* poipoi != begin_line) {
	(* poipoi) --;
  }
}

/*
 * utf8value () determines the value of the UTF-8 character pointed to
 */
unsigned long
utf8value (poi)
  character * poi;
{
  int len;
  unsigned long unichar;
  utf8_info (poi, & len, & unichar);
  return unichar;
}

/*
 * charvalue () determines the value of the character pointed to
 */
unsigned long
charvalue (poi)
  character * poi;
{
  int len;

  if (utf8_text) {
	unsigned long unichar;
	utf8_info (poi, & len, & unichar);
	return unichar;
  } else if (cjk_text && multichar (* poi)) {
	unsigned long cjkchar;
	len = CJK_len (poi);
	cjkchar = * poi ++;
	len --;
	while (len > 0 && * poi != '\0' && * poi != '\n') {
		cjkchar = (cjkchar << 8) | * poi ++;
		len --;
	}
	if (len > 0) {
		return CHAR_INVALID;
	} else {
		return cjkchar;
	}
  } else {
	return * poi;
  }
}

/**
   unicode () returns the Unicode value of the character code
 */
unsigned long
unicode (code)
  unsigned long code;
{
  if (cjk_text || mapped_text) {
	return lookup_encodedchar (code);
  } else {
	return code;
  }
}

/**
   unicodevalue () determines the Unicode value of the character pointed to
 */
unsigned long
unicodevalue (poi)
  character * poi;
{
  return unicode (charvalue (poi));
}

/*
 * precedingchar () determines the preceding character value
 */
unsigned long
precedingchar (curpoi, begin_line)
  char * curpoi;
  char * begin_line;
{
  char * poi;

  if (curpoi == begin_line) {
	return '\n';
  } else {
	poi = curpoi;
	precede_char (& poi, begin_line);
	return charvalue (poi);
  }
}

/*
 * Advance character pointer and screen column counter to next character.
 * UTF-8 mode.
 */
void
advance_utf8_scr (poipoi, colpoi, linebegin)
  char * * poipoi;
  int * colpoi;
  char * linebegin;
{
  unsigned long unichar;
  int follow;

	utf8_info (* poipoi, & follow, & unichar);
	(* colpoi) += uniscrwidth (unichar, * poipoi, linebegin);
	follow --;
	(* poipoi) ++;
	while (follow > 0 && (* * poipoi & 0xC0) == 0x80) {
		(* poipoi) ++;
		follow --;
	}
}

/*
 * Advance character pointer and screen column counter to next character.
 * Handle tab characters and different character encodings correctly.
 */
void
advance_char_scr (poipoi, colpoi, linebegin)
  char * * poipoi;
  int * colpoi;
  char * linebegin;
{
  if (ebcdic_text ? * * poipoi == code_TAB : * * poipoi == '\t') {
	* colpoi = tab (* colpoi);
	(* poipoi) ++;
  } else if (utf8_text) {
	advance_utf8_scr (poipoi, colpoi, linebegin);
  } else if (cjk_text) {
	int len = CJK_len (* poipoi);

	(* colpoi) += cjkscrwidth (charvalue (* poipoi), * poipoi, linebegin);

	/* make sure pointer is incremented at least once in case it's \n */
	(* poipoi) ++;
	len --;
	while (len > 0 && * * poipoi != '\0' && * * poipoi != '\n') {
		(* poipoi) ++;
		len --;
	}
  } else if (mapped_text) {
	unsigned long unichar = lookup_encodedchar ((character) * * poipoi);
	if (combining_mode && iscombined (unichar, * poipoi, linebegin)) {
		if (separate_isolated_combinings) {
			if (* poipoi == linebegin || * (* poipoi - 1) == '\t') {
				if (iswide (unichar)) {
					(* colpoi) += 2;
				} else {
					(* colpoi) ++;
				}
			}
		} else {
			/* * colpoi stays where it is */
		}
	} else if (cjk_term || cjk_width_data_version) {
		(* colpoi) += uniscrwidth (unichar, * poipoi, linebegin);
	} else {
		(* colpoi) ++;
	}
	(* poipoi) ++;
  } else if (cjk_term || cjk_width_data_version) {
	(* colpoi) += uniscrwidth ((character) * * poipoi, * poipoi, linebegin);
	(* poipoi) ++;
  } else {
	(* colpoi) ++;
	(* poipoi) ++;
  }
}


/*======================================================================*\
|*	UTF-8 character statistics and quote style detection		*|
\*======================================================================*/

/**
   determine if character precedes opening rather than closing quotation
   - add further characters (=, /) to quote openers?
 */
FLAG
opensquote (prevchar)
  unsigned long prevchar;
{
  switch (prevchar) {
	case '\n':
	case '(':
	case '[':
	case '{':
		return True;
  }
  if (iswhitespace (prevchar) || isdash (prevchar) || isopeningparenthesis (prevchar)) {
	return True;
  }
  return False;
}

/**
   determine if current position (if quote mark) is opening
 */
static
FLAG
isopeningquote (s, beg)
  char * s;
  char * beg;
{
  /* simplified approach; don't consider quotes after quotes 
     or CJK embedded quotes
   */
  return opensquote (precedingchar (s, beg));
}

/**
   language-specific quotation mark counters
 */
static unsigned long count_plain = 0;
static unsigned long count_English = 0;
static unsigned long count_German = 0;
static unsigned long count_Swiss = 0;
static unsigned long count_inwards = 0;
static unsigned long count_Dutch = 0;
static unsigned long count_SwedFinn_q = 0;
static unsigned long count_SwedFinn_g = 0;
static unsigned long count_Greek = 0;
static unsigned long count_CJKcorners = 0;
static unsigned long count_CJKtitles = 0;
static unsigned long count_CJKsquares = 0;
static unsigned long count_Danish = 0;
static unsigned long count_French = 0;
static unsigned long count_Norwegian = 0;
static unsigned long count_Russian = 0;
static unsigned long count_Polish = 0;
static unsigned long count_Macedonian = 0;
static unsigned long count_Serbian = 0;

static
void
reset_quote_statistics ()
{
	count_plain = 0;
	count_English = 0;
	count_German = 0;
	count_Swiss = 0;
	count_inwards = 0;
	count_Dutch = 0;
	count_SwedFinn_q = 0;
	count_SwedFinn_g = 0;
	count_Greek = 0;
	count_CJKcorners = 0;
	count_CJKtitles = 0;
	count_CJKsquares = 0;
	count_Danish = 0;
	count_French = 0;
	count_Norwegian = 0;
	count_Russian = 0;
	count_Polish = 0;
	count_Macedonian = 0;
	count_Serbian = 0;
}

static unsigned long count_quotes;

static
void
check_quote_style (c, s)
  unsigned long c;
  char * s;
{
/*printf ("%4ld %s\n", c, s);*/
  if (c > count_quotes) {
	count_quotes = c;
	set_quote_style (s);
  }
}

/* Unicode quotation marks
“ 201C; LEFT DOUBLE QUOTATION MARK; DOUBLE TURNED COMMA QUOTATION MARK
” 201D; RIGHT DOUBLE QUOTATION MARK; DOUBLE COMMA QUOTATION MARK
„ 201E; DOUBLE LOW-9 QUOTATION MARK; LOW DOUBLE COMMA QUOTATION MARK
‟ 201F; DOUBLE HIGH-REVERSED-9 QUOTATION MARK; DOUBLE REVERSED COMMA QUOTATION MARK
‘ 2018; LEFT SINGLE QUOTATION MARK; SINGLE TURNED COMMA QUOTATION MARK
’ 2019; RIGHT SINGLE QUOTATION MARK; SINGLE COMMA QUOTATION MARK
‚ 201A; SINGLE LOW-9 QUOTATION MARK; LOW SINGLE COMMA QUOTATION MARK
‛ 201B; SINGLE HIGH-REVERSED-9 QUOTATION MARK; SINGLE REVERSED COMMA QUOTATION MARK
« 00AB; LEFT-POINTING DOUBLE ANGLE QUOTATION MARK; LEFT POINTING GUILLEMET
» 00BB; RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK; RIGHT POINTING GUILLEMET
‹ 2039; SINGLE LEFT-POINTING ANGLE QUOTATION MARK; LEFT POINTING SINGLE GUILLEMET
› 203A; SINGLE RIGHT-POINTING ANGLE QUOTATION MARK; RIGHT POINTING SINGLE GUILLEMET
〈3008; LEFT ANGLE BRACKET; OPENING ANGLE BRACKET
〉3009; RIGHT ANGLE BRACKET; CLOSING ANGLE BRACKET
《300A; LEFT DOUBLE ANGLE BRACKET; OPENING DOUBLE ANGLE BRACKET
》300B; RIGHT DOUBLE ANGLE BRACKET; CLOSING DOUBLE ANGLE BRACKET
「300C; LEFT CORNER BRACKET; OPENING CORNER BRACKET
」300D; RIGHT CORNER BRACKET; CLOSING CORNER BRACKET
『300E; LEFT WHITE CORNER BRACKET; OPENING WHITE CORNER BRACKET
』300F; RIGHT WHITE CORNER BRACKET; CLOSING WHITE CORNER BRACKET
【 U+3010; LEFT BLACK LENTICULAR BRACKET; OPENING BLACK LENTICULAR BRACKET
】 U+3011; RIGHT BLACK LENTICULAR BRACKET; CLOSING BLACK LENTICULAR BRACKET
〖 U+3016; LEFT WHITE LENTICULAR BRACKET; OPENING WHITE LENTICULAR BRACKET
〗 U+3017; RIGHT WHITE LENTICULAR BRACKET; CLOSING WHITE LENTICULAR BRACKET
*/

static
void
determine_quote_style ()
{
	count_quotes = 0;
	check_quote_style (count_plain,		"\"\"");
	check_quote_style (count_English,	"“”");
	check_quote_style (count_inwards,	"»«");
	check_quote_style (count_German,	"„“");
	check_quote_style (count_Swiss,		"«» ‹›");
	check_quote_style (count_Dutch,		"„”");
	check_quote_style (count_SwedFinn_q,	"””");
	check_quote_style (count_SwedFinn_g,	"»»");
	check_quote_style (count_Greek,		"«» ‟”");
	check_quote_style (count_CJKcorners,	"『』");
	check_quote_style (count_CJKtitles,	"《》");
	check_quote_style (count_CJKsquares,	"【】");
	check_quote_style (count_Danish,	"»« ’’");	/* alt Danish */
	check_quote_style (count_French,	"« » “ ”");
	check_quote_style (count_Norwegian,	"«» ‘’");
	check_quote_style (count_Russian,	"«» „“");
	check_quote_style (count_Polish,	"„” «»");
	check_quote_style (count_Macedonian,	"„“ ’‘");
	check_quote_style (count_Serbian,	"„“ ’’");
}

/*
 * utf8_count () returns the number of UTF-8 characters in the string
   (like char_count) and also detects quotation marks and updates 
   their statistics.
 */
static
int
utf8_count (string)
  char * string;
{
  char * start = string;
  int count = 0;
  unsigned long unichar = 0;
  unsigned long prev_unichar;
  int utflen;

  if (string != NIL_PTR) {
    while (* string != '\0') {
	/* Detect quotation marks.
	   The UTF-8 codes of all quotation marks are either 
	   C2AB or C2BB or start with either E280 or E380. 
	   This may help for efficient detection during file loading.
	 */
	if ((((character) * string) <= 0x27
	     && (* string == '\'' || * string == '"')
	    )
	    ||
	    ((* string & 0xDE) == 0xC2
	     && (((character) * string) == 0xC2 || ((character) * (string + 1)) == 0x80)
	    )
	   )
	{
	    prev_unichar = unichar;
	    utf8_info (string, & utflen, & unichar);
	    switch ((unsigned int) unichar) {
	    case (character) '"':
	    case (character) '\'':
			count_plain ++;
			break;
	    case 0x201C: /* “ LEFT DOUBLE QUOTATION MARK; DOUBLE TURNED COMMA QUOTATION MARK */
		if (isopeningquote (string, start)) {
			count_English ++;
			count_French ++;
		} else {
			count_German ++;
			count_Russian ++;
			count_Macedonian ++;
			count_Serbian ++;
		}
		break;
	    case 0x2018: /* ‘ LEFT SINGLE QUOTATION MARK; SINGLE TURNED COMMA QUOTATION MARK */
		if (isopeningquote (string, start)) {
			count_English ++;
			count_Norwegian ++;
		} else {
			count_German ++;
			count_Macedonian ++;
		}
		break;
	    case 0x201D: /* ” RIGHT DOUBLE QUOTATION MARK; DOUBLE COMMA QUOTATION MARK */
		count_SwedFinn_q ++;
		if (! isopeningquote (string, start)) {
			count_English ++;
			count_Dutch ++;
			count_Greek ++;
			count_Polish ++;
			count_French ++;
			if (iswhitespace (prev_unichar)) {
				count_French ++;
			}
		}
		break;
	    case 0x2019: /* ’ RIGHT SINGLE QUOTATION MARK; SINGLE COMMA QUOTATION MARK */
		count_SwedFinn_q ++;
		count_Danish ++;
		count_Serbian ++;
		if (isopeningquote (string, start)) {
			count_Macedonian ++;
		} else {
			count_English ++;
			count_Dutch ++;
			count_Norwegian ++;
		}
		break;
	    case 0x201E: /* „ DOUBLE LOW-9 QUOTATION MARK; LOW DOUBLE COMMA QUOTATION MARK */
		if (isopeningquote (string, start)) {
			count_German ++;
			count_Dutch ++;
			count_Russian ++;
			count_Polish ++;
			count_Macedonian ++;
			count_Serbian ++;
		}
		break;
	    case 0x201A: /* ‚ SINGLE LOW-9 QUOTATION MARK; LOW SINGLE COMMA QUOTATION MARK */
		if (isopeningquote (string, start)) {
			count_German ++;
			count_Dutch ++;
		}
		break;
	    case 0x201F: /* ‟ DOUBLE HIGH-REVERSED-9 QUOTATION MARK; DOUBLE REVERSED COMMA QUOTATION MARK */
		if (isopeningquote (string, start)) {
			count_Greek ++;
		}
		break;
	    case 0x201B: /* ‛ SINGLE HIGH-REVERSED-9 QUOTATION MARK; SINGLE REVERSED COMMA QUOTATION MARK */
		break;
	    case 0x00AB: /* « LEFT-POINTING DOUBLE ANGLE QUOTATION MARK; LEFT POINTING GUILLEMET */
		if (isopeningquote (string, start)) {
			count_Swiss ++;
			count_French ++;
			count_Norwegian ++;
			count_Russian ++;
			count_Polish ++;
			count_Greek ++;
		} else {
			count_inwards ++;
			count_Danish ++;
		}
		break;
	    case 0x2039: /* ‹ SINGLE LEFT-POINTING ANGLE QUOTATION MARK; LEFT POINTING SINGLE GUILLEMET */
		if (isopeningquote (string, start)) {
			count_Swiss ++;
		} else {
			count_inwards ++;
		}
		break;
	    case 0x00BB: /* » RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK; RIGHT POINTING GUILLEMET */
		count_SwedFinn_g ++;
		if (isopeningquote (string, start)) {
			count_inwards ++;
			count_Danish ++;
		} else {
			count_Swiss ++;
			count_Norwegian ++;
			count_Russian ++;
			count_Polish ++;
			count_Greek ++;
			count_French ++;
			if (iswhitespace (prev_unichar)) {
				count_French ++;
			}
		}
		break;
	    case 0x203A: /* › SINGLE RIGHT-POINTING ANGLE QUOTATION MARK; RIGHT POINTING SINGLE GUILLEMET */
		count_SwedFinn_g ++;
		if (isopeningquote (string, start)) {
			count_inwards ++;
		} else {
			count_Swiss ++;
		}
		break;
	    case 0x300A: /* 《 LEFT DOUBLE ANGLE BRACKET; OPENING DOUBLE ANGLE BRACKET */
	    case 0x3008: /* 〈 LEFT ANGLE BRACKET; OPENING ANGLE BRACKET */
	    case 0x300B: /* 》 RIGHT DOUBLE ANGLE BRACKET; CLOSING DOUBLE ANGLE BRACKET */
	    case 0x3009: /* 〉 RIGHT ANGLE BRACKET; CLOSING ANGLE BRACKET */
		count_CJKtitles ++;
		break;
	    case 0x300C: /* 「 LEFT CORNER BRACKET; OPENING CORNER BRACKET */
	    case 0x300E: /* 『 LEFT WHITE CORNER BRACKET; OPENING WHITE CORNER BRACKET */
	    case 0x300D: /* 」 RIGHT CORNER BRACKET; CLOSING CORNER BRACKET */
	    case 0x300F: /* 』 RIGHT WHITE CORNER BRACKET; CLOSING WHITE CORNER BRACKET */
		count_CJKcorners ++;
		break;
	    case 0x3010: /* 【 LEFT BLACK LENTICULAR BRACKET; OPENING BLACK LENTICULAR BRACKET */
	    case 0x3011: /* 】 RIGHT BLACK LENTICULAR BRACKET; CLOSING BLACK LENTICULAR BRACKET */
	    case 0x3016: /* 〖 LEFT WHITE LENTICULAR BRACKET; OPENING WHITE LENTICULAR BRACKET */
	    case 0x3017: /* 〗 RIGHT WHITE LENTICULAR BRACKET; CLOSING WHITE LENTICULAR BRACKET */
		count_CJKsquares ++;
		break;
	    }
	}

	/* Advance and count */
	advance_utf8 (& string);
	count ++;
    }
  }
  return count;
}


/*======================================================================*\
|*			Auxiliary functions				*|
\*======================================================================*/

static
int
strcaseeq (s1, s2)
  char * s1;
  char * s2;
{
  do {
	char c1, c2;
	if (! * s1 && ! * s2) {
		return True;
	}
	c1 = * s1;
	if (c1 >= 'a' && c1 <= 'z') {
		c1 = c1 - 'a' + 'A';
	}
	c2 = * s2;
	if (c2 >= 'a' && c2 <= 'z') {
		c2 = c2 - 'a' + 'A';
	}
	if (c1 != c2) {
		return False;
	}
	s1 ++;
	s2 ++;
  } while (True);
}


/*
   Set flags depending on file type.
 */
static
void
set_file_type_flags ()
{
  char * bn = getbasename (file_name);
  char * suffix = strrchr (file_name, '.');
  if (suffix != NIL_PTR) {
	suffix ++;
  } else {
	suffix = "";
  }

  if (hide_passwords == UNSURE) {
	hide_passwords = * bn == '.';
  }

  if (viewing_help) {
	mark_HTML = True;
  } else if (mark_HTML == UNSURE) {
	if (
		   strcaseeq (suffix, "html")
		|| strcaseeq (suffix, "htm")
		|| strcaseeq (suffix, "xhtml")
		|| strcaseeq (suffix, "shtml")
		|| strcaseeq (suffix, "mhtml")
		|| strcaseeq (suffix, "sgml")
		|| strcaseeq (suffix, "xml")
		|| strcaseeq (suffix, "xul")
		|| strcaseeq (suffix, "xsd")
		|| strcaseeq (suffix, "xsl")
		|| strcaseeq (suffix, "xslt")
		|| strcaseeq (suffix, "wsdl")
		|| strcaseeq (suffix, "dtd")
	   )
	{
		mark_HTML = True;
		mark_JSP = False;	/* no effect (not checked yet) */
	} else if
	   ( strcaseeq (suffix, "jsp")
		|| strcaseeq (suffix, "php")
		|| strcaseeq (suffix, "asp")
		|| strcaseeq (suffix, "aspx")
	   )
	{
		mark_HTML = True;
		mark_JSP = True;
	} else {
		mark_HTML = False;
		mark_JSP = False;
	}
  }

  if (! strop_selected) {
	if (strcaseeq (suffix, "a68")) {
		lowcapstrop = True;
		dispunstrop = True;
	} else {
		lowcapstrop = False;
		dispunstrop = False;
	}
  }
}


/*======================================================================*\
|*			File I/O buffer					*|
\*======================================================================*/

#define dont_debug_filebuf
#ifdef debug_filebuf
#define filebuflen 16
#else
#define filebuflen (20 * 1024)
#endif
static char filebuf [filebuflen + 1]; /* allow for 1 byte overflow, e.g. from Cygwin /dev/clipboard */


/* filebuf read parameters */
static char * last_bufpos = NIL_PTR;
static char * current_bufpos = NIL_PTR;
static char * UTF16buf = NIL_PTR;
static char * fini_byte = NIL_PTR;
static char * next_byte = NIL_PTR;
static long read_bytes;
static long read_chars;
/*long long file_position;*/

/**
   Clear filebuf read parameters
 */
static
void
clear_get_line ()
{
  last_bufpos = NIL_PTR;
  current_bufpos = NIL_PTR;
}


/* filebuf write parameters */
static unsigned int filebuf_count = 0;

/**
   Clear filebuf write parameters
 */
void
clear_filebuf ()
{
  filebuf_count = 0;
  /* need to clear buffer completely (see break in configure_preferences) */
  clear_get_line ();
}


/*======================================================================*\
|*			File line reading				*|
\*======================================================================*/

int
line_gotten (ret)
  int ret;
{
  return ret != ERRORS && ret != NO_INPUT;
}

/*
 * get_line reads one line from filedescriptor fd. If EOF is reached on fd,
 * get_line () returns ERRORS, else it returns the length of the string.
 */

static long count_good_utf;	/* count good UTF-8 sequences */
static long count_bad_utf;	/* count bad UTF-8 sequences */
static long count_utf_bytes;	/* count UTF-8 sequence bytes */
static long counted_utf_bytes;	/* count UTF-8 sequence bytes */
static long count_good_iso;	/* count good ISO-8859 bytes */
static long count_good_cp1252;	/* count good CP1252 (Windows Western) bytes */
static long count_good_cp850;	/* count good CP850 (DOS) bytes */
static long count_good_mac;	/* count good MacRoman bytes */
static long count_good_ebcdic;	/* count good EBCDIC bytes */
static long count_good_viscii;	/* count good VISCII bytes */
static long count_good_tcvn;	/* count good TCVN bytes */
static long count_1read_op;	/* count 1st read operation by get_line */
static long count_lineend_LF;	/* count Unix lines */
static long count_lineend_CRLF;	/* count MSDOS lines */
static long count_lineend_CR;	/* count Mac lines */
static long count_lineend_NL;	/* count ISO 8859 lines */
static FLAG BOM;		/* BOM found at beginning of file? */
static FLAG consider_transform;	/* consider UTF-16 or EBCDIC when reading? */

/*
   CJK character encoding auto-detection
 */
static character last_cjkbyte = '\0';
static character last2_cjkbyte = '\0';
static long count_good_cjk;	/* count good CJK codes */
static long count_weak_cjk;	/* count weak (unsure) CJK codes */
static long count_bad_cjk;	/* count bad CJK codes */
static long count_big5;		/* count Big5 codes */
static long count_gb;		/* count GB (GB2312, GBK, GB18030) codes */
static int detect_gb18030 = 0;	/* observe GB18030 byte state */
static long count_uhc;		/* count UHC (KS C 5601/KS X 1001) codes */
static long count_jp;		/* count EUC-JP codes */
static long count_jisx;		/* count JIS X 0213 codes */
static long count_sjis;		/* count Shift-JIS/CP932 codes */
static long count_sjisx;	/* count Shift JIS X 0213 codes */
static long count_sjis1;	/* count Shift-JIS single-byte codes */
static long count_johab;	/* count Johab codes */
static long count_cns;		/* count CNS codes */

static long count_max_cjk;
static char max_cjk_tag;

static char * get_line_error;
static int get_line_errno;

static
void
set_error (err)
  char * err;
{
  if (err == NIL_PTR || get_line_error == NIL_PTR) {
	get_line_error = err;
	get_line_errno = -1;
  }
}

static
void
set_errno (err)
  char * err;
{
  set_error (err);
  get_line_errno = geterrno ();
}

static
void
max_cjk_count (cnt, tag)
  long cnt;
  char tag;
{
  if (cnt > count_max_cjk) {
	count_max_cjk = cnt;
	max_cjk_tag = tag;
  }
}

/*
   UTF-16 transformation
 */
static unsigned long surrogate = 0;

static
void
clear_UTF16_transform ()
{
  surrogate = 0;
}

#define dont_debug_auto_detect
#define dont_debug_read

/**
   if preparing for text file reading,
   make sure utf16_file is adjusted afterwards,
   e.g. by calling set_text_encoding (default_text_encoding)
 */
void
reset_get_line (from_text_file)
  FLAG from_text_file;	/* consider UTF-16/EBCDIC transformation ? */
{
  set_error (NIL_PTR);

  count_good_utf = 0;
  count_bad_utf = 0;
  count_utf_bytes = 0;

  count_good_cjk = 0;
  count_weak_cjk = 0;
  count_bad_cjk = 0;
  last_cjkbyte = '\0';
  last2_cjkbyte = '\0';

  count_good_iso = 0;
  count_good_cp1252 = 0;
  count_good_cp850 = 0;
  count_good_mac = 0;
  count_good_ebcdic = 0;
  count_good_viscii = 0;
  count_good_tcvn = 0;
  count_big5 = 0;
  count_gb = 0;
  count_uhc = 0;
  count_jp = 0;
  count_jisx = 0;
  count_sjis = 0;
  count_sjis1 = 0;
  count_sjisx = 0;
  count_johab = 0;
  count_cns = 0;

  count_lineend_LF = 0;
  count_lineend_CRLF = 0;
  count_lineend_CR = 0;
  count_lineend_NL = 0;

  count_1read_op = 0;

  reset_quote_statistics ();

  BOM = False;
  consider_transform = from_text_file;

  clear_UTF16_transform ();

/*  file_position = 0;*/
}

static FLAG save_utf16_file;
static FLAG save_BOM;

/**
   save_text_info and restore_text_info are needed
   to avoid spoiling UTF-16 information while pasting
   -> refactoring!
 */
void
save_text_info ()
{
  save_utf16_file = utf16_file;
  save_BOM = BOM;
}

void
restore_text_info ()
{
  utf16_file = save_utf16_file;
  BOM = save_BOM;
}

void
show_get_l_errors ()
{
  if (! only_detect_text_encoding && get_line_error != NIL_PTR) {
	ring_bell ();
	status_fmt2 (get_line_error, " - Loading failed!");
/*
	while (readcharacter () != ' ' && quit == False) {
		ring_bell ();
		flush ();
	}
*/
  }
}


#define dont_debug_utf16

#ifdef debug_utf16
#define trace_utf16(params)	printf params
#else
#define trace_utf16(params)	
#endif

/*
   Transform UTF-16 input into UTF-8.
 */
static
int
UTF16_transform (little_endian, UTF8buf, maxbufl, next_byte_poi, fini_byte)
  FLAG little_endian;
  char * UTF8buf;
  int maxbufl;
  character * * next_byte_poi;
  character * fini_byte;
{
  register char * ptr = UTF8buf;
  int trans_bytes = 0;
  unsigned int utf16char;

  while (trans_bytes + 4 < maxbufl && * next_byte_poi < fini_byte) {
	utf16char = * * next_byte_poi;
	(* next_byte_poi) ++;
	if (* next_byte_poi < fini_byte) {
		if (little_endian) {
			utf16char |= (* * next_byte_poi) << 8;
		} else {
			utf16char = (utf16char << 8) | (* * next_byte_poi);
		}
		(* next_byte_poi) ++;
	} else if (! little_endian) {
		utf16char = 0;
	}

	if ((utf16char & 0xFC00) == 0xD800) {
	/* high surrogates */
		surrogate = (unsigned long) (utf16char - 0xD7C0) << 10;
		trace_utf16 (("%04X -> surrogate %04lX\n", utf16char, surrogate));
	} else if ((utf16char & 0xFC00) == 0xDC00) {
	/* low surrogates */
		unsigned long unichar = surrogate | (utf16char & 0x03FF);
		trace_utf16 (("surrogate %04lX + %04X -> %04lX -> %02lX %02lX %02lX %02lX\n", surrogate, utf16char, unichar, 0xF0 | (unichar >> 18), 0x80 | ((unichar >> 12) & 0x3F), 0x80 | ((unichar >> 6) & 0x3F), 0x80 | (unichar & 0x3F)));
		surrogate = 0;
		* ptr ++ = 0xF0 | (unichar >> 18);
		* ptr ++ = 0x80 | ((unichar >> 12) & 0x3F);
		* ptr ++ = 0x80 | ((unichar >> 6) & 0x3F);
		* ptr ++ = 0x80 | (unichar & 0x3F);
		trans_bytes += 4;
	} else if (utf16char < 0x80) {
		trace_utf16 (("%04X -> (1) %02X\n", utf16char, utf16char));
		* ptr ++ = utf16char;
		trans_bytes ++;
	} else if (utf16char < 0x800) {
		trace_utf16 (("%04X -> (2) %02X %02X\n", utf16char, 0xC0 | (utf16char >> 6), 0x80 | (utf16char & 0x3F)));
		* ptr ++ = 0xC0 | (utf16char >> 6);
		* ptr ++ = 0x80 | (utf16char & 0x3F);
		trans_bytes += 2;
	} else {
		trace_utf16 (("%04X -> (3) %02X %02X %02X\n", utf16char, 0xE0 | (utf16char >> 12), 0x80 | ((utf16char >> 6) & 0x3F), 0x80 | (utf16char & 0x3F)));
		* ptr ++ = 0xE0 | (utf16char >> 12);
		* ptr ++ = 0x80 | ((utf16char >> 6) & 0x3F);
		* ptr ++ = 0x80 | (utf16char & 0x3F);
		trans_bytes += 3;
	}
  }

  return trans_bytes;
}

#ifdef debug_read
#define trace_read(params)	printf params
#else
#define trace_read(params)	
#endif

static
int
alloc_UTF16buf ()
{
	if (UTF16buf == NIL_PTR) {
		UTF16buf = alloc (filebuflen + 1);
		if (UTF16buf == NIL_PTR) {
			set_error ("Not enough memory for UTF-16 transformation");
			viewonly_err = True;
			modified = False;
			return ERRORS;
		}
	}
	return FINE;
}


/**
   Tables supporting auto-detection of some 8-bit character encodings;
   for each character >= 0x80 they indicate whether the character is 
   a letter (cl), a valid character (cv), or invalid (nc).
 */

#define cl 2	/* count weight of letter */
#define cc 1	/* count weight of non-letter */
#define nc -3	/* count weight of invalid character */

static signed char good_iso [0x80] = {
/*80*/	nc, nc, nc, nc, nc, cl, nc, nc, nc, nc, nc, nc, nc, nc, nc, nc,
/*90*/	nc, nc, nc, nc, nc, nc, nc, nc, nc, nc, nc, nc, nc, nc, nc, nc,
/*A0*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*B0*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*C0*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl,
/*D0*/	cl, cl, cl, cl, cl, cl, cl, cc, cl, cl, cl, cl, cl, cl, cl, cl,
/*E0*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl,
/*F0*/	cl, cl, cl, cl, cl, cl, cl, cc, cl, cl, cl, cl, cl, cl, cl, cl,
};

static signed char good_cp1252 [0x80] = {
/*80*/	cc, nc, cc, cc, cc, cc, cc, cc, cc, cc, cl, cc, cl, nc, cl, nc,
/*90*/	nc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cl, cc, cl, nc, cl, cl,
/*A0*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*B0*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*C0*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl,
/*D0*/	cl, cl, cl, cl, cl, cl, cl, cc, cl, cl, cl, cl, cl, cl, cl, cl,
/*E0*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl,
/*F0*/	cl, cl, cl, cl, cl, cl, cl, cc, cl, cl, cl, cl, cl, cl, cl, cl,
};

static signed char good_cp850 [0x80] = {
/*80*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl,
/*90*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cc, cl, cc, cc,
/*A0*/	cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*B0*/	cc, cc, cc, cc, cc, cl, cl, cl, cc, cc, cc, cc, cc, cc, cc, cc,
/*C0*/	cc, cc, cc, cc, cc, cc, cl, cl, cc, cc, cc, cc, cc, cc, cc, cc,
/*D0*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cl, cc,
/*E0*/	cl, cl, cl, cl, cl, cl, cc, cl, cl, cl, cl, cl, cl, cl, cc, cc,
/*F0*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
};

static signed char good_mac [0x80] = {
/*80*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl,
/*90*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl,
/*A0*/	cc, cc, cc, cc, cc, cc, cc, cl, cc, cc, cc, cc, cc, cc, cl, cl,
/*B0*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cl, cl,
/*C0*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cl, cl, cl, cl, cl,
/*D0*/	cc, cc, cc, cc, cc, cc, cc, cc, cl, cl, cc, cc, cc, cc, cc, cc,
/*E0*/	cc, cc, cc, cc, cc, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cl,
/*F0*/	nc, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
};

static signed char good_ebcdic [0x100] = {
/*00*/	nc, nc, nc, nc, cc, cc, cc, cc, cc, cc, cc, nc, nc, nc, nc, nc,
/*10*/	nc, nc, nc, nc, cc, cc, cc, cc, nc, nc, cc, cc, nc, nc, nc, nc,
/*20*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*30*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*40*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*50*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*60*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*70*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*80*/	cc, cl, cl, cl, cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc,
/*90*/	cc, cl, cl, cl, cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc,
/*A0*/	cc, cc, cl, cl, cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc,
/*B0*/	cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc, cc,
/*C0*/	cc, cl, cl, cl, cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc,
/*D0*/	cc, cl, cl, cl, cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc,
/*E0*/	cc, cc, cl, cl, cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc,
/*F0*/	cl, cl, cl, cl, cl, cl, cl, cl, cl, cl, cc, cc, cc, cc, cc, cc,
};

static
void auto_detect_byte (curbyte, do_auto_detect)
  character curbyte;
  FLAG do_auto_detect;
{
	/* begin character encoding auto-detection */
	character followbyte = curbyte;

	/* UTF-8 auto-detection */
#define dont_debug_utf_detect

#ifdef debug_utf_detect
	printf ("count_utf_bytes %d cur %02X\n", count_utf_bytes, curbyte);
#endif

	if (count_utf_bytes == 0) {
		if ((curbyte & 0xC0) == 0x80) {
			count_bad_utf ++;
		} else {
			count_utf_bytes = UTF8_len (curbyte) - 1;
			counted_utf_bytes = count_utf_bytes;
		}
	} else if ((curbyte & 0xC0) == 0x80) {
		count_utf_bytes --;
		if (count_utf_bytes == 0) {
#ifdef ignore_ambigous_utf__does_not_work
			if (counted_utf_bytes == 2
			 && strchr ("���������������", curbyte)
			   ) {
				/* ignoring some sequences that could 
				   as well be Latin-1 */
			} else {
				count_good_utf ++;
			}
#else
			count_good_utf ++;
#endif
		}
	} else {
		count_utf_bytes = 0;
		count_bad_utf ++;
	}

	/* VISCII and ISO-8859 auto-detection */
	if (curbyte >= 0x80) {

	  if (do_auto_detect) {
		count_good_viscii += cl;
		count_good_tcvn += cl;

		count_good_iso += good_iso [curbyte - 0x80];
		count_good_cp1252 += good_cp1252 [curbyte - 0x80];
		count_good_cp850 += good_cp850 [curbyte - 0x80];
		count_good_mac += good_mac [curbyte - 0x80];
		count_good_ebcdic += good_ebcdic [curbyte];
#ifdef debug_auto_detect
		printf ("%02X -> iso %ld viscii %ld\n", curbyte, count_good_iso, count_good_viscii);
#endif
	  } else {
		/* Defending ISO-8859 vs. CJK auto-detection */
		count_good_iso += good_iso [curbyte - 0x80];
	  }

	} else if (do_auto_detect) {
		switch (curbyte) {
			case '':
			case '':
			case '':
			case '':
				count_good_viscii += 2 * cl;
				count_good_tcvn += 2 * cl;
				break;

			case '':
			case '':
				count_good_viscii += 2 * cl;
				count_good_tcvn += nc;
				break;

			case '':
			case '':
			case '':
			case '':
			case '':
			case '':
			case '':
			case '':
				count_good_viscii += nc;
				count_good_tcvn += 2 * cl;
				break;

			case '\000':
			case '':
			case '':
			case '':
			case '':
			case '':
			case '':
			case '':
			case '':
			case '':
			case 0x1A:
			case '':
			case '':
			case '':
			case '':
				count_good_viscii += nc;
				count_good_tcvn += nc;
				break;
		}
	}

	/* CJK/Han encoding auto-detection */
	/* maintain GB18030 detection state */
	if (detect_gb18030 != 0) {
		detect_gb18030 --;
	}

	/* perform detection on bytes after non-ASCII bytes */
	if (last_cjkbyte >= 0x80) {
	  if (curbyte >= 0x80) {
			if (curbyte != 0xA0 || last_cjkbyte != 0xA0) {
				count_good_cjk += cl;
			}
	  } else if (curbyte < 0x30) {
			count_bad_cjk += cl;
	  } else if (curbyte <= 0x39) {
			if (detect_gb18030 == 1) {
				count_good_cjk += cl;
			}
	  } else {
			count_weak_cjk ++;
	  }

	  /* detect specific CJK encoding */
	  if (do_auto_detect && ! utf16_file) {

		/* CJK character set ranges
		GB	GBK		81-FE	40-7E, 80-FE
			GB18030		81-FE	30-39	81-FE	30-39
		Big5	Big5-HKSCS	87-FE	40-7E, A1-FE
		CNS	EUC-TW		A1-FE	A1-FE
					8E	A1-A7	A1-FE	A1-FE

		EUC-JP			8E	A1-DF
					A1-A8	A1-FE
					B0-F4	A1-FE
					8F	A2,A6,A7,A9-AB,B0-ED	A1-FE
					8F	A1-FE	A1-FE
		EUC-JIS X 0213		8E	A1-DF
					A1-FE	A1-FE
					8F	A1,A3-A5,A8,AC-AF,EE-FE	A1-FE
		Shift-JIS		A1-DF
					81-84, 87-9F		40-7E, 80-FC
					E0-EA, ED-EE, FA-FC	40-7E, 80-FC
		Shift-JIS X 0213	A1-DF
					81-9F		40-7E, 80-FC
					E0-FC		40-7E, 80-FC

		UHC	UHC		81-FE	41-5A, 61-7A, 81-FE
		Johab			84-DE	31-7E, 81-FE
					E0-F9	31-7E, 81-FE
		*/

	   if (last_cjkbyte >= 0x81 && last_cjkbyte <= 0xFE) {

		/*	Big5	Big5-HKSCS	87-FE	40-7E, A1-FE
		*/
		if (last_cjkbyte >= 0x87 /* && last_cjkbyte <= 0xFE */
		    && ((curbyte >= 0x40 && curbyte <= 0x7E)
		        || (curbyte >= 0xA1 && curbyte <= 0xFE)))
		{
			count_big5 ++;
			followbyte = 0;
		} else {
			/*count_big5 --;*/
		}

		/*	GB	GBK		81-FE	40-7E, 80-FE
				GB18030		81-FE	30-39	81-FE	30-39
			UHC	UHC		81-FE	41-5A, 61-7A, 81-FE
		*/
		/* if (last_cjkbyte >= 0x81 && last_cjkbyte <= 0xFE) { */
		    if ((curbyte >= 0x40 && curbyte <= 0x7E)
		        || (curbyte >= 0x80 && curbyte <= 0xFE)) {
			count_gb ++;
			followbyte = 0;

			if ((curbyte >= 0x41 && curbyte <= 0x5A)
			        || (curbyte >= 0x61 && curbyte <= 0x7A)
			        || (curbyte >= 0x81 /* && curbyte <= 0xFE */))
			{
				count_uhc ++;
			} else {
				count_uhc --;
			}

		    } else if (curbyte >= 0x30 && curbyte <= 0x39) {
			if (detect_gb18030 == 1) {
				count_gb += 5;
			} else {
				count_gb --;
				detect_gb18030 = 3;
			}
		    }
		/* } */

		/*	JIS	EUC-JP		A1-FE	A1-FE
						8F	A1-FE	A1-FE
						8E	A1-DF
		*/
		if (last_cjkbyte >= 0xA1 /* && last_cjkbyte <= 0xFE */
		    && curbyte >= 0xA1 && curbyte <= 0xFE)
		{
			followbyte = 0;
			if (last2_cjkbyte == 0x8F) {
				if (last_cjkbyte == 0xA1
				 || (last_cjkbyte >= 0xA3 && last_cjkbyte <= 0xA5)
				 || last_cjkbyte == 0xA8
				 || (last_cjkbyte >= 0xAC && last_cjkbyte <= 0xAF)
				 || last_cjkbyte >= 0xEE
				) {
					count_jisx += 3;
					count_jp --;
				} else {
					count_jp += 3;
					count_jisx --;
				}
			} else {
				count_jisx ++;
				if (last_cjkbyte <= 0xA8 ||
				    (last_cjkbyte >= 0xB0 && last_cjkbyte <= 0xF4)
				) {
					count_jp ++;
				} else {
					count_jp --;
				}
			}
		}
		if (last_cjkbyte == 0x8E
		    && curbyte >= 0xA1 && curbyte <= 0xDF)
		{
			followbyte = 0;
			count_jisx += 3;
			count_jp += 3;
		} else {
			count_jisx --;
			count_jp --;
		}

		/*	JIS	Shift-JIS	A1-DF
						81-9F, E0-EF	40-7E, 80-FC
		*/
		if (last_cjkbyte >= 0xA1 && last_cjkbyte <= 0xDF) {
		    if (curbyte >= 0xA1 && curbyte <= 0xDF) {
			/* two consecutive single-byte SJIS characters */
			followbyte = 0;
			count_sjis += 1;
			count_sjisx += 1;
			count_sjis1 ++;
		    } else {
			count_sjis --;
			count_sjisx --;
		    }
		} else {
		    if (curbyte >= 0x40 && curbyte <= 0xFC && curbyte != 0x7F) {
			followbyte = 0;
			count_sjisx ++;
			if (last_cjkbyte <= 0x84
			 || (last_cjkbyte >= 0x87 && last_cjkbyte <= 0x9F)
			 || (last_cjkbyte >= 0xE0 && last_cjkbyte <= 0xEA)
			 || (last_cjkbyte >= 0xED && last_cjkbyte <= 0xEE)
			 || last_cjkbyte >= 0xFA
			) {
				count_sjis ++;
			}
		    } else {
			count_sjis --;
		    }
		}

		/*	Johab	Johab		84-DE, E0-F9	31-7E, 81-FE
		*/
		if (((last_cjkbyte >= 0x84 && last_cjkbyte <= 0xDE)
		        || (last_cjkbyte >= 0xE0 && last_cjkbyte <= 0xF9))
		    && ((curbyte >= 0x31 && curbyte <= 0x7E)
		        || (curbyte >= 0x81 && curbyte <= 0xFE)))
		{
			count_johab ++;
		} else {
			count_johab --;
		}

#ifdef sjis_broken
	   } /* if (last_cjkbyte >= 0x81 && last_cjkbyte <= 0xFE) */
	     else { 
		    if (curbyte >= 0x40 && curbyte <= 0xFC && curbyte != 0x7F) {
			followbyte = 0;
			count_sjis ++;
		    } else {
			count_sjis --;
		    }
#endif
	   }
	  } /* if do_auto_detect */
	}

	/* shift CJK byte state */
	last2_cjkbyte = last_cjkbyte;
	last_cjkbyte = followbyte;

	/* end character encoding auto-detection */
}

#define dont_debug_read_error

int
get_line (fd, buffer, len, do_auto_detect)
  int fd;
  char buffer [maxLINElen];
  int * len;
  FLAG do_auto_detect;
{
  register char * cur_pos = current_bufpos;
  char * begin = buffer;
  char * fini = (char *) (buffer + maxLINElen - 2) /* leave space for '\n\0' */;
  int Ulineend_state = 0;
  character curbyte = '\0';
  int ret = FINE;
  got_lineend = lineend_NONE;

#define dont_debug_buffer_overflow
#ifdef debug_buffer_overflow
  fini = (char *) (buffer + 20) /* debug overlong line input */;
#endif

  /* read one line */
  do {	/* read one byte */
	if (cur_pos == last_bufpos) {
		if (utf16_file && consider_transform) {
		    if (next_byte >= fini_byte) {
			do {
				interrupted = False;
				if (alloc_UTF16buf () == ERRORS) {
					return ERRORS;
				}
				read_bytes = read (fd, UTF16buf, filebuflen);
/*				if (read_bytes > 0) {
					file_position += read_bytes;
				}
*/
				trace_read (("read utf16 (%d, %X, %d) %d\n", fd, UTF16buf, filebuflen, read_bytes));
			} while (
				     (read_chars == -1 && geterrno () == EINTR)
				  || (read_chars <= 0 && interrupted)
				);

			if (read_bytes <= 0) {
				read_chars = read_bytes;
				break;
			}
			next_byte = UTF16buf;
			fini_byte = & UTF16buf [read_bytes];
		    }
		    read_chars = UTF16_transform (utf16_little_endian, filebuf, filebuflen, & next_byte, fini_byte);
		    if (count_1read_op == 0) {
			if (strncmp (filebuf, "﻿", 3) == 0) {
				/* already transformed UTF-8 BOM */
				BOM = True;
			}
			count_1read_op = 1;
		    }
		} else {
		    do {
			interrupted = False;
			read_chars = read (fd, filebuf, filebuflen);
#ifdef debug_read_error
			read_chars = -1;
			errno = EIO;
#endif
/*			if (read_chars > 0) {
				file_position += read_chars;
			}
*/
			trace_read (("read (%d, %X, %d) %d\n", fd, filebuf, filebuflen, read_chars));
		    } while (
				   (read_chars == -1 && geterrno () == EINTR)
				|| (read_chars <= 0 && interrupted)
			    );

		    if (read_chars <= 0) {
			break;
		    }

		    if (ebcdic_file && consider_transform) {
			character * epoi = filebuf;
			int i;
			mapped_text = True;	/* enable lookup_encodedchar */
			for (i = 0; i < read_chars; i ++) {
				* epoi = lookup_encodedchar (* epoi);
				epoi ++;
			}
			mapped_text = False;
		    }
		}
		last_bufpos = & filebuf [read_chars];
		cur_pos = filebuf;

		if (count_1read_op == 0 && consider_transform) {
			if (! (utf8_text && utf16_file)
			    && strncmp (filebuf, "﻿", 3) == 0) {
				/* UTF-8 BOM */
				BOM = True;
			} else if (strncmp (filebuf, "\376\377", 2) == 0
				   && do_auto_detect) {
				/* big endian UTF-16 BOM */
				(void) set_text_encoding (":16", ' ', "BOM 16");
				BOM = True;
				/* strip converted UTF-8 BOM from text */
				cur_pos += 3;
			} else if (strncmp (filebuf, "\377\376", 2) == 0
				   && do_auto_detect) {
				/* little-endian UTF-16 BOM */
				(void) set_text_encoding (":61", ' ', "BOM 61");
				BOM = True;
				/* strip converted UTF-8 BOM from text */
				cur_pos += 3;
			} else if (utf8_text && utf16_file) { /* UTF-16 pre-selection */
			} else if (do_auto_detect) {
				/* UTF-16 auto-detection */
				char * sp = filebuf;
				int even_0 = 0;
				int odd_0 = 0;
				FLAG odd = False;
				while (sp < last_bufpos) {
					if (* sp ++ == '\0') {
						if (odd) {
							odd_0 ++;
						} else {
							even_0 ++;
						}
					}
					odd = ! odd;
				}
				if (even_0 > read_chars / 133
				    && even_0 > 2 * (odd_0 + 1)) {
					/* big endian UTF-16 */
					(void) set_text_encoding (":16", ' ', "detect 16");
				} else if (odd_0 > read_chars / 133
					   && odd_0 > 2 * (even_0 + 1)) {
					/* little-endian UTF-16 */
					(void) set_text_encoding (":61", ' ', "detect 61");
				}
			}

			if (utf16_file) {
				/* do_auto_detect = False; */

				/* move UTF-16 input to UTF-16 buffer */
				if (alloc_UTF16buf () == ERRORS) {
					return ERRORS;
				}
				memcpy (UTF16buf, filebuf, (unsigned int) read_chars);
				read_bytes = read_chars;
				next_byte = UTF16buf;
				fini_byte = & UTF16buf [read_bytes];

				/* transform to UTF-8 */
				read_chars = UTF16_transform (utf16_little_endian, filebuf, filebuflen, & next_byte, fini_byte);
				last_bufpos = & filebuf [read_chars];
			}
			count_1read_op = 1;
		}
	}

	/* detect if no more lines available */
	if (cur_pos == last_bufpos) {
		read_chars = 0;
		break;
	}

	curbyte = * cur_pos ++;


	auto_detect_byte (curbyte, do_auto_detect);


	/* detect lineends */

	/* NUL character handling */
	if (curbyte == '\0') {
		* buffer ++ = '\n';
		got_lineend = lineend_NUL;
		ret = NUL_LINE;
		break;
	}

	/* handle CR/CRLF lookahead */
	if (lineends_detectCR && curbyte != '\n' && buffer != begin && * (buffer - 1) == '\r') {
		* (buffer - 1) = '\n';
		got_lineend = lineend_CR;
		cur_pos --;
		count_lineend_CR ++;	/* count Mac lines */
		if (lineends_CRtoLF) {
			set_modified ();
			got_lineend = lineend_LF;
			if (lineends_LFtoCRLF) {
				got_lineend = lineend_CRLF;
			}
		}
		break;
	}
	if (curbyte == '\n' && buffer != begin && * (buffer - 1) == '\r') {
		* (buffer - 1) = '\n';
		got_lineend = lineend_CRLF;
		count_lineend_CRLF ++;	/* count MSDOS lines */
		if (lineends_CRLFtoLF) {
			set_modified ();
			got_lineend = lineend_LF;
		}
		break;
	}

	/* handle LF */
	if (curbyte == '\n') {
		* buffer ++ = '\n';
		got_lineend = lineend_LF;
		count_lineend_LF ++;	/* count Unix lines */
		if (lineends_LFtoCRLF) {
			set_modified ();
			got_lineend = lineend_CRLF;
		}
		break;
	}

	/* check multi-byte line ends (other than CRLF) */
	if ((loading && utf8_lineends && (do_auto_detect || utf8_text))
	    || (pasting && (utf8_text || (pastebuf_utf8 && ! cjk_text)))
	   ) {
		if (Ulineend_state == 0 && curbyte == (character) 0xE2) {
			Ulineend_state = 1;
		} else if (Ulineend_state == 0 && curbyte == (character) 0xC2) {
			Ulineend_state = 8;
		} else if (Ulineend_state == 8) {
			if (curbyte == (character) 0x85) {
				Ulineend_state = 9;
				/* Unicode NEXT LINE U+0085 detected */
				buffer --;
				* buffer ++ = '\n';
				if (pasting && pastebuf_utf8 && ! utf8_text) {
					got_lineend = lineend_NL1;
				} else {
					got_lineend = lineend_NL2;
				}
				break;
			} else {
				Ulineend_state = 0;
			}
		} else if (Ulineend_state > 0) {
			if (Ulineend_state == 1 && curbyte == (character) 0x80) {
				Ulineend_state = 2;
			} else if (Ulineend_state == 2 && curbyte == (character) 0xA8) {
				Ulineend_state = 3;
				/* Unicode LS U+2028 detected */
				buffer -= 2;
				* buffer ++ = '\n';
				got_lineend = lineend_LS;
				break;
			} else if (Ulineend_state == 2 && curbyte == (character) 0xA9) {
				Ulineend_state = 3;
				/* Unicode PS U+2029 detected */
				buffer -= 2;
				* buffer ++ = '\n';
				got_lineend = lineend_PS;
				break;
			} else {
				Ulineend_state = 0;
			}
		}
	}

	/* handle NL */
	if (((! do_auto_detect && loading) || pasting_encoded)
	    && (
		(ebcdic_file && curbyte == 0x85)
		|| (! ebcdic_file && ! utf16_file && ! no_char (code_NL) && curbyte == code_NL)
	       )
	   ) {
		* buffer ++ = '\n';
		got_lineend = lineend_NL1;
		count_lineend_NL ++;	/* count ISO 8859 lines */
		if (lineends_CRtoLF) {
			set_modified ();
			got_lineend = lineend_LF;
		}
		break;
	}


	/* handle if line buffer full */
	if (buffer > fini - 6 && * cur_pos != '\n') {
	    /* try not to split within a multi-byte character sequence */
	    if (buffer == fini ||	/* last chance to split! */
		(cjk_text && (! do_auto_detect || pasting)
		? charbegin (begin, buffer) == buffer
		: (* cur_pos & 0xC0) != 0x80
		))
	    {
		* buffer ++ = '\n';
		got_lineend = lineend_NONE;
		ret = SPLIT_LINE;
		break;
	    }
	}


	/* copy byte from input buffer into line buffer */
	* buffer ++ = curbyte;

  } while (1);

  current_bufpos = cur_pos;


  /* handle errors and EOF */
  if (read_chars < 0) {
	return ERRORS;
  } else if (read_chars == 0) {
    if (buffer == begin) {
	return NO_INPUT;
    } else {
	/* consider incomplete UTF-8 sequence for auto-detection */
	if (count_utf_bytes > 0) {
		count_bad_utf ++;
	}

	if (lineends_detectCR && curbyte == '\r') {
		* (buffer - 1) = '\n';
		got_lineend = lineend_CR;
		count_lineend_CR ++;	/* count Mac lines */
		if (lineends_CRtoLF) {
			set_modified ();
			got_lineend = lineend_LF;
			if (lineends_LFtoCRLF) {
				got_lineend = lineend_CRLF;
			}
		}
	} else {
		if (loading) {
			/* Add '\n' to last line of file, for internal handling */
			* buffer ++ = '\n';
		}
		got_lineend = lineend_NONE;
		ret = NO_LINE;
	}
    }
  }

  * buffer = '\0';
  * len = (int) (buffer - begin);
  return ret;
}


/*======================================================================*\
|*			File position handling				*|
\*======================================================================*/

#ifdef vms
#define info_fn		".$mined"
#else
#define info_fn		".@mined"
#endif
#define info_dosfn	"@MINED~1"
#define mark_fn		"@mined.mar"

static char * mark_file_out;


/*
 * get_open_pos and save_open_pos look up and save the current file 
   position in file info file
   For save_open_pos, line_number must be up-to-date
 */
static
void
escape_filename (fn_escaped, fn)
  char * fn_escaped;
  char * fn;
{
  char * ipoi = fn;
  char * opoi = fn_escaped;
  while (* ipoi) {
	if (* ipoi == '\n') {
		* opoi ++ = '\\';
		* opoi ++ = 'n';
	} else {
		if (* ipoi == '\\' || * ipoi == ' ') {
			* opoi ++ = '\\';
		}
		* opoi ++ = * ipoi;
	}
	ipoi ++;
  }
  * opoi ++ = ' ';
  * opoi = '\0';
}

static
void
get_open_pos (fn)
  char * fn;
{
  char * mark_file_in;
  int mark_fd = -1;
  FLAG use_unix_fn = True;
#ifdef msdos
  if (! is_Windows ()) {
	use_unix_fn = False;
  }
#endif

  if (use_unix_fn) {
	mark_file_in = info_fn;
	mark_fd = open (mark_file_in, O_RDONLY | O_BINARY, 0);
  }
  if (mark_fd < 0) {
	mark_file_in = info_dosfn;
	mark_fd = open (mark_file_in, O_RDONLY | O_BINARY, 0);
  }
  if (mark_fd < 0) {
	mark_file_in = mark_fn;
	mark_fd = open (mark_file_in, O_RDONLY | O_BINARY, 0);
  }

  if (mark_fd >= 0) {
	FLAG modif = modified;
	int dumlen;
	char fn_escaped [maxFILENAMElen * 2 + 1];
	int fnlen;
	escape_filename (fn_escaped, fn);
	fnlen = strlen (fn_escaped);

	reset_get_line (False);

	while (line_gotten (get_line (mark_fd, text_buffer, & dumlen, False))) {
	    if (strncmp (fn_escaped, text_buffer, fnlen) == 0) {
		char * spoi = text_buffer + fnlen;
		int v4, v5, v6 = -1;
		int vq = -1;
		int vt = -1;
		int vtabexp = -1;
		open_linum = -1;
		open_col = 0;
		open_pos = 0;
		lines_per_page = 0;
		spoi = scan_int (spoi, & open_linum);
		spoi = scan_int (spoi, & open_col);
		if (open_col < 0) {
			/* indicates new character index semantics */
			open_pos = - open_col;
		}
		spoi = scan_int (spoi, & lines_per_page);
		spoi = scan_int (spoi, & v4);
		if (v4 >= 0) {
			JUSlevel = 1;
			spoi = scan_int (spoi, & v5);
			spoi = scan_int (spoi, & v6);
			if (v6 > 0) {
				first_left_margin = v4;
				next_left_margin = v5;
				right_margin = v6;
			}
		} else {
			JUSlevel = 0;
		}

		/* get quote type */
		spoi = scan_int (spoi, & vq);
		if (vq >= 0) {
		    if (smart_quotes != VALID) {
			/* French spacing quote style and legacy int entries */
			if (vq == 70) {
				set_quote_style ("« »");
			} else if (vq == 1) {
				set_quote_style ("“” ‘’");
			} else if (vq == 2) {
				set_quote_style ("„“ ‚‘");
			} else if (vq == 3) {
				set_quote_style ("«» ‹›");
			} else if (vq == 4) {
				set_quote_style ("»« ›‹");
			} else if (vq == 5) {
				set_quote_style ("„” ‚’");
			} else if (vq == 6) {
				set_quote_style ("”” ’’");
			} else if (vq == 7) {
				set_quote_style ("»» ››");
			} else if (vq == 8) {
				set_quote_style ("『』 「」");
			} else {
				set_quote_type (default_quote_type);
			}
		    }
		} else {
			/* string entries */
			char qs [maxMSGlen];
			char * qpoi = qs;
			/* skip leading space */
			while (* spoi == ' ') {
				spoi ++;
			}
			/* scan quote style indication */
#define scan_French_quote_style
#ifdef scan_French_quote_style
			qpoi = spoi;
			advance_utf8 (& spoi);
			if (* spoi == ' ') {
				spoi ++;
			}
			advance_utf8 (& spoi);
			if (* spoi == ' ') {
				spoi ++;
			}
			advance_utf8 (& spoi);
			if (* spoi == ' ') {
				spoi ++;
			}
			advance_utf8 (& spoi);
			* spoi ++ = '\0';
			if (smart_quotes != VALID) {
				set_quote_style (qpoi);
			}
#else
			while ((character) * spoi > ' ') {
				* qpoi ++ = * spoi ++;
			}
			if (* spoi == ' ') {
				* qpoi ++ = * spoi ++;
			}
			while ((character) * spoi > ' ') {
				* qpoi ++ = * spoi ++;
			}
			* qpoi = '\0';
			if (smart_quotes != VALID) {
				set_quote_style (qs);
			}
#endif
		}

		/* get tab size */
		spoi = scan_int (spoi, & vt);
		if (vt >= 0 && ! tabsize_selected) {
			tabsize = vt;
		}

		/* get keyboard mapping */
		/* skip leading space */
		while (* spoi == ' ') {
			spoi ++;
		}
		if ((character) * spoi > ' ' && ! explicit_keymap) {
			if (* spoi == '-' && * (spoi + 1) == '-') {
				spoi += 2;
			}
			setKEYMAP (spoi);
		}
		/* skip field */
		while ((character) * spoi > ' ') {
			spoi ++;
		}

		/* get tab/space expand flag */
		spoi = scan_int (spoi, & vtabexp);
		if (vtabexp >= 0 && ! tabsize_selected) {
			if (vtabexp > 0) {
				expand_tabs = True;
			} else {
				expand_tabs = False;
			}
		}
	    }
	}
	(void) close (mark_fd);
	clear_filebuf ();

	/* prevent affecting the modified flag when loading the line number file */
	modified = modif;
  }
}

/**
   Write file position and other information to file info file.
   Return False if writing to the file fails.
 */
static
FLAG
write_open_pos (fd, fn)
  int fd;
  char * fn;
{
	int cur_pos = get_cur_pos ();
	char marktext [maxPROMPTlen];
	char * quote_style_marker;

#ifdef scan_French_quote_style
	quote_style_marker = quote_mark (quote_type, 0);
#else
	if (spacing_quote_type (quote_type)) {
		/* French quote style */
		quote_style_marker = "70";	/* ASCII 'F' */
	} else {
		quote_style_marker = quote_mark (quote_type, 0);
	}
#endif

	if (JUSlevel > 0) {
	    build_string (marktext, "%s%d %d %d %d %d %d %s %d %s-%s %d\n",
			  fn, line_number, - cur_pos, lines_per_page, 
			  first_left_margin, next_left_margin, right_margin, 
			  quote_style_marker, 
			  tabsize, 
			  keyboard_mapping, last_keyboard_mapping, 
			  expand_tabs);
	} else {
	    build_string (marktext, "%s%d %d %d -3 %s %d %s-%s %d\n",
			  fn, line_number, - cur_pos, lines_per_page, 
			  quote_style_marker, 
			  tabsize, 
			  keyboard_mapping, last_keyboard_mapping, 
			  expand_tabs);
	}
	if (write (fd, marktext, strlen (marktext)) <= 0) {
		return False;
	} else {
		return True;
	}
}

#ifdef old_marker_creation
FLAG groom_info_files = True;	/* groom once per dir */
FLAG groom_info_file = True;	/* groom once ... */
#else
FLAG groom_info_files = False;	/* groom once per dir */
FLAG groom_info_file = False;	/* groom once ... */
#endif
FLAG groom_stat = False;	/* stat files for grooming */

struct marker_entry {
	char * fn;
	char * info;
	struct marker_entry * next;
};

static struct marker_entry * marker_list = 0;

static
FLAG
put_marker_list (mlpoi, fn, info)
  struct marker_entry * * mlpoi;
  char * fn;
  char * info;
{
	if (! * mlpoi) {
		struct marker_entry * new = alloc (sizeof (struct marker_entry));
		if (new) {
			new->fn = dupstr (fn);
			new->info = dupstr (info);
			if (new->fn && new->info) {
				/* append new node */
				new->next = * mlpoi;
				* mlpoi = new;
				return True;
			}
		}
	} else if (streq ((* mlpoi)->fn, fn)) {
		char * _info = dupstr (info);
		if (_info) {
			/* update node with info */
			free_space ((* mlpoi)->info);
			(* mlpoi)->info = _info;
			return True;
		}
	} else {
		return put_marker_list (& (* mlpoi)->next, fn, info);
	}
	return False;
}

static
FLAG
write_marker_list (fd, ml)
  int fd;
  struct marker_entry * ml;
{
#ifndef VAXC
	FLAG ret = True;
	while (ml) {
		struct stat fstat_buf;
		if (! groom_stat || stat (ml->fn, & fstat_buf) == 0) {
			if (write (fd, ml->fn, strlen (ml->fn)) < 0) {
				ret = False;
			}
			if (write (fd, ml->info, strlen (ml->info)) < 0) {
				ret = False;
			}
		}
		ml = ml->next;
	}
	return ret;
#else
	return True;
#endif
}

static
void
clear_marker_list (mlpoi)
  struct marker_entry * * mlpoi;
{
	if (* mlpoi) {
		clear_marker_list (& (* mlpoi)->next);
		free_space ((* mlpoi)->fn);
		free_space ((* mlpoi)->info);
		free_space (* mlpoi);
		* mlpoi = 0;
	}
}

/**
   Update file position and other information in file info file, 
   reading all info and writing back.
   Housekeeping: clean up duplicate entries,
   also (if groom_stat) drop non-existing files.
   Return False if saving to the file fails.
 */
static
FLAG
rewrite_open_pos (fn, force)
  char * fn;
  int force;
{
  char * mark_file_in;
  int mark_fd = -1;
  FLAG ret = True;
  FLAG use_unix_fn = True;
#ifdef msdos
  if (! is_Windows ()) {
	use_unix_fn = False;
  }
#endif

  clear_marker_list (& marker_list);	/* in case of previous error */

  if (use_unix_fn) {
	mark_file_in = info_fn;
	mark_fd = open (mark_file_in, O_RDONLY);
	mark_file_out = info_fn;
  } else {
	mark_file_out = info_dosfn;
  }
  if (mark_fd < 0) {
	mark_file_in = info_dosfn;
	mark_fd = open (mark_file_in, O_RDONLY);
  }
  if (mark_fd < 0) {
	mark_file_in = mark_fn;
	mark_fd = open (mark_file_in, O_RDONLY);
  }

  if (mark_fd < 0 && ! force) {
	return False;
  }

  /* read file info */
  if (mark_fd >= 0) {
	/* scan mark file for old entries for same file and skip them;
	   copy other entries to output file
	 */
	int dumlen;
	int fnlen = strlen (fn);
	FLAG memerr = False;
	int ret;

	reset_get_line (False);	/* disable UTF-16 detection */
	while (line_gotten (ret = get_line (mark_fd, text_buffer, & dumlen, False))) {
		if (strncmp (fn, text_buffer, fnlen) == 0) {
			/* skip entry for this filename */
		} else {
			/* scan file name */
			char * mf = text_buffer;
			char fn_buffer [maxFILENAMElen];
			char * fnpoi = fn_buffer;
			int duml = -1;
			while (* mf && * mf != '\n' && * mf != ' ') {
				if (* mf == '\\') {
					mf ++;
				}
				* fnpoi ++ = * mf ++;
			}
			* fnpoi = '\0';
			/* put marker file line into list
				fn_buffer: file name (unescaped)
				mf: remainder of info line
			 */
			(void) scan_int (mf, & duml);
			if (duml >= 0) {
				if (! put_marker_list (& marker_list, fn_buffer, mf)) {
					memerr = True;
					break;
				}
			}
		}
	}
	(void) close (mark_fd);
	clear_filebuf ();
	if (memerr || ret == ERRORS) {
		return False;
	}
  }

  /* migrate to new info file, i.e. delete the old one if different */
  if (mark_fd >= 0 && mark_file_out != mark_file_in) {
	if (delete_file (mark_file_in) < 0) {
		/* deletion failed, stay with old file */
		mark_file_out = mark_file_in;
	}
  }

  mark_fd = open (mark_file_out, O_WRONLY | O_TRUNC | O_BINARY | O_CREAT, fprot0);
  if (mark_fd < 0) {
	return False;
  }

  /* write file info */
  ret = write_marker_list (mark_fd, marker_list);
  clear_marker_list (& marker_list);

  groom_stat = groom_info_file;	/* file checking once */
  groom_info_file = False;	/* ... suppress next time */

  if (* fn == ' ') {
	/* dummy file name, grooming only, skip writing new entry */
  } else {
	ret &= write_open_pos (mark_fd, fn);
  }
  if (close (mark_fd) < 0) {
	ret = False;
  }
  return ret;
}


/**
   Save file position and other information in file info file.
   Return False if saving to the file fails.
 */
static
FLAG
save_open_pos (fn, force)
  char * fn;
  int force;
{
  if (fn [0] != '\0') {
	char fn_escaped [maxFILENAMElen * 2 + 1];
	escape_filename (fn_escaped, fn);
	return rewrite_open_pos (fn_escaped, force);
  } else {
	return True;
  }
}


/*
 * Save current file pos
 */
void
SAVPOS ()
{
  if (file_name [0] != '\0') {
	fstatus ("Remembering file position", -1L, -1L);
	if (save_open_pos (file_name, 1) == False) {
		error2 ("Error when saving file position to ", mark_file_out);
	}
  }
}

void
GROOM_INFO ()
{
  (void) rewrite_open_pos (" ", True);
}


/*======================================================================*\
|*			Determine derived file names			*|
\*======================================================================*/

static
char *
get_directory (dir)
  char * dir;
{
  if (is_absolute_path (dir)) {
	return dir;
  } else {
	if (mkdir (dir, 0700) == 0 || geterrno () == EEXIST) {
		/* local directory OK */
		return dir;
	} else {
		return ".";
	}
  }
}

static
char *
get_recovery_name (fn)
  char * fn;
{
#ifdef vms
#define recovery_tag "$"
#else
#define recovery_tag "#"
#endif

  static char rn [maxFILENAMElen];
  char * dirname;
  char * bn = getbasename (fn);

  /* "dir/name" -> "${AUTO_SAVE_DIRECTORY:-dir}", bn "name" */
  if (recover_directory) {
	dirname = get_directory (recover_directory);
  } else if (bn == fn) {
	dirname = ".";
  } else {	/* copy and clip dir name from path name */
	char * fini = rn + (bn - fn) - 1;
	strcpy (rn, fn);
	dirname = rn;
#ifdef vms
	if (* fini == ']' || * fini == '/' || * fini == ':') {
		fini ++;
	} else {	/* just in case */
		fini ++;
		* fini = ':';
		fini ++;
	}
#endif
	* fini = '\0';
  }

  /* compose recovery dir and file name */
  if (streq (dirname, ".")) {
#ifdef vms
	/* force $...$ to refer to local file 
	   in case it's a logical name
	 */
	strcpy (rn, "[]");
#else
	strcpy (rn, "");
#endif
  } else {
	if (dirname != rn) {
		strcpy (rn, dirname);
	}
#ifndef vms
	strappend (rn, "/", maxFILENAMElen);
#endif
  }
  strappend (rn, recovery_tag, maxFILENAMElen);
  strappend (rn, bn, maxFILENAMElen);
  strappend (rn, recovery_tag, maxFILENAMElen);
  return rn;
}

#ifndef vms

static
char *
get_backup_name (file_name)
  char * file_name;
{
  static char backup_name [maxFILENAMElen];
  char * dirname;
  char * prefix;
  char newsuffix [30];

  if (! backup_mode) {
	return NIL_PTR;
  }

  /* "dir/name" -> "${BACKUP_DIRECTORY:-dir}", prefix "name" */
  prefix = getbasename (file_name);
  if (backup_directory) {
	dirname = get_directory (backup_directory);
  } else if (prefix == file_name) {
	dirname = ".";
  } else {	/* copy and clip dir name from path name */
	strcpy (backup_name, file_name);
	dirname = backup_name;
	dirname [prefix - file_name - 1] = '\0';
  }

  if (backup_mode == 's') {	/* simple */
	strcpy (newsuffix, "~");
  } else {
	DIR * dir;
	struct dirent * direntry;

	int ver_e = 0;
	int ver_v = 0;
	int maxver;

	dir = opendir (dirname);
	if (! dir) {
		error2 ("Cannot open directory ", dirname);
		return NIL_PTR;
	}

	while ((direntry = readdir (dir)) != 0) {
	    if (strisprefix (prefix, direntry->d_name)) {
		char * suffix = direntry->d_name + strlen (prefix);
		int ver = -1;
		char * afterver;
		if (streq (suffix, "~")) {
			/* simple backup file */
		} else if (* suffix == ';') {
			/* check VMS style numbered backup file */
			suffix ++;
			afterver = scan_int (suffix, & ver);
			if (ver > 0 && * afterver == '\0') {
				/* VMS style numbered backup file */
				if (ver > ver_v) {
					ver_v = ver;
				}
			}
		} else if (* suffix == '.' && * (suffix + 1) == '~') {
			/* check emacs style numbered backup file */
			suffix += 2;
			afterver = scan_int (suffix, & ver);
			if (ver > 0 && * afterver == '~' && * (afterver + 1) == '\0') {
				/* emacs style numbered backup file */
				if (ver > ver_e) {
					ver_e = ver;
				}
			}
		}
	    }
	}
	closedir (dir);
	maxver = ver_e > ver_v ? ver_e : ver_v;

	if (backup_mode == 'v' || (ver_v > ver_e && backup_mode != 'e')) {
		/* VMS style backup filename */
		build_string (newsuffix, ";%d", maxver + 1);
	} else if (backup_mode == 'e' || backup_mode == 'n' || ver_e > 0) {
		/* emacs style backup filename */
		build_string (newsuffix, ".~%d~", maxver + 1);
	} else {	/* backup_mode == 'a' going simple mode */
		/* simple backup filename */
		strcpy (newsuffix, "~");
	}
  }

  /* compose backup dir and file name */
  if (streq (dirname, ".")) {
	strcpy (backup_name, "");
  } else {
	if (dirname != backup_name) {
		strcpy (backup_name, dirname);
	}
	strappend (backup_name, "/", maxFILENAMElen);
  }
  strappend (backup_name, prefix, maxFILENAMElen);

  /* append backup suffix */
  if (strlen (backup_name) + strlen (newsuffix) >= sizeof (backup_name)) {
	return NIL_PTR;
  } else {
	strcat (backup_name, newsuffix);
	return backup_name;
  }
}

#endif

#ifdef use_locking

static
char *
get_lockfile_name (fn)
  char * fn;
{
  static char lf [maxFILENAMElen];
  char * bn = getbasename (fn);

  /* "dir/name" -> "dir", bn "name" */
  if (bn == fn) {
#ifdef vms
	strcpy (lf, "$$");	/* not used anyway */
#else
	strcpy (lf, ".#");
#endif
  } else {	/* copy and clip dir name from path name */
	strcpy (lf, fn);
#ifdef vms
#error fix this, see get_recovery_name
#endif
	lf [bn - fn - 1] = '\0';
#ifdef vms
	strappend (lf, "$$", maxFILENAMElen);	/* not used anyway */
#else
	strappend (lf, "/.#", maxFILENAMElen);
#endif
  }
  strappend (lf, bn, maxFILENAMElen);

  return lf;
}

#endif


/*======================================================================*\
|*			Handle file locking				*|
\*======================================================================*/

#ifdef use_locking

static
int
getsymboliclink (name, target, size)
  char * name;
  char * target;
  int size;
{
  int ret = readlink (name, target, size - 1);
  if (ret >= 0) {
	target [ret] = '\0';
  } else {
	/* try to read cygwin pseudo symbolic link;
	   on network file systems, cygwin is likely to create a text file 
	   rather than a symbolic link
	 */
	int fd = open (name, O_RDONLY | O_BINARY, 0);
	int rd;
	char linkbuf [maxLINElen];
	if (fd < 0) {
		return fd;
	}
	rd = read (fd, linkbuf, sizeof (linkbuf));
	if (rd > 0 && strisprefix ("!<symlink>��", linkbuf)) {
		char * linkpoi = linkbuf + 12;
		char * linkend = linkbuf + rd;
		clear_UTF16_transform ();
		rd = UTF16_transform (True, target, size, & linkpoi, linkend);
		target [rd] = '\0';
		ret = rd;
	} else if (rd == 0) {
		/* indicate empty pseudo-link (plain file) */
		ret = 0;
	} else {
		ret = -1;
	}
	(void) close (fd);
  }
  return ret;
}

static
void
setlocktarget (target)
  char * target;
{
  char hn [maxFILENAMElen];
  if (gethostname (hn, sizeof (hn)) == 0) {
	hn [sizeof (hn) - 1] = '\0';
  } else {
	strcpy (hn, "?");
  }
  build_string (target, "%s@%s.%d", getusername (), hn, (int) getpid ());
}

#endif

/**
   Don't modify? - check whether file is view-only or locked
 */
FLAG
dont_modify ()
{
#ifndef use_locking
  if (viewonly) {
	viewonlyerr ();
	return True;
  } else {
	return False;
  }
#else
  if (viewonly) {
	viewonlyerr ();
	return True;
  } else if (file_locked != False) {
	/* True (locked myself) or NOT_VALID (ignored / file readonly) */
	return False;
  } else if (file_name [0] == '\0') {
	return False;
  } else if (writable == False) {
	file_locked = NOT_VALID;
	return False;
  } else {
	char * lf = get_lockfile_name (file_name);
	char target [maxFILENAMElen];
	int ret = getsymboliclink (lf, target, sizeof (target));
	if (ret > 0) {
		/* non-empty lock file found: notify and set viewonly */
		char * dot;
		char msg [maxMSGlen];

		dot = strchr (target, '.');
		if (dot) {
			/* shorten lock info display, keep unlock hint visible */
			* dot = '\0';
		}
		viewonly_locked = True;
#ifdef delay_flags
		flags_changed = True;
#else
		displayflags ();
#endif

		build_string (msg, "Notice: File is locked by %s; Unlock from File menu", target);
		status_fmt2 ("", msg);
		sleep (2);	/* give time to see the notice */

		return True;
	} else if (ret == 0) {
		/* empty pseudo (plain) lock file found; 
		   assume unlock workaround */
		file_locked = NOT_VALID;	/* ? */
		return False;
	} else {
		/* no lock file, create one */
		setlocktarget (target);
		if (symlink (target, lf) == 0) {
			file_locked = True;
		} else {
			int err = geterrno ();
			if (err == EEXIST) {
				/* race condition after readlink () */
				viewonly_locked = True;
				flags_changed = True;

				status_fmt2 ("", "Notice: File has just been locked by someone; Unlock from File menu");
				sleep (2);	/* time to see notice */
				return True;
			} else if (0
#ifdef ENOSYS
				|| err == ENOSYS	/* not on VMS */
#endif
#ifdef EOPNOTSUPP
				|| err == EOPNOTSUPP	/* ? */
#endif
#ifdef ENOTSUP
				|| err == ENOTSUP	/* ? */
#endif
				) {
				/* file system does not support symbolic links */
				file_locked = NOT_VALID;
			} else {
				error ("Cannot lock file");
				/* no sleep() here, editing effect would be postponed */
				file_locked = NOT_VALID; /* ? */
			}
		}

		return False;
	}
  }
#endif
}

#ifdef use_locking
static
void
delete_lockfile (lf)
  char * lf;
{
  if (delete_file (lf) < 0) {
	/* if the link cannot be deleted although created by mined before 
	   (unless if grabbing lock), that is likely due to weird 
	   network file system configuration and the link is likely not 
	   even a symbolic link but rather a plain file;
	   check whether it's indeed a plain file and make it empty 
	   to indicate 'unlocked'
	 */
	char target [maxFILENAMElen];
	if (readlink (lf, target, sizeof (target) - 1) < 0) {
		(void) truncate (lf, 0);
	}
  }
}
#endif

void
unlock_file ()
{
#ifdef use_locking
  if (file_locked == True) {
	char * lf = get_lockfile_name (file_name);
	char target [maxFILENAMElen];
	if (getsymboliclink (lf, target, sizeof (target)) >= 0) {
		char mylocktext [maxFILENAMElen];
		setlocktarget (mylocktext);
		if (streq (target, mylocktext)) {
			delete_lockfile (lf);
		} else {
			/* don't delete if grabbed by somebody else */
		}
#ifdef __CYGWIN__force_unlock_workaround
	} else {
		/* now obsolete by generic handling in getsymboliclink */
		/* on nfs file systems, cygwin is likely to create a 
		   text file rather than a symbolic link;
		   delete anyway, as it is probably "my lock" 
		   (file_locked == True)
		 */
		delete_lockfile (lf);
#endif
	}
  }
#endif
  file_locked = False;
}

void
relock_file ()
{
  loaded_from_filename = False;
  if (modified) {
	(void) dont_modify ();
  }
}


/**
   Grab file lock by other user.
   (emacs, joe: "steal")
 */
void
grab_lock ()
{
#ifdef use_locking
  char * lf = get_lockfile_name (file_name);
  char target [maxFILENAMElen];
  if (getsymboliclink (lf, target, sizeof (target)) >= 0) {
	/* it's really a lock file, maybe a pseudo (plain) lock file, 
	   or at least empty */
	delete_lockfile (lf);
  }
#endif
  file_locked = False;
  viewonly_locked = False;
}

/**
   Ignore file lock by other user.
   (emacs: "proceed", joe: "edit anyway")
 */
void
ignore_lock ()
{
  file_locked = NOT_VALID;
  viewonly_locked = False;
}


/*======================================================================*\
|*			Time check debugging				*|
\*======================================================================*/

#define dont_debug_check_modtime

#ifdef debug_check_modtime
#undef dont_check_modtime

#include <time.h>

static
void
trace_modtime (timepoi, tag, fn)
  time_t * timepoi;
  char * tag;
  char * fn;
{
  char timbuf [99];
  strftime (timbuf, sizeof (timbuf), "%G-%m-%d %H:%M:%S", localtime (timepoi));
  printf ("%s (%s) %s\n", timbuf, tag, fn);
  debuglog (tag, timbuf, fn);
}

#else
#define trace_modtime(timepoi, tag, fn)	
#endif


/*======================================================================*\
|*			File operations					*|
\*======================================================================*/

#define dont_debug_filter

static
int
is_dev (fn, fsbufpoi)
  char * fn;
  struct stat * fsbufpoi;
{
#ifndef VAXC
  if (S_ISCHR (fsbufpoi->st_mode) || S_ISBLK (fsbufpoi->st_mode)) {
#ifdef __CYGWIN__
	if (streq (fn, "/dev/clipboard")) {
		return 0;
	}
#endif
	return 1;
  }
#endif
  return 0;
}

static
FLAG
file_changed (fn, fstatpoi)
  char * fn;
  struct stat * fstatpoi;
{
  if (filestat.st_mtime) {
	if (fstatpoi->st_mtime != filestat.st_mtime
		|| fstatpoi->st_size != filestat.st_size
#ifndef vms
		|| fstatpoi->st_dev != filestat.st_dev
		|| fstatpoi->st_ino != filestat.st_ino
#endif
	   ) {
		/*overwriteOK = False;	will ask anyway; this would toggle wrong question */
#ifdef flush_writable
		if (access (fn, W_OK) >= 0) {
			writable = True;
		} else {
			writable = False;
		}
#else
		writable = True;	/* may try on new or modified file */
#endif

		set_modified ();
		relock_file ();

#ifndef vms
		if (fstatpoi->st_dev != filestat.st_dev || fstatpoi->st_ino != filestat.st_ino) {
			return NOT_VALID;	/* file replaced */
		}
#endif
		return True;	/* file modified */
	} else {
		return False;	/* file unchanged */
	}
  } else {
	/* file appeared (if it did not exist before) */
	return VALID;
  }
}


static
void
check_recovery_file (delay_msg)
  FLAG delay_msg;
{
#ifndef VAXC
  char * recovery_fn = get_recovery_name (file_name);
  struct stat fstat_buf;
  int statres = stat (recovery_fn, & fstat_buf);

  /* does a recovery file exist and is it newer? */
  if (statres == 0 && fstat_buf.st_mtime > filestat.st_mtime) {
	recovery_exists = True;
	if (delay_msg) {
		sleep (2) /* give time to see read error msg */;
	}
	status_fmt2 ("", "Notice: A newer recovery file exists; Recover from File menu");
	sleep (2);	/* give time to see the notice */
  }
#endif
}

static
void
update_file_name (newname, update_display, addtolist)
  char * newname;
  FLAG update_display;
  FLAG addtolist;
{
  loaded_from_filename = False;

  /* Save file name */
  strncpy (file_name, newname, maxFILENAMElen);
  file_name [maxFILENAMElen - 1] = '\0';

  if (addtolist) {
	filelist_add (dupstr (file_name), False);
  }

  /* configure file name specific preferences */
  configure_preferences (False); /* restoring initial preferences first */
  /* fix yet unclear filename suffix-specific options... */
  if (update_display) {	/* after writing */
	set_file_type_flags ();
  }

  if (update_display) {
	/* fix syntax state in case highlighting was switched on */
	if (mark_HTML /* && not before... */) {
		update_syntax_state (header->next);
	}

	/* could check whether a display-relevant option has been changed 
	   by configure_preferences above, e.g. hide_passwords or tabsize 
	   or ... - but better go the safe way and always:
	 */
	RDwin ();
  }
}


/**
   called before a file is loaded.
   frees allocated space (linked list), initializes header/tail pointer
 */
static
void
clear_textbuf ()
{
  register LINE * line;
  register LINE * next_line;

/* Delete the whole list */
  for (line = header->next; line != tail; line = next_line) {
	next_line = line->next;
	if (line->allocated) {
		free_space (line->text);
		free_header (line);
	}
  }

/* header and tail should point to itself */
  line->next = line->prev = line;
}


static char prev_encoding_tag;
static char * prev_text_encoding;
static long nr_of_bytes = 0L;
static long nr_of_chars = 0L;
static long nr_of_utfchars = 0L;
static long nr_of_cjkchars = 0L;

static
void
encoding_auto_detection (empty, do_auto_detect)
  FLAG empty;
  FLAG do_auto_detect;
{
  /* auto-detection stuff */

#ifdef debug_auto_detect
	printf ("good CJK %ld, weak CJK %ld, bad CJK %ld, good UTF %ld, bad UTF %ld\n", 
		count_good_cjk, count_weak_cjk, count_bad_cjk, count_good_utf, count_bad_utf);
	printf ("iso %ld, cp1252 %ld, cp850 %ld, mac %ld, viscii %ld, tcvn %ld\n", 
		count_good_iso, count_good_cp1252, count_good_cp850, count_good_mac, count_good_viscii, count_good_tcvn);
	printf ("big5 %ld, gb %ld, jp %ld, jis x %ld, sjis %ld, sjis x %ld, uhc %ld, johab %ld\n", 
		count_big5, count_gb, count_jp, count_jisx, count_sjis, count_sjisx, count_uhc, count_johab);
#endif

  /* consider line-end types */
  count_good_cp1252 += 5 * count_lineend_CRLF;
  count_good_cp850 += 5 * count_lineend_CRLF;
  count_good_mac += 5 * count_lineend_CR;
  count_good_iso += count_lineend_LF;
  count_good_ebcdic += 5 * count_lineend_NL;
  count_sjis += 12 * count_lineend_CRLF;
  count_sjisx += 12 * count_lineend_CRLF;

  /* heuristic adjustments */
/*
	count_good_viscii *= 0.9;
	count_big5 *= 2;
	count_uhc *= 2;
	count_jp *= 3;
	count_jisx *= 3;
	count_sjis *= 1.5;
	count_sjisx *= 1.5;
*/
	count_sjis += count_sjis1 / 2;
	count_sjisx += count_sjis1 / 2;

#ifdef debug_auto_detect
	printf ("-> iso %ld, cp1252 %ld, cp850 %ld, mac %ld, viscii %ld, tcvn %ld\n", 
		count_good_iso, count_good_cp1252, count_good_cp850, count_good_mac, count_good_viscii, count_good_tcvn);
	printf ("-> big5 %ld, gb %ld, jp %ld, jis x %ld, sjis %ld, sjis x %ld, uhc %ld, johab %ld\n", 
		count_big5, count_gb, count_jp, count_jisx, count_sjis, count_sjisx, count_uhc, count_johab);
#endif

  /* remember text encoding before auto-detection (for CJK char counting) */
  prev_encoding_tag = text_encoding_tag;

  /* filter out encodings that shall not be auto-detected */
  if (detect_encodings != NIL_PTR && * detect_encodings != '\0') {
	if (strchr (detect_encodings, 'G') == NIL_PTR) {
		count_gb = 0;
	}
	if (strchr (detect_encodings, 'B') == NIL_PTR) {
		count_big5 = 0;
	}
	if (strchr (detect_encodings, 'K') == NIL_PTR) {
		count_uhc = 0;
	}
	if (strchr (detect_encodings, 'J') == NIL_PTR) {
		count_jp = 0;
	}
	if (strchr (detect_encodings, 'X') == NIL_PTR) {
		count_jisx = 0;
	}
	if (strchr (detect_encodings, 'S') == NIL_PTR) {
		count_sjis = 0;
		count_sjis1 = 0;
	}
	if (strchr (detect_encodings, 'x') == NIL_PTR) {
		count_sjisx = 0;
	}
	if (strchr (detect_encodings, 'C') == NIL_PTR) {
		count_cns = 0;
	}
	if (strchr (detect_encodings, 'H') == NIL_PTR) {
		count_johab = 0;
	}

	if (strpbrk (detect_encodings, "V8") == NIL_PTR) {
		count_good_viscii = 0;
	}
	if (strpbrk (detect_encodings, "N8") == NIL_PTR) {
		count_good_tcvn = 0;
	}

	if (strpbrk (detect_encodings, "L8") == NIL_PTR) {
		count_good_iso = 0;
	}
	if (strpbrk (detect_encodings, "W8") == NIL_PTR) {
		count_good_cp1252 = 0;
	}
	if (strpbrk (detect_encodings, "P8") == NIL_PTR) {
		count_good_cp850 = 0;
	}
	if (strpbrk (detect_encodings, "M8") == NIL_PTR) {
		count_good_mac = 0;
	}
	if (strpbrk (detect_encodings, "E") == NIL_PTR) {
		count_good_ebcdic = 0;
	}
#ifdef debug_auto_detect
	printf ("!> iso %ld, cp1252 %ld, cp850 %ld, mac %ld, viscii %ld, tcvn %ld\n", 
		count_good_iso, count_good_cp1252, count_good_cp850, count_good_mac, count_good_viscii, count_good_tcvn);
	printf ("!> big5 %ld, gb %ld, jp %ld, jis x %ld, sjis %ld, sjis x %ld, uhc %ld, johab %ld\n", 
		count_big5, count_gb, count_jp, count_jisx, count_sjis, count_sjisx, count_uhc, count_johab);
#endif
	/* disable EBCDIC auto-detection after loading */
	count_good_ebcdic = 0;
  }

  count_max_cjk = 0;
  max_cjk_tag = ' ';
  max_cjk_count (count_cns, 'C');
  max_cjk_count (count_johab, 'H');
  max_cjk_count (count_sjis, 'S');
  max_cjk_count (count_sjisx, 'x');
  max_cjk_count (count_jp, 'J');
  max_cjk_count (count_jisx, 'X');
  max_cjk_count (count_big5, 'B');
  max_cjk_count (count_uhc, 'K');
  max_cjk_count (count_gb, 'G');

  /* Unicode encoding detection */
  if (! empty && do_auto_detect && ! utf16_file && ! ebcdic_file) {
	if (BOM || 
#ifdef utf_preference_in_utf_term
		utf8_screen ? count_good_utf >= count_bad_utf
				: count_good_utf > count_bad_utf
#else
		count_good_utf >= count_bad_utf
#endif
	   )
	{
		nr_of_chars = nr_of_utfchars;
		(void) set_text_encoding (NIL_PTR, 'U', "load: U");
	} else {
		if (strisprefix ("UTF", get_text_encoding ())) {
			(void) set_text_encoding (NIL_PTR, 'L', "load: L");
		} else {
			/* allow fallback to default encoding set on function entry */
		}

		if (count_good_cjk > 
#ifdef poor_tuning_attempt
			count_bad_cjk * 2 + count_weak_cjk * 5 / nr_of_cjkchars
#else
			count_bad_cjk * 2 + count_weak_cjk / 5
#endif
		     && count_good_cjk > count_good_iso / 2
		     /* at least one CJK encoding is enabled for auto-detection */
		     && max_cjk_tag != ' '
		   ) {
			/* setting cjk_text = True; */
			(void) set_text_encoding (":??", ' ', "load: CJK");
			nr_of_chars = nr_of_cjkchars;
		} else if (
		    /* count_good_viscii / 2 > count_good_cjk + count_weak_cjk && */
		    /* count_good_viscii / 3 > count_good_cjk + count_weak_cjk - count_bad_cjk && */
		       count_good_viscii > count_good_iso
		    && count_good_viscii > count_good_cp1252
		    && count_good_viscii > count_good_cp850
		    && count_good_viscii > count_good_mac
		   ) {
			(void) set_text_encoding ("VISCII", 'V', "detect");
		} else if (count_good_ebcdic > count_good_cp1252 && 
			count_good_ebcdic > count_good_cp850 && 
			count_good_ebcdic > count_good_mac) {
			(void) set_text_encoding ("CP1047", ' ', "detect");
		} else if (count_good_cp1252 > count_good_iso && 
			count_good_cp1252 > count_good_cp850 && 
			count_good_cp1252 > count_good_mac) {
			(void) set_text_encoding ("CP1252", 'W', "detect");
		} else if (count_good_cp850 > count_good_iso && 
			count_good_cp850 > count_good_mac) {
			(void) set_text_encoding ("CP850", 'P', "detect");
		} else if (count_good_mac > count_good_iso) {
			(void) set_text_encoding ("MacRoman", 'M', "detect");
		} else {
			/* leave default/fallback encoding */
		}
	}
  }

  /* detect CJK encoding based on counters */
  if (! empty && cjk_text && do_auto_detect && ! (utf8_text && utf16_file)) {
	if (max_cjk_tag != ' ') {
		(void) set_text_encoding (NIL_PTR, max_cjk_tag, "detect CJK");
	}
  }

  /* detect style of quotation marks */
  if (quote_type == 0) {
	determine_quote_style ();
  }

  /* if text encoding changed, reset previous one, then toggle */
  if (! streq (prev_text_encoding, get_text_encoding ())) {
	char * new_text_encoding = get_text_encoding ();
	(void) set_text_encoding (prev_text_encoding, ' ', "load: prev");
	change_encoding (new_text_encoding);
	/* -> ... set_text_encoding (new_text_encoding, ' ', "men: change_encoding"); */
  }

  /* end auto-detection stuff */
}

static
LINE *
read_file (fd, retpoi, do_auto_detect)
  int fd;
  int * retpoi;
  FLAG do_auto_detect;
{
  int ret = * retpoi;
  int len;
  LINE * line = header;

#ifdef debug_timing
	long time_get = 0;
	long time_check = 0;
	long time_line = 0;
	long time_count = 0;
#endif
	mark_time (time_get);

	while (line != NIL_LINE
		&& line_gotten (ret = get_line (fd, text_buffer, & len, do_auto_detect)))
	{
		lineend_type new_return_type;

		elapsed_mark_time (time_get, time_check);
		if (ret == NO_LINE || ret == SPLIT_LINE) {
			new_return_type = lineend_NONE;
		} else if (ret == NUL_LINE) {
			new_return_type = lineend_NUL;
		} else {
			new_return_type = got_lineend;
		}
		elapsed_mark_time (time_check, time_line);
		line = line_insert_after (line, text_buffer, len, new_return_type);
		elapsed_time (time_line);

		/* if editing buffer cannot be filled (out of memory), 
		   assure that file is not accidentally overwritten 
		   with truncated buffer version
		 */
		if (line == NIL_LINE) {
			set_error ("Out of memory for new lines when reading file");
			/* double-prevent incomplete overwriting */
			file_name [0] = '\0';
		}

		mark_time (time_count);
		nr_of_bytes += (long) len;

		if (do_auto_detect) {
			nr_of_chars += char_count (text_buffer);
			nr_of_utfchars += utf8_count (text_buffer);
			cjk_text = True;
			nr_of_cjkchars += char_count (text_buffer);
			cjk_text = False;
		} else if (utf8_text) {
			nr_of_chars += utf8_count (text_buffer);
		} else {
			nr_of_chars += char_count (text_buffer);
		}

		if (new_return_type == lineend_NONE) {
			nr_of_chars --;
			nr_of_utfchars --;
			nr_of_cjkchars --;
			nr_of_bytes --;
		}
		elapsed_mark_time (time_count, time_get);
	}
	elapsed_time (time_get);
#ifdef debug_timing
	printf ("get %ld, check %ld, line_insert %ld, count %ld\n", time_get, time_check, time_line, time_count);
#endif

  * retpoi = ret;
  return line;
}

/*
 * Load_file loads the file with given name or the input pipe into memory.
 * If the file couldn't be opened, just an empty line is installed.
 * Buffer pointers are initialized.
 * The current position is set to the last saved position.
 *
 * Load_file_position loads and positions as follows:
	if to_open_linum > 0, the given position is used
	if to_open_linum == 0, the last saved position is used
	if to_open_linum < 0, no position is set (stays at beginning)
   Beware: length of file name argument is not limited!
 * aux = False: include in filename list
	 True: don't "
	 OPEN: include in filename list, append after current file
 */
static
int
load_file_position (file, aux, from_pipe, with_display, to_open_linum, to_open_pos)
  char * file;
  FLAG aux;	/* e.g. loading recovery file */
  FLAG from_pipe;
  FLAG with_display;
  int to_open_linum;
  int to_open_pos;
{
  LINE * line = header;
  int fd = -1;		/* Filedescriptor for file */
  FLAG fcntl_locked = False;	/* lock acquired ? */
  FLAG do_auto_detect = auto_detect;
  FLAG empty = False;
  int ret = FINE;
  FLAG errshown = False;
  FLAG restore_cmd_options = False;

  nr_of_bytes = 0L;
  nr_of_chars = 0L;
  nr_of_utfchars = 0L;
  nr_of_cjkchars = 0L;
  if (aux == OPEN) {
	/* todo: notify/mark filelist add mode */
	aux = False;
  }
  prev_text_encoding = get_text_encoding ();

  /* Remove previous file lock (if any) */
  unlock_file ();
  viewonly_locked = False;

  clearscreen ();
  flush ();
  clear_textbuf ();
  /* initialize cursor position */
  x = 0;
  y = 0;
  /* initialize some file mode and status flags */
  modified = reading_pipe = from_pipe;
  backup_pending = True;
  overwriteOK = False;
  file_locked = False;
  recovery_exists = False;
  viewonly_err = False;
  total_lines = 0;
  total_chars = 0L;

  open_linum = to_open_linum;
  open_pos = to_open_pos;
  open_col = -1;

  writable = True;
  file_is_dir = False;
  file_is_dev = False;
  file_is_fifo = False;

  fprot1 = fprot0;		/* default file properties */
  filestat.st_mtime = 0;	/* fall-back modification time */
  trace_modtime (& filestat.st_mtime, "default", file);

  set_quote_type (default_quote_type);
  if (preselect_quote_style != NIL_PTR && smart_quotes != VALID) {
	set_quote_style (preselect_quote_style);
  }

  if (file == NIL_PTR) {
	/* This is called at most once; otherwise we would have to 
	   consider configure_preferences (False) (see update_file_name)
	 */
	if (reading_pipe == False) {
		status_msg ("New file");
		empty = True;
	} else {
		fd = 0;
		file = "standard input";
	}
	file_name [0] = '\0';
  } else {
	update_file_name (file, False, ! aux);
	restore_cmd_options = True;
	status_line ("Accessing ", file);

	if (! from_pipe) {
		/* Determine file type and properties */
#ifndef VAXC
		struct stat fstat_buf;
		if (stat (file, & fstat_buf) == 0) {
			if (S_ISDIR (fstat_buf.st_mode)) {
				file_is_dir = True;
			}
			if (S_ISFIFO (fstat_buf.st_mode)) {
				file_is_fifo = True;
			}
			if (is_dev (file, & fstat_buf)) {
				file_is_dev = True;
			}

			/* Determine file protection in case text 
			   is saved to other file */
# ifdef msdos
			/* fstat retrieval does not work properly with djgpp */
# else
			fprot1 = fstat_buf.st_mode;
# endif
			/* Determine file modification time */
			if (! file_is_fifo) {
				memcpy (& filestat, & fstat_buf, sizeof (struct stat));
			}
			trace_modtime (& filestat.st_mtime, "load is", file);
		}
#endif
	}

	if (file_is_dev) {
		error ("Cannot edit char/block device file");
		ret = ERRORS;
		overwriteOK = False;
		viewonly_err = True;
		empty = True;
	} else if (access (file, F_OK) < 0) {	/* Cannot access file */
		status_line ("New file ", file);
		overwriteOK = False;
		empty = True;
#ifndef vms
	/* On VMS, open with O_RDWR would already change the 
	   'modified' file timestamp and thus spoil the 'modified' checks
	 */
	} else if (! viewonly_mode && ! file_is_fifo
		   && (fd = open (file, O_RDWR | O_BINARY, 0)) >= 0) {
#define dont_use_fcntl_locking
# ifdef use_fcntl_locking
		struct flock wlock;
		wlock.l_type = F_WRLCK;
		wlock.l_whence = SEEK_SET;
		wlock.l_start = 0;
		wlock.l_len = 0;
		if (fcntl (fd, F_SETLK, & wlock) < 0) {
			status_line ("Other program claims lock on ", file);
			sleep (2) /* give time to see warning */;
		} else {
			fcntl_locked = True;
		}
# endif
		overwriteOK = True;
		writable = True;
		if (open_linum == 0) {
			get_open_pos (file_name);
			restore_cmd_options = True;
		}
#endif
	} else if ((fd = open (file, O_RDONLY | O_BINARY, 0)) >= 0) {
		overwriteOK = True;
		if (access (file, W_OK) < 0) {	/* Cannot write file */
			writable = False;
		}

		if (open_linum == 0) {
			get_open_pos (file_name);
			restore_cmd_options = True;
		}
	} else {
		error2 ("Cannot open: " /*, file */, serror ());
		ret = ERRORS;
		viewonly_err = True;
		empty = True;
	}
  }

  /* set file type specific options */
  set_file_type_flags ();
  if (restore_cmd_options) {
	/* this call for preferences management is currently 
	   not effective as it would still interfere with 
	   cumulative options
	 */
	configure_preferences (OPEN);	/* restore command line options */
  }

  /* initiate reading file */
  loading = True;		/* Loading file, so set flag */
  loaded_from_filename = False;
  reset_get_line (True);	/* must be called after get_open_pos ! */

  /* restore determined default text encoding */
	/* must be set after reset_get_line ! */
  (void) set_text_encoding (default_text_encoding, ' ', "load: def");

  if (fd >= 0) {
	top_line_dirty = True;
	splash_logo ();
  }

  /* display header area already, even if later displayed again
	   - to show something while loading
	   -! to make sure proper height of filename tabs is calculated
     but do not show filelist yet
  */
  displaymenuline (False);
  flush ();

  if (fd >= 0) {
    FLAG save_utf8_text = utf8_text;
    FLAG save_cjk_text = cjk_text;
    FLAG save_mapped_text = mapped_text;
    if (do_auto_detect) {
	utf8_text = False;
	cjk_text = False;
    }
    /* determine total_chars from here to change_encoding () */

#ifndef __TURBOC__
    if (fd > 0 && filtering_read) {
	int pfds [2];
	int pid;
	int status;
	int w = -1;	/* avoid -Wmaybe-uninitialized */
	int waiterr = 0;	/* avoid -Wmaybe-uninitialized */

	(void) close (fd);
	if (pipe (pfds) < 0) {
		ret = ERRORS;
		pid = -1;
	} else {
		raw_mode (False);
		/* clean-up primary screen */
		set_cursor (0, YMAX);
		flush ();

		pid = fork ();
	}
	if (ret == ERRORS) {	/* pipe error */
		set_errno ("Cannot create filter pipe");
	} else if (pid < 0) {	/* fork error */
		raw_mode (True);
		set_errno ("Cannot fork filter");
		ret = ERRORS;
	} else if (pid == 0) {	/* child */
		(void) close (pfds [0]);
		/* attach stdout to pipe */
		(void) dup2 (pfds [1], 1);
		(void) close (pfds [1]);
		/* invoke filter */
		if (strchr (filter_read, ' ')) {
			/* filter spec includes parameters */
			char filter [maxFILENAMElen];
			if (strstr (filter_read, "%s")) {
				/* filter spec has filename placeholder */
				sprintf (filter, filter_read, file_name);
			} else {
				/* append filename */
				sprintf (filter, "%s %s", filter_read, file_name);
			}
#ifdef debug_filter
			fprintf (stderr, "system ('%s')\n", filter);
#endif
			status = system (filter);
			if (status >> 8) {
				_exit (status >> 8);
			} else {
				_exit (status);
			}
		} else {
#ifdef debug_filter
			fprintf (stderr, "%s %s\n", filter_read, file_name);
#endif
			execlp (filter_read, "filter_read", file_name, NIL_PTR);
#ifdef debug_filter
			fprintf (stderr, "_exit (127) [%s]\n", serror ());
#endif
			_exit (127);
		}
	} else {	/* pid > 0: parent */
		(void) close (pfds [1]);
		/* read file contents from pipe */
		line = read_file (pfds [0], & ret, do_auto_detect);
		/* wait for filter to terminate */
		do {
			w = wait (& status);
			waiterr = geterrno ();
		} while (w != pid && (w != -1 || waiterr == EINTR));
	}
	quit = False;
	intr_char = False;
#ifdef debug_filter
	printf ("wait status %04X\n", status);
#endif

	raw_mode (True);
	clearscreen ();	/* clear double height splash logo area */

	/* check child process errors */
	if (ret == ERRORS) {
		/* fork failed */
	} else if (w == -1) {
		status_fmt ("Filter wait error: ", serrorof (waiterr));
		errshown = True;
		ret = ERRORS;
	} else if ((status >> 8) == 127) {	/* child could not exec filter */
		status_fmt2 (filter_read, ": Failed to start filter");
		errshown = True;
		ret = ERRORS;
	} else if ((status & 0x80) != 0) {	/* filter dumped */
		status_fmt ("Filter dumped: ", dec_out (status & 0x7F));
		errshown = True;
		ret = ERRORS;
	} else if ((status & 0xFF) != 0) {	/* filter aborted */
		status_fmt ("Filter aborted: ", dec_out (status & 0x7F));
		errshown = True;
		ret = ERRORS;
	} else if ((status >> 8) != 0) {	/* filter reported error */
		status_fmt ("Filter error: ", serrorof (status >> 8));
		errshown = True;
		ret = ERRORS;
	}
    } else
#endif
    {
	if (file_name [0] != '\0') {
		status_line ("Reading ", file_name);
	} else {
		status_line ("Reading ", file);
	}

	line = read_file (fd, & ret, do_auto_detect);

	clearscreen ();	/* clear double height splash logo area */
    }


    if (utf16_file) {
	/* workaround: skip following restore;
	   in this case, set_text_encoding has been called 
	   within the save/restore pair
	   -> refactoring!
	 */
    } else {
	utf8_text = save_utf8_text;
	cjk_text = save_cjk_text;
	mapped_text = save_mapped_text;
    }

    if (line != NIL_LINE &&
	(total_lines == 0 /* empty file */
	|| line->return_type == lineend_NUL /* NUL-terminated file */
	)) {
	line = line_insert_after (line, "\n", 1, lineend_NONE);
    }
    clear_filebuf ();
    cur_line = header->next;
    line_number = 1;
    if (fcntl_locked == False) {
	debuglog ("closing", "loaded file", "");
	(void) close (fd);
    }

    if (count_lineend_CRLF > count_lineend_LF) {
	default_lineend = lineend_CRLF;
    } else {
	default_lineend = lineend_LF;
    }
  } else {
    /* Handle empty buffer / new file */

    /* Restore default properties if file could not be opened */
    fprot1 = fprot0;
    filestat.st_mtime = 0;
    trace_modtime (& filestat.st_mtime, "loaded none", file);

    default_lineend = newfile_lineend;
    if (ebcdic_text || ebcdic_file) {
	default_lineend = lineend_NL1;
    }
    if (lineends_LFtoCRLF) {	/* +r */
	default_lineend = lineend_CRLF;
    } else if (lineends_CRLFtoLF) {	/* -r */
	default_lineend = lineend_LF;
    }

    /* Just install an empty line */
    line = line_insert_after (line, "\n", 1, default_lineend);
    if (line == NIL_LINE) {
	set_error ("Out of memory for new text buffer");
    }
  }


  encoding_auto_detection (empty, do_auto_detect);
  total_chars = nr_of_chars;
  total_chars = -1L;	/* nr_of_chars counting does not work properly */


  /* show header area (with filelist) */
  displaymenuline (True);


  /* handle errors */
  if (line == NIL_LINE) {
	set_error ("Out of memory for new lines when loading file");
	file_name [0] = '\0';	/* double-prevent incomplete overwriting */

	/* insert static emergency line to avoid crash */
	line = line_insert_after (tail->prev, "\n", -1, lineend_NONE);

	viewonly_err = True;
	modified = False;
	ret = ERRORS;
  }

  if (get_line_error != NIL_PTR && ! errshown) {
	/* show postponed set_error messages */
	show_get_l_errors ();
	errshown = True;
	ret = ERRORS;
	overwriteOK = False;
  }

  if (fd >= 0) {
	if (! errshown && ret != ERRORS && line != NIL_LINE) {
		fstatus ("Loaded", nr_of_bytes, nr_of_chars);
	}
  }

  if (fd >= 0 && ret == ERRORS) {
	if (! errshown) {
		ring_bell ();
		if (nr_of_bytes > 0) {
			status_fmt ("Reading failed (buffer incomplete): ", serror ());
		} else {
			status_fmt ("Reading failed (could not read): ", serror ());
		}
		errshown = True;
		overwriteOK = False;
	}
	file_name [0] = '\0';	/* double-prevent incomplete overwriting */
	viewonly_err = True;
	modified = False;
  }

  /* finalize loading, find file and screen position */
  reset (header->next, 0);	/* Initialize pointers */
  move_to (0, 0);
  loading = False;		/* Stop loading, reset flag */
  loaded_from_filename = True;	/* indicate relation of text and filename */

  if (open_linum > 0) {
	LINE * open_line = proceed (header->next, open_linum - 1);
	char * cpoi;
	int cur_column = 0;

	move_to (0, find_y_w_o_RD (open_line));

	/* re-position within line */
	cpoi = cur_line->text;
	if (open_col >= 0) {
		char * prev_cpoi = cpoi;
		while (* cpoi != '\n' && cur_column <= open_col) {
			prev_cpoi = cpoi;
			advance_char_scr (& cpoi, & cur_column, cur_line->text);
		}
		if (cur_column <= open_col) {
			prev_cpoi = cpoi;
		}
		move_address (prev_cpoi, y);
	} else {
		while (* cpoi != '\n' && open_pos > 0) {
			advance_char (& cpoi);
			open_pos --;
		}
		move_address (cpoi, y);
	}

	if (with_display) {
		display (0, top_line, last_y, 0);
	}
	if (! errshown && ret != ERRORS && line != NIL_LINE) {
		if (! cjk_text || prev_encoding_tag == text_encoding_tag) {
			fstatus ("Loaded", nr_of_bytes, nr_of_chars);
		} else {
			fstatus ("Loaded", nr_of_bytes, -1L);
		}
	}
	mark_n (0);
  } else if (with_display) {
	display (0, top_line, last_y, 0);
	move_to (0, 0);
  }

#ifdef unix
  RD_window_title ();
#endif

  /* check recovery file */
  if (! reading_pipe && ! aux) {
	check_recovery_file (ret == ERRORS);
  }

  if (viewonly_err) {
	top_line_dirty = True;
  }

  return ret;
}

/**
   Load file.
   Beware: length of file name argument is not limited!
 */
static
int
load_file (file, aux, from_pipe, with_display)
  char * file;
  FLAG aux;	/* e.g. loading recovery file */
  FLAG from_pipe;
  FLAG with_display;
{
  return load_file_position (file, aux, from_pipe, with_display, 0, 0);
}


/**
   Load file name (which may be a wild card name in case of MSDOS).
   Beware: length of file name argument is not limited!
 */
void
load_wild_file (file, from_pipe, with_display)
  char * file;
  FLAG from_pipe;
  FLAG with_display;
{
  (void) load_file (file, False, from_pipe, with_display);
}


/**
   Check if disk file has been modified since loaded.
   Issue warning.
   Called if window gets mouse focus or after returning from external cmd.
 */
void
check_file_modified ()
{
#ifndef dont_check_modtime
	/* VMS does not report the proper time of the existing file;
	   (maybe because it's looking at a prospective new version??)
	   fixed: avoiding O_RDWR on VMS
	 */
#ifndef VAXC
  struct stat fstat_buf;
  if (stat (file_name, & fstat_buf) == 0) {
	FLAG filechanged;
	FLAG wasmodified = modified;
	trace_modtime (& filestat.st_mtime, "check was", file_name);
	trace_modtime (& fstat_buf.st_mtime, "check now", file_name);
	filechanged = file_changed (file_name, & fstat_buf);
	if (filechanged) {
		char * hint = "";
		if (! wasmodified) {
			hint = " (Reload with ESC #/Alt-F3)";
		}
		if (filechanged == VALID) {
			status_fmt ("Warning: New file appeared on disk", hint);
		} else if (filechanged == NOT_VALID) {
			status_fmt ("Warning: File was replaced on disk", hint);
		} else {
			status_fmt ("Warning: File was modified meanwhile", hint);
		}
	}
  }
#endif
#endif
}


/*
 * Ask user if named file should be overwritten.
 */
FLAG
checkoverwrite (name)
  char * name;
{
  character c;
#ifdef use_locking
  char target [maxFILENAMElen];
  int ret;
#endif
  char * ov_prompt = ": OK to overwrite? (y/n/ESC)";

  status_line ("Checking ", name);
  if (access (name, F_OK) < 0) {	/* Cannot access file */
	return NOT_VALID;	/* no danger of unwanted damage */
  }

#ifdef use_locking
  ret = getsymboliclink (get_lockfile_name (file_name), target, sizeof (target));
  if (ret > 0) {
	char mylocktext [maxFILENAMElen];
	setlocktarget (mylocktext);
	if (! streq (target, mylocktext)) {
		ov_prompt = " (locked): OK to overwrite? (y/n/ESC)";
	}
  } else if (ret == 0) {
	/* empty pseudo (plain) lock file found; 
	   assume unlock workaround */
  }
#endif
  c = status2_prompt ("yn", name [0] ? name : "[unknown file]",
			ov_prompt);
  clear_status ();
  if (c == 'y') {
	return True;
  } else if (c == 'n') {
	return False;
  } else {
/*	quit = False;	abort character has been given */
	return False;
  }
}

/*
 * Attach new file name to buffer
 */
static
FLAG
set_NN ()
{
  char file [maxFILENAMElen];	/* Buffer for new file name */

  if (restricted) {
	restrictederr ();
	return False;
  }
  if (get_filename ("Enter new file name:", file, False) == ERRORS) {
	return False;
  }
  writing_pipe = False;	/* cancel pipe output if explicitly editing file */

  /* Remove previous file lock (if any) */
  unlock_file ();

  /* Clear forced viewonly */
  if (viewonly_err && ! streq (file, file_name)) {
	viewonly_err = False;
	flags_changed = True;
  }

  overwriteOK = False;
  writable = True;
  update_file_name (file, True, True);
#ifdef unix
  if (modified) {
	RD_window_title ();
  }
#endif
  check_recovery_file (False);

  set_modified ();	/* referring to different file now */
  relock_file ();

  clear_status ();
  return True;
}

void
NN ()
{
  (void) set_NN ();
}


/*======================================================================*\
|*			File I/O basics					*|
\*======================================================================*/

#define dont_debug_writefile


/*
 * Show file write error
 */
static
void
msg_write_error (op)
  char * op;
{
  char msg [maxMSGlen];

  ring_bell ();
  build_string (msg, "%s failed (File incomplete): %s", op, serror ());
  status_fmt2 ("", msg);
}

/*
 * Flush the I/O buffer on filedescriptor fd.
 */
int
flush_filebuf (fd)
  int fd;
{
  if (filebuf_count == 0) {	/* There is nothing to flush */
	return FINE;
  } else {
	char * writepoi = filebuf;
	int written = 0;
	int none_count = 0;

	while (filebuf_count > 0) {
		written = write (fd, writepoi, filebuf_count);
#ifdef debug_writefile
		printf ("write -> %d: %s\n", written, serror ());
#endif
		if (written == -1) {
			if (geterrno () == EINTR && winchg) {
				/* try again */
			} else if (geterrno () == EINTR) {
				none_count ++;
				if (none_count > 9) {
					return ERRORS;
				}
				/* try again */
			} else {
				return ERRORS;
			}
		} else if (written == 0) {
			none_count ++;
			if (none_count > 9) {
				return ERRORS;
			}
			/* try again */
		} else {
			none_count = 0;
			filebuf_count -= written;
			writepoi += written;
		}
	}
  }
  filebuf_count = 0;
  return FINE;
}

/*
 * writechar does a buffered output to file.
 */
int
writechar (fd, c)
  int fd;
  char c;
{
  filebuf [filebuf_count ++] = c;
  if (filebuf_count == filebuflen) {
	return flush_filebuf (fd);
  }
  return FINE;
}

/*
 * writeucs writes a UCS Unicode code in UTF-16
 * Return # words written or ERRORS.
 */
static
int
writeucs (fd, c)
  int fd;
  unsigned long c;
{
  int err = FINE;

  if (c > (unsigned long) 0x10FFFF) {
	return writeucs (fd, 0xFFFD);
  } else if (c > (unsigned long) 0xFFFF) {
	err = 2;
	c -= 0x10000;
	err |= writeucs (fd, 0xD800 | (c >> 10));
	err |= writeucs (fd, 0xDC00 | (c & 0x03FF));
  } else {
	err = 1;
	if (utf16_little_endian) {
		err |= writechar (fd, c & 0xFF);
		err |= writechar (fd, c >> 8);
	} else {
		err |= writechar (fd, c >> 8);
		err |= writechar (fd, c & 0xFF);
	}
  }

  return err;
}

/*
 * writelechar writes a line-end character to file
   Only called by write_lineend.
 */
static
int
writelechar (fd, c, handle_utf16)
  int fd;
  character c;
  FLAG handle_utf16;
{
  if (utf8_text && utf16_file && handle_utf16) {
	return writeucs (fd, (character) c);
  } else if (ebcdic_file && handle_utf16) {
	mapped_text = True;
	c = encodedchar (c);
	mapped_text = False;
	return writechar (fd, c);
  } else {
	return writechar (fd, c);
  }
}

/*
 * write_lineend writes a line-end in the respective form to file
   Called by write_line, yank_text.
 */
int
write_lineend (fd, return_type, handle_utf16)
  register int fd;
  lineend_type return_type;
  FLAG handle_utf16;
{
  switch (return_type) {
    case lineend_NONE:
			return 0;
    case lineend_NUL:
			if (writelechar (fd, '\0', handle_utf16) == ERRORS) {
				return ERRORS;
			}
			return 1;
    case lineend_CRLF:
			if (writelechar (fd, '\r', handle_utf16) == ERRORS) {
				return ERRORS;
			}
			if (writelechar (fd, ebcdic_text ? code_LF : '\n', handle_utf16) == ERRORS) {
				return ERRORS;
			}
			return 2;
    case lineend_CR:
			if (writelechar (fd, '\r', handle_utf16) == ERRORS) {
				return ERRORS;
			}
			return 1;
    case lineend_NL1:	/* ISO 8859/EBCDIC NEXT LINE U+85 */
			if (handle_utf16 || pasting_encoded
			    || (pasting && pastebuf_utf8 && utf8_text)
			   ) {
				if (writelechar (fd, ebcdic_text ? code_NL : (character) 0x85, handle_utf16) == ERRORS) {
					return ERRORS;
				}
				return 1;
			} else {
				if (writelechar (fd, '\302', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				if (writelechar (fd, '\205', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				return 2;
			}
    case lineend_NL2:	/* Unicode NEXT LINE U+85 */
			if (utf8_text && utf16_file && handle_utf16) {
				if (writeucs (fd, 0x0085) == ERRORS) {
					return ERRORS;
				}
				return 1;
			} else {
				if (writelechar (fd, '\302', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				if (writelechar (fd, '\205', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				return 2;
			}
    case lineend_LS:	/* Unicode line separator U+2028 */
			if (utf8_text && utf16_file && handle_utf16) {
				if (writeucs (fd, 0x2028) == ERRORS) {
					return ERRORS;
				}
				return 1;
			} else {
				if (writelechar (fd, '\342', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				if (writelechar (fd, '\200', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				if (writelechar (fd, '\250', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				return 3;
			}
    case lineend_PS:	/* Unicode paragraph separator U+2029 */
			if (utf8_text && utf16_file && handle_utf16) {
				if (writeucs (fd, 0x2029) == ERRORS) {
					return ERRORS;
				}
				return 1;
			} else {
				if (writelechar (fd, '\342', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				if (writelechar (fd, '\200', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				if (writelechar (fd, '\251', handle_utf16) == ERRORS) {
					return ERRORS;
				}
				return 3;
			}
    case lineend_LF:
    default:
			if (writelechar (fd, ebcdic_text ? code_LF : '\n', handle_utf16) == ERRORS) {
				return ERRORS;
			}
			return 1;
  }
}

/*
 * write_line writes the given string on the given filedescriptor.
 * (buffered via writechar via misused screen buffer!)
 * return # bytes written or ERRORS.
   Only called by write_file, so handle_utf16 is always True.
 */
static
int
write_line (fd, text, return_type, handle_utf16)
  int fd;
  char * text;
  lineend_type return_type;
  FLAG handle_utf16;
{
  int len;
  int ccount = 0;

  while (* text != '\0') {
	if (* text == '\n') {
		/* handle different line ends */
		len = write_lineend (fd, return_type, handle_utf16);
		if (len == ERRORS) {
			return ERRORS;
		}
		text ++;
		ccount += len;
	} else {
		if (utf8_text && utf16_file && handle_utf16) {
			unsigned long unichar;
			int utflen;
			utf8_info (text, & utflen, & unichar);
			if (UTF8_len (* text) == utflen) {
				len = writeucs (fd, unichar);
			} else {
				len = writeucs (fd, 0xFFFD);
			}
			if (len == ERRORS) {
				return ERRORS;
			}
			advance_utf8 (& text);
			ccount += len;
		} else {
			character c = * text;

			if (ebcdic_file && handle_utf16) {
				mapped_text = True;
				c = encodedchar (c);
				mapped_text = False;
			}

			if (writechar (fd, c) == ERRORS) {
				return ERRORS;
			}
			text ++;
			ccount ++;
		}
	}
  }

  if (utf8_text && utf16_file && handle_utf16) {
	return ccount * 2;
  } else {
	return ccount;
  }
}


/* Call graph for writing functions:
	panic --\------> panicwrite --------------------------\
		 > QUED ----------\			       \
	ESC q --/		   > ask_save --\		> write_file
	ESC e ---> EDIT --> edit_file -/	 \	       /
	ESC v ---> VIEW -/			  \	      /
	ESC w -------------------> WT -------------> write_text
	ESC W -------------------> WTU -----------/
	ESC z -------------------> SUSP ---------/
	ESC ESC -> EXED ---------> EXFILE ------/
			\--------> EXMINED ----/
	ESC t -------------------> Stag ------/
*/
long write_count;	/* number of bytes written */
long chars_written;	/* number of chars written */

/*
 * Write text in memory to file.
 */
static
void
write_file (fd)
  int fd;
{
  register LINE * line;
  int ret = FINE;
  static FLAG handle_utf16 = True;

  write_count = 0L;
  chars_written = 0L;
  clear_filebuf ();

  if (utf8_text && utf16_file && handle_utf16) {
	/* prepend BOM if there was one */
	if (BOM && strncmp (header->next->text, "﻿", 3) != 0) {
		ret = write_line (fd, "﻿", lineend_NONE, handle_utf16);
		if (ret == ERRORS) {
			msg_write_error ("Write");
			write_count = -1L;
			chars_written = -1L;
		} else {
			ret = FINE;
			write_count = 2;
			chars_written = 1;
		}
	}
  }

  if (ret == FINE) {
    for (line = header->next; line != tail; line = line->next) {
	ret = write_line (fd, line->text, line->return_type, handle_utf16);
	if (ret == ERRORS) {
		msg_write_error ("Write");
		write_count = -1L;
		chars_written = -1L;
		break;
	}
	write_count += (long) ret;
	chars_written += (long) char_count (line->text);
	if (line->return_type == lineend_NONE) {
		chars_written --;
	}
    }
  }

  if (write_count > 0L && flush_filebuf (fd) == ERRORS) {
	if (ret != ERRORS) {
		msg_write_error ("Write");
		ret = ERRORS;
	}
	write_count = -1L;
	chars_written = -1L;
  }

  if (close (fd) == -1) {
#ifdef debug_writefile
	printf ("close: %s\n", serror ());
#endif
	if (ret != ERRORS) {
		msg_write_error ("Close");
		ret = ERRORS;
	}
	write_count = -1L;
	chars_written = -1L;
  }
}

static
void
write_recovery ()
{
  char * recovery_fn = get_recovery_name (file_name);
  int fd;

  fd = open (recovery_fn, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, bufprot);
  write_file (fd);
}

int
panicwrite ()
{
  int fd;

  fd = open (panic_file, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, bufprot);
  write_file (fd);

  write_recovery ();

  if (write_count == -1L) {
	return ERRORS;
  } else {
	return FINE;
  }
}


#define use_touch

#if defined (msdos) || defined (__ANDROID__)
#undef use_touch
#endif

#ifdef __MINGW32__
#define system_touch
#endif

FLAG
do_backup (fn)
  char * fn;
{
#ifdef vms
  /* VMS does the backups itself */
  return True;
#else
  FLAG backup_ok = False;
  char * backup_name = get_backup_name (fn);
  if (backup_name) {
	status_line ("Copying to backup file ", backup_name);
	backup_ok = copyfile (fn, backup_name);
  }
  if (backup_ok == False) {
	error ("Could not copy to backup file");
	sleep (1);	/* give some time to see the hint */
	return False;
  } else if (backup_ok == True) {
	/* let backup have the original file timestamp */
#ifndef use_touch
#include <time.h>
	/* what a hack!
		djgpp reports EIO and fails
		turbo-c (utime only) reports ENOENT but yet succeeds
	 */
	struct stat fstat_buf;
	if (stat (fn, & fstat_buf) == 0) {
# if defined (BSD)
		struct timeval times [2];
		times [0].tv_sec = 0;
		times [0].tv_usec = 0;
		(void) gettimeofday (& times [0], 0);
		times [1].tv_sec = fstat_buf.st_mtime;
		times [1].tv_usec = 0;
		(void) utimes (backup_name, times);
# else
#  ifndef __TURBOC__
#  include <utime.h>
#  endif
		struct utimbuf times;
#  ifdef __TURBOC__
		times.actime = 0;
#  else
		struct timeval now;
		now.tv_sec = 0;
		(void) gettimeofday (& now, 0);
		times.actime = now.tv_sec;
#  endif
		times.modtime = fstat_buf.st_mtime;
		(void) utime (backup_name, & times);
# endif
	}
#else	/* #ifndef use_touch */
	/* On Unix, could use utime (svr4) or utimes (bsd) anyway, 
	   but let's avoid the year 2038 problem here...
	 */
# ifdef system_touch
	char syscommand [maxCMDlen];
	build_string (syscommand, "touch -r '%s' '%s' 2> /dev/null", fn, backup_name);
	(void) system (syscommand);
	/*RDwin ();*/
# else
	/* Avoid crap message "couldn't set locale correctly" on SunOS */
	(void) progcallpp (NIL_PTR, -1, (char * *) 0,
		0,
		"touch", 
#  ifdef __ultrix
		/* Ultrix also links with utimes although it's undeclared */
		"-f", 
#  else
		"-r", 
#  endif
		fn, backup_name, NIL_PTR);
# endif
#endif	/* #else defined (msdos) */
  } else {	/* backup_ok == NOT_VALID */
	/* return True; */
  }
  return True;
#endif
}

/**
   Write text to its associated file.
 */
static
int
write_text_pos (force_write, force_savepos, keep_screenmode)
  FLAG force_write;
  FLAG force_savepos;
  FLAG keep_screenmode;
{
  int fd;			/* Filedescriptor of file */

  if (writing_pipe) {
    fd = STD_OUT;
    status_line ("Writing ", "to standard output");
    writing_pipe = False;	/* write to pipe only once */

    /* avoid screen interference with program following in pipe */
    raw_mode (False);
    set_cursor (0, YMAX);
    flush ();

    write_file (fd);

    if (keep_screenmode) {
	raw_mode (True);
    }
  } else {
    int o_trunc;
#ifndef VAXC
    struct stat fstat_buf;
    FLAG stat_pending = True;
# ifdef backup_method_depends_nlinks
    int nlinks = 0;
# endif
#endif

    if (force_write == False && modified == False) {
	if (file_name [0] != '\0') {
		fstatus ("(Write not necessary)", -1L, -1L);
		(void) save_open_pos (file_name, force_savepos | hop_flag);
	} else {
		status_msg ("Write not necessary.");
	}
	return FINE;
    }
    if (force_savepos == True && modified == False) {
	if (file_name [0] != '\0') {
		(void) save_open_pos (file_name, True);
	}
    }

    /* Check if file_name is valid and if file can be written */
    if (file_name [0] == '\0' || writable == False) {
	char file_name2 [maxFILENAMElen];	/* Buffer for new file name */
	int ret;
	overwriteOK = False;
	ret = get_filename ("Saving edited text; Enter file name:", file_name2, False);
	if (ret != FINE) {
		return ret;
	}
	update_file_name (file_name2, True, True);
#ifdef unix
	RD_window_title ();
#endif
	check_recovery_file (False);
    }

    if (overwriteOK == False) {
	FLAG ovw = checkoverwrite (file_name);
	if (ovw != False) {
		overwriteOK = True;
#ifdef backup_only_edited_file
		backup_pending = False;
#else
		if (ovw == NOT_VALID) {
			backup_pending = False;
		}
#endif
		stat_pending = False;
#ifndef VAXC
		if (stat (file_name, & fstat_buf) == 0) {
			if (is_dev (file_name, & fstat_buf)) {
				error ("Not writing to char/block device file");
				return ERRORS;
			}
		}
#endif
	} else {
		if (quit == False) {
			writable = False;
		}
		return ERRORS;
	}
    } else {
#ifndef dont_check_modtime
	/* VMS does not report the proper time of the existing file;
	   (maybe because it's looking at a prospective new version??)
	   fixed: avoiding O_RDWR on VMS
	 */
#ifndef VAXC
	stat_pending = False;
	if (stat (file_name, & fstat_buf) == 0) {
		FLAG filechanged;
		if (is_dev (file_name, & fstat_buf)) {
			overwriteOK = False;
			error ("Not writing to char/block device file");
			return ERRORS;
		}
# ifdef backup_method_depends_nlinks
		nlinks = fstat_buf.st_nlink;
# endif
		trace_modtime (& filestat.st_mtime, "write was", file_name);
		trace_modtime (& fstat_buf.st_mtime, "write now", file_name);
		filechanged = file_changed (file_name, & fstat_buf);
		if (filechanged) {
			character c = status2_prompt ("yn", file_name,
				filechanged == VALID
				? ": New file appeared on disk - Overwrite? (y/n/ESC)"
				: filechanged == NOT_VALID
				  ? ": File was replaced on disk - Overwrite? (y/n/ESC)"
				  : ": File was modified on disk - Overwrite? (y/n/ESC)"
				);
			clear_status ();
			if (c == 'y') {
				/* go on */
			} else if (c == 'n') {
				SAVEAS ();
				return FINE;
			} else {
				return ERRORS;
			}
		}
	}
#endif
#endif
    }

    if (overwriteOK && backup_mode && backup_pending && ! file_is_fifo) {
#ifndef VAXC
	if (stat_pending) {
		stat_pending = False;
		if (stat (file_name, & fstat_buf) == 0) {
# ifdef backup_method_depends_nlinks
			nlinks = fstat_buf.st_nlink;
# endif
		}
	}
#endif
	if (do_backup (file_name)) {
		backup_pending = False;
	}
    }

    status_line ("Opening to write ", file_name);
    if (filtering_write) {
	/* let the filter truncate the file so the content is not lost 
	   if the filter fails to start */
	o_trunc = 0;
    } else {
	o_trunc = O_TRUNC;
    }
    fd = open (file_name, O_CREAT | o_trunc | O_WRONLY | O_BINARY, fprot1 | ((fprot1 >> 2) & xprot));
#ifdef vms
    if (fd < 0) {
	char * version = strrchr (file_name, ';');
	if (version != NIL_PTR) {
		* version = '\0';	/* strip version */
		fd = open (file_name, O_CREAT | o_trunc | O_WRONLY | O_BINARY, fprot1 | ((fprot1 >> 2) & xprot));
	}
    }
#endif
    if (fd < 0) {	/* Opening for write failed */
	if (loaded_from_filename) {
		if (access (file_name, F_OK) < 0) {
			status_fmt ("File not accessible (retry or Save As...): ", serror ());
		} else {
			status_fmt ("File not writable (retry or Save As...): ", serror ());
		}
	} else {
		status_fmt ("Cannot create or write (try Save As...): ", serror ());
	}
	/* don't set writable = False as there might be a 
	   temporary network problem */
	return ERRORS;
    } else {
	writable = True;
    }

#ifndef __TURBOC__
    if (filtering_write) {
	int pfds [2];
	int pid;
	int status;
	int w;
	int waiterr;

	(void) close (fd);
	if (pipe (pfds) < 0) {
		error2 ("Cannot create filter pipe: ", serror ());
		return ERRORS;
	}

	raw_mode (False);
	/* clean-up primary screen */
	set_cursor (0, YMAX);
	flush ();

	pid = fork ();
	if (pid < 0) {	/* fork error */
		raw_mode (True);
		error2 ("Cannot fork filter: ", serror ());
		return ERRORS;
	} else if (pid == 0) {	/* child */
		(void) close (pfds [1]);
		/* attach stdin to pipe */
		(void) dup2 (pfds [0], 0);
		(void) close (pfds [0]);
		/* invoke filter */
		if (strchr (filter_write, ' ')) {
			/* filter spec includes parameters */
			char filter [maxFILENAMElen];
			if (strstr (filter_write, "%s")) {
				/* filter spec has filename placeholder */
				sprintf (filter, filter_write, file_name);
			} else {
				/* append filename */
				sprintf (filter, "%s %s", filter_write, file_name);
			}
#ifdef debug_filter
			printf ("system ('%s')\n", filter);
#endif
			status = system (filter);
			if (status >> 8) {
				_exit (status >> 8);
			} else {
				_exit (status);
			}
		} else {
#ifdef debug_filter
			printf ("%s %s\n", filter_write, file_name);
#endif
			execlp (filter_write, "filter_write", file_name, NIL_PTR);
#ifdef debug_filter
			printf ("_exit (127) [%s]\n", serror ());
#endif
			_exit (127);
		}
	} else {	/* pid > 0: parent */
		(void) close (pfds [0]);
		/* write file contents to pipe */
		write_file (pfds [1]);
		/* wait for filter to terminate */
		do {
			w = wait (& status);
			waiterr = geterrno ();
		} while (w != pid && (w != -1 || waiterr == EINTR));
	}
	quit = False;
	intr_char = False;
#ifdef debug_filter
	printf ("wait status %04X\n", status);
#endif

	raw_mode (True);
	clear_status ();
	RD ();

	/* check child process errors */
	if (w == -1) {
		status_fmt ("Filter wait error: ", serrorof (waiterr));
		write_count = -1L;	/* indicate error */
	} else if ((status >> 8) == 127) {	/* child could not exec filter */
		status_fmt2 (filter_write, ": Failed to start filter");
		write_count = -1L;	/* indicate error */
	} else if ((status & 0x80) != 0) {	/* filter dumped */
		status_fmt ("Filter dumped: ", dec_out (status & 0x7F));
		write_count = -1L;	/* indicate error */
	} else if ((status & 0xFF) != 0) {	/* filter aborted */
		status_fmt ("Filter aborted: ", dec_out (status & 0x7F));
		write_count = -1L;	/* indicate error */
	} else if ((status >> 8) != 0) {	/* filter reported error */
		status_fmt ("Filter error: ", serrorof (status >> 8));
		write_count = -1L;	/* indicate error */
	}
    } else
#endif
    {
	status_line ("Writing ", file_name);
	write_file (fd);
    }

#ifndef VAXC
    if (stat (file_name, & fstat_buf) == 0) {
	memcpy (& filestat, & fstat_buf, sizeof (struct stat));
	trace_modtime (& filestat.st_mtime, "write new", file_name);
    }
#endif
  }

  if (write_count == -1L) {
	return ERRORS;
  }

  /*filelist_add (dupstr (file_name), False);*/

  modified = False;
  unlock_file ();
#ifdef unix
  RD_window_title ();
#endif
  reading_pipe = False;	/* File name is now assigned */

/* Display how many chars (and lines) were written */
  fstatus ("Wrote", write_count, chars_written);
/*  fstatus ("Wrote", -1L); */
  (void) save_open_pos (file_name, hop_flag || ! groom_info_files);
  return FINE;
}

int
write_text ()
{
  return write_text_pos (False, False, True);
}

static
int
write_text_defer_screenmode ()
{
  return write_text_pos (False, False, False);
}

static
void
restore_screenmode ()
{
  if (! isscreenmode) {
	raw_mode (True);
  }
}


/*======================================================================*\
|*			File commands					*|
\*======================================================================*/

int
save_text_load_file (fn)
  char * fn;
{
  if (modified) {
	if (write_text () == ERRORS) {
		return ERRORS;
	}
  }

  (void) load_file (fn, OPEN, False, True);
  return FINE;
}

void
SAVEAS ()
{
  if (restricted) {
	restrictederr ();
	return;
  }
  if (set_NN () == True) {
	WT ();
  }
}

void
WT ()
{
  (void) write_text_pos (False, True, True);
}

void
WTU ()
{
  if (restricted && viewonly) {
	restrictederr ();
	return;
  }
  (void) write_text_pos (True, True, True);
}

/*
 * Ask the user if he wants to save the file or not.
 */
static
int
ask_save_recover_keepscreenmode (do_recover, keep_screenmode)
  FLAG do_recover;
  FLAG keep_screenmode;
{
  register character c;

  c = status2_prompt (do_recover ? "ynr" : "yn",
  			file_name [0] ? file_name : 
					reading_pipe ? "[standard input]" 
							: "[new file]",
			do_recover
			? ": Save modified text? (yes/no/to recover/ESC)"
			: ": Save modified text? (yes/no/ESC)");
  clear_status ();
  if (c == 'y') {
	return write_text_pos (False, False, keep_screenmode);
  } else if (c == 'r') {
	if (do_recover) {
		write_recovery ();
	}
	return FINE;
  } else if (c == 'n') {
	return FINE;
  } else {
	quit = False;	/* abort character has been given */
	return ERRORS;
  }
}

static
int
ask_save ()
{
  return ask_save_recover_keepscreenmode (True, True);
}

static
int
ask_save_no_recover ()
{
  return ask_save_recover_keepscreenmode (False, True);
}

static
int
ask_save_defer_screenmode ()
{
  return ask_save_recover_keepscreenmode (True, False);
}

/*
 * Edit/view another file. If the current file has been modified, 
 * ask whether the user wants to save it.
 * (We could allow to switch between edit and view mode without changing 
 * the file, but we would have to consider carefully the relationship 
 * between viewonly and modified.)
 */
static
void
edit_file (prompt, vomode)
  char * prompt;
  FLAG vomode;
{
  char new_file [maxFILENAMElen];	/* Buffer to hold new file name */

  if (modified && viewonly == False && ask_save () != FINE) {
	return;
  }

/* Get new file name */
  if (get_filename (prompt, new_file, False) == ERRORS) {
	return;
  }
  writing_pipe = False;	/* cancel pipe output if explicitly editing file */

  viewonly_mode = vomode;

  load_wild_file (new_file [0] == '\0' ? NIL_PTR : new_file, False, True);
}

void
RECOVER ()
{
  char * rn;
  char orig_name [maxFILENAMElen];	/* Name of file being edited */
  struct stat orig_filestat;

  if (! recovery_exists) {
	error ("No recovery file");
	return;
  }

  rn = get_recovery_name (file_name);

  if (modified && viewonly == False && ask_save_no_recover () != FINE) {
	status_fmt2 ("", "Aborted file recovery");
	return;
  }

  strcpy (orig_name, file_name);
  memcpy (& orig_filestat, & filestat, sizeof (struct stat));
  if (load_file (rn, True, False, True) != ERRORS) {
	set_modified ();
	/* to do: postpone deletion until recovered file has been saved? */
	(void) delete_file (rn);
  } else {
	/* avoid overwriting file with incompletely recovered buffer */
	modified = False;
	overwriteOK = False;
  }
  strcpy (file_name, orig_name);
  memcpy (& filestat, & orig_filestat, sizeof (struct stat));

#ifdef unix
  RD_window_title ();
#endif
}

void
EDIT ()
{
  if (restricted) {
	restrictederr ();
	return;
  }
  edit_file ("Edit file:", False);
}

void
VIEW ()
{
  if (restricted) {
	restrictederr ();
	return;
  }
  edit_file ("View file:", True);
}

void
EDITmode ()
{
  if (restricted) {
	restrictederr ();
	return;
  }
  viewonly_mode = False;
  if (viewonly_locked) {
	status_fmt2 ("", "File is still view-only because it is locked; Unlock from File menu");
	sleep (2);
  } else if (viewonly) {
	status_fmt2 ("", "File is still view-only after read error");
	sleep (2);
  }
  FSTATUS ();
  flags_changed = True;
}

void
VIEWmode ()
{
  if (modified == False) {
	viewonly_mode = True;
	FSTATUS ();
	flags_changed = True;
  } else {
	error ("Cannot view only - already modified");
  }
}

void
toggle_VIEWmode ()
{
  if (viewonly) {
	EDITmode ();
  } else {
	VIEWmode ();
  }
}

void
view_help (helpfile, item)
  char * helpfile;
  char * item;
{
  char searchstring [maxPROMPTlen];

  /* unless already viewing help, save edited text */
  if (viewing_help == False) {
	if (modified) {
		if (write_text () != FINE) {
			return;
		}
	}

	/* save current position */
	save_cur_line = line_number;
	save_cur_pos = get_cur_pos ();

	/* save editing mode and file name */
	save_restricted = restricted;
	copy_string (save_file_name, file_name);

	/* set mode appropriate for viewing online help */
	viewonly_err = True;
	restricted = True;
	viewing_help = True;

	/* load online help file */
	(void) load_file_position (helpfile, True, False, True, -1, 0);
  }

  /* position to selected help topic */
  BFILE ();
  build_string (searchstring, "mined help topic '%s'", item);
  search_for (searchstring, FORWARD, True);
}

#ifdef viewing_help_within_session
#warning leaving help not properly implemented
static
void
end_view_help ()
{
  restricted = save_restricted;
  viewing_help = False;

  (void) load_file_position (save_file_name, True, False, True, save_cur_line, save_cur_pos);
}
#endif

/**
   final clean-up (temp. files, terminal), exit
   no return from here!
 */
static
void
quit_mined ()
{
#ifdef viewing_help_within_session
  if (viewing_help) {
	end_view_help ();
	return;
  }
#endif

  /* Remove file lock (if any) */
  unlock_file ();

  delete_yank_files ();

  clear_status ();
  set_cursor (0, YMAX);
  putchar ('\n');
#ifdef unix
  clear_window_title ();
#endif
#ifdef msdos
  clear_screen ();
#endif
  flush ();

  /* avoid double raw_mode (False) after closing pipe */
  if (isscreenmode) {
	raw_mode (False);
	set_cursor (0, YMAX);
	flush ();
  }

  debuglog (0, 0, "close");
  exit (0);
}


/*======================================================================*\
|*			File selector					*|
\*======================================================================*/

struct fileentry {
	struct fileentry * prev;
	struct fileentry * next;
	char * fn;
	short line;
	int left;
	int right;
};

static struct fileentry * filelist = 0;

int
filelist_count ()
{
  struct fileentry * fl = filelist;
  int i = 0;
  while (fl) {
	fl = fl->next;
	i ++;
  }
  return i;
}

static struct fileentry * last_fl = 0;

/**
   Get i'th filename from File selector list.
 */
char *
filelist_get (i)
  int i;
{
  struct fileentry * fl = filelist;
  while (i > 0 && fl) {
	fl = fl->next;
	i --;
  }
  last_fl = fl;
  if (fl) {
	return fl->fn;
  } else {
	return NIL_PTR;
  }
}

/**
   Set screen coordinates into last delivered file entry.
 */
void
filelist_set_coord (line, left, right)
  short line;
  int left;
  int right;
{
  if (last_fl) {
	last_fl->line = line;
	last_fl->left = left;
	last_fl->right = right;
  }
}

/**
   Search filename by screen (mouse) coordinates.
 */
char *
filelist_search (line, col)
  short line;
  int col;
{
  struct fileentry * fl = filelist;
  while (fl) {
	if (fl->line == line && fl->left <= col && fl->right > col) {
		break;
	}
	fl = fl->next;
  }
  if (fl) {
	return fl->fn;
  } else {
	return NIL_PTR;
  }
}

static
void
filelist_append (flpoi, fn, allowdups, versionbase, prevfl)
  struct fileentry * * flpoi;
  char * fn;
  FLAG allowdups;
  char * versionbase;
  struct fileentry * prevfl;
{
  if (* flpoi) {
	/* suppress subsequent backup/version names 
	   as generated by command line filename completion
	 */
	if (versionbase && streq ((* flpoi)->fn, versionbase)) {
		return;
	}

	if (allowdups || ! streq ((* flpoi)->fn, fn)) {
		filelist_append (& ((* flpoi)->next), fn, allowdups, versionbase, * flpoi);
	}
  } else {
	* flpoi = alloc (sizeof (struct fileentry));
	if (* flpoi) {
		top_line_dirty = True;
		(* flpoi)->fn = fn;
		(* flpoi)->line = 0;
		(* flpoi)->left = 0;
		(* flpoi)->right = 0;
		(* flpoi)->next = 0;
		(* flpoi)->prev = prevfl;
	}
  }
}

static
char *
filelist_delete_next (flpoi, fn)
  struct fileentry * * flpoi;
  char * fn;
{
  if (* flpoi) {
	if (streq ((* flpoi)->fn, fn)) {
		top_line_dirty = True;
		* flpoi = (* flpoi)->next;
		if (* flpoi && (* flpoi)->prev) {
			(* flpoi)->prev = (* flpoi)->prev->prev;
		}
		if (* flpoi) {
			return (* flpoi)->fn;
		} else {
			return NIL_PTR;
		}
	} else {
		return filelist_delete_next (& ((* flpoi)->next), fn);
	}
  } else {
	return NIL_PTR;
  }
}

static
char *
filelist_next (fl, fn)
  struct fileentry * fl;
  char * fn;
{
  if (fl) {
	if (streq (fl->fn, fn)) {
		if (fl->next) {
			return fl->next->fn;
		} else {
			return NIL_PTR;
		}
	} else {
		return filelist_next (fl->next, fn);
	}
  } else {
	return NIL_PTR;
  }
}


static
char *
filelist_prev (fl, fn)
  struct fileentry * fl;
  char * fn;
{
  if (fl) {
	if (streq (fl->fn, fn)) {
		if (fl->prev) {
			return fl->prev->fn;
		} else {
			return NIL_PTR;
		}
	} else {
		return filelist_prev (fl->next, fn);
	}
  } else {
	return NIL_PTR;
  }
}

static
char *
backup_suffix (fn)
  char * fn;
{
  char * suffix = fn + strlen (fn) - 1;
  if (suffix >= fn && * suffix == '~') {
	char * suffe = suffix - 1;
	/* check emacs style numbered backup file name x.~N~*/
	while (suffe > fn && * suffe >= '0' && * suffe <= '9') {
		suffe --;
	}
	if (suffe < suffix - 1 && * suffe == '~') {
		suffe --;
		if (suffe >= fn && * suffe == '.') {
			return suffe;
		}
	}
	/* simple backup file name x~ */
	return suffix;
  } else {
	/* check VMS style numbered backup file name x;N */
	suffix = strrchr (fn, ';');
	if (suffix != NIL_PTR) {
		int ver = -1;
		char * afterver;
		suffix ++;
		afterver = scan_int (suffix, & ver);
		if (ver > 0 && * afterver == '\0') {
			suffix --;
			return suffix;
		}
	}
  }
  return NIL_PTR;
}

/**
   Add filename to File selector list.
   String must not be volatile.
 */
void
filelist_add (fn, allowdups)
  char * fn;
  FLAG allowdups;
{
  if (fn) {
	char * bs = backup_suffix (fn);
	if (allowdups && (bs != NIL_PTR)) {
		/* suppress subsequent backup/version names 
		   as generated by command line filename completion;
		   could be separate parameter but correlates with allowdups
		 */
		char basename [maxFILENAMElen];
		strcpy (basename, fn);
		basename [bs - fn] = '\0';	/* strip version suffix */
		filelist_append (& filelist, fn, allowdups, basename, 0);
	} else {
		filelist_append (& filelist, fn, allowdups, NIL_PTR, 0);
	}
  }
}

static
int
select_file ()
{
  int fi = 0;
  struct fileentry * fl = filelist;
  menuitemtype * filemenu;

  if (! filelist) {
	error ("No files opened");
	return ERRORS;
  }

  /* allocate menu structure */
  filemenu = alloc (filelist_count () * sizeof (menuitemtype));
  if (! filemenu) {
	error ("Cannot allocate memory for file menu");
	return ERRORS;
  }

  while (fl) {
	fill_menuitem (& filemenu [fi ++], fl->fn, NIL_PTR);
	fl = fl->next;
  }
  hop_flag = 0;
  fi = popup_menu (filemenu, filelist_count (), 0, 4, "Switch to file", True, False, "*");
  if (fi < 0) {
	return ERRORS;
  }

#ifdef keep_position_on_reload
  if (streq (file_name, filemenu [fi].itemname)) {
	(void) load_file_position (file_name, False, False, True, 
		line_number, get_cur_pos ());
  } else
#endif
  {
	Pushmark ();
	load_wild_file (filemenu [fi].itemname, False, True);
  }

  return FINE;
}


void
SELECTFILE ()
{
  if (modified && ! viewonly) {
#ifdef auto_save
	if (write_text () == ERRORS) {
		return;
	}
#else
	if (ask_save () != FINE) {
		return;
	}
#endif
  }

  (void) select_file ();
}

void
CLOSEFILE ()
{
  char * nextfn;

  if (modified && ! viewonly) {
#ifdef auto_save
	if (write_text () == ERRORS) {
		return;
	}
#else
	if (ask_save () != FINE) {
		return;
	}
#endif
  }

  nextfn = filelist_delete_next (& filelist, file_name);
  Pushmark ();
  load_wild_file (nextfn, False, True);
}

static
FLAG
nextfile ()
{
  char * nextfn = filelist_next (filelist, file_name);
  if (nextfn) {
	restore_screenmode ();
	Pushmark ();
	load_wild_file (nextfn, False, True);
	return True;
  } else {
	return False;
  }
}

static
void
edit_this_file (fn)
  char * fn;
{
  if (modified && ! viewonly) {
#ifdef auto_save
	if (write_text () == ERRORS) {
		return;
	}
#else
	if (ask_save () != FINE) {
		return;
	}
#endif
  }

  Pushmark ();
  load_wild_file (fn, False, True);
}

void
NXTFILE ()
{
  char * nextfn;
  if (hop_flag > 0) {
	nextfn = filelist_get (filelist_count () - 1);
  } else {
	nextfn = filelist_next (filelist, file_name);
  }

  if (nextfn) {
	edit_this_file (nextfn);
  } else {
	error ("Already at last file");
  }
}

void
PRVFILE ()
{
  char * prevfn;
  if (hop_flag > 0) {
	prevfn = filelist_get (0);
  } else {
	prevfn = filelist_prev (filelist, file_name);
  }

  if (prevfn) {
	edit_this_file (prevfn);
  } else {
	error ("Already at first file");
  }
}

void
edit_nth_file (n)
  int n;
{
  char * fn = n > 0 ? filelist_get (n - 1) : NIL_PTR;

  if (fn) {
	edit_this_file (fn);
  } else {
	error ("No such file");
  }
}


/*======================================================================*\
|*			Tag search with file change			*|
\*======================================================================*/

static
int
get_tagline (idf, filename, search)
  char * idf;
  char * filename;
  char * search;
{
  int tags_fd = open ("tags", O_RDONLY | O_BINARY, 0);
  if (tags_fd >= 0) {
	FLAG found = False;
	int dumlen;
	FLAG modif = modified;
	unsigned int len = strlen (idf);

	reset_get_line (False);
	flush (); /* obsolete?! clear the shared screen/get_line buffer! */
	while (/*found != VALID &&*/
		line_gotten (get_line (tags_fd, text_buffer, & dumlen, False)))
	{
	    if (strncmp (idf, text_buffer, len) == 0 && text_buffer [len] == '\t') {
		char * poi = text_buffer + len + 1;
		char * outpoi;
		char lastpat = '\0';

		found = True;

		outpoi = filename;
		while (* poi != '\0' && * poi != '\t') {
			* outpoi ++ = * poi ++;
		}
		* outpoi = '\0';

		outpoi = search;
		poi ++;
		if (* poi == '/') {
			poi ++;
		}
		while (* poi != '\0' && (* poi != '/' || lastpat == '\\')) {
			if (* poi == '[' || * poi == ']' || * poi == '*') {
				* outpoi ++ = '\\';
			}
			lastpat = * poi ++;
			* outpoi ++ = lastpat;
		}
		* outpoi = '\0';
	    } else if (found == True) {
		found = VALID;
	    }
	}
	(void) close (tags_fd);
	clear_filebuf ();

	modified = modif; /* don't let the tags file affect the modified flag */

	if (found == False) {
		error2 ("Identifier not found in tags file: ", idf);
		return ERRORS;
	} else {
		return FINE;
	}
  } else {
	error ("No tags file present; apply the ctags command to your source files");
	return ERRORS;
  }
}

/*
 * Stag () opens file and moves to idf, using tags file
 */
void
Stag ()
{
  char idf_buf [maxLINElen];	/* identifier to search for */
  char new_file [maxFILENAMElen];	/* new file name */
  char search [maxLINElen];	/* search expression */
  FLAG go_idf = True;

  if (hop_flag > 0) {
	if (get_string ("Enter identifier (to locate definition):", idf_buf, True, "") != FINE) {
		return;
	}
  } else if (cur_text == cur_line->text &&
	(* cur_text == '#' || strisprefix ("include", cur_text))) {
	char * cp = cur_text;
	if (* cp == '#') {
		cp ++;
	}
	while (white_space (* cp)) {
		cp ++;
	}
	if (strisprefix ("include", cp)) {
		char * ep = NIL_PTR;
		cp += 7;
		while (white_space (* cp)) {
			cp ++;
		}
		if (* cp == '"') {
			cp ++;
			ep = strchr (cp, '"');
			strcpy (new_file, "");
		} else if (* cp == '<') {
			cp ++;
			ep = strchr (cp, '>');
			strcpy (new_file, "/usr/include/");
		} else {
			ep = strchr (cp, '\n');
			strcpy (new_file, "");
		}
		if (ep && ep - cp < maxFILENAMElen - strlen (new_file)) {
			strncat (new_file, cp, ep - cp);
			strcpy (search, "");
			go_idf = False;
		} else {
			error ("No include file name");
			return;
		}
	}
  } else {
	if (get_idf (idf_buf, cur_text, cur_line->text) == ERRORS) {
		return;
	}
  }

  if (go_idf) {
	if (get_tagline (idf_buf, new_file, search) == ERRORS) {
		return;
	}
  }

  Pushmark ();

  if (! streq (new_file, file_name)) {
	FLAG save_lineends_detectCR = lineends_detectCR;
	/* force line counting compatible with ctags */
	lineends_detectCR = True;
	if (save_text_load_file (new_file) == ERRORS) {
		lineends_detectCR = save_lineends_detectCR;
		return;
	}
  }

  if (* search >= '0' && * search <= '9') {
	int lineno;
	LINE * line = header->next;

	(void) scan_int (search, & lineno);

	/* don't call goline for two reaons:
	   line # mismatch in presence of Mac or Unicode line ends
	   don't call Pushmark
	 */
	while (lineno > 1 && line != tail) {
		if (line->return_type == lineend_LF
		 || line->return_type == lineend_CR
		 || line->return_type == lineend_CRLF) {
			lineno --;
		}
		line = line->next;
	}
	clear_status ();
	move_y (find_y (line));
  } else {
	search_for (search, FORWARD, False);
  }
}


/*======================================================================*\
|*			Checkin/out					*|
\*======================================================================*/

/*
 * Checkout (from version managing system).
 */
void
checkout ()
{
  int save_cur_pos;
  int save_cur_line;
  char syscommand [maxCMDlen];	/* Buffer for full system command */
  int sysres;

	if (modified) {
		if (write_text () != FINE) {
			return;
		}
	}

	/* save current position */
	save_cur_line = line_number;
	save_cur_pos = get_cur_pos ();

	/* try to check out */
	build_string (syscommand, "co %s", file_name);
	sysres = systemcall (NIL_PTR, syscommand, 1);
	RDwin ();
	if (sysres != 0) {
		error ("Checkout failed");
	}

	/* reload file */
	(void) load_file_position (file_name, True, False, True, save_cur_line, save_cur_pos);
}

/*
 * Checkin (to version managing system).
 */
void
checkin ()
{
  char syscommand [maxCMDlen];	/* Buffer for full system command */
  int sysres;

	if (modified) {
		if (write_text () != FINE) {
			return;
		}
	}

	/* try to check in */
	build_string (syscommand, "ci %s", file_name);
	sysres = systemcall (NIL_PTR, syscommand, 1);
	RDwin ();
	if (sysres != 0) {
		error ("Checkin failed");
	}
}


/*======================================================================*\
|*			Exiting						*|
\*======================================================================*/

/*
 * Leave editor. If the file has changed, ask if the user wants to save it.
 */
void
QUED ()
{
  if (modified && viewonly == False && ask_save_defer_screenmode () != FINE) {
	restore_screenmode ();
	return;
  }

  quit_mined ();
  restore_screenmode ();
}

/*
 * Exit editing current file. If the file has changed, save it.
 * Edit next file if there is one.
 */
void
EXFILE ()
{
  if (modified) {
	if (write_text_defer_screenmode () != FINE) {
		restore_screenmode ();
		return;
	}
  }

  if (hop_flag == 0) {
	if (! nextfile ()) {
		quit_mined ();
		restore_screenmode ();
	}
  } else {
	quit_mined ();
	restore_screenmode ();
  }
}

/*
 * Exit editor. If the file has changed, save it.
 */
void
EXMINED ()
{
  if (modified) {
	if (write_text_defer_screenmode () != FINE) {
		restore_screenmode ();
		return;
	}
  }

  quit_mined ();
  restore_screenmode ();
}

/*
 * Exit editing current file.
   Either switch to next file or exit editor.
 */
void
EXED ()
{
  if (multiexit) {
	EXFILE ();
  } else {
	EXMINED ();
  }
}


/*======================================================================*\
|*			End						*|
\*======================================================================*/
