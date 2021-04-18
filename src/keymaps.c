/*======================================================================*\
|*		Editor mined						*|
|*		keyboard mapping tables					*|
\*======================================================================*/

#include "mined.h"
#include "termprop.h"


#if defined (msdos)
#define noCJKkeymaps
#endif


#define use_CJKkeymaps

#if defined (noCJKkeymaps) || defined (NOCJK)
#undef use_CJKkeymaps
#endif


/*======================================================================*\
|*		Keyboard mapping tables format and type			*|
\*======================================================================*/

#ifndef NOSTRINGTABLES
#define keymapping_stringtables
#endif


#ifdef keymapping_stringtables
#define keymaptableelem char
#else
#define keymaptableelem struct keymap
struct keymap {
	char * fk;
	char * fp;
};
#endif
#define keymaptabletype keymaptableelem *


struct keymapping {
	keymaptabletype table;
	char * shortcut;
};


/*======================================================================*\
|*		Keyboard mapping tables					*|
\*======================================================================*/

#define use_keymap_tables


#ifdef use_keymap_tables

#ifndef use_concatenated_keymaps
#define use_concatenated_keymaps
#endif

#define no_extern_keymap_tables

#ifdef extern_keymap_tables

# include "keymapsx.h"
# ifdef keymapping_stringtables
	/* link with keymaps1/*.o */
# else
	/* link with keymaps0/*.o from keymaps0/*.h */
# endif

#else

# ifdef keymapping_stringtables
#  ifdef use_concatenated_keymaps
#  include "keymaps.t"
#  else
#  include "keymapsc.h"
	/* #includes keymaps1/*.c, link with -Ikeymaps1 */
#  endif
# else
#  include "keymapsi.h"
	/* #includes keymaps0/*.h, link with -Ikeymaps0 */
# endif

#endif

#endif


/*======================================================================*\
|*		Table of tables						*|
\*======================================================================*/

static struct keymapping keymappingtable [] = {
# ifdef use_keymap_tables
# include "keymapsk.t"
# else
	{0, "--"},	/* some compilers don't like empty arrays */
# endif
};

static unsigned int keymappingtable_len = arrlen (keymappingtable);


/*======================================================================*\
|*		Input method switching					*|
\*======================================================================*/

char * keyboard_mapping = "--";
char * last_keyboard_mapping = "--";
static char * next_keyboard_mapping = "";

static keymaptabletype keyboard_map = (keymaptabletype) NIL_PTR;
static keymaptabletype last_keyboard_map = (keymaptabletype) NIL_PTR;

/**
   Notify user of changed input method
 */
static
void
notify_input_method ()
{
  menuitemtype * km = lookup_Keymap_menuitem (keyboard_mapping);
  char * source = NIL_PTR;

  switch (km->extratag ? km->extratag [0] : ' ') {
	case 'U':	source = "UnicodeData"; break;
	case 'H':	source = "Unihan"; break;
	case 'C':	source = "cxterm"; break;
	case 'M':	source = "m17n"; break;
	case 'Y':	source = "yudit"; break;
	case 'V':	source = "vim"; break;
	case 'X':	source = "X"; break;
  }
  if (source) {
	build_string (text_buffer, "Input method: %s (source: %s)", 
			km->itemname, source);
  } else {
	build_string (text_buffer, "Input method: %s", km->itemname);
  }
  status_uni (text_buffer);
}

/**
   Set keymap.
 */
static
void
set_keymap (new_keymap, current, next)
  keymaptabletype new_keymap;
  char * current;
  char * next;
{
  last_keyboard_map = keyboard_map;
  last_keyboard_mapping = keyboard_mapping;
  keyboard_map = new_keymap;
  keyboard_mapping = current;
  next_keyboard_mapping = next;

  if (! loading && ! stat_visible && ! input_active) {
	notify_input_method ();
  }
}

/**
   Exchange current and previous keyboard mapping; set previous active.
   With HOP, reset keyboard mapping to none.
 */
void
toggleKEYMAP ()
{
  if (allow_keymap) {
	if (hop_flag > 0) {
		set_keymap ((keymaptabletype) NIL_PTR, "--", keymappingtable [0].shortcut);
	} else {
		set_keymap (last_keyboard_map, last_keyboard_mapping, last_keyboard_mapping);
	}
	flags_changed = True;
  } else {
	error ("Keyboard mapping not active");
  }
}

/**
   Set keyboard mapping.
 */
static
FLAG
selectKEYMAP (script)
  char * script;
{
  if (script == NIL_PTR || * script == '\0') {
	set_keymap ((keymaptabletype) NIL_PTR, "--", keymappingtable [0].shortcut);
	flags_changed = True;
	return True;
  } else {
	/* map alternative shortcuts */
	int i;
	if (strisprefix ("el", script)) {
		script = "gr";
	} else if (strisprefix ("ru", script)) {
		script = "cy";
	}
	/* map shortcut to keymap */
	for (i = 0; i < keymappingtable_len; i ++) {
	    if (strisprefix (keymappingtable [i].shortcut, script)) {
		set_keymap (keymappingtable [i].table,
			    keymappingtable [i].shortcut,
			    i + 1 == keymappingtable_len ?
			      "" : keymappingtable [i + 1].shortcut);
		flags_changed = True;
		return True;
	    }
	}
	return False;
  }
}

