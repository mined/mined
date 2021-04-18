/*======================================================================*\
|*		Editor mined						*|
|*		CJK character set <-> Unicode mapping tables		*|
\*======================================================================*/

#include "mined.h"
#include "charcode.h"
#include "charprop.h"
#include "termprop.h"


/*======================================================================*\
|*			Character values				*|
\*======================================================================*/

unsigned char code_SPACE = ' ';
unsigned char code_TAB = '\t';
unsigned long code_LF = '\n';
unsigned long code_NL = CHAR_INVALID;


/*======================================================================*\
|*			Character properties				*|
\*======================================================================*/

FLAG
no_char (c)
  unsigned long c;
{
  return c == CHAR_UNKNOWN || c == CHAR_INVALID;
}

FLAG
no_unichar (u)
  unsigned long u;
{
  return u == CHAR_UNKNOWN || u == CHAR_INVALID;
}

/**
   Check if character is a control character in current encoding.
   (Should be more generic...)
 */
int
iscontrol (c)
  unsigned long c;
{
  if (mapped_text) {
	unsigned long u = lookup_encodedchar (c);
	return u == '\177' || (! no_unichar (u) && u < ' ');
  } else if (utf8_text) {
	if (unassigned_single_width) {
		if (rxvt_version > 0) {
			/* handle weird mapping of non-Unicode ranges */
			if (c < 0x80000000) {
				c &= 0x1FFFFF;
			}
		}
	}
	return c == '\177' || c < ' ';
  } else if (cjk_text) {
	return c == '\177' || c < ' ';
  } else {
	return c == '\177' || (c & '\177') < ' ';
  }
}

/**
   Check if character is any of the following white space characters:
	U+0009;<control>;Cc;0;S;;;;;N;CHARACTER TABULATION;;;;
	U+0020;SPACE;Zs;0;WS;;;;;N;;;;;
	U+00A0;NO-BREAK SPACE;Zs;0;CS;<noBreak> 0020;;;;N;NON-BREAKING SPACE;;;;
	U+2002;EN SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+2003;EM SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+2004;THREE-PER-EM SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+2005;FOUR-PER-EM SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+2006;SIX-PER-EM SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+2007;FIGURE SPACE;Zs;0;WS;<noBreak> 0020;;;;N;;;;;
	U+2008;PUNCTUATION SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+2009;THIN SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+200A;HAIR SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+200B;ZERO WIDTH SPACE;Cf;0;BN;;;;;N;;;;;
	U+202F;NARROW NO-BREAK SPACE;Zs;0;CS;<noBreak> 0020;;;;N;;;;;
	U+205F;MEDIUM MATHEMATICAL SPACE;Zs;0;WS;<compat> 0020;;;;N;;;;;
	U+3000;IDEOGRAPHIC SPACE;Zs;0;WS;<wide> 0020;;;;N;;;;;
 */
int
iswhitespace (c)
  unsigned long c;
{
  return c == ' ' || c == '\t' || c == 0xA0
	|| (c >= 0x2002 && c <= 0x200B) || c == 0x3000
	|| c == 0x202F || c == 0x205F
	|| c == 0xFEFF;
}

struct interval {
    unsigned long first;
    unsigned long last;
};
/*
static struct interval list_Quotation_Mark [] =
static struct interval list_Dash [] =
...
*/
#include "typoprop.t"

static
int
lookup (ucs, table, length)
  unsigned long ucs;
  struct interval * table;
  int length;
{
  int min = 0;
  int mid;
  int max = length - 1;

  if (ucs < table [0].first || ucs > table [max].last) {
	return 0;
  }
  while (max >= min) {
	mid = (min + max) / 2;
	if (ucs > table [mid].last) {
		min = mid + 1;
	} else if (ucs < table [mid].first) {
		max = mid - 1;
	} else {
		return 1;
	}
  }

  return 0;
}

/**
   Check if character is a quotation mark
 */
int
isquotationmark (unichar)
  unsigned long unichar;
{
  return lookup (unichar, list_Quotation_Mark, arrlen (list_Quotation_Mark));
}

/**
   Check if character is a dash
 */
int
isdash (unichar)
  unsigned long unichar;
{
  return lookup (unichar, list_Dash, arrlen (list_Dash));
}

/**
   Check if character is an opening parenthesis
 */
int
isopeningparenthesis (unichar)
  unsigned long unichar;
{
  return lookup (unichar, list_Ps, arrlen (list_Ps));
}

/**
   Return display indication for a control character.
 */
character
controlchar (c)
  character c;
{
  if (c == '\177') {
	return '?';
  } else {
	return c + '@';
  }
}


/**
   Return the isolated form of an ALEF character.
 */
unsigned long
isolated_alef (unichar)
  unsigned long unichar;
{
	if (unichar == 0x0622) {
		/* ALEF WITH MADDA ABOVE */
		return 0xFE81;
	} else if (unichar == 0x0623) {
		/* ALEF WITH HAMZA ABOVE */
		return 0xFE83;
	} else if (unichar == 0x0625) {
		/* ALEF WITH HAMZA BELOW */
		return 0xFE87;
	} else if (unichar == 0x0627) {
		/* ALEF */
		return 0xFE8D;
	} else {
		return 0xFE8D;
	}
}

