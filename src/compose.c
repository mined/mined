/*======================================================================*\
|*		Editor mined						*|
|*		Character composition					*|
\*======================================================================*/

#include "mined.h"
#include "charprop.h"
#include "io.h"	/* for modifier _mask definitions */


/*======================================================================*\
|*			Accented composition				*|
|*			using prefix function keys			*|
|*			or character prefix combinations		*|
\*======================================================================*/

/*
   Mappings from prefix function keys to mnemonic sequences 
   to be used for composition with subsequent character.
   Vietnamese input method found on
	http://www.vovisoft.com/vovisoft/UnicodeChoVN.htm
   and corresponding RFC 1345 mnemonics:
	á : Ctrl+1, A	ACUTE
		A'	ACUTE
	à : Ctrl+2, A	GRAVE
		N!	GRAVE
	ả : Ctrl+3, A	HOOK ABOVE
		A2	HOOK ABOVE
	ã : Ctrl+4, A	TILDE
		A?	TILDE
	ạ : Ctrl+5, A	DOT BELOW
		T-.	DOT BELOW
	â : Ctrl+6, A	CIRCUMFLEX
		A>	CIRCUMFLEX
	ă : Ctrl+7, A	BREVE
		A(	BREVE
	ư : Ctrl+8, U	HORN
		O9	HORN
	đ : Ctrl+9, D	STROKE
		v/	STROKE
	ấ : Alt+1, A	CIRCUMFLEX AND ACUTE
		A>'	CIRCUMFLEX AND ACUTE
	ầ : Alt+2, A	CIRCUMFLEX AND GRAVE
		A>!	CIRCUMFLEX AND GRAVE
	ẩ : Alt+3, A	CIRCUMFLEX AND HOOK ABOVE
		A>2	CIRCUMFLEX AND HOOK ABOVE
	ẫ : Alt+4, A	CIRCUMFLEX AND TILDE
		A>?	CIRCUMFLEX AND TILDE
	ậ : Alt+5, A	CIRCUMFLEX AND DOT BELOW
		A>-.	CIRCUMFLEX AND DOT BELOW
	ắ : Ctrl+Alt+1, A	BREVE/HORN AND ACUTE
		A('		BREVE AND ACUTE
		O9'		HORN AND ACUTE
	ằ : Ctrl+Alt+2, A	BREVE/HORN AND GRAVE
		A(!		BREVE AND GRAVE
		O9!		HORN AND GRAVE
	ẳ : Ctrl+Alt+3, A	BREVE/HORN AND HOOK ABOVE
		A(2		BREVE AND HOOK ABOVE
		O92		HORN AND HOOK ABOVE
	ẵ : Ctrl+Alt+4, A	BREVE/HORN AND TILDE
		A(?		BREVE AND TILDE
		O9?		HORN AND TILDE
	ặ : Ctrl+Alt+5, A	BREVE/HORN AND DOT BELOW
		A(-.		BREVE AND DOT BELOW
		O9-.		HORN AND DOT BELOW
 */
static struct prefixspec prefixmap [] = {
	/* function keys */
	{F5, altshift_mask, "breve/vrachy", "˘", "x("},
	{F5, ctrlshift_mask, "ogonek/prosgegrammeni", "˛", "x;"},
	{F5, alt_mask, "stroke", "/", "x/"},
	{F5, shift_mask, "tilde/perispomeni", "~", "~x", "x?"},
	{F5, ctrl_mask, "ring/cedilla/iota", "˚/¸/ͺ", "°x", "x0", "x,"},
	{F5, 0, "diaeresis (umlaut)/dialytika", "¨", "\"x", "x:"},
	{F6, altshift_mask, "dot above/dasia", "˙/῾", "x."},
	{F6, ctrlshift_mask, "macron/descender", "¯", "x-"},
	{F6, alt_mask, "háček (caron)/psili", " ̌/᾿", "x<", "x,,"},
	{F6, shift_mask, "grave/varia", "`", "`x", "x!"},
	{F6, ctrl_mask, "circumflex/oxia", "^", "^x", "x>", "x´"},
	{F6, 0, "acute (d'aigu)/tonos", "´", "'x", "x´", "x'"},

