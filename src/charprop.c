/*======================================================================*\
|*		Editor mined						*|
|*		Character properties					*|
\*======================================================================*/

#include "mined.h"
#include "charprop.h"


/* #include "charname.t" ? */
#if defined (__TURBOC__) || defined (VAX) || defined (__DJGPP__)
#define NOCHARNAMES
#endif

/* #include "decompos.t" ? */
#if defined (__TURBOC__) || defined (VAX)
#define NODECOMPOSE
#endif


/*======================================================================*\
|*			Character properties				*|
\*======================================================================*/

#include "scriptdf.t"

static struct scriptentry scripttable [] = {
/* this is checked also for word moves and identifier searches... */
#include "scripts.t"
};


#define compact_charname_t

#ifdef compact_charname_t
struct charnameentry {
	char * charname_e;
};
#else
struct charnameentry {
	unsigned long u;
	char * charname;
};
#endif

static struct charnameentry charnametable [] = {
#ifdef NOCHARNAMES
# ifdef compact_charname_t
	{"\x00\x00\x00"},
# else
	{0, ""},
# endif
#else
#include "charname.t"
#endif
};

static struct charseqentry {
	char * name;
	unsigned long u [4];
} charseqtable [] = {
#ifdef NOCHARNAMES
	{"", 0},
#else
#include "charseqs.t"
#endif
};


typedef enum {
	decomp_canonical,
	decomp_circle,
	decomp_compat,
	decomp_final,
	decomp_font,
	decomp_fraction,
	decomp_initial,
	decomp_isolated,
	decomp_medial,
	decomp_narrow,
	decomp_noBreak,
	decomp_small,
	decomp_square,
	decomp_sub,
	decomp_super,
	decomp_vertical,
	decomp_wide
} decomposetype;
struct decomposeentry {
	unsigned long u;
	decomposetype decomposition_type;
	unsigned long decomposition_mapping [18];
};
static char * decomposition_type [] = {
	/* decomp_canonical */	"",
	/* decomp_circle */	" <encircled>",
	/* decomp_compat */	" <compatibility>",
	/* decomp_final */	" <final form>",
	/* decomp_font */	" <font variant>",
	/* decomp_fraction */	" <fraction>",
	/* decomp_initial */	" <initial form>",
	/* decomp_isolated */	" <isolated form>",
	/* decomp_medial */	" <medial form>",
	/* decomp_narrow */	" <narrow/hankaku>",
	/* decomp_noBreak */	" <no-break>",
	/* decomp_small */	" <small variant>",
	/* decomp_square */	" <squared font variant>",
	/* decomp_sub */	" <subscript>",
	/* decomp_super */	" <superscript>",
	/* decomp_vertical */	" <vertical layout form>",
	/* decomp_wide */	" <wide/zenkaku>",
};
static struct decomposeentry decomposetable [] = {
#ifdef NODECOMPOSE
	{0, 0, 0},
#else
#include "decompos.t"
#endif
};


static char decomposition_str [maxMSGlen];

/*
   Lookup character decomposition of Unicode character.
 */
static
unsigned long *
decomposition_lookup (ucs, typepoi)
  unsigned long ucs;
  decomposetype * typepoi;
{
  int min = 0;
  int max = sizeof (decomposetable) / sizeof (struct decomposeentry) - 1;
  int mid;

  /* binary search in table */
  while (max >= min) {
    mid = (min + max) / 2;
    if (decomposetable [mid].u < ucs) {
      min = mid + 1;
    } else if (decomposetable [mid].u > ucs) {
      max = mid - 1;
    } else {
	decomposetype t = decomposetable [mid].decomposition_type;
	if (t >= arrlen (decomposition_type)
#ifndef __clang__
	/* clang would whine about 'tautological-compare' here 
	   but that is not true for all C compilers (e.g. Sun Studio), 
	   so let's be defensive */
	   || t < 0
#endif
	   ) {
		return 0;
	} else {
		* typepoi = t;
		return decomposetable [mid].decomposition_mapping;
	}
    }
  }

  return 0;
}

/*
   Determine character decomposition of Unicode character.
   Return as string.
 */