/**
   Return the ligature with LAM of an ALEF character.
   Use the ISOLATED FORM.
 */
unsigned long
ligature_lam_alef (unichar)
  unsigned long unichar;
{
	if (unichar == 0x0622) {
		/* ALEF WITH MADDA ABOVE */
		return 0xFEF5;
	} else if (unichar == 0x0623) {
		/* ALEF WITH HAMZA ABOVE */
		return 0xFEF7;
	} else if (unichar == 0x0625) {
		/* ALEF WITH HAMZA BELOW */
		return 0xFEF9;
	} else if (unichar == 0x0627) {
		/* ALEF */
		return 0xFEFB;
	} else {
		return 0xFEFB;
	}
}


/**
   Return max value in current encoding.
 */
unsigned long
max_char_value ()
{
  if (cjk_text) switch (text_encoding_tag) {
	case 'G': return 0xFF39FF39;
	case 'C': return 0x8EFFFFFF;
	case 'J': return 0x8FFFFF;
	case 'X': return 0x8FFFFF;
	default: return 0xFFFF;
  } else if (utf8_text) {
	return 0x7FFFFFFF;
  } else {
	return 0xFF;
  }
}


/**
   Convert CJK character in current text encoding to byte sequence.
   Terminate with NUL byte.
   Return byte count.
 */
int
cjkencode (cjkchar, buf)
  unsigned long cjkchar;
  character * buf;
{
  return cjkencode_char (False, cjkchar, buf);
}

static
int
multi_char (term, c)
  FLAG term;
  character c;
{
  if (term) {
	return (character) c >= 0x80
		&& (! cjk_term 
		 || (term_encoding_tag != 'S' && term_encoding_tag != 'x')
		 || (character) c < 0xA1
		 || (character) c > 0xDF);
  } else {
	return multichar (c);
  }
}

/**
   Convert CJK character in terminal or text encoding to byte sequence.
 */
int
cjkencode_char (term, cjkchar, buf)
  FLAG term;
  unsigned long cjkchar;
  character * buf;
{
  int len = 0;
  int i;
  char encoding_tag = term ? term_encoding_tag : text_encoding_tag;

  if (cjkchar >= 0x1000000) {
	i = (cjkchar >> 16) & 0xFF;
	if (encoding_tag == 'G' && cjkchar >= 0x80000000
	 && i >= '0' && i <= '9') {
		len = 4;
	} else if (encoding_tag == 'C' && (cjkchar >> 24) == 0x8E) {
		len = 4;
	}
  } else if (cjkchar >= 0x10000) {
	if ((encoding_tag == 'J' || encoding_tag == 'X')
	    && (cjkchar >> 16) == 0x8F) {
		len = 3;
	}
  } else if (cjkchar >= 0x8000 && (cjkchar & 0xFF) > 0 &&
		multi_char (term, (character) (cjkchar >> 8))) {
	len = 2;
  } else if (cjkchar < 0x100 && ! multi_char (term, cjkchar)) {
	len = 1;
  }

  for (i = len - 1; i >= 0; i --) {
	buf [i] = cjkchar & 0xFF;
	cjkchar = cjkchar >> 8;
	if (buf [i] == '\0') {
		len = 0;
	}
  }
  buf [len] = '\0';

  return len;
}

/**
   Convert Unicode character to UTF-8.
   Terminate with NUL byte.
   Return byte count.
 */
int
utfencode (unichar, buf)
  unsigned long unichar;
  character * buf;
{
  int len;

  if (unichar < 0x80) {
	len = 1;
	* buf ++ = unichar;
  } else if (unichar < 0x800) {
	len = 2;
	* buf ++ = 0xC0 | (unichar >> 6);
	* buf ++ = 0x80 | (unichar & 0x3F);
  } else if (unichar < 0x10000) {
	len = 3;
	* buf ++ = 0xE0 | (unichar >> 12);
	* buf ++ = 0x80 | ((unichar >> 6) & 0x3F);
	* buf ++ = 0x80 | (unichar & 0x3F);
  } else if (unichar < 0x200000) {
	len = 4;
	* buf ++ = 0xF0 | (unichar >> 18);
	* buf ++ = 0x80 | ((unichar >> 12) & 0x3F);
	* buf ++ = 0x80 | ((unichar >> 6) & 0x3F);
	* buf ++ = 0x80 | (unichar & 0x3F);
  } else if (unichar < 0x4000000) {
	len = 5;
	* buf ++ = 0xF8 | (unichar >> 24);
	* buf ++ = 0x80 | ((unichar >> 18) & 0x3F);
	* buf ++ = 0x80 | ((unichar >> 12) & 0x3F);
	* buf ++ = 0x80 | ((unichar >> 6) & 0x3F);
	* buf ++ = 0x80 | (unichar & 0x3F);
  } else if (unichar < 0x80000000) {
	len = 6;
	* buf ++ = 0xFC | (unichar >> 30);
	* buf ++ = 0x80 | ((unichar >> 24) & 0x3F);
	* buf ++ = 0x80 | ((unichar >> 18) & 0x3F);
	* buf ++ = 0x80 | ((unichar >> 12) & 0x3F);
	* buf ++ = 0x80 | ((unichar >> 6) & 0x3F);
	* buf ++ = 0x80 | (unichar & 0x3F);
  } else {
	len = 0;
  }
  * buf = '\0';
  return len;
}