	/* modified punctuation keys and spacing accents */
	{COMPOSE, '\'', "acute (d'aigu)/tonos", "´", "'x", "x´", "x'"},
	{COMPOSE, 0x00B4 /*´*/, "acute/oxia", "´", "x´", "´x", "x'"},
	{COMPOSE, 0x02DD /*˝*/, "double acute", "˝", "x''"},
	{COMPOSE, '`', "grave/varia", "`", "`x", "x!"},
	{COMPOSE, '^', "circumflex/oxia", "^", "^x", "x>", "x´"},
	{COMPOSE, '~', "tilde/perispomeni", "~", "~x", "x?"},
	{COMPOSE, 0x02DC /*˜*/, "tilde/perispomeni", "˜", "~x", "x?"},
	{COMPOSE, ':', "diaeresis (umlaut)/dialytika", "¨", "\"x", "x:"},
	{COMPOSE, '"', "diaeresis (umlaut)/dialytika", "¨", "\"x", "x:"},
	{COMPOSE, 0x00A8 /*¨*/, "diaeresis (umlaut)/dialytika", "¨", "\"x", "x:"},
	{COMPOSE, ',', "cedilla/ring/iota", "¸/˚/ͺ", "x,", "°x", "x0"},
	{COMPOSE, 0x00B0 /*°, actually degree sign*/, "ring/cedilla/iota", "˚/¸/ͺ", "°x", "x0", "x,"},
	{COMPOSE, 0x02DA /*˚*/, "ring above", "˚/¸/ͺ", "°x", "x0", "x,"},
	{COMPOSE, 0x00B8 /*¸*/, "cedilla/ring/iota", "¸/˚/ͺ", "x,", "°x", "x0"},
	{COMPOSE, 0x037A /*ͺ*/, "ypogegrammeni", "ͺ/¸/˚", "°x", "x,", "x0"},
	{COMPOSE, 0x0384 /*΄*/, "tonos", "΄", "x'"},
	{COMPOSE, 0x0385 /*΅*/, "dialytika and tonos", "΅", "x:'"},
	{COMPOSE, '/', "stroke", "/", "x/"},
	{COMPOSE, '-', "macron/descender", "¯", "x-"},
	{COMPOSE, 0x00AF /*¯*/, "macron/descender", "¯", "x-"},
	{COMPOSE, '<', "háček (caron)/psili", " ̌/᾿", "x<", "x,,"},
	{COMPOSE, '.', "dot above/dasia", "˙/῾", "x."},
	{COMPOSE, 0x02D9 /*˙*/, "dot above/dasia", "˙/῾", "x."},
	{COMPOSE, '(', "breve/vrachy", "˘", "x("},
	{COMPOSE, 0x02D8 /*˘*/, "breve/vrachy", "˘", "x("},
	{COMPOSE, ';', "ogonek/prosgegrammeni/tail", "˛", "x;"},
	{COMPOSE, 0x02DB /*˛*/, "ogonek/prosgegrammeni", "˛", "x;"},
	{COMPOSE, ')', "inverted breve", " ̑", "x)"},
	{COMPOSE, '&', "hook/a/ſ", "hook/æ/œ/ß", "&x", "x&"},
	{COMPOSE, '0', "ring/cedilla/iota", "˚/¸/ͺ", "°x", "x0", "x,"},

	/* modified punctuation keys, Windows compatibility */
	{COMPOSE, '@', "ring", "˚", "x0"},
	{COMPOSE, '?', "", "", "¿"},
	{COMPOSE, '!', "", "", "¡"},

	/* modified digit keys */
	{key_1, altctrl_mask, "breve/horn and acute", "˘/ ̛ and ´", "x('", "x9'"},
	{key_1, alt_mask, "circumflex and acute", "^ and ´", "x>'"},
	{key_1, ctrl_mask, "acute", "´", "x´", "´x", "x'"},
	{key_2, altctrl_mask, "breve/horn and grave", "˘/ ̛ and `", "x(!", "x9!"},
	{key_2, alt_mask, "circumflex and grave", "^ and `", "x>!"},
	{key_2, ctrl_mask, "grave", "`", "x!"},
	{key_3, altctrl_mask, "breve/horn and hook above", "˘/ ̛ and  ̉", "x(2", "x92"},
	{key_3, alt_mask, "circumflex and hook above", "^ and  ̉", "x>2"},
	{key_3, ctrl_mask, "hook above", " ̉", "x2"},
	{key_4, altctrl_mask, "breve/horn and tilde", "˘/ ̛ and ~", "x(?", "x9?"},
	{key_4, alt_mask, "circumflex and tilde", "^ and ~", "x>?"},
	{key_4, ctrl_mask, "tilde", "~", "x?"},
	{key_5, altctrl_mask, "breve/horn and dot below", "˘/ ̛ and  ̣", "x(-.", "x9-."},
	{key_5, alt_mask, "circumflex and dot below", "^ and  ̣", "x>-."},
	{key_5, ctrl_mask, "dot below", " ̣", "x-."},