char *
decomposition_string (ucs)
  unsigned long ucs;
{
  decomposetype type;
  unsigned long * decomp = decomposition_lookup (ucs, & type);
  if (decomp == 0) {
	return 0;
  } else {
	int i = 0;

	strcpy (decomposition_str, decomposition_type [type]);

	while (i < arrlen (decomposetable [0].decomposition_mapping) && decomp [i]) {
		char su [9];
		build_string (su, " U+%04lX", decomp [i]);
		strcat (decomposition_str, su);
		i ++;
	}

	return decomposition_str;
  }
}

/*
   Determine character decomposition base character of Unicode character.
 */
unsigned long
decomposition_base (ucs)
  unsigned long ucs;
{
  decomposetype type;
  unsigned long * decomp = decomposition_lookup (ucs, & type);
  if (decomp == 0) {
	if (ucs == (unsigned char) 'ß') {
		return 's';
	} else {
		return 0;
	}
  } else {
	if (decomp [0] == 0x28 && decomp [1]) {
		/* PARENTHESIZED form */
		return decomp [1];
	} else {
		return decomp [0];
	}
  }
}


/*
   Determine Unicode named sequence.
 */
char *
charseqname (u0, follow, lenpoi, seqpoi)
  unsigned long u0;
  char * follow;
  int * lenpoi;
  unsigned long * * seqpoi;
{
  int i;
  unsigned long u1 = CHAR_UNKNOWN;
  unsigned long u2 = CHAR_UNKNOWN;
  unsigned long u3 = CHAR_UNKNOWN;
  for (i = 0; i < arrlen (charseqtable); i ++) {
	if (u0 == charseqtable [i].u [0]) {
		* seqpoi = charseqtable [i].u;
		if (u1 == CHAR_UNKNOWN && * follow && * follow != '\n') {
			u1 = unicodevalue (follow);
			advance_char (& follow);
			if (* follow && * follow != '\n') {
				u2 = unicodevalue (follow);
				advance_char (& follow);
			}
			if (* follow && * follow != '\n') {
				u3 = unicodevalue (follow);
				advance_char (& follow);
			}
		}
		if (u1 == charseqtable [i].u [1]) {
			if (! charseqtable [i].u [2]) {
				* lenpoi = 2;
				return charseqtable [i].name;
			} else if (u2 == charseqtable [i].u [2]) {
				if (! charseqtable [i].u [3]) {
					* lenpoi = 3;
					return charseqtable [i].name;
				} else if (u3 == charseqtable [i].u [3]) {
					* lenpoi = 4;
					return charseqtable [i].name;
				}
			}
		}
	}
  }
  return NIL_PTR;
}

/*
   Determine character name of Unicode character.
 */
char *
charname (ucs)
  unsigned long ucs;
{
  int min = 0;
  int max = sizeof (charnametable) / sizeof (struct charnameentry) - 1;
  int mid;

  /* binary search in table */
  while (max >= min) {
    unsigned long midu;
#ifdef compact_charname_t
    unsigned char * mide;
    mid = (min + max) / 2;
    mide = (unsigned char *) charnametable [mid].charname_e;
    midu = (mide [0] << 16) + (mide [1] << 8) + mide [2];
#else
    mid = (min + max) / 2;
    midu = charnametable [mid].u;
#endif
    if (midu < ucs) {
      min = mid + 1;
    } else if (midu > ucs) {
      max = mid - 1;
    } else {
#ifdef compact_charname_t
      return (char *) mide + 3;
#else
      return charnametable [mid].charname;
#endif
    }
  }

  return 0;
}

/*
   Determine script info of Unicode character according to script range table.
 */
struct scriptentry *
scriptinfo (ucs)
  unsigned long ucs;
{
  int min = 0;
  int max = sizeof (scripttable) / sizeof (struct scriptentry) - 1;
  int mid;

  /* binary search in table */
  while (max >= min) {
    mid = (min + max) / 2;
    if (scripttable [mid].last < ucs) {
      min = mid + 1;
    } else if (scripttable [mid].first > ucs) {
      max = mid - 1;
    } else if (scripttable [mid].first <= ucs && scripttable [mid].last >= ucs) {
      return & scripttable [mid];
    }
  }

  return 0;
}

char *
script (ucs)
  unsigned long ucs;
{
  struct scriptentry * se = scriptinfo (ucs);
  if (se) {
	return category_names [se->scriptname];
  } else {
	return "";
  }
}