/**
   Set keyboard mapping.
 */
FLAG
setKEYMAP (script)
  char * script;
{
  if (script != NIL_PTR && * script == '-') {
	/* set keymap as secondary map */
	script ++;
	(void) selectKEYMAP (script);
	(void) selectKEYMAP (NIL_PTR);
  } else if (script != NIL_PTR) {
	char * secondary = strchr (script, '-');
	if (secondary != NIL_PTR) {
		secondary ++;
		(void) selectKEYMAP (secondary);
	}
	return selectKEYMAP (script);
  }
  return True;
}

#ifdef not_used
/**
   Cycle through keyboard mappings.
 */
void
cycleKEYMAP ()
{
  (void) selectKEYMAP (next_keyboard_mapping);
}
#endif

/**
   Enable keyboard mapping interactively.
   With HOP, cycle through keyboard mappings.
 */
void
setupKEYMAP ()
{
  if (allow_keymap) {
	if (hop_flag > 0) {
		hop_flag = 0;
		(void) selectKEYMAP (next_keyboard_mapping);
	} else {
		handleKeymapmenu ();
	}
  } else {
	error ("Keyboard mapping not active");
  }
}


/*======================================================================*\
|*		Key mapping lookup					*|
\*======================================================================*/

/*
   Look up key sequence in keyboard mapping table.
   Similar to findkeyin but with modified parameter details.
 * if matchmode == 0 (not used):
 * mapkb (str) >=  0: key sequence str found
 *			* mapped set to mapped string
 *		 == -1: key sequence prefix recognised
 *				(str is prefix of some table entry)
 *		 == -2: key sequence not found
 *				(str is not contained in table)
 * * found == index of exact match if found
 * if matchmode == 1:
 * return -1 if any prefix detected (even if other exact match found)
 * if matchmode == 2:
 * don't return -1, only report exact or no match
 */
static
int
mapkb (str, kbmap, matchmode, found, mapped)
  char * str;
  keymaptabletype kbmap;
  int matchmode;
  char * * found;
  char * * mapped;
{
#ifdef keymapping_stringtables

  int lastmatch = 0;	/* last index with string matching prefix */
  register int i;
  int ret = -2;
  int len = strlen (str);

  * found = NIL_PTR;

  if (* kbmap == '\0') {
	return ret;
  }

  i = lastmatch;
  do {
	if (strncmp (str, kbmap, len) == 0) {
		/* str is prefix of current entry */
		lastmatch = i;
		if (len == strlen (kbmap)) {
			/* str is equal to current entry */
			* found = kbmap;
			* mapped = kbmap + len + 1;
			if (matchmode == 1) {
				/* return index unless other prefix 
				   was or will be found */
				if (ret == -2) {
					ret = i;
				}
			} else {
				return i;
			}
		} else {
			/* str is partial prefix of current entry */
			if (matchmode == 1) {
				/* return -1 but continue to search 
				   for exact match */
				ret = -1;
			} else if (matchmode != 2) {
				return -1;
			} else {
			}
		}
	}
	++ i;
	kbmap += strlen (kbmap) + 1;
	kbmap += strlen (kbmap) + 1;
	if (* kbmap == '\0') {
		i = 0;
		return ret;
	}
  } while (i != lastmatch);

  return ret;

#else

  int lastmatch = 0;	/* last index with string matching prefix */
  register int i;
  int ret = -2;

  * found = NIL_PTR;

  if (kbmap [0].fk == NIL_PTR) {
	return ret;
  }

  i = lastmatch;
  do {
	if (strncmp (str, kbmap [i].fk, strlen (str)) == 0) {
		/* str is prefix of current entry */
		lastmatch = i;
		if (strlen (str) == strlen (kbmap [i].fk)) {
			/* str is equal to current entry */
			* found = kbmap [i].fk;
			* mapped = kbmap [i].fp;
			if (! * mapped) {
				/* fix missing mapping in keyboard mapping table */
				* mapped = "";
			}
			if (matchmode == 1) {
				/* return index unless other prefix 
				   was or will be found */
				if (ret == -2) {
					ret = i;
				}
			} else {
				return i;
			}
		} else {
			/* str is partial prefix of current entry */
			if (matchmode == 1) {
				/* return -1 but continue to search 
				   for exact match */
				ret = -1;
			} else if (matchmode != 2) {
				return -1;
			} else {
			}
		}
	}
	++ i;
	if (kbmap [i].fk == NIL_PTR) {
		i = 0;
		return ret;
	}
  } while (i != lastmatch);

  return ret;

#endif
}

int
map_key (str, matchmode, found, mapped)
  char * str;
  int matchmode;
  char * * found;
  char * * mapped;
{
  return mapkb (str, keyboard_map, matchmode, found, mapped);
}

int
keyboard_mapping_active ()
{
  return keyboard_map != (keymaptabletype) NIL_PTR;
}


/*======================================================================*\
|*				End					*|
\*======================================================================*/