	{key_6, altctrl_mask, "dasia and oxia", "῞", "x.´"},
	{key_6, alt_mask, "psili and oxia", "῎", "x,,´"},
	{key_7, altctrl_mask, "dasia and varia", "῝", "x.!"},
	{key_7, alt_mask, "psili and varia", "῍", "x,,!"},
	{key_8, altctrl_mask, "dasia and perispomeni", "῟", "x.?"},
	{key_8, alt_mask, "psili and perispomeni", "῏", "x,,?"},

	{key_6, ctrl_mask, "circumflex", "^", "x>"},
	{key_7, ctrl_mask, "breve", "˘", "x("},
	{key_8, ctrl_mask, "horn", " ̛", "x9"},
	{key_9, altctrl_mask},
	{key_9, alt_mask},
	{key_9, ctrl_mask, "stroke", "/", "x/"},
	{key_0, altctrl_mask},
	{key_0, alt_mask},
	{key_0, ctrl_mask, "ring/cedilla", "˚/¸/ͺ", "°x", "x0", "x,"},
};

struct prefixspec *
lookup_prefix (prefunc, shift)
  voidfunc prefunc;
  unsigned int shift;
{
  int i;
  for (i = 0; i < arrlen (prefixmap); i ++) {
	if (prefixmap [i].prefunc == prefunc &&
	    shift == prefixmap [i].preshift)
	{
		if (prefixmap [i].accentname) {
			return & prefixmap [i];
		} else {
			return 0;
		}
	}
  }
  return 0;
}

struct prefixspec *
lookup_prefix_char (c)
  unsigned long c;
{
  char pat [8];
  char * xp = pat;
  int i;

  xp += utfencode (c, xp);
  * xp ++ = 'x';
  * xp = '\0';

  for (i = 0; i < arrlen (prefixmap); i ++) {
	if (prefixmap [i].accentname) {
		char * p = prefixmap [i].pat1;
		if (p && streq (p, pat)) {
			return & prefixmap [i];
		}
		p = prefixmap [i].pat2;
		if (p && streq (p, pat)) {
			return & prefixmap [i];
		}
		p = prefixmap [i].pat3;
		if (p && streq (p, pat)) {
			return & prefixmap [i];
		}
	}
  }
  return 0;
}


/*======================================================================*\
|*			Mnemonic composition				*|
\*======================================================================*/

static
struct {
	char * mnemo;
	int uni;
} unimnemo3 [] = {
#include "mnemos.t"
	{NIL_PTR}
};


/**
   Return all mnemos for a character, concatenated in a string with separators.
 */
char *
mnemos (ucs)
  int ucs;
{
  static char mnemos [maxMSGlen];
  char * mpoi = mnemos;
  int i = 0;
  strcpy (mpoi, "");
  mpoi += 0;
  while (unimnemo3 [i].mnemo != NIL_PTR) {
	if (unimnemo3 [i].uni == ucs) {
		int l = strlen (unimnemo3 [i].mnemo);
		if (mpoi + 1 + l < & mnemos [maxMSGlen - 1]) {
			strcpy (mpoi, " ");
			mpoi += 1;
			strcpy (mpoi, unimnemo3 [i].mnemo);
			mpoi += l;
		}
	}
	i ++;
  }
  return mnemos;
}


#define dont_debug_compose_mnemonic
#define dont_debug_compose_patterns


/**
   Compose character from mnemonic.
   Param variable_length tweaks preferences for two-letter mnemonic input;
	variable_length = False: two-letter mnemonic
   Return CHAR_UNKNOWN if mnemonic is unknown.
   Return CHAR_INVALID if character found is invalid (CJK conversion).
 */