/**
   Convert character to byte sequence.
   Terminate with NUL byte.
   Return buffer pointer.
 */
char *
encode_char (c)
  unsigned long c;
{
  static char buf [7];
  if (utf8_text) {
	(void) utfencode (c, buf);
  } else if (cjk_text) {
	(void) cjkencode (c, buf);
  } else {
	buf [0] = c;
	buf [1] = '\0';
  }
  return buf;
}


/**
   Check if a character code is a valid CJK encoding pattern
   (not necessarily in the assigned ranges)
   of the currently active text encoding.
 */
FLAG
valid_cjk (cjkchar, cjkbytes)
  unsigned long cjkchar;
  character * cjkbytes;
{
  return valid_cjkchar (False, cjkchar, cjkbytes);
}

/**
   Check if a character code is a valid CJK encoding pattern
   (not necessarily in the assigned ranges)
   of the currently active terminal or text encoding.
 */
FLAG
valid_cjkchar (term, cjkchar, cjkbytes)
  FLAG term;
  unsigned long cjkchar;
  character * cjkbytes;
{
  character cjkbuf [5];
  char encoding_tag = term ? term_encoding_tag : text_encoding_tag;

  if (cjkchar < 0x80) {
	return True;
  }

  if (! cjkbytes) {
	cjkbytes = cjkbuf;
	(void) cjkencode_char (term, cjkchar, cjkbytes);
  }

  switch (encoding_tag) {
/*
		GB	GBK		81-FE	40-7E, 80-FE
			GB18030		81-FE	30-39	81-FE	30-39
		Big5	Big5-HKSCS	87-FE	40-7E, A1-FE
		CNS	EUC-TW		A1-FE	A1-FE
					8E	A1-A7	A1-FE	A1-FE
*/
    case 'G':	if (cjkchar > 0xFFFF) {
			return cjkbytes [0] >= 0x81 && cjkbytes [0] <= 0xFE
				&& cjkbytes [1] >= '0' && cjkbytes [1] <= '9'
				&& cjkbytes [2] >= 0x81 && cjkbytes [2] <= 0xFE
				&& cjkbytes [3] >= '0' && cjkbytes [3] <= '9';
		} else {
			return cjkbytes [0] >= 0x81 && cjkbytes [0] <= 0xFE
				&& cjkbytes [1] >= 0x40 && cjkbytes [1] <= 0xFE
				&& cjkbytes [1] != 0x7F;
		}
    case 'B':	return cjkbytes [0] >= 0x87 && cjkbytes [0] <= 0xFE
				&& ((cjkbytes [1] >= 0x40 && cjkbytes [1] <= 0x7E)
				    ||
				    (cjkbytes [1] >= 0xA1 && cjkbytes [1] <= 0xFE)
				   )
				&& cjkbytes [2] == 0;
    case 'C':	return (cjkbytes [0] >= 0xA1 && cjkbytes [0] <= 0xFE
			&& cjkbytes [1] >= 0xA1 && cjkbytes [1] <= 0xFE
			&& cjkbytes [2] == 0)
			||
			(cjkbytes [0] == 0x8E
			&& cjkbytes [1] >= 0xA1 && cjkbytes [1] <= 0xAF
			&& cjkbytes [2] >= 0xA1 && cjkbytes [2] <= 0xFE
			&& cjkbytes [3] >= 0xA1 && cjkbytes [3] <= 0xFE);
/*
		EUC-JP			8E	A1-DF
					A1-A8	A1-FE
					B0-F4	A1-FE
					8F	A2,A6,A7,A9-AB,B0-ED	A1-FE
					8F	A1-FE	A1-FE
		EUC-JIS X 0213		8E	A1-DF
					A1-FE	A1-FE
					8F	A1,A3-A5,A8,AC-AF,EE-FE	A1-FE
*/
    case 'X':
    case 'J':	return  (cjkbytes [0] >= 0xA1 && cjkbytes [0] <= 0xFE
			 && cjkbytes [1] >= 0xA1 && cjkbytes [1] <= 0xFE
			 && cjkbytes [2] == 0
			)
			||
			(cjkbytes [0] == 0x8E
			 && cjkbytes [1] >= 0xA1 && cjkbytes [1] <= 0xDF
			 && cjkbytes [2] == 0
			)
			||
			(cjkbytes [0] == 0x8F
			 && cjkbytes [1] >= 0xA1 && cjkbytes [1] <= 0xFE
			 && cjkbytes [2] >= 0xA1 && cjkbytes [2] <= 0xFE
			 && cjkbytes [3] == 0
			);
/*
		Shift-JIS		A1-DF
					81-84, 87-9F		40-7E, 80-FC
					E0-EA, ED-EE, FA-FC	40-7E, 80-FC
		Shift-JIS X 0213	A1-DF
					81-9F		40-7E, 80-FC
					E0-FC		40-7E, 80-FC
*/
    case 'x':
    case 'S':	return (cjkchar >= 0xA1 && cjkchar <= 0xDF)
			|| (cjkbytes [1] >= 0x40 && cjkbytes [1] <= 0xFC
			    && cjkbytes [1] != 0x7F && cjkbytes [2] == 0
			    && (
				(cjkbytes [0] >= 0x81 && cjkbytes [0] <= 0x9F)
			     || (cjkbytes [0] >= 0xE0 && cjkbytes [0] <= 0xFC)
			       )
			   );
/*
		UHC	UHC		81-FE	41-5A, 61-7A, 81-FE
		Johab			84-DE	31-7E, 81-FE
					E0-F9	31-7E, 81-FE
*/
    case 'K':	return cjkbytes [0] >= 0x81 && cjkbytes [0] <= 0xFE
			&& ((cjkbytes [1] >= 0x41 && cjkbytes [1] <= 0x5A)
			    ||
			    (cjkbytes [1] >= 0x61 && cjkbytes [1] <= 0x7A)
			    ||
			    (cjkbytes [1] >= 0x81 && cjkbytes [1] <= 0xFE)
			   )
			&& cjkbytes [2] == 0;
    case 'H':	return ((cjkbytes [0] >= 0x84 && cjkbytes [0] <= 0xDE)
			 ||
			(cjkbytes [0] >= 0xE0 && cjkbytes [0] <= 0xF9)
			)
			&&
			((cjkbytes [1] >= 0x31 && cjkbytes [1] <= 0x7E)
			 ||
			 (cjkbytes [1] >= 0x81 && cjkbytes [1] <= 0xFE)
			)
			&& cjkbytes [2] == 0;
    default:	return False;
  }
}