char *
category (ucs)
  unsigned long ucs;
{
  struct scriptentry * se = scriptinfo (ucs);
  if (se) {
	return category_names [se->categoryname];
  } else {
	return "";
  }
}


int
is_right_to_left (ucs)
  unsigned long ucs;
{
  if (ucs < 0x0590)
      return 0;

  return
     (ucs >= 0x0590 && ucs <= 0x05FF)	/* Hebrew */
  || (ucs >= 0xFB1D && ucs <= 0xFB4F)	/* Hebrew presentation forms */
  || (ucs >= 0x0600 && ucs <= 0x07FF)	/* Arabic, Syriac, Thaana, NKo */
  || (ucs >= 0xFB50 && ucs <= 0xFDFF)	/* Arabic presentation forms A */
  || (ucs >= 0xFE70 && ucs <= 0xFEFF)	/* Arabic presentation forms B */
  || (ucs >= 0x0800 && ucs <= 0x08FF)	/* Samaritan, Mandaic, ..., Arabic Ext-A */
  || (ucs == 0x200F)			/* right-to-left mark */
#ifdef RLmarks
  || (ucs == 0x202B)			/* right-to-left embedding */
  || (ucs == 0x202E)			/* right-to-left override */
#endif
  || (ucs >= 0x10800 && ucs <= 0x10FFF)
  || (ucs >= 0x1E800 && ucs <= 0x1EFFF)	/* ..., Arabic Mathematical, ... */
  ;
}


struct hanentry *
lookup_handescr (unichar)
  unsigned long unichar;
{
  int min = 0;
  int max = hantable_len - 1;
  int mid;

  /* binary search in table */
  while (max >= min) {
	mid = (min + max) / 2;
	if (hantable [mid].unicode < unichar) {
		min = mid + 1;
	} else if (hantable [mid].unicode > unichar) {
		max = mid - 1;
	} else {
		/* construct struct hanentry from raw_hanentry */
		static struct hanentry han;
		char * text = hantable [mid].text;

		han.unicode = hantable [mid].unicode;
		han.Mandarin = text;
		text += strlen (text) + 1;
		han.Cantonese = text;
		text += strlen (text) + 1;
		han.Japanese = text;
		text += strlen (text) + 1;
		han.Sino_Japanese = text;
		text += strlen (text) + 1;
		han.Hangul = text;
		text += strlen (text) + 1;
		han.Korean = text;
		text += strlen (text) + 1;
		han.Vietnamese = text;
		text += strlen (text) + 1;
		han.HanyuPinlu = text;
		text += strlen (text) + 1;
		han.HanyuPinyin = text;
		text += strlen (text) + 1;
		han.XHCHanyuPinyin = text;
		text += strlen (text) + 1;
		han.Tang = text;
		text += strlen (text) + 1;
		han.Definition = text;

		return & han;
	}
  }
  return 0;
}


FLAG
is_bullet_or_dash (unich)
  unsigned long unich;
{
  char * chname = charname (unich);
  if (unich == 0xB7) {	/* MIDDLE DOT */
	return True;
  }
  if (unich == 0x2015) {	/* HORIZONTAL BAR / QUOTATION DASH */
	return True;
  }
  if (chname != NIL_PTR) {
	char * bull = strstr (chname, "BULLET");
	if (bull != NIL_PTR && strlen (bull) == 6) {
		return True;
	}
	bull = strstr (chname, "DASH");
	if (bull != NIL_PTR && strlen (bull) == 4) {
		return True;
	}
  }
  return False;
}


/*======================================================================*\
|*			Some Unicode character properties		*|
\*======================================================================*/

struct interval {
    unsigned long first;
    unsigned long last;
};