static
unsigned long
compose_mnemo (mnemo, variable_length, return_unicode)
  char * mnemo;
  FLAG variable_length;
  FLAG return_unicode;
{
  int i = 0;
  int res1 = -1;	/* prio 1 result index */
  int res2 = -1;	/* prio 2 result index */
  char * mnemo2 = & mnemo [1];
  unsigned long ret;

#ifdef debug_compose_mnemonic
  printf ("    compose_mnemo <%s> (full %d)\n", mnemo, variable_length);
#endif

  while (unimnemo3 [i].mnemo != NIL_PTR) {
	if (streq (unimnemo3 [i].mnemo, mnemo)) {
		if (variable_length) {
			res2 = i;
		} else {
			res1 = i;
			break;
		}
	}
	if (variable_length && unimnemo3 [i].mnemo [0] == ' '
	                    && streq (unimnemo3 [i].mnemo + 1, mnemo)) {
		/* give precedence to RFC 1345 mnemo */
		res1 = i;
		break;
	}
	if (mnemo [0] == '&' && streq (unimnemo3 [i].mnemo, mnemo2) && strlen (mnemo2) > 1) {
		res2 = i;
	}
	i ++;
  }

  if (res1 >= 0) {
	ret = unimnemo3 [res1].uni;
  } else if (res2 >= 0) {
	ret = unimnemo3 [res2].uni;
  } else {
	return CHAR_UNKNOWN;
  }

  if (return_unicode || utf8_text) {
	return ret;
	/* need to guard (above) with ! utf8_text in case of temp. utf8_text */
  } else if (cjk_text || mapped_text) {
	return encodedchar (ret);
  } else {
	return ret;
  }
}

/**
   Normalize accent shortcuts to usually available keys as used in tables.
 */
static
void
subst_acc (c1p, c2p, acc, sub)
  unsigned long * c1p;
  unsigned long * c2p;
  character acc;
  character sub;
{
  if (* c1p == acc) {
	* c1p = sub;
  }
  if (* c2p == acc) {
	* c2p = sub;
  }
}

/**
   Compose character from two-letter mnemonic.
   Return CHAR_UNKNOWN if mnemonic is unknown.
   Return CHAR_INVALID if character found is invalid (CJK conversion).
 */
unsigned long
compose_chars (c1, c2)
  unsigned long c1;
  unsigned long c2;
{
  character cs [13];
  character * cp = cs;
  unsigned long ret;
  unsigned long ret2;
  unsigned long c1subst = c1;
  unsigned long c2subst = c2;

  if (c2 == FUNcmd) {
	return CHAR_UNKNOWN;
  }

  cp += utfencode (c1, cp);
  (void) utfencode (c2, cp);

  ret = compose_mnemo (cs, False, False);

  if (no_char (ret)) {
	/* accept double quote for diaeresis */
	subst_acc (& c1subst, & c2subst, (character) '"', ':');
	/* accept real circumflex */
	subst_acc (& c1subst, & c2subst, (character) '^', '>');
	/* accept real grave */
	subst_acc (& c1subst, & c2subst, (character) '`', '!');
	/* accept real tilde */
	subst_acc (& c1subst, & c2subst, (character) '~', '?');
	/* accept real diaeresis */
	subst_acc (& c1subst, & c2subst, (character) '�', ':');
	/* accept real macron */
	subst_acc (& c1subst, & c2subst, (character) '�', '-');
	/* accept real acute */
	subst_acc (& c1subst, & c2subst, (character) '�', '\'');
	/* accept real cedilla */
	subst_acc (& c1subst, & c2subst, (character) '�', ',');
	/* accept degree for ring above */
	subst_acc (& c1subst, & c2subst, (character) '�', '0');

	cp = cs;
	cp += utfencode (c1subst, cp);
	(void) utfencode (c2subst, cp);
	ret2 = compose_mnemo (cs, False, False);
	if (ret2 != CHAR_UNKNOWN) {
		/* upgrade from UNKNOWN to INVALID or to found char */
		ret = ret2;
	}
  }

  /* no mnemo found, try reversing the two composites */
  if (no_char (ret)) {
	cp = cs;
	cp += utfencode (c2, cp);
	(void) utfencode (c1, cp);
	ret2 = compose_mnemo (cs, False, False);
	if (ret2 != CHAR_UNKNOWN) {
		/* upgrade from UNKNOWN to INVALID or to found char */
		ret = ret2;
	}

	if (no_char (ret)) {
		cp = cs;
		cp += utfencode (c2subst, cp);
		(void) utfencode (c1subst, cp);
		ret2 = compose_mnemo (cs, False, False);
		if (ret2 != CHAR_UNKNOWN) {
			/* upgrade from UNKNOWN to INVALID or to found char */
			ret = ret2;
		}
	}
  }

  return ret;
}