/*======================================================================*\
	Conversion tables mapping various CJK encodings to Unicode
\*======================================================================*/

#include "charmaps.h"

struct charmap_table_entry {
	struct encoding_table_entry * table;
	unsigned int * table_len;
	char * charmap;
	char * tag2;
	char tag1;
};

static struct charmap_table_entry charmaps_table [] = {
# ifdef __TURBOC__
	{cp437_table, & cp437_table_len, "CP437", "PC", 'p'},
	{cp850_table, & cp850_table_len, "CP850", "PL", 'P'},
# else
# include "charmaps.t"
# endif
};


/*======================================================================*\
|*			Configuration string matching			*|
\*======================================================================*/

/**
   matchprefix determines whether its first parameter contains its 
   second parameter matching approximately as an initial prefix,
   at word boundaries!
   The match ignores separating '-', '_', and space characters, 
   and does not match case.
   The algorithm assumes that letters are ASCII as this is used for 
   configuration strings only.
 */
static
int
matchprefix (s, m)
  char * s;
  char * m;
{
  do {
	char cs, cm;
	while (* m == '-' || * m == '_' || * m == ' ') {
		m ++;
	}
	while (* s == '-' || * s == '_' || * s == ' ') {
		s ++;
	}
	if (! * m) {
		return True;
#ifdef koi8_ru_fix
		/* approx. prefix match found; check word boundary */
		if ( (* s >= 'a' && * s <= 'z')
		  || (* s >= 'A' && * s <= 'Z')
		  || (* s >= '0' && * s <= '9')
		   ) {
			/* continue */
		} else {
			return True;
		}
#endif
	}
	if (! * s) {
		return False;
	}
	cs = * s;
	if (cs >= 'a' && cs <= 'z') {
		cs = cs - 'a' + 'A';
	}
	cm = * m;
	if (cm >= 'a' && cm <= 'z') {
		cm = cm - 'a' + 'A';
	}
	if (cm != cs) {
		return False;
	}
	s ++;
	m ++;
  } while (True);
}

/**
   matchpart determines whether its first parameter contains its 
   non-empty second parameter matching approximately as an initial 
   prefix or as a prefix of any part after a '/' or '>' separator,
   at word boundaries!
   The match ignores separating '-', '_', and space characters, 
   and does not match case.
   The algorithm assumes that letters are ASCII as this is used for 
   configuration strings only.
 */
static
int
matchpart (s, m)
  char * s;
  char * m;
{
  char * p;
  if (! * m) {
	return False;
  }
  if (matchprefix (s, m)) {
	return True;
  } else {
	p = strpbrk (s, ">/");
	if (p) {
		p ++;
		return matchpart (p, m);
	} else {
		return False;
	}
  }
}


/*======================================================================*\
|*			Mapping tables and functions			*|
\*======================================================================*/

/**
   Terminal character mapping table and its length
 */
static struct encoding_table_entry * terminal_table = (struct encoding_table_entry *) 0;
static unsigned int terminal_table_len = 0;