static
int
lookup (ucs, table, len)
  unsigned long ucs;
  struct interval * table;
  int len;
{
  int min = 0;
  int mid;
  int max = len - 1;

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


FLAG
isLetter (unichar)
  unsigned long unichar;
{
	char * cat = category (unichar);
	return streq (cat, "Letter");
}

/* struct interval list_wide [] */
#include "wide.t"

FLAG
is_wideunichar (ucs)
  unsigned long ucs;
{
  return lookup (ucs, list_wide, arrlen (list_wide));
}


struct caseconv_entry caseconv_table [] = {
#include "casetabl.t"
};

#define caseconv_table_size	(sizeof (caseconv_table) / sizeof (* caseconv_table))


struct caseconv_special_entry caseconv_special [] = {
#include "casespec.t"
};

#define caseconv_special_size	(sizeof (caseconv_special) / sizeof (* caseconv_special))


/* struct interval list_Soft_Dotted [] */
#include "softdot.t"


static struct {
    unsigned long first;
    unsigned long last;
    char category;
    short combining_class;
} combining_classes [] = {
#include "combin.t"
};

/**
   Look up combining class;
   return: if not found, -1
           if category is "Spacing Combining" (Mc): -1 - combining class
           else combining class
 */
static
int
combining_class (ucs)
  unsigned long ucs;
{
  int min = 0;
  int mid;
  int max = arrlen (combining_classes) - 1;

  if (ucs < combining_classes [0].first) {
	return -1;
  }
  while (max >= min) {
	mid = (min + max) / 2;
	if (ucs > combining_classes [mid].last) {
		min = mid + 1;
	} else if (ucs < combining_classes [mid].first) {
		max = mid - 1;
	} else {
		if (combining_classes [mid].category == 'c') {
			return -2 - combining_classes [mid].combining_class;
		} else {
			return combining_classes [mid].combining_class;
		}
	}
  }
  return -1;
}

FLAG
iscombining_unichar (ucs)
  unsigned long ucs;
{
#ifdef spacingcombining_isnt_combining
  return combining_class (ucs) >= 0;
#else
  return combining_class (ucs) != -1;
#endif
}

FLAG
isspacingcombining_unichar (ucs)
  unsigned long ucs;
{
  return combining_class (ucs) <= -2;
}

int
soft_dotted (ucs)
  unsigned long ucs;
{
  return lookup (ucs, list_Soft_Dotted, arrlen (list_Soft_Dotted));
}


int
lookup_caseconv (basechar)
  unsigned long basechar;
{
	int low = 0;
	int high = caseconv_table_size - 1;
	int i;

	while (low <= high) {
		i = (low + high) / 2;
		if (caseconv_table [i].base == basechar) {
			return i;
		} else if (caseconv_table [i].base >= basechar) {
			high = i - 1;
		} else {
			low = i + 1;
		}
	}
	/* notify "not found" */
	return -1;
}

/**
   case_convert converts a Unicode character to
   +2: title case
   +1: upper case
   -1: lower case
 */
unsigned long
case_convert (unichar, dir)
  unsigned long unichar;
  int dir;
{
  int tabix = lookup_caseconv (unichar);

  if (tabix >= 0) {
	if (dir == 2 && caseconv_table [tabix].title != 0) {
		return caseconv_table [tabix].title;
	} else if (dir > 0 && caseconv_table [tabix].toupper != 0) {
		return unichar + caseconv_table [tabix].toupper;
	} else if (dir < 0 && caseconv_table [tabix].tolower != 0) {
		return unichar + caseconv_table [tabix].tolower;
	}
  }

  return unichar;
}

int
lookup_caseconv_special (basechar, langcond)
  unsigned long basechar;
  short langcond;
{
  int i;
#ifdef caseconvsearch_uncond
  int low = 0;
  int high = caseconv_special_size - 1;

	/* plain binary search is not applicable as keys are ambiguous */
	while (low <= high) {
		i = (low + high) / 2;
		if (caseconv_special [i].base == basechar) {
			return i;
		} else if (caseconv_special [i].base >= basechar) {
			high = i - 1;
		} else {
			low = i + 1;
		}
	}
#else
	for (i = 0; i < caseconv_special_size; i ++) {
		if (caseconv_special [i].base == basechar) {
			short langcondi = caseconv_special [i].condition & U_conds_lang;
			if (langcondi == 0 || (langcondi & langcond)) {
				return i;
			}
		}
	}
#endif
	/* notify "not found" */
	return -1;
}

int
iscombining_notabove (unichar)
  unsigned long unichar;
{
  int cc = combining_class (unichar);
  return cc > 0 && cc != 230;
}

int
iscombining_above (unichar)
  unsigned long unichar;
{
  return combining_class (unichar) == 230;
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