/**
   Compose character from mnemonic.
   Param variable_length tweaks preferences for two-letter mnemonic input;
	variable_length = False: two-letter mnemonic
   Return CHAR_UNKNOWN if mnemonic is unknown.
   Return CHAR_INVALID if character found is invalid (CJK conversion).
 */
static
unsigned long
compose_mnemonic_mode (variable_length, mnemo)
  FLAG variable_length;
  char * mnemo;
{
#ifdef debug_compose_patterns
  printf ("  compose_mnemonic_mode (var %d) %s\n", variable_length, mnemo);
#endif

  unsigned long ret = compose_mnemo (mnemo, variable_length, False);

  if (no_char (ret)) {
	/* semi-normalize character sequence: bring letter to front, 
	   replace accent symbols with standard accent mnemonics;
	   handle max. 1 letter and 3 accent mnemonics */
	unsigned long let = 0;
	unsigned long a [3];
	int ai = 0;
	char * m = mnemo;
	char mbuf [25];
	int i;

#ifdef debug_compose_mnemonic
	printf ("    compose_mnemonic normalizing\n");
#endif

	while (* m) {
		int len;
		unsigned long unichar;
		utf8_info (m, & len, & unichar);
		if (isLetter (unichar)) {
			if (let == 0) {
				let = unichar;
			} else {	/* two letters */
				return ret;
			}
		} else {
			/* normalize accent mnemo */
			if (unichar == '"') {
				unichar = ':';
			} else if (unichar == '~') {
				unichar = '?';
			} else if (unichar == 0xB4) {	/* ´ */
				unichar = '\'';
			} else if (unichar == 0xB0) {	/* ° */
				unichar = '0';
			} else if (unichar == '`') {
				unichar = '!';
			} else if (unichar == '^') {
				unichar = '>';
			}

			/* store */
			if (ai < 3) {
				a [ai] = unichar;
				ai ++;
			} else {	/* too many accent mnemos */
				return ret;
			}
		}
		m += len;
	}

	/* generate normalized UTF-8 string */
	m = mbuf;
	if (let == 0) {	/* no letter */
		return ret;
	}
	m += utfencode (let, mbuf);
	for (i = 0; i < ai; i ++) {
		if (a [i] != 0) {
			m += utfencode (a [i], m);
		}
	}

	return compose_mnemo (mbuf, variable_length, False);
  }

  return ret;
}

/**
   Compose character from mnemonic.
   Return CHAR_UNKNOWN if mnemonic is unknown.
   Return CHAR_INVALID if character found is invalid (CJK conversion).
 */
unsigned long
compose_mnemonic (mnemo)
  char * mnemo;
{
  return compose_mnemonic_mode (True, mnemo);
}

static
void
postpat (ps, patpoia, patpoib)
  struct prefixspec * ps;
  char * * patpoia;
  char * * patpoib;
{
  * patpoib = NIL_PTR;
  if (ps && ps->pat1 && * ps->pat1 == 'x') {
	* patpoia = ps->pat1 + 1;
	if (ps && ps->pat2 && * ps->pat2 == 'x') {
		* patpoib = ps->pat2 + 1;
	} else if (ps && ps->pat3 && * ps->pat3 == 'x') {
		* patpoib = ps->pat3 + 1;
	}
  } else if (ps && ps->pat2 && * ps->pat2 == 'x') {
	* patpoia = ps->pat2 + 1;
	if (ps && ps->pat3 && * ps->pat3 == 'x') {
		* patpoib = ps->pat3 + 1;
	}
  } else if (ps && ps->pat3 && * ps->pat3 == 'x') {
	* patpoia = ps->pat3 + 1;
  } else {
	* patpoia = NIL_PTR;
  }
}