/**
   Current CJK/Unicode mapping table and its length
 */
static struct encoding_table_entry * text_table = (struct encoding_table_entry *) 0;
static unsigned int text_table_len = 0;


/**
   Are mapped text and terminal encodings different?
 */
FLAG
remapping_chars ()
{
  return text_table != terminal_table;
}


/**
   List of 2nd characters of 2 Unicode character mappings (mostly accents) 
   for certain 2-character CJK mappings (JIS or HKSCS);
   must be consistent with range and order of according #defines in charcode.h
 */
static unsigned int uni2_accents [] = 
	{0x309A, 0x0300, 0x0301, 0x02E5, 0x02E9, 0x0304, 0x030C};

/**
   Current encoding indications
 */
char text_encoding_tag = '-';
char * text_encoding_flag = "??";	/* for display in flags menu area */
char term_encoding_tag = '-';
static char * current_text_encoding = "";
static char * term_encoding = "";

/**
   Return charmap name of current text encoding.
 */
char *
get_text_encoding ()
{
  if (utf8_text) {
	if (utf16_file) {
		if (utf16_little_endian) {
			return "UTF-16LE";
		} else {
			return "UTF-16BE";
		}
	} else {
		return "UTF-8";
	}
  } else if (! cjk_text && ! mapped_text) {
	if (ebcdic_file) {
		return "CP1047";
	} else {
		return "ISO 8859-1";
	}
  } else {
	return current_text_encoding;
  }
}

/**
   Return charmap name of terminal encoding.
 */
char *
get_term_encoding ()
{
  if (utf8_screen) {
	return "UTF-8";
  } else if (! cjk_term && ! mapped_term) {
	return "ISO 8859-1";
  } else {
	return term_encoding;
  }
}


static FLAG combined_text;

/**
   Return True if active encoding has combining characters.
 */
FLAG
encoding_has_combining ()
{
  return utf8_text
	|| (mapped_text && combined_text)
	|| (cjk_text && combined_text);
}

/**
   Determine if active encoding has combining characters.
 */
static
FLAG
mapping_has_combining (term)
  FLAG term;
{
  unsigned long i;
  for (i = 0; i < 0x100; i ++) {
	unsigned long unichar;
	if (term) {
		unichar = lookup_mappedtermchar (i);
	} else {
		unichar = lookup_encodedchar (i);
	}
	if (term ? term_iscombining (unichar) : iscombining_unichar (unichar)) {
		return True;
	}
  }
  return False;
}

#ifdef split_map_entries
/*
   Decode CJK character value from split table entry.
 */
static
unsigned long
decode_cjk (entrypoi, map_table)
  struct encoding_table_entry * entrypoi;
  struct encoding_table_entry * map_table;
{
#ifdef use_CJKcharmaps
  if (map_table == gb_table) {
	if ((unsigned int) entrypoi->cjk_ext == 0xFF) {
		return entrypoi->cjk_base;
	} else {
		return ((entrypoi->cjk_base & 0x00FF) << 24)
			| (entrypoi->cjk_base & 0xFF00)
			| 0x00300030
			| ((((unsigned int) entrypoi->cjk_ext) & 0xF0) << 12)
			| (((unsigned int) entrypoi->cjk_ext) & 0x0F);
	}
  }
#endif

  if ((unsigned int) entrypoi->cjk_ext >= 0x90) {
	return 0x8E000000 | (((unsigned int) entrypoi->cjk_ext) << 16) | entrypoi->cjk_base;
  } else {
	return (((unsigned int) entrypoi->cjk_ext) << 16) | entrypoi->cjk_base;
  }
}
#endif

static
void
setup_mapping (term, map_table, map_table_len, tag1, tag2)
  FLAG term;
  struct encoding_table_entry * map_table;
  unsigned int map_table_len;
  char tag1;
  char * tag2;
{
  FLAG multi_byte = False;
  unsigned int j;

  if (term) {
	terminal_table = map_table;
	terminal_table_len = map_table_len;
	term_encoding_tag = tag1;
  } else {
	text_table = map_table;
	text_table_len = map_table_len;
	text_encoding_tag = tag1;
	text_encoding_flag = tag2;
  }

  /* check if it is a multi-byte mapping table */
  for (j = 0; j < map_table_len; j ++) {
	unsigned long cjki;
#ifdef split_map_entries
		cjki = decode_cjk (& map_table [j], map_table);
#else
		cjki = map_table [j].cjk;
#endif
	if (cjki > 0xFF) {
		multi_byte = True;
		break;
	}
  }

  if (term) {
	if (multi_byte) {
		cjk_term = True;
		mapped_term = False;
		/* combining_screen is auto-detected */
	} else {
		mapped_term = True;
		cjk_term = False;
		/* combining_screen is auto-detected */
	}
  } else {
	if (multi_byte) {
		cjk_text = True;
		mapped_text = False;
		combined_text = text_encoding_tag == 'G'
				|| text_encoding_tag == 'X'
				|| text_encoding_tag == 'x';
	} else {
		mapped_text = True;
		cjk_text = False;
		combined_text = mapping_has_combining (term);
	}
  }
}