static
unsigned long
compose_three_patterns (base, pat1a, pat1b, pat2a, pat2b, pat3a, pat3b)
  unsigned long base;
  char * pat1a;
  char * pat1b;
  char * pat2a;
  char * pat2b;
  char * pat3a;
  char * pat3b;
{
  char mnemo [maxMSGlen];
  unsigned long composed;

  (void) utfencode (base, mnemo);
  strcat (mnemo, pat1a);
  strcat (mnemo, pat2a);
  if (pat3a) {
	strcat (mnemo, pat3a);
  }
  composed = compose_mnemonic_mode (False, mnemo);
  if (! no_char (composed)) {
	return composed;
  } else if (pat1b != NIL_PTR) {
	return compose_three_patterns (base, pat1b, NIL_PTR, pat2a, pat2b, pat3a, pat3b);
  } else if (pat2b != NIL_PTR) {
	return compose_three_patterns (base, pat1a, pat1b, pat2b, NIL_PTR, pat3a, pat3b);
  } else if (pat3b != NIL_PTR) {
	return compose_three_patterns (base, pat1a, pat1b, pat2a, pat2b, pat3b, NIL_PTR);
  } else {
	return composed;
  }
}

/**
   Compose character with multiple accents from patterns and fill-in character.
   Return CHAR_UNKNOWN if mnemonic is unknown.
   Return CHAR_INVALID if character found is invalid (CJK conversion).
 */
static
unsigned long
compose_multiple_patterns (base, ps, ps2, ps3)
  unsigned long base;
  struct prefixspec * ps;
  struct prefixspec * ps2;
  struct prefixspec * ps3;
{
  unsigned long composed;
  /* Among the up to three alternative patterns for each accent prefix, 
     max. two are postfix (start with "x"); both should be tried.
  */
  char * pat1a;
  char * pat1b;
  char * pat2a;
  char * pat2b;
  char * pat3a;
  char * pat3b;
  postpat (ps, & pat1a, & pat1b);
  postpat (ps2, & pat2a, & pat2b);
  postpat (ps3, & pat3a, & pat3b);

  if (! pat1a) {
	pat1a = "";
  }
  if (! pat2a) {
	pat2a = "";
  }

  /* try combining patterns in the following sequence, and in each 
     case, try patNa first, then patNb, for each component:
	12(3)
	if (pat3) 132
	21(3)
	if (pat3) 231
	if (pat3) 312
	if (pat3) 321
  */

  composed = compose_three_patterns (base, pat1a, pat1b, pat2a, pat2b, pat3a, pat3b);
  if (! no_char (composed)) {
	return composed;
  }

  if (pat3a) {
	composed = compose_three_patterns (base, pat1a, pat1b, pat3a, pat3b, pat2a, pat2b);
	if (! no_char (composed)) {
		return composed;
	}
  }

  composed = compose_three_patterns (base, pat2a, pat2b, pat1a, pat1b, pat3a, pat3b);
  if (! no_char (composed)) {
	return composed;
  }

  if (pat3a) {
	composed = compose_three_patterns (base, pat2a, pat2b, pat3a, pat3b, pat1a, pat1b);
	if (! no_char (composed)) {
		return composed;
	}

	composed = compose_three_patterns (base, pat3a, pat3b, pat1a, pat1b, pat2a, pat2b);
	if (! no_char (composed)) {
		return composed;
	}

	composed = compose_three_patterns (base, pat3a, pat3b, pat2a, pat2b, pat1a, pat1b);
  }

  return composed;
}

/**
   Compose base character from mnemonic patterns and fill-in character.
   Return CHAR_UNKNOWN if mnemonic is unknown.
   Return CHAR_INVALID if character found is invalid (CJK conversion).
 */
static
unsigned long
compose_base_patterns (base, ps, ps2, ps3)
  unsigned long base;
  struct prefixspec * ps;
  struct prefixspec * ps2;
  struct prefixspec * ps3;
{
  char * patpoi = ps->pat1;
  char mnemo [maxMSGlen];
  char * mnemopoi = mnemo;
  unsigned long composed;

  while (* patpoi != '\0') {
	if (* patpoi == 'x') {
		patpoi ++;
		mnemopoi += utfencode (base, mnemopoi);
	} else {
		* mnemopoi ++ = * patpoi ++;
	}
  }
  * mnemopoi = '\0';