/**
   Set either text or terminal character mapping table.
   Return True on success, False if tag unknown.
 */
static
FLAG
set_char_encoding (term, charmap, tag)
  FLAG term;
  char * charmap;
  char tag;
{
  if (term) {
	ascii_screen = False;
  }
  if (charmap && ! term
	&& (streq (":16", charmap) || matchpart ("UTF-16BE", charmap))) {
	utf8_text = True;
	utf16_file = True;
	utf16_little_endian = False;
	cjk_text = False;
	mapped_text = False;
	current_text_encoding = "UTF-16BE";
	text_encoding_flag = "16";
	return True;
  } else if (charmap && ! term
	&& (streq (":61", charmap) || matchpart ("UTF-16LE", charmap))) {
	utf8_text = True;
	utf16_file = True;
	utf16_little_endian = True;
	cjk_text = False;
	mapped_text = False;
	current_text_encoding = "UTF-16LE";
	text_encoding_flag = "61";
	return True;
  } else if (charmap && ! term && streq (":??", charmap)) {
	text_table_len = 0;
	text_encoding_tag = ' ';
	text_encoding_flag = "??";
	utf8_text = False;
	utf16_file = False;
	cjk_text = True;
	mapped_text = False;
	current_text_encoding = "[CJK]";
	return True;
  } else if (charmap ? strisprefix ("UTF-8", charmap) : tag == 'U') {
	if (term) {
		utf8_screen = True;
		utf8_input = True;
		cjk_term = False;
		mapped_term = False;
		term_encoding = "UTF-8";
		term_encoding_tag = 'U';
	} else {
		utf8_text = True;
		utf16_file = False;
		cjk_text = False;
		mapped_text = False;
		current_text_encoding = "UTF-8";
		text_encoding_flag = "U8";
	}
	return True;
  } else if (charmap ? matchpart ("ISO 8859-1", charmap) : tag == 'L') {
	if (term) {
		utf8_screen = False;
		utf8_input = False;
		cjk_term = False;
		mapped_term = False;
		term_encoding = "ISO 8859-1";
		term_encoding_tag = 'L';
	} else {
		utf8_text = False;
		utf16_file = False;
		cjk_text = False;
		mapped_text = False;
		current_text_encoding = "ISO 8859-1";
		text_encoding_flag = "L1";
	}
	return True;
  } else {
    int i;
    for (i = 0; i < arrlen (charmaps_table); i ++) {
	if (charmap ? (charmap [0] == ':' ?
			  streq (& charmap [1], charmaps_table [i].tag2)
			: matchpart (charmaps_table [i].charmap, charmap)
		      )
		    : charmaps_table [i].tag1 == tag) {
		if (term) {
			if (streq (charmaps_table [i].charmap, "CP1047")) {
				/* not supporting EBCDIC terminal */
				break;
			}
			utf8_screen = False;
			utf8_input = False;
			term_encoding = charmaps_table [i].charmap;
			if (streq (term_encoding, "ASCII")) {
				ascii_screen = True;
			}
		} else {
			utf8_text = False;
			utf16_file = False;
			current_text_encoding = charmaps_table [i].charmap;
		}
		setup_mapping (term, 
				charmaps_table [i].table, 
				* charmaps_table [i].table_len, 
				charmaps_table [i].tag1, 
				charmaps_table [i].tag2);
		return True;
	}
    }
  }
  return False;
}

static struct {
	char * alias;
	char * codepage;
} cpaliases [] = {
	{"CP819", "ISO-8859-1"},
	{"CP912", "ISO-8859-2"},
	{"CP913", "ISO-8859-3"},
	{"CP914", "ISO-8859-4"},
	{"CP915", "ISO-8859-5"},
	{"CP1089", "ISO-8859-6"},
	{"CP813", "ISO-8859-7"},
	{"CP916", "ISO-8859-8"},
	{"CP920", "ISO-8859-9"},
	{"CP919", "ISO-8859-10"},
	{"CP923", "ISO-8859-15"},
	{"CP28591", "ISO-8859-1"},
	{"CP28592", "ISO-8859-2"},
	{"CP28593", "ISO-8859-3"},
	{"CP28594", "ISO-8859-4"},
	{"CP28595", "ISO-8859-5"},
	{"CP28596", "ISO-8859-6"},
	{"CP28597", "ISO-8859-7"},
	{"CP28598", "ISO-8859-8"},	/* indicates visual ordering ... */
	{"CP28599", "ISO-8859-9"},
	{"CP28603", "ISO-8859-13"},
	{"CP28605", "ISO-8859-15"},
	{"CP38598", "ISO-8859-8"},	/* indicates logical ordering ... */
	{"CP20000", "CNS"},
	{"CP20127", "ASCII"},
	{"CP20866", "KOI8-R"},
	{"CP20936", "GB2312"},
	{"CP21866", "KOI8-U"},
	{"CP51949", "EUC-KR"},
	{"CP54936", "GB18030"},
	{"CP65001", "UTF-8"},
	{"CP932", "Shift-JIS"},
	{"CP936", "GBK"},
	{"CP949", "EUC-KR"},
	{"CP950", "Big5"},
	{"CP20932", "EUC-JP"},
};

/**
   Set terminal character code mapping table according to encoding tag.
   Return True on success, False if tag unknown.
 */
FLAG
set_term_encoding (charmap, tag)
  char * charmap;
  char tag;
{
  /* handle generic codepage notation (cygwin 1.7 / DOS support) */
  if (charmap && strisprefix ("CP", charmap)) {
	if (set_char_encoding (True, charmap, tag)) {
		return True;
	} else {
		int i;
		/* check DOS/Windows ISO-8859 codepage aliases */
		for (i = 0; i < arrlen (cpaliases); i ++) {
			if (streq (charmap, cpaliases [i].alias)) {
				if (set_char_encoding (True, cpaliases [i].codepage, tag)) {
					return True;
				}
			}
		}
		(void) set_char_encoding (True, "ASCII", ' ');
		return False;
	}
  }

  return set_char_encoding (True, charmap, tag);
}

#define dont_debug_set_text_encoding

/**
   Set character mapping table and text encoding variables according 
   to encoding tag.
   Return True on success, False if tag unknown.
 */
FLAG
set_text_encoding (charmap, tag, debug_tag)
  char * charmap;
  char tag;
  char * debug_tag;
{
  FLAG ret = set_char_encoding (False, charmap, tag);

#ifdef debug_set_text_encoding
  printf ("set_text_encoding [%s] %s [%c] -> %d: <%s>\n", debug_tag, charmap, tag, ret, get_text_encoding ());
#endif

  /* EBCDIC kludge */
  code_SPACE = encodedchar (' ');
  code_TAB = encodedchar ('\t');
  code_LF = encodedchar ('\n');
  code_NL = encodedchar (0x85);
  if (code_SPACE == ' ') {
	ebcdic_text = False;
	ebcdic_file = False;
  } else {
	ebcdic_text = True;
	/* or rather transform than map: */
	ebcdic_text = False;
	ebcdic_file = True;
	mapped_text = False;
  }

  return ret;
}

/*
   Look up a Unicode value in a character set mapping table.
   @return CJK value, or CHAR_INVALID if not found
 */
static
unsigned long
unmap_char (unichar, map_table, map_table_len, term)
  unsigned long unichar;
  struct encoding_table_entry * map_table;
  unsigned int map_table_len;
  FLAG term;
{
#ifdef split_map_entries
	unsigned char unichar_high = unichar >> 16;
	unsigned short unichar_low = unichar & 0xFFFF;
#endif
	int i;

	/* workaround for handling ambiguous mappings:
	   for terminal output, prefer longer code point:
	   term ? scan table downwards : scan table upwards
	 */
	term = False;	/* don't use this quirk-around */

	for (i = 0; i < map_table_len; i ++) {
		struct encoding_table_entry * map_table_poi =
			& map_table [term ? map_table_len - 1 - i : i];
#ifdef split_map_entries
		if (
		    unichar_low == map_table_poi->unicode_low
		 && unichar_high == map_table_poi->unicode_high
		   ) {
			return decode_cjk (map_table_poi, map_table);
		}
#else
		if (
		    unichar == map_table_poi->unicode
		   ) {
			return map_table_poi->cjk;
		}
#endif
	}
	return CHAR_INVALID;
}

/*
   Map a character in a character set mapping table.
   @return Unicode value, or CHAR_INVALID if not found
 */
static
unsigned long
map_char (cjk, map_table, map_table_len)
  unsigned long cjk;
  struct encoding_table_entry * map_table;
  unsigned int map_table_len;
{
	int low = 0;
	int high = map_table_len - 1;
	int i;

	unsigned long cjki;

	while (low <= high) {
		i = (low + high) / 2;
#ifdef split_map_entries
		cjki = decode_cjk (& map_table [i], map_table);
#else
		cjki = map_table [i].cjk;
#endif
		if (cjki == cjk) {
#ifdef split_map_entries
			if (map_table [i].unicode_high & 0x80) {
				return 0x80000000 | (uni2_accents [map_table [i].unicode_high & 0x7F] << 16) | (map_table [i].unicode_low);
			} else {
				return (((unsigned long) map_table [i].unicode_high) << 16) | (map_table [i].unicode_low);
			}
#else
			if (map_table [i].unicode & 0x800000) {
				return 0x80000000 | (uni2_accents [(map_table [i].unicode >> 16) & 0x7F] << 16) | (map_table [i].unicode & 0xFFFF);
			} else {
				return map_table [i].unicode;
			}
#endif
		} else if (cjki >= cjk) {
			high = i - 1;
		} else {
			low = i + 1;
		}
	}
	return CHAR_INVALID;
}


/*======================================================================*\
|*		Conversion functions					*|
\*======================================================================*/

/**
   GB18030 algorithmic mapping part
 */