  composed = compose_mnemonic_mode (False, mnemo);

  if (no_char (composed) && ps->pat2 != NIL_PTR) {
	patpoi = ps->pat2;
	mnemopoi = mnemo;
	while (* patpoi != '\0') {
		if (* patpoi == 'x') {
			patpoi ++;
			mnemopoi += utfencode (base, mnemopoi);
		} else {
			* mnemopoi ++ = * patpoi ++;
		}
	}
	* mnemopoi = '\0';

	composed = compose_mnemonic_mode (False, mnemo);
  }

  if (no_char (composed) && ps->pat3 != NIL_PTR) {
	patpoi = ps->pat3;
	mnemopoi = mnemo;
	while (* patpoi != '\0') {
		if (* patpoi == 'x') {
			patpoi ++;
			mnemopoi += utfencode (base, mnemopoi);
		} else {
			* mnemopoi ++ = * patpoi ++;
		}
	}
	* mnemopoi = '\0';

	composed = compose_mnemonic_mode (False, mnemo);
  }

  return composed;
}

static
char *
decompose (basepoi)
  unsigned long * basepoi;
{
  int i = 0;
  while (unimnemo3 [i].mnemo != NIL_PTR) {
	if ((unsigned long) unimnemo3 [i].uni == * basepoi) {
		char basename [122];
		char * bp = basename;
		char * mp = unimnemo3 [i].mnemo;
		int basenamelen = 0;
		while ((* mp >= 'A' && * mp <= 'Z') || (* mp >= 'a' && * mp <= 'z')) {
			* bp ++ = * mp ++;
			basenamelen ++;
		}
		* bp = '\0';
		/* if both base name and mnemonic suffix found, return */
		if (* mp) {
			if (basenamelen == 1) {
				* basepoi = (unsigned long) basename [0];
				return mp;
			} else if (basenamelen > 1) {
				/* "schwa&" -> schwa */
				unsigned long c = compose_mnemo (basename, False, True);
				if (! no_char (c)) {
					* basepoi = c;
					return mp;
				}
			}
		}
	}
	i ++;
  }
  return NIL_PTR;
}

/**
   Compose character (may be precomposed already) 
   from mnemonic patterns and fill-in character.
   Assume two-letter mnemonic, so param variable_length of compose_mnemo 
	should be passed as False.
	This property propagates to
	- compose_multiple_patterns -> compose_three_patterns
		-> compose_mnemonic (if called from there);
	and to
	compose_base_patterns -> compose_mnemonic (if called from there)

   Return CHAR_UNKNOWN if mnemonic is unknown.
   Return CHAR_INVALID if character found is invalid (CJK conversion).
 */
unsigned long
compose_patterns (base, ps, ps2, ps3)
  unsigned long base;
  struct prefixspec * ps;
  struct prefixspec * ps2;
  struct prefixspec * ps3;
{
  unsigned long composed;

  if (ps->pat1 == NIL_PTR) {
	return CHAR_UNKNOWN;
  }

#ifdef debug_compose_patterns
	printf ("compose_patterns %04lX\n	%s %s %s %s\n", base, ps->accentname, ps->pat1, ps->pat2, ps->pat3);
	if (ps2) {
		printf ("	%s %s %s %s\n", ps2->accentname, ps2->pat1, ps2->pat2, ps2->pat3);
	}
	if (ps3) {
		printf ("	%s %s %s %s\n", ps3->accentname, ps3->pat1, ps3->pat2, ps3->pat3);
	}
#endif

  if (ps2) {
	composed = compose_multiple_patterns (base, ps, ps2, ps3);
  } else {
	composed = compose_base_patterns (base, ps, ps2, ps3);
  }

  if (no_char (composed) && base > 0x80 && ! ps3) {
	/* in case of precomposed base character, try to decompose it 
	   into its base character and mnemonic postfix, 
	   then try again to compose with accents */
	char * postfix = decompose (& base);
	if (postfix != NIL_PTR) {
		static char newmnemo [50];
		static struct prefixspec newps = {0, 0, "[decomposed]", "[]", newmnemo, 0, 0};
		strcpy (newmnemo, "x");
		strcat (newmnemo, postfix);
		return compose_patterns (base, & newps, ps, ps2);
	}
  }

  return composed;
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