static
unsigned long
gb_to_unicode (gb)
  unsigned long gb;
{
	unsigned int byte2 = (gb >> 16) & 0xFF;
	unsigned int byte3 = (gb >> 8) & 0xFF;
	unsigned int byte4 = gb & 0xFF;

	if (byte2 < '0' || byte2 > '9' || byte3 < 0x81 || byte4 < '0' || byte4 > '9') {
		return CHAR_INVALID;
	}

	return (((((gb >> 24) & 0xFF) - 0x90) * 10
		+ (byte2 - 0x30)) * 126L
		+ (byte3 - 0x81)) * 10L
		+ (byte4 - 0x30)
		+ 0x10000;
}

static
unsigned long
unicode_to_gb (uc)
  unsigned long uc;
{
	unsigned int a, b, c, d;

	if (uc >= 0x200000) {
		return CHAR_INVALID;
	}

	uc -= 0x10000;
	d = 0x30 + uc % 10;
	uc /= 10;
	c = 0x81 + uc % 126;
	uc /= 126;
	b = 0x30 + uc % 10;
	uc /= 10;
	a = 0x90 + uc;

	return (a << 24) | (b << 16) | (c << 8) | d;
}


/*
   mapped_char () converts a Unicode value into an encoded character,
   using the table given as parameter.
 */
static
unsigned long
mapped_char (unichar, map_table, map_table_len, term)
  unsigned long unichar;
  struct encoding_table_entry * map_table;
  unsigned int map_table_len;
  FLAG term;
{
	unsigned long cjkchar;

#ifdef use_CJKcharmaps
	if (map_table == gb_table && unichar >= 0x10000) {
		return unicode_to_gb (unichar);
	}
#endif

	cjkchar = unmap_char (unichar, map_table, map_table_len, term);
	if (cjkchar != CHAR_INVALID) {
		return cjkchar;
	}

	if (unichar < 0x20) {
		/* transparently return control range (for commands) */
		return unichar;
	} else if (unichar < 0x80) {
		/* transparently map ASCII range unless mapped already */
		cjkchar = unichar;
		unichar = map_char (cjkchar, map_table, map_table_len);
		if (! no_unichar (unichar) && unichar != cjkchar) {
			return CHAR_INVALID;
		} else {
			return cjkchar;
		}
	} else {
		/* notify "not found" */
		return CHAR_INVALID;
	}
}

/*
   mappedtermchar () converts a Unicode value into an encoded character,
   using the terminal encoding (terminal_table).
 */
unsigned long
mappedtermchar (unichar)
  unsigned long unichar;
{
	return mapped_char (unichar, terminal_table, terminal_table_len, True);
}

/*
   encodedchar () converts a Unicode value into an encoded character,
   using the current text encoding (text_table).
 */
unsigned long
encodedchar (unichar)
  unsigned long unichar;
{
  if (cjk_text || mapped_text) {
	return mapped_char (unichar, text_table, text_table_len, False);
  } else if (utf8_text || unichar < 0x100) {
	return unichar;
  } else {
	return CHAR_INVALID;
  }
}

/*
   encodedchar2 () converts two Unicode values into one JIS character,
   using the current text encoding (text_table).
 */
unsigned long
encodedchar2 (uc1, uc2)
  unsigned long uc1;
  unsigned long uc2;
{
  int i;
  for (i = 0; i < arrlen (uni2_accents); i ++) {
	if (uni2_accents [i] == uc2) {
		unsigned long unichar = uc1 | ((0x80 + i) << uni2tag_shift);
		return mapped_char (unichar, text_table, text_table_len, False);
	}
  }
  return CHAR_INVALID;
}


/*
   lookup_mapped_char () converts an encoded character to Unicode,
   using the table given as parameter.
 */
static
unsigned long
lookup_mapped_char (cjk, map_table, map_table_len)
  unsigned long cjk;
  struct encoding_table_entry * map_table;
  unsigned int map_table_len;
{
	unsigned long unichar;

#ifdef use_CJKcharmaps
	if (map_table == gb_table && cjk >= 0x90000000) {
		return gb_to_unicode (cjk);
	}
#endif

	unichar = map_char (cjk, map_table, map_table_len);
	if (! no_unichar (unichar)) {
		return unichar;
	} else if (cjk < 0x80) {
		/* transparently map ASCII range unless mapped already */
		return cjk;
	} else {
		/* notify "not found" */
		return CHAR_INVALID;
	}
}

/*
   lookup_mappedtermchar () converts an encoded character to Unicode,
   using the terminal encoding (terminal_table).
 */
unsigned long
lookup_mappedtermchar (cjk)
  unsigned long cjk;
{
	return lookup_mapped_char (cjk, terminal_table, terminal_table_len);
}

/*
   lookup_encodedchar () converts an encoded character to Unicode,
   using the current text encoding (text_table).
 */
unsigned long
lookup_encodedchar (cjk)
  unsigned long cjk;
{
  if (cjk_text || mapped_text) {
	return lookup_mapped_char (cjk, text_table, text_table_len);
  } else if (utf8_text || cjk < 0x100) {
	return cjk;
  } else {
	return CHAR_INVALID;
  }
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
