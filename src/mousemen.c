/***********************************************************************\
		Editor mined
		menu handling part
\***********************************************************************/

#include "syntax.h"

#include "mined.h"
#include "charcode.h"	/* #define use_CJKcharmaps for charemen.t ? */
#include "charprop.h"
#include "io.h"
#include "termprop.h"

#include "textfile.h"	/* grab_lock, ignore_lock */


/***********************************************************************\
	Menu types and aux. function
\***********************************************************************/

#define STAYINMENU 0x8000
#define stayinmenu(flag)	(STAYINMENU | (int) flag)

typedef struct {
	char * menuname; int menulen;
	menuitemtype * menuitems;
	} menutype;

typedef struct {
	char * (* dispflag) ();
	void (* toggle) ();
	char * menutitle;
	menuitemtype * menu;
	int menulen;
	} flagitemtype;


local
void
separator (item, ii)
  menuitemtype * item;
  int ii;
{
}

local
void
dummyfunc (item, ii)
  menuitemtype * item;
  int ii;
{
}

local
void
dummytoggle ()
{
}

local
int
dummyflagoff (item, ii)
  menuitemtype * item;
  int ii;
{
  return 0;
}

local
int
dummyflagon (item, ii)
  menuitemtype * item;
  int ii;
{
  return 1;
}

void
fill_menuitem (item, s1, s2)
  menuitemtype * item;
  char * s1;
  char * s2;
{
  /* init menuitemtype struct to null */
  menuitemtype nullitem;
  memcpy ((void *) item, (void *) & nullitem, sizeof (menuitemtype));

  if (s1) {
	item->itemname = s1;
	item->itemfu = dummyfunc;
  } else {
	item->itemname = "";
	item->itemfu = separator;
  }

  if (s2) {
	item->hopitemname = s2;
	item->itemon = dummyflagon;
  } else {
	item->hopitemname = "";
	item->itemon = dummyflagoff;
  }

  item->extratag = NIL_PTR;
}


/***********************************************************************\
	Global variables
\***********************************************************************/

/* mode tuning */
#define use_graphic_borders	(menu_border_style != '@')
local int popupmenumargin;

local int flags_pos = 62;
local int flags_displayed = 9;

/* layout tuning */
local int pulldownmenu_width = 12;
local int horizontal_bar_width = 1;
local int extratag_length = 1;

/* display status */
local FLAG popupmenu_active = False;
local FLAG menuline_dirty = False;

/* menu mode */
local FLAG is_char_menu = False;

/* state */
local int first_dirty_line = -1;
local int last_dirty_line = 0;
local int last_menulen;
local int last_menuwidth;
local FLAG last_disp_only;
local char * last_menutitle;

/* state for menu history */
local FLAG menu_reposition = False;	/* token to go to previous item */
local menuitemtype * last_fullmenu = 0;
local menuitemtype * previous_fullmenu = 0;
local int previous_menuline = -1;
local int previous_scroll = -1;

/* menu state for sub-menu positioning */
local int last_menucolumn;
local int last_menuline;


/* menu state and saved menu status for redrawmenu () */
local int last_popup_column;
local int last_popup_line;
local int last_popup_item;
local int last_is_flag_menu;
local int last_minwidth;
local int last_scroll;
local int last_maxscroll;
local int last_popup_index;


/***********************************************************************\
	Local functions
\***********************************************************************/

local void action_menu _((menuitemtype * menu, int menulen, 
			int column, int line, char * title, FLAG is_flag_menu));
local void openmenu _((int i));
local void that_menu _((menuitemtype * menu));

local void handleInfomenu _((void));
local void handleBuffermenu _((void));
local void handleJustifymenu _((void));
#ifdef unused
local void handleParagraphmodemenu _((void));
#endif
#ifdef more_menu_handlers
local void handleCombiningmenu _((void));
local void handleTextmenu _((void));
local void handleIndentmenu _((void));
#endif

local void handleUnlockmenu _((void));
local void handleScreensizemenu _((void));
local void handleEmulmenu _((void));
local void handleLineendtypemenu _((void));
local void handleFilechoosermenu _((void));

local void disp_tab_colour _((void));


/***********************************************************************\
	utf8_col_count () returns # screen columns in UTF-8 string
\***********************************************************************/

local int
	utf8_col_count (string)
		char * string;
{
	int count = 0;
	char * start = string;

	If string != NIL_PTR
	Then	Dowhile * string != '\0'
		Do	advance_utf8_scr (& string, & count, start);
		Done
	Fi
	return count;
}


/***********************************************************************\
	Flag operations
\***********************************************************************/

local void
	toggle_nothing ()
{
}

local char *
	dispnothing ()
{
	return " ";
}


local void
	toggle_HOP ()
{
	If hop_flag > 0
	Then	hop_flag = 0;
	Else	hop_flag = 1;
	Fi
	displayflags ();
}

local char *
	dispHOP ()
{
	hop_flag_displayed = hop_flag;
	return hop_flag > 0 ? "H" : "h";
}


#ifdef debug_ring_buffer
local char *
	disp_buffer_open ()
{
	static char f [3];
	f [0] = '';
	f [1] = buffer_open_flag + '0';
	f [2] = '\0';
	return f;
}
#endif


local void
	select_editmode (item, ii)
		menuitemtype * item;
		int ii;
{
	If ii == 0
	Then	EDITmode ();
	Else	VIEWmode ();
	Fi
}

local char *
	dispVIEW ()
{
	return viewonly ? "V" : "E";
}

local int
	editorviewon (item, ii)
		menuitemtype * item;
		int ii;
{
	return viewonly == ii;
}


global void
	toggle_append ()
{
	If append_flag
	Then	append_flag = False;
	Else	append_flag = True;
	Fi
	displayflags ();
}

local void
	select_buffermode (item, ii)
		menuitemtype * item;
		int ii;
{
	If ii == 0
	Then	append_flag = False;
	Else	append_flag = True;
	Fi
}

local void
	toggle_pastebuf_mode ()
{
	If pastebuf_utf8
	Then	pastebuf_utf8 = False;
	Else	pastebuf_utf8 = True;
	Fi
	displayflags ();
}

local int
	pastebuf_utf8on (item, ii)
		menuitemtype * item;
		int ii;
{
	return pastebuf_utf8;
}

local void
	select_visselect_keeponsearch ()
{
	If visselect_keeponsearch
	Then	visselect_keeponsearch = False;
	Else	visselect_keeponsearch = True;
	Fi
}

local int
	visselect_keeponsearch_on (item, ii)
		menuitemtype * item;
		int ii;
{
	return visselect_keeponsearch;
}

local void
	select_visselect_autocopy ()
{
	If visselect_autocopy
	Then	visselect_autocopy = False;
	Else	visselect_autocopy = True;
	Fi
}

local int
	visselect_autocopy_on (item, ii)
		menuitemtype * item;
		int ii;
{
	return visselect_autocopy;
}

#ifdef more_visselect

local void
	select_visselect_key ()
{
	If visselect_key
	Then	visselect_key = False;
	Else	visselect_key = True;
	Fi
}

local int
	visselect_key_on (item, ii)
		menuitemtype * item;
		int ii;
{
	return visselect_key;
}

local void
	select_visselect_anymouse ()
{
	If visselect_anymouse
	Then	visselect_anymouse = False;
	Else	visselect_anymouse = True;
		visselect_setonfind = True;
	Fi
}

local int
	visselect_anymouse_on (item, ii)
		menuitemtype * item;
		int ii;
{
	return visselect_anymouse;
}

local void
	select_visselect_autodelete ()
{
	If visselect_autodelete
	Then	visselect_autodelete = False;
	Else	visselect_autodelete = True;
	Fi
}

local int
	visselect_autodelete_on (item, ii)
		menuitemtype * item;
		int ii;
{
	return visselect_autodelete;
}

local void
	select_visselect_setonfind ()
{
	If visselect_setonfind
	Then	visselect_setonfind = False;
	Else	visselect_setonfind = True;
	Fi
}

local int
	visselect_setonfind_on (item, ii)
		menuitemtype * item;
		int ii;
{
	return visselect_setonfind;
}

#endif

/**	normal/rectangular
	 overwrite/append
	%=
	%+
	[=
	[+
 */
local char *
	disppastebuf_1 ()
{
	If rectangular_paste_flag
	Then	If pastebuf_utf8 && ! utf8_text
		Then	return "[";
		Else	return "[";
		Fi
	Else	If pastebuf_utf8 && ! utf8_text
		Then	return "%";
		Else	return "%";
		Fi
	Fi
}

local char *
	disppastebuf_2 ()
{
	If append_flag
	Then	If pastebuf_utf8 && ! utf8_text
		Then	return "+";
		Else	return "+";
		Fi
	Else	If pastebuf_utf8 && ! utf8_text
		Then	return "=";
		Else	return "=";
		Fi
	Fi
}

local int
	overwriteorappendon (item, ii)
		menuitemtype * item;
		int ii;
{
	return append_flag == ii;
}

local int
	rectangularpasteon (item, ii)
		menuitemtype * item;
		int ii;
{
	return rectangular_paste_flag == True;
}


local void
	toggle_autoindent ()
{
	If autoindent
	Then	autoindent = False;
	Else	autoindent = True;
	Fi
}

local char *
	disp_autoindent ()
{
	If autoindent
	Then	return "»";
	Else	return "¦";
	Fi
}

local int
	autoindenton (item, ii)
		menuitemtype * item;
		int ii;
{
	return autoindent;
}

local void
	toggle_autonumber ()
{
	If autonumber
	Then	autonumber = False;
	Else	autonumber = True;
		autoindent = True;
	Fi
}

local int
	autonumberon (item, ii)
		menuitemtype * item;
		int ii;
{
	return autoindent && autonumber;
}

#ifdef fine_numbering_undent_control

local void
	toggle_backnumber ()
{
	If backnumber
	Then	backnumber = False;
	Else	backnumber = True;
	Fi
}

local int
	backnumberon (item, ii)
		menuitemtype * item;
		int ii;
{
	return autoindent && autonumber && backnumber;
}

local void
	toggle_backincrem ()
{
	If backincrem
	Then	backincrem = False;
	Else	backincrem = True;
	Fi
}

local int
	backincremon (item, ii)
		menuitemtype * item;
		int ii;
{
	return backincrem;
}

#endif


local void
	toggle_tabexpand ()
{
	If expand_tabs
	Then	expand_tabs = False;
	Else	expand_tabs = True;
	Fi
}

local char *
	disp_tabmode ()
{
	static char tabmode [3];
	tabmode [0] = '';
	tabmode [1] = '0' + tabsize;
	tabmode [2] = '\0';
	If expand_tabs
	Then	return tabmode;
	Else	return tabmode + 1;
	Fi
}

local int
	tabexpansionon (item, ii)
		menuitemtype * item;
		int ii;
{
	return expand_tabs;
}


local void
	select_tabsize (item, ii)
		menuitemtype * item;
		int ii;
{
	tabsize = item->itemname [0] - '0';
	RD ();
}

local int
	tabsizeon (item, ii)
		menuitemtype * item;
		int ii;
{
	return '0' + tabsize == item->itemname [0];
}


local void
	toggle_JUSlevel ()
{
	JUSlevel ++;
	If JUSlevel > 2
	Then	JUSlevel = 0;
	Fi
}

local void
	select_justify (item, ii)
		menuitemtype * item;
		int ii;
{
/*	JUSlevel = ii;	*/
	If strcontains (item->hopitemname, "!")
	Then	JUSlevel = 2;
	Elsif strcontains (item->hopitemname, "«")
	Then	JUSlevel = 1;
	Else	JUSlevel = 0;
	Fi
}

local char *
	dispJUSlevel ()
{
	If JUSlevel == 0
	Then	return "j";
	Elsif JUSlevel == 1
	Then	return "j";
	Else	return "J";
	Fi
}

local int
	justifymodeon (item, ii)
		menuitemtype * item;
		int ii;
{
/*	return JUSlevel == ii;	*/
	If strcontains (item->hopitemname, "!")
	Then	return JUSlevel == 2;
	Elsif strcontains (item->hopitemname, "«")
	Then	return JUSlevel == 1;
	Else	return JUSlevel == 0;
	Fi
}


local void
	toggle_JUSmode ()
{
	JUSmode = 1 - JUSmode;
}

local void
	select_paragraph (item, ii)
		menuitemtype * item;
		int ii;
{
/*	JUSmode = ii;	*/
	If strcontains (item->hopitemname, " ")
	Then	JUSmode = 0;
	Else	JUSmode = 1;
	Fi
}

local char *
	dispJUSmode ()
{
	return JUSmode == 1 ? "«" : " ";
}

local int
	paragraphmodeon (item, ii)
		menuitemtype * item;
		int ii;
{
/*	return JUSmode == ii;	*/
	If strcontains (item->hopitemname, " ")
	Then	return JUSmode == 0;
	Else	return JUSmode == 1;
	Fi
}


local char * prev_text_encoding = "";

global void
	change_encoding (new_text_encoding)
		char * new_text_encoding;
{
	If ! streq (get_text_encoding (), new_text_encoding)
	Then	prev_text_encoding = get_text_encoding ();
	Fi
	(void) set_text_encoding (new_text_encoding, ' ', "men: change_encoding");

	total_chars = -1L;
	/*recount_chars ();*/
}

global void
	toggle_encoding ()
{
	char * curpos = cur_text;
	char * new_prev_text_encoding = get_text_encoding ();

	If utf8_text && utf16_file
	Then	error ("Text encoding not switchable when editing UTF-16 file");
			ring_bell ();
			return;
	Fi

	/** Determine encoding to switch to:
		1. Previously set text encoding (toggle) if defined
		2. Terminal encoding if different from current text encoding
		3. UTF-8 unless currently UTF-8
		4. Latin-1
	 */
	If * prev_text_encoding == '\0'
	Then	prev_text_encoding = get_term_encoding (); /* sic */
		If prev_text_encoding == new_prev_text_encoding
		Then	If utf8_text
			Then	prev_text_encoding = "ISO 8859-1";
			Else	prev_text_encoding = "UTF-8";
			Fi
		Fi
	Fi

	/** Special case: instead of UTF-16, switch to UTF-8 unless 
		it's current, then switch to Latin-1
	 */
	If strisprefix ("UTF-16", prev_text_encoding)
	Then	If utf8_text
		Then	prev_text_encoding = "ISO 8859-1";
		Else	prev_text_encoding = "UTF-8";
		Fi
	Fi

	If set_text_encoding (prev_text_encoding, ' ', "men: toggle_encoding")
	Then
		prev_text_encoding = new_prev_text_encoding;

		recount_chars ();

		RD ();
		displaymenuline (True);
		/* move cursor to or behind actual previous character position */
		move_address (curpos, y);
		/* adjust not to stay amidst a character */
		move_to (x, y);
	Fi
}

local void
	select_encoding (item, ii)
		menuitemtype * item;
		int ii;
{
	char * curpos = cur_text;
	char * new_prev_text_encoding = get_text_encoding ();

#ifdef selectable_term_encoding
	/* this is a debugging option */
	If hop_flag
	Then	If set_term_encoding (item->hopitemname, ' ')
		Then	RD ();
			move_address (curpos, y);
			move_to (x, y);
		Else	ring_bell ();
		Fi
		return;
	Fi
#endif

	If utf8_text && utf16_file
	Then	error ("Text encoding not switchable when editing UTF-16 file");
		ring_bell ();
		return;
	Fi

	If set_text_encoding (item->hopitemname, ' ', "men: select_encoding")
	Then
#ifdef cjk_term_constraint
		If cjk_term & ! cjk_text
		Then	error ("Non-CJK text encoding not supported in CJK terminal");
			ring_bell ();
			(void) set_text_encoding (new_prev_text_encoding, ' ', "men: select_encoding prev");
			return;
		Fi
#endif

		recount_chars ();

		If ! streq (new_prev_text_encoding, item->hopitemname)
		Then	prev_text_encoding = new_prev_text_encoding;
		Fi

		RD ();
		menuline_dirty = True;
		/* move cursor to or behind actual previous character position */
		move_address (curpos, y);
		/* adjust not to stay amidst a character */
		move_to (x, y);
	Else	error ("Selected encoding not supported in this version");
		ring_bell ();
		return;
	Fi
}

local char encoding_flag [3];

local char *
	disp_encoding_1 ()
{
	encoding_flag [0] = '';
	encoding_flag [1] = text_encoding_flag [0];
	encoding_flag [2] = '\0';
	return encoding_flag;
}

local char *
	disp_encoding_2 ()
{
	encoding_flag [0] = '';
	encoding_flag [1] = text_encoding_flag [1];
	encoding_flag [2] = '\0';
	return encoding_flag;
}

local int
	encodingon (item, ii)
		menuitemtype * item;
		int ii;
{
	return streq (item->hopitemname, get_text_encoding ());
}

local int
	kbemulon (item, ii)
		menuitemtype * item;
		int ii;
{
	return emulation == item->hopitemname [0];
}

local int
	kpmodeon (item, ii)
		menuitemtype * item;
		int ii;
{
	return keypad_mode == item->hopitemname [0];
}


local void
	toggle_sort_dirs_first (item, ii)
		menuitemtype * item;
		int ii;
{
	sort_dirs_first = ! sort_dirs_first;
}

local int
	sortdirsfirston (item, ii)
		menuitemtype * item;
		int ii;
{
	return sort_dirs_first;
}

local void
	set_sort_by (item, ii)
		menuitemtype * item;
		int ii;
{
	sort_by_extension = item->hopitemname [0] == 'e';
}

local int
	sortbyon (item, ii)
		menuitemtype * item;
		int ii;
{
	return sort_by_extension == (item->hopitemname [0] == 'e');
}


local void
	toggle_combining ()
{
	If encoding_has_combining ()
	Then	If combining_screen
		Then	char * curpos = cur_text;
			If combining_mode
			Then	combining_mode = False;
			Else	combining_mode = True;
			Fi
			RD ();
			displaymenuline (True);
			/* move cursor to actual previous character position */
			move_address (curpos, y);
			/* adjust not to stay amidst a combined character */
		/*	move_to (x, y);	*/
		Elsif ! combining_mode
		Then	error ("Terminal cannot display combined characters");
		Fi
	Else	error ("Combining display mode not applicable to current encoding");
	Fi
}

local void
	select_combining (item, ii)
		menuitemtype * item;
		int ii;
{
	If encoding_has_combining ()
	Then	If combining_screen
		Then	char * curpos = cur_text;
			FLAG old_combining_mode = combining_mode;
			If ii == 0
			Then	combining_mode = True;
			Else	combining_mode = False;
			Fi
			If combining_mode != old_combining_mode
			Then	RD ();
				menuline_dirty = True;
				/* move cursor to actual previous character position */
				move_address (curpos, y);
			Fi
		Elsif ii == 0
		Then	error ("Terminal cannot display combined characters");
		Fi
	Else	error ("Combining display mode not applicable to current encoding");
	Fi
}

local char *
	disp_combining ()
{
	If encoding_has_combining ()
	Then	If combining_screen && combining_mode
		Then	return "ç";
		Else	return "`";
		Fi
	Else	return " ";
	Fi
}

local int
	comborsepon (item, ii)
		menuitemtype * item;
		int ii;
{
	return (! combining_mode) == ii;
}


local char *
	disp_leftquote ()
{
	If smart_quotes
	Then	return quote_mark (quote_type, 0);
	Else	return " ";
	Fi
}

local char *
	disp_rightquote ()
{
	int utfcount;
	unsigned long unichar;
	char * quotemark;

	If smart_quotes
	Then	quotemark = quote_mark (quote_type, 0);
		utf8_info (quotemark, & utfcount, & unichar);
		If uniscrwidth (unichar, quotemark, quotemark) == 2
		Then	/* left quote mark used up the space already */
			return "";
		Else	/* proceed to right quote mark */
			advance_utf8 (& quotemark);
			utf8_info (quotemark, & utfcount, & unichar);
			If uniscrwidth (unichar, quotemark, quotemark) == 2
			Then	/* no space to display */
				return " ";
			Else
				return quotemark;
			Fi
		Fi
	Else	return " ";
	Fi
}


local void
	toggle_HTML ()
{
	If mark_HTML
	Then	mark_HTML = False;
	Else	mark_HTML = True;
		update_syntax_state (header->next);
	Fi
	RD ();
}


local void
	toggle_password_hiding ()
{
	hide_passwords = ! hide_passwords;
	RD ();
}


local void
	toggle_vt100_graphics ()
{
	show_vt100_graph = ! show_vt100_graph;
	If show_vt100_graph
	Then	status_uni ("Displaying small letters as VT100 graphics/line drawing - ESC . to cancel");
	Fi
	RD ();
}


local void
	select_keymap_entry (item, ii)
		menuitemtype * item;
		int ii;
{
	/* strip off optional additional note from mapping tag */
	/* this is actually obsolete already as additional notes 
	   are explicitly included in the menu table as extratag;
	   leaving in the code for now just in case...
	 */
	char tag [55];
	char * tp = tag;
	char * np = item->hopitemname;
	Dowhile * np && * np != ' '
	Do	* tp ++ = * np ++;
	Done
	* tp = '\0';
	/* set input method according to stripped tag */
	If ! setKEYMAP (tag)
	Then	error ("Selected input method not configured");
	Fi
}

local int
	keymapon (item, ii)
		menuitemtype * item;
		int ii;
{
	return strisprefix (keyboard_mapping, item->hopitemname);
}


local void
	select_quote_type (item, ii)
		menuitemtype * item;
		int ii;
{
	prev_quote_type = quote_type;
	set_quote_type (ii);
}

local void
	toggle_quote_type ()
{
	int prev = prev_quote_type;
	prev_quote_type = quote_type;
	set_quote_type (prev);
}

local void
	locale_quote_type ()
{
	handle_locale_quotes (NIL_PTR, False);
}

local int
	quoteon (item, ii)
		menuitemtype * item;
		int ii;
{
	return quote_type == ii;
}


local char km [3];

local char *
	dispKEYMAP0 ()
{
	If allow_keymap
	Then	km [0] = '';
		km [1] = keyboard_mapping [0];
		km [2] = '\0';
		return km;
	Else	return " ";
	Fi
}

local char *
	dispKEYMAP1 ()
{
	If allow_keymap
	Then	km [0] = '';
		km [1] = keyboard_mapping [1];
		km [2] = '\0';
		return km;
	Else	return " ";
	Fi
}


local char *
	dispinfo ()
{
	return "?";
}

local void
	toggle_info ()
{
}

local void
	select_file_info ()
{
	hop_flag = 1;
	FS ();

	If always_disp_fstat
	Then	always_disp_code = False;
		If ! disp_Han_full
		Then	always_disp_Han = False;
		Fi
	Fi
}

local void
	select_char_info ()
{
	hop_flag = 1;
	display_code ();

	If always_disp_code
	Then	always_disp_fstat = False;
		If ! disp_Han_full
		Then	always_disp_Han = False;
		Fi
	Fi
}

local void
	select_script ()
{
	disp_scriptname = ! disp_scriptname;

	If disp_scriptname
	Then	always_disp_code = True;
	Fi
}

local void
	select_charname ()
{
	disp_charname = ! disp_charname;

	If disp_charname
	Then	always_disp_code = True;
		disp_scriptname = False;
		disp_decomposition = False;
		disp_mnemos = False;
	Fi
}

local void
	select_namedseq ()
{
	disp_charseqname = ! disp_charseqname;

	If disp_charseqname
	Then	always_disp_code = True;
	Fi
}

local void
	select_decomposition ()
{
	disp_decomposition = ! disp_decomposition;

	If disp_decomposition
	Then	always_disp_code = True;
		disp_charname = False;
		disp_mnemos = False;
	Fi
}

local void
	select_mnemos ()
{
	disp_mnemos = ! disp_mnemos;

	If disp_mnemos
	Then	always_disp_code = True;
		disp_charname = False;
		disp_decomposition = False;
	Fi
}

local void
	select_Han_info ()
{
	always_disp_Han = ! always_disp_Han;

	if (always_disp_Han && ! disp_Han_full) {
		always_disp_fstat = False;
		always_disp_code = False;
	}
}

local void
	toggle_Mandarin ()
{
	disp_Han_Mandarin = ! disp_Han_Mandarin;
	If disp_Han_Mandarin
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_Cantonese ()
{
	disp_Han_Cantonese = ! disp_Han_Cantonese;
	If disp_Han_Cantonese
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_Japanese ()
{
	disp_Han_Japanese = ! disp_Han_Japanese;
	If disp_Han_Japanese
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_Sino_Japanese ()
{
	disp_Han_Sino_Japanese = ! disp_Han_Sino_Japanese;
	If disp_Han_Sino_Japanese
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_Hangul ()
{
	disp_Han_Hangul = ! disp_Han_Hangul;
	If disp_Han_Hangul
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_Korean ()
{
	disp_Han_Korean = ! disp_Han_Korean;
	If disp_Han_Korean
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_Vietnamese ()
{
	disp_Han_Vietnamese = ! disp_Han_Vietnamese;
	If disp_Han_Vietnamese
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_HanyuPinlu ()
{
	disp_Han_HanyuPinlu = ! disp_Han_HanyuPinlu;
	If disp_Han_HanyuPinlu
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_HanyuPinyin ()
{
	disp_Han_HanyuPinyin = ! disp_Han_HanyuPinyin;
	If disp_Han_HanyuPinyin
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_XHCHanyuPinyin ()
{
	disp_Han_XHCHanyuPinyin = ! disp_Han_XHCHanyuPinyin;
	If disp_Han_XHCHanyuPinyin
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_Tang ()
{
	disp_Han_Tang = ! disp_Han_Tang;
	If disp_Han_Tang
	Then	always_disp_Han = True;
	Fi
}

local void
	toggle_Han_short_description ()
{
	If always_disp_Han && disp_Han_full
	Then	disp_Han_full = False;
	Else	always_disp_Han = ! always_disp_Han;
		disp_Han_full = False;
	Fi

	If always_disp_Han && ! disp_Han_full
	Then	always_disp_fstat = False;
		always_disp_code = False;
	Fi
}

local void
	toggle_Han_full_description ()
{
	If always_disp_Han && ! disp_Han_full
	Then	disp_Han_full = True;
	Else	always_disp_Han = ! always_disp_Han;
		disp_Han_full = True;
	Fi

	If always_disp_Han && ! disp_Han_full
	Then	always_disp_fstat = False;
		always_disp_code = False;
	Fi
}

local int
	infoon_file (item, ii)
		menuitemtype * item;
		int ii;
{
	return always_disp_fstat;
}

local int
	infoon_char (item, ii)
		menuitemtype * item;
		int ii;
{
	return always_disp_code;
}

local int
	infoon_script (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_scriptname);
}

local int
	infoon_charname (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_charname);
}

local int
	infoon_namedseq (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_charseqname);
}

local int
	infoon_decomposition (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_decomposition);
}

local int
	infoon_mnemos (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_mnemos);
}

local int
	infoon_Han (item, ii)
		menuitemtype * item;
		int ii;
{
	return always_disp_Han;
}

local int
	infoon_Mandarin (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_Mandarin);
}

local int
	infoon_Cantonese (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_Cantonese);
}

local int
	infoon_Japanese (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_Japanese);
}

local int
	infoon_Sino_Japanese (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_Sino_Japanese);
}

local int
	infoon_Hangul (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_Hangul);
}

local int
	infoon_Korean (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_Korean);
}

local int
	infoon_Vietnamese (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_Vietnamese);
}

local int
	infoon_HanyuPinlu (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_HanyuPinlu);
}

local int
	infoon_HanyuPinyin (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_HanyuPinyin);
}

local int
	infoon_XHCHanyuPinyin (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_XHCHanyuPinyin);
}

local int
	infoon_Tang (item, ii)
		menuitemtype * item;
		int ii;
{
	return stayinmenu (disp_Han_Tang);
}

local int
	infoon_descr_short (item, ii)
		menuitemtype * item;
		int ii;
{
	return always_disp_Han && ! disp_Han_full;
}

local int
	infoon_descr_full (item, ii)
		menuitemtype * item;
		int ii;
{
	return always_disp_Han && disp_Han_full;
}


/***********************************************************************\
	Pulldown menu tables
\***********************************************************************/

local menuitemtype Filemenu [] = {
	{"Open", EDIT, ""},
	{"View", VIEW, ""},
	{"Save", WT, ""},
	{"Save As...", SAVEAS, ""},
	{"Save & Exit", EXED, ""},

	{"file switching", separator, ""},
	{"Switch file", SELECTFILE, ""},
	{"Close file", CLOSEFILE, ""},
/*
	{"Next file", NXTFILE, "First file"},
	{"Prev file", PRVFILE, "Last file"},
*/

	{"", separator, ""},
	{"Insert file", INSFILE, ""},
	{"Copy to file", WB, "Append to file"},
	{"Print", PRINT, ""},

	{"manage file", separator, ""},
	{"Recover file", RECOVER, ""},
#ifndef msdos
	{"Unlock file", handleUnlockmenu, "", 0, "▶"},
#endif
	{"Check Out", checkout, ""},
	{"Check In", checkin, ""},

	{"", separator, ""},
	{"Quit", QUED, ""},

#ifdef obsolete
	{"Set Name...", NN, ""},
	{"Save Position", SAVPOS, ""},
#endif
};

local menuitemtype Editmenu [] = {
	{"Mark/Select", MARK, "Go to Mark"},
	{"continue Select", SELECTION, "clear Select"},
	{"Cut", CUT, "Cut & Append"},
	{"Copy", COPY, "Append"},
	{"Paste", PASTE, "Paste Other"},
	{"Paste Previous", YANKRING, ""},
#ifdef __CYGWIN__
	{"Paste Other", PASTEEXT, "Paste Clipboard"},
#else
	{"Paste Other", PASTEEXT, ""},
#endif

	{"set/go markers", separator, ""},
	{"Go to Mark", GOMA, ""},
	{"Set marker N", MARKER, ""},
	{"Go marker N", GOMARKER, ""},

	{"insert", separator, ""},
	{"HTML/XML tag", HTML, "Embed in HTML/XML"},
	{"Control/Composed", CTRLINS, ""},

	{"conversion", separator, ""},
	{"Combine mnemonic", (voidfunc) UML, ""},
	{"Case toggle", LOWCAP, "Case toggle word"},
	{"Word case switch", LOWCAPWORD, ""},

	{"char -> code", separator, "code -> char"},
	{"Hex bytes", changehex, ""},
	{"Unicode value", changeuni, ""},
	{"Decimal value", changedec, ""},
	{"Octal value", changeoct, ""},
};

local menuitemtype Searchmenu [] = {
	{"Find", SFW, "Find Identifier"},
	{"Find backw.", SRV, "Find Idf. backw."},
	{"Find again", RES, "Previous Search"},

	{"", separator, ""},
	{"Find Identifier", SIDFW, ""},
	{"Find Idf. backw.", SIDRV, ""},
	{"Find Character", SCURCHARFW, ""},
	{"Find <matching>", (voidfunc) SCORR, "Find wrong enc."},

	{"", separator, ""},
	{"Substitute", GR, ""},
	{"Replace (?)", REPL, ""},
	{"Replace on line", LR, ""},

	{"go...", separator, ""},
	{"to Definition", Stag, "to Def. of ..."},
	{"<- Back", Popmark, ""},
	{"-> Forward", Upmark, ""},
	{"to Mark", GOMA, ""},
	{"to...", GOTO, ""},
};

local menuitemtype Paragraphmenu [] = {
	{"Justify Clever", JUSclever, "Clever justify other"},
	{"Justify Simple", JUS, "Justify other mode"},

	{"set parameters", separator, ""},
	{"Set Left margin", ADJLM, ""},
	{"Set First line left m.", ADJFLM, ""},
	{"Set Other lines l. m.", ADJNLM, ""},
	{"Set Right margin", ADJRM, ""},
	{"Toggle Auto-justif.", toggle_JUSlevel, ""},
	{"Toggle Line-end mode", toggle_JUSmode, ""},
};

local menuitemtype Optionsmenu [] = {
	{"editing features", separator, ""},
	{"Toggle TAB expansion", toggle_tab_expansion, "", tabexpansionon},
	{"Toggle auto indent", toggle_autoindent, "", autoindenton},
	{"Word wrap mode...", handleJustifymenu, "", 0, "▶"},
	{"Paragraph end...", handleJustifymenu /*handleParagraphmodemenu*/, "", 0, "▶"},
	{"Edit/View only", toggle_VIEWmode, ""},
	{"Paste buffer mode...", handleBuffermenu, "", 0, "▶"},

	{"display settings", separator, ""},
	{"Toggle TAB width", toggle_tabsize, ""},
	{"Toggle HTML disp", toggle_HTML, ""},
	{"Toggle passwd. hiding", toggle_password_hiding, ""},
	{"Display size...", handleScreensizemenu, "", 0, "▶"},
	{"File info", FS, "Toggle File info"},
	{"Char info", display_code, "Toggle Char info"},
	{"CJK/Info display...", handleInfomenu, "", 0, "▶"},
	{"Toggle VT100 graphics", toggle_vt100_graphics, ""},

	{"character handling", separator, ""},
	{"Text Encoding...", handleEncodingmenu, "", 0, "▶"},
	{"Combined/Sep. display", toggle_combining, ""},
	{"Input Method...", handleKeymapmenu, "", 0, "▶"},
	{"Smart Quotes style...", handleQuotemenu, "", 0, "▶"},

	{"emulation / adaption", separator, ""},
	{"Keyboard/Editing...", handleEmulmenu, "", 0, "▶"},
	{"Lineend type...", handleLineendtypemenu, "", 0, "▶"},

	{"file chooser", separator, ""},
	{"File sort options...", handleFilechoosermenu, "", 0, "▶"},

	{"help", separator, ""},
	{"Help topics", HELP_topics, ""},
	{"About mined", ABOUT, ""},
	{"function key bar", HELP_Fn, ""},
	{"accent prefix bar", HELP_accents, ""},
	{"toggle help bar", toggle_FHELP, ""},
};


local menuitemtype Unlockmenu [] = {
	{"grab lock (steal)", grab_lock, ""},
	{"ignore lock (proceed)", ignore_lock, ""},
};

local menuitemtype Screensizemenu [] = {
#ifdef msdos
	{"screen size", separator, ""},
	{"more lines", screenmorelines, "font bank"},
	{"fewer lines", screenlesslines, "char height"},
	{"switch line #", LNSW, "cycle line #"},
	{"", separator, ""},
	{"bigger", screenbigger, "video mode"},
	{"smaller", screensmaller, "graphic mode"},
#else
	{"screen size", separator, ""},
	{"more lines", screenmorelines, ""},
	{"fewer lines", screenlesslines, ""},
	{"more columns", screenmorecols, ""},
	{"fewer columns", screenlesscols, ""},
	{"", separator, ""},
	{"maximize/restore", maximize_restore_screen, ""},
	{"bigger", screenbigger, ""},
	{"smaller", screensmaller, ""},
	{"font size", separator, ""},
	{"bigger", fontbigger, ""},
	{"smaller", fontsmaller, ""},
	{"default", fontdefault, ""},
#endif
};

local menuitemtype Emulmenu [] = {
	{"mined", emul_mined, "m", kbemulon},
	{"", separator, ""},
	{"Windows", emul_Windows, "w", kbemulon},
	{"emacs", emul_emacs, "e", kbemulon},
	{"WordStar", emul_WordStar, "s", kbemulon},
	{"pico", emul_pico, "p", kbemulon},
	{"keypad only", separator, ""},
	{"mined", set_keypad_mined, "m", kpmodeon},
	{"Shift-select", set_keypad_shift_selection, "S", kpmodeon},
	{"Windows", set_keypad_windows, "w", kpmodeon},
};

local menuitemtype Lineendtypemenu [] = {
	{"convert this line...", separator, ""},
	{"to LF", convlineend_cur_LF, ""},
	{"to CRLF", convlineend_cur_CRLF, ""},
	{"convert all lines...", separator, ""},
	{"all to LF", convlineend_all_LF, ""},
	{"all to CRLF", convlineend_all_CRLF, ""},
};

local menuitemtype Filechoosermenu [] = {
	{"directories first", toggle_sort_dirs_first, "", sortdirsfirston},
	{"sort by...", separator, ""},
	{"name", set_sort_by, "n", sortbyon},
	{"extension", set_sort_by, "e", sortbyon},
};


local menutype Pulldownmenu [] = {
	{"File", arrlen (Filemenu), Filemenu},
	{"Edit", arrlen (Editmenu), Editmenu},
	{"Search/go", arrlen (Searchmenu), Searchmenu},
	{"Paragraph", arrlen (Paragraphmenu), Paragraphmenu},
	{"Options", arrlen (Optionsmenu), Optionsmenu},
};


/***********************************************************************\
	Flag menu tables
\***********************************************************************/

#ifdef use_stringification
#define sep(label)	{#label, separator, ""},
#define quotes(prim, sec, tag)	{#tag, select_quote_type, #prim" "#sec, quoteon},
/* this does not work everywhere */
#warning use Quotemenu macros without embedding "quotes"
#else
#define sep(label)	{label, separator, ""},
#define quotes(prim, sec, tag)	{tag, select_quote_type, prim" "sec, quoteon},
#endif

local char quotes_current [] = "current: “…” ‘…’"; /* reserve enough bytes */
local char quotes_standby [] = "standby: “…” ‘…’"; /* reserve enough bytes */

local menuitemtype Quotemenu [] = {
	quotes ("\"\"",	"''",	"\"plain\"")
	{"by locale", locale_quote_type, "(locale)"},
	{quotes_current, dummytoggle, "(active)", dummyflagon},
	{quotes_standby, toggle_quote_type, "(toggle)"},

	sep ("typographic")

	quotes ("“”",	"‘’",	"“English”")
	quotes ("„“",	"‚‘",	"„German“") /* Czech, Slov., Icel., Sorb. */
	quotes ("”“",	"’‘",	"“Arabic/alt Hebrew”")
	sep ("guillemets")
	quotes ("»«",	"›‹",	"»Danish/alt German«")
	sep ("")
	quotes ("«»",	"‹›",	"«Swiss/Fr.Arabic»") /* Azerb., Basq., alt Turk. */
	quotes ("« »",	"“ ”",	"« French “ … ” »")
	quotes ("«»",	"“”",	"«Spanish “…” »") /* Armenian */
	quotes ("«»",	"‘’",	"«Norwegian ‘…’ »") /* alt Bulgarian */
	quotes ("«»",	"„“",	"«Russian „…“ »") /* Ukrainian, Latvian */
	sep ("in unison")
	quotes ("””",	"’’",	"”Swedish/Finnish”") /* alt Norwegian */
	quotes ("»»",	"››",	"»alt Finn/Swed»")
	quotes ("„”",	"‚’",	"„Dutch/Pl”/”Hebrew„") /* Croatian, Polish */
	sep ("")
	quotes ("„”",	"«»",	"„Roman./Pl. «…» ”")
	quotes ("„”",	"»«",	"„Hungar./Pl. »…« ”")
	sep ("CJK")
	quotes ("『』",	"「」",	"「corners 『… 』」") /* trad Chin., Kor., Jap. */
	quotes ("【】",	"〖〗",	"【squares 〖… 〗】") /* Jap. */
	quotes ("《》",	"〈〉",	"《titles 〈… 〉》") /* North Korean */
	quotes ("〝〞",	"“”",	"〝dbl primes “…” 〞")
	quotes ("〝〟",	"“”",	"〝low primes “…” 〟")
	sep ("misc")
	quotes ("«»",	"‟”",	"«Greek ‟…” »")
	quotes ("“„",	"‘‚",	"“alt Greek ‘…‚ „")
	quotes ("« »",	"‹ ›",	"« alt French ‹ › »")
	sep ("")
	quotes ("„“",	"’‘",	"„Macedonian ’…‘ “")
	quotes ("„“",	"’’",	"„Serb.Lat/a.Dan.’’“")
	quotes ("»«",	"’’",	"»alt Danish ’…’ «")
	quotes ("«»",	"‚’",	"«alt Polish ‚…’ »")
	quotes ("„”",	"’’",	"„alt Hungarian ’…’”")
};


local menuitemtype Keymapmenu [] = {
	{"none", select_keymap_entry, "--", keymapon},
#include "keymapsm.t"
};

#include "charesub.t"

local menuitemtype encodingmenu [] = {
	{"Unicode", select_encoding, "UTF-8", encodingon},

	{"8 Bit", separator, ""},
	{"Latin-1", select_encoding, "ISO 8859-1", encodingon},
#include "charemen.t"
};

local menuitemtype combiningmenu [] = {
	{"combined", select_combining, "", comborsepon},
	{"separated", select_combining, "", comborsepon},
};

local menuitemtype infomenu [] = {
	{"File info", select_file_info, "", infoon_file},
#define dont_debug_menu_display
#ifdef debug_menu_display
	{"debugging entries", separator, ""},
	{".ctrl.ng",		dummytoggle, "", dummyflagoff},
	{".han.�Է�ϰ�Է�ϰ�xy",	dummytoggle, "", dummyflagoff},
	{".ind..��- .�����",	dummytoggle, "", dummyflagoff},
	{".combining.ảxٕf",	dummytoggle, "", dummyflagoff},
	{".join-lamلاalef-xây",	dummytoggle, "", dummyflagoff},
	{".scriptδφόalef",	dummytoggle, "", dummyflagoff},
	{".uäöüß€噞xalef",	dummytoggle, "", dummyflagoff},
#endif

	{"display char info", separator, ""},
	{"Char info", select_char_info, "", infoon_char},
	{" with script", select_script, "", infoon_script},
	{" with char name", select_charname, "", infoon_charname},
	{" with named seq.", select_namedseq, "", infoon_namedseq},
	{" with decompos.", select_decomposition, "", infoon_decomposition},
	{" with mnemos", select_mnemos, "", infoon_mnemos},

	{"display Han info", separator, ""},
	{"Han info", select_Han_info, "", infoon_Han},
	{" in status line", toggle_Han_short_description, "", infoon_descr_short},
	{" in popup", toggle_Han_full_description, "", infoon_descr_full},

	{"pronunciations", separator, ""},
	{"Mandarin", toggle_Mandarin, "", infoon_Mandarin},
	{"Cantonese", toggle_Cantonese, "", infoon_Cantonese},
	{"Japanese", toggle_Japanese, "", infoon_Japanese},
	{"Sino-Japanese", toggle_Sino_Japanese, "", infoon_Sino_Japanese},
	{"Hangul", toggle_Hangul, "", infoon_Hangul},
	{"Korean", toggle_Korean, "", infoon_Korean},
	{"Vietnamese", toggle_Vietnamese, "", infoon_Vietnamese},
	{"Hanyu Pinlu", toggle_HanyuPinlu, "", infoon_HanyuPinlu},
	{"Hanyu Pinyin", toggle_HanyuPinyin, "", infoon_HanyuPinyin},
	{"XHC Hànyǔ pīnyīn", toggle_XHCHanyuPinyin, "", infoon_XHCHanyuPinyin},
	{"Tang", toggle_Tang, "", infoon_Tang},
};

local menuitemtype textmenu [] = {
	{"edit", select_editmode, "modify", editorviewon},
	{"view", select_editmode, "readonly", editorviewon},
};

local menuitemtype buffermenu [] = {
	{"overwrite", select_buffermode, "", overwriteorappendon},
	{"append", select_buffermode, "", overwriteorappendon},

	{"buffer encoding", separator, ""},
	{"Unicode", toggle_pastebuf_mode, "", pastebuf_utf8on},

	{"copy/paste area", separator, ""},
	{"rectangular", toggle_rectangular_paste_mode, "", rectangularpasteon},

	{"visual select", separator, ""},
	{"keep on search", select_visselect_keeponsearch, "", visselect_keeponsearch_on},
	{"auto-copy", select_visselect_autocopy, "", visselect_autocopy_on},
};

local menuitemtype autoindentmenu [] = {
	{"auto indent", toggle_autoindent, "", autoindenton},
	{"auto number", toggle_autonumber, "", autonumberon},
#ifdef fine_numbering_undent_control
	{"back-number", toggle_backnumber, "", backnumberon},
	{"back-increm", toggle_backincrem, "", backincremon},
#endif

	{"", separator, ""},
	{"TAB expand", toggle_tab_expansion, "", tabexpansionon},

	{"TAB size", separator, ""},
	{"2", select_tabsize, "", tabsizeon},
	{"4", select_tabsize, "", tabsizeon},
	{"8", select_tabsize, "", tabsizeon},
};

local menuitemtype justifymenu [] = {
	{"Word wrap...", separator, ""},
	{"on command", select_justify, "ESC j", justifymodeon},
	{"at line end", select_justify, "«", justifymodeon},
	{"always", select_justify, "!", justifymodeon},

	{"Paragraph ends at", separator, ""},
	{"non-blank line-end", select_paragraph, "'... «... «...«'", paragraphmodeon},
	{"empty line", select_paragraph, "'...«...««'", paragraphmodeon},
};

#ifdef unused
local menuitemtype paragraphmodemenu [] = {
	{"non-blank line-end", select_paragraph, "'... «... «...«'", paragraphmodeon},
	{"empty line", select_paragraph, "'...«...««'", paragraphmodeon},
};
#endif


/***********************************************************************\
	Popup menu table
\***********************************************************************/

local menuitemtype Popupmenu [] = {
	{"Mark/Select", MARK, "Go to Mark"},
	{"continue Select", SELECTION, "clear Select"},
	{"toggle rect area", toggle_rectangular_paste_mode, ""},
	{"Cut", CUT, "Cut-Append"},
	{"Copy", COPY, "Append"},
	{"Paste", PASTE, "Paste external"},
	{"HTML/XML tag", HTML, "Embed in HTML/XML"},

	{"search", separator, ""},
	{"Find", SFW, "FindIdf"},
	{"FindReverse", SRV, "FindIdfRev"},
	{"FindNext", RES, "Previous Find"},
	{"FindIdf", SIDFW, ""},
	{"FindIdfRev", SIDRV, ""},
	{"Find matching ()", (voidfunc) SCORR, "Find wrong enc."},

	{"go...", separator, ""},
	{"to Definition", Stag, "to Def. of ..."},
	{"<- Back", Popmark, ""},
	{"-> Forward", Upmark, ""},
	{"to Mark", GOMA, ""},
};


/***********************************************************************\
	menu structure auxiliary functions
\***********************************************************************/

global int
	count_quote_types ()
{
	return arrlen (Quotemenu);
}


global FLAG
	spacing_quote_type (qt)
		int qt;
{
	If strcontains (Quotemenu [qt].itemname, "French")
	Then	return True;
	Else	return False;
	Fi
}


global char *
	quote_mark (i, n)
		int i;
		quoteposition_type n;
{
	char * q = Quotemenu [i].hopitemname;
	while (* q == ' ') {
		q ++;
	}
	while (n > 0) {
		advance_utf8 (& q);
		while (* q == ' ') {
			q ++;
		}
		n --;
	}
	return q;
}


global void
	set_quote_menu ()
{
	char q_buf [maxPROMPTlen];

	strcpy (quotes_standby + 9, quote_mark (prev_quote_type, 0));
	strcpy (quotes_current + 9, quote_mark (quote_type, 0));

	if (isscreenmode) {
		strcpy (q_buf, "Quote marks style ");
		strcat (q_buf, Quotemenu [quote_type].itemname);
		strcat (q_buf, " ");
		strcat (q_buf, quote_mark (quote_type, 0));
		strcat (q_buf, " - standby ");
		strcat (q_buf, Quotemenu [prev_quote_type].itemname);
		strcat (q_buf, " ");
		strcat (q_buf, quote_mark (prev_quote_type, 0));
		strcat (q_buf, "");
		status_uni (q_buf);
	}
}


global int
	lookup_quotes (q)
		char * q;
{
	int i;
	For i = 0 While i < arrlen (Quotemenu) Step i ++
	Do	If strisprefix (q, quote_mark (i, 0))
		Then	return i;
		Fi
	Done
	For i = 0 While i < arrlen (Quotemenu) Step i ++
	Do	If strisprefix (q, quote_mark (i, 2))
		Then	return i;
		Fi
	Done
	return -1;
}




local menuitemtype *
	lookup_menuitem (menu, len, hopitem)
		menuitemtype * menu;
		int len;
		char * hopitem;
{
	int i;
	For i = 0 While i < len Step i ++
	Do	If streq (hopitem, menu [i].hopitemname)
		Then	return & menu [i];
		Fi
	Done
	return 0;
}


global menuitemtype *
	lookup_Keymap_menuitem (hopitem)
		char * hopitem;
{
	return lookup_menuitem (Keymapmenu, arrlen (Keymapmenu), hopitem);
}




/***********************************************************************\
	Flag table and next/previous function
\***********************************************************************/

local char * dispKEYMAP0 ();
local char * dispKEYMAP1 ();


local flagitemtype Flagmenu [] = {
	{dispinfo, toggle_info, "Info display", infomenu, arrlen (infomenu)},
	{dispnothing, toggle_nothing, NIL_PTR},
	{dispKEYMAP0, toggleKEYMAP, "Input Method", Keymapmenu, arrlen (Keymapmenu)},
	{dispKEYMAP1, toggleKEYMAP, "Input Method", Keymapmenu, arrlen (Keymapmenu)},
	{dispnothing, toggle_nothing, NIL_PTR},
	{disp_leftquote, toggle_quote_type, "Quote marks", Quotemenu, arrlen (Quotemenu)},
	{disp_rightquote, toggle_quote_type, "Quote marks", Quotemenu, arrlen (Quotemenu)},
	{dispnothing, toggle_nothing, NIL_PTR},
	{disp_encoding_1, toggle_encoding, "Encoding", encodingmenu, arrlen (encodingmenu)},
	{disp_encoding_2, toggle_encoding, "Encoding", encodingmenu, arrlen (encodingmenu)},
	{disp_combining, toggle_combining, "Combining", combiningmenu, arrlen (combiningmenu)},
	{dispnothing, toggle_nothing, NIL_PTR},

	{dispHOP, toggle_HOP, NIL_PTR},
	{dispVIEW, toggle_VIEWmode, "Text", textmenu, arrlen (textmenu)},
	{dispnothing, toggle_nothing, NIL_PTR},

	{disppastebuf_1, toggle_rectangular_paste_mode, "Paste buffer", buffermenu, arrlen (buffermenu)},
	{disppastebuf_2, toggle_append, "Paste buffer", buffermenu, arrlen (buffermenu)},
#ifdef debug_ring_buffer
	{disp_buffer_open, toggle_nothing, NIL_PTR},
#endif
	{dispnothing, toggle_nothing, NIL_PTR},

	{disp_autoindent, toggle_autoindent, "Indent/TAB", autoindentmenu, arrlen (autoindentmenu)},
	{disp_tabmode, toggle_tabexpand, "Indent/TAB", autoindentmenu, arrlen (autoindentmenu)},
	{dispnothing, toggle_nothing, NIL_PTR},
	{dispJUSlevel, toggle_JUSlevel, "Justification", justifymenu, arrlen (justifymenu)},
	{dispJUSmode, toggle_JUSmode, "Justification", justifymenu, arrlen (justifymenu)},
};


local void
	flag_menu (i)
		int i;
{
	action_menu (Flagmenu [i].menu, 
			Flagmenu [i].menulen, 
			- (flags_pos + i), 0, 
			Flagmenu [i].menutitle, True);
}


local FLAG
	open_next_menu (current_menu)
		menuitemtype * current_menu;
{
	int i;
	FLAG passed = False;

	If current_menu == (menuitemtype *) 0
	Then	passed = True;
	Fi

	If current_menu == Popupmenu && ! (keyshift & alt_mask)
	Then	passed = True;
	Fi

	For i = 0 While i < arrlen (Pulldownmenu) Step i ++
	Do
		If Pulldownmenu [i].menuitems == current_menu
		Then	passed = True;
			If keyshift & alt_mask
			Then	break;
			Fi
		Elsif passed
		Then	openmenu (i);
			return True;
		Fi
	Done

	If current_menu == Popupmenu
	Then	passed = True;
	Fi

	For i = 0 While i < arrlen (Flagmenu) Step i ++
	Do
		If Flagmenu [i].menutitle != NIL_PTR
		&& Flagmenu [i].menu == current_menu
		Then	passed = True;
			If keyshift & alt_mask
			Then	break;
			Fi
		Elsif passed && Flagmenu [i].menutitle != NIL_PTR
		      && * ((* Flagmenu [i].dispflag) ()) != ' '
		Then
			/* on a double-column flag menu, move right */
			If i + 1 < arrlen (Flagmenu)
			&& Flagmenu [i + 1].menutitle != NIL_PTR
			&& streq (Flagmenu [i].menutitle, Flagmenu [i + 1].menutitle)
			Then	i ++;
			Fi
			flag_menu (i);
			return True;
		Fi
	Done

	If passed
	Then	openmenu (0);
		return True;
	Else	return False;
	Fi
}


local FLAG
	open_prev_menu (current_menu)
		menuitemtype * current_menu;
{
	int i;
	FLAG passed = False;

	If current_menu == (menuitemtype *) 0
	Then	passed = True;
	Fi

	If current_menu == Popupmenu && ! (keyshift & alt_mask)
	Then	passed = True;
	Fi

	For i = arrlen (Pulldownmenu) - 1 While i >= 0 Step i --
	Do
		If Pulldownmenu [i].menuitems == current_menu
		Then	passed = True;
			If keyshift & alt_mask
			Then	break;
			Fi
		Elsif passed
		Then	openmenu (i);
			return True;
		Fi
	Done

	If current_menu == Popupmenu
	Then	passed = True;
	Fi

	For i = arrlen (Flagmenu) - 1 While i >= 0 Step i --
	Do
		If Flagmenu [i].menutitle != NIL_PTR
		&& Flagmenu [i].menu == current_menu
		Then	passed = True;
			If keyshift & alt_mask
			Then	break;
			Fi
		Elsif	passed && Flagmenu [i].menutitle != NIL_PTR
			&& * ((* Flagmenu [i].dispflag) ()) != ' '
		Then	flag_menu (i);
			return True;
		Fi
	Done

	If passed
	Then	openmenu (arrlen (Pulldownmenu) - 1);
		return True;
	Else	return False;
	Fi
}




/***********************************************************************\
	tag/background attribute handling
	putblock () writes a block graphics character
\***********************************************************************/

local void
	tagdisp_on ()
{
	menuitem_on ();
}

local void
	tagdisp_off ()
{
	menuitem_off ();
}

local void
	menubg_on ()
{
	If redefined_ansi2
	Then	putescape ("\033[42m");
	Elsif ! bw_term
	Then	If dark_term
		Then	/* moderately dark shading background */
			If colours_256 || colours_88
			Then	putescape ("\033[100;48;5;81;48;5;238m");
			Elsif ! is_char_menu
			Then	putescape ("\033[44;100m");
			Fi
		Else	/* moderately light shading background */
			If colours_256 || colours_88
			Then	/*putescape ("\033[43;48;5;57;48;5;227m");*/
				putescape ("\033[43;48;5;77;48;5;230m");
			Elsif ! is_char_menu
			Then	putescape ("\033[43m");
			Fi
		Fi
		If (bright_term && ! darkness_detected)
		|| (! dark_term && screen_version)
		|| ! (colours_256 || colours_88 || is_char_menu)
		Then	If dark_term
			Then	putescape ("\033[33m");
			Else	putescape ("\033[34m");
			Fi
		Fi
	Fi
}

local void
	menubg_off ()
{
	disp_normal ();
}


local void
	putblock (c)
		char c;
{
	If horizontal_bar_width == 2
	Then	/* workaround for xterm remaining background glitch */
		putstring ("  ");
	Fi

	If in_menu_border
	Then	putblockchar (c);
	Else	menuborder_on ();
		putblockchar (c);
		menuborder_off ();
	Fi
}


/***********************************************************************\
	putnstr () writes a string to screen at adjusted length
		used for menu items or menu borders
	putnstr_mark () does the same and marks given item (word)
	putnstr_tag () does the same and marks last column with tag
	putnstr_fill () does the same as subheading and fills the line
\***********************************************************************/

#define dont_debug_item_marking
#ifdef debug_item_marking
#define trace_item_marking(params)	printf params
#else
#define trace_item_marking(params)	
#endif


local char *
	putnstr_tag_mark (s, s2, extratag, l, mark_item, fill_line, selected)
		char * s;	/* item string */
		char * s2;	/* item string 2 (field) */
		char * extratag;	/* end item with this tag (highlight) */
		int l;		/* width of item contents, incl. extratag, excl. flag */
		int mark_item;	/* item to be marked in pick list (character selection menu) */
		FLAG fill_line;	/* fill line for sub-heading */
		FLAG selected;	/* line already marked as selected menu item */
{
	int i = 0;		/* column counter */
	char * spoi = s;
	int mark_index = 0;	/* subitem counter for marking */
	voidfunc mark_state = 0;
	FLAG title_active = True;
	FLAG lookup_show_info = True;
	char * show_char = NIL_PTR;
	trace_item_marking (("putnstr <%s>\n", s));

	If extratag && * extratag
	Then	l -= extratag_length;
	Fi

	If mark_item == 0
	Then
		trace_item_marking (("border on @ %d\n", i));

		If use_graphic_borders
		Then	/* workaround for graphics/colour attribute quirk */
			/*reverse_on ();*/
		Fi

		If * spoi >= '0' && * spoi <= '9'
		Then	/* normal subitem index label */
			reverse_off ();
			mark_on ();
			mark_state = mark_on;
		Else	/* R/S stroke count prefix */
			reverse_on ();
			mark_state = reverse_on;
		Fi
	Elsif mark_item > 0
	Then
		trace_item_marking (("header on @ %d\n", i));

		reverse_on ();
		mark_state = reverse_on;
	Fi
	Dowhile i < l
	Do
		If * spoi == '\0'
		Then	If mark_item < 0 && mark_state == menuitem_on
			Then	/* end display of title input field */
				menuborder_on ();
				menuheader_on ();
				mark_state = 0;
			Elsif mark_index == mark_item
			Then	/* end marked item */
				trace_item_marking (("end string @ %d\n", i));

				menuitem_off ();
				reverse_on ();
				mark_state = reverse_on;
			Fi
			mark_index ++;
			If fill_line
			Then
				If title_active
				Then	reverse_off ();
					title_active = False;
					If full_menu_bg
					Then	menubg_on ();
						menuborder_on ();
					Fi
					If use_graphic_borders
					&& horizontal_bar_width == 2
					&& (i & 1)
					Then
						putchar (' ');
					Fi
				Fi
				If use_graphic_borders
				Then	If horizontal_bar_width == 1
					|| ! (i & 1)
					Then	putblock ('q');
					Fi
				Else	menudisp_on ();
					putchar (' ');
				Fi
			Else	putchar (' ');
			Fi
			i ++;
			If s2 != NIL_PTR
			Then	/* begin display of title input field */
				spoi = s2;
				s2 = NIL_PTR;
				disp_normal ();
				mark_state = menuitem_on;
			Fi
		Elsif * spoi == ' '
		Then
			If mark_index == mark_item
			Then	/* end marked item */
				trace_item_marking (("end marked item @ %d\n", i));

				menuitem_off ();
				reverse_on ();
				mark_state = reverse_on;
			Fi
			mark_index ++;

			/* workaround for stateless colour attribute handling */
			If mark_item >= 0 && selected && ! utf8_screen
			Then	/* restore background screen attribute */
				disp_normal ();
				reverse_on ();
			Fi

			putchar (* spoi ++);
			i ++;

			If mark_index == mark_item
			Then	/* begin marked item */
				trace_item_marking (("begin marked item @ %d\n", i));

				reverse_off ();
				mark_on ();
				mark_state = mark_on;
			Fi
		Elsif mark_state == mark_on && * spoi == ':'
		Then	putchar (* spoi ++);
			i ++;

			mark_off ();
			menuitem_on ();
			mark_state = menuitem_on;
		Elsif mark_item == 0 && mark_index == 0
			&& mark_state == reverse_on && (* spoi & 0x80) == 0
		Then	putchar (* spoi ++);
			i ++;

			/* late begin marked item for R/S menu */
			trace_item_marking (("late begin marked item @ %d\n", i));

			reverse_off ();
			mark_on ();
			mark_state = mark_on;
		Else	int utfcount;
			unsigned long unichar;
			utf8_info (spoi, & utfcount, & unichar);
			If mark_index == mark_item && lookup_show_info && unichar >= 0x2E80
			Then	/* found Han character within marked item;
				   remember for returning, so the caller 
				   can display Han info for it */
				show_char = spoi;
				lookup_show_info = False;
			Fi

			If combining_screen && ! combining_mode
			   /*&& iscombined (unichar, spoi, s)*/
			   && term_iscombining (unichar)
			Then	If iswide (unichar)
				Then	put_unichar (0x3000);
				Else	put_unichar (' ');
				Fi
			Fi
			advance_utf8_scr (& spoi, & i, s);
			If unichar == 0x0644
			Then
			    unsigned long unichar2;
			    utf8_info (spoi, & utfcount, & unichar2);
			    If unichar2 == 0x0622 || unichar2 == 0x0623 || unichar2 == 0x0625 || unichar2 == 0x0627
			    Then	/* handle LAM/ALEF ligature joining */
				If apply_joining
				Then	If combining_screen && ! combining_mode
					Then	/* enforce separated display */
						unichar = 0xFEDD; /* isolated LAM */
					Else	advance_utf8 (& spoi);
						unichar = ligature_lam_alef (unichar2);
					Fi
				Elsif joining_screen
				Then	If combining_screen && ! combining_mode
					Then	/* enforce separated display */
						put_unichar (0xFEDD); /* isolated LAM */
						unichar = unichar2;
						i ++;
					Else	put_unichar (unichar);
						unichar = unichar2;
					Fi
					advance_utf8 (& spoi);
				Fi
			    Fi
			Fi
			If ! utf8_text
			Then	/* workaround for width handling problem */
				utf8_text = True;
				put_unichar (unichar);
				utf8_text = False;
			Else	put_unichar (unichar);
			Fi

			/* restore background colour after 
			   highlighted replacement display; workaround 
			   for stateless colour attribute handling */
			If ((cjk_term || mapped_term) && no_char (mappedtermchar (unichar)))
			|| (! utf8_screen && ! cjk_term && ! mapped_term && unichar >= 0x100)
			|| unichar < (unsigned long) ' '
			|| (unichar >= 0x7F && unichar <= 0xA0)
			Then
				If full_menu_bg
				Then	menubg_on ();
				Fi
				If selected
				Then	/* restore screen attribute 
					   of selected item */
					If mark_state
					Then	disp_normal ();
						(* mark_state) ();
					Else	disp_selected (True, False);
					Fi
				Else	/* possibly restore screen attribute 
					   of filename tab item */
					disp_tab_colour (); /* what a hack */
				Fi
			Fi
		Fi
	Done
	If mark_state == reverse_on
	Then
		trace_item_marking (("header off @ %d\n", i));

		reverse_off ();
	Elsif mark_state == mark_on || mark_state == menuitem_on
	Then
		trace_item_marking (("border off @ %d\n", i));

		menuitem_off ();
		reverse_off ();
	Elsif fill_line && title_active
	Then	reverse_off ();
		If full_menu_bg
		Then	menubg_on ();
			menuborder_on ();
		Fi
	Fi

	If extratag && * extratag
	Then	If streq (extratag, "▶")
		Then	/* highlight submenu marker with one of:
				mark
				tagdisp
				unidisp
				menuitem+menuheader
				menuitem+reverse
			   don't use menuborder_on (alt_cset↯, cygwin...)
			 */
			mark_on ();
			put_submenu_marker (False);
			mark_off ();
		Else	tagdisp_on ();
			put_unichar (utf8value (extratag));
			tagdisp_off ();
		Fi
		If full_menu_bg
		Then	menubg_on ();
		Fi
	Fi

	return show_char;
}


local char *
	putnstr_mark (s, l, mark_item, fill_line, selected)
		char * s;
		int l;
		int mark_item;
		FLAG fill_line;
		FLAG selected;
{
	return putnstr_tag_mark (s, NIL_PTR, NIL_PTR, l, mark_item, fill_line, selected);
}


local void
	putnstr (s, l)
		char * s;
		int l;
{
	(void) putnstr_mark (s, l, -2, False, False);
}


local void
	putnstr2 (s, s2, l)
		char * s;
		char * s2;
		int l;
{
	(void) putnstr_tag_mark (s, s2, NIL_PTR, l, -2, False, False);
}


local void
	putnstr_tag (s, extratag, l, selected)
		char * s;
		char * extratag;
		int l;
		FLAG selected;	/* mark line as selected menu item */
{
	(void) putnstr_tag_mark (s, NIL_PTR, extratag, l, -2, False, selected);
}


local void
	putnstr_fill (s, l)
		char * s;
		int l;
{
	(void) putnstr_mark (s, l, -2, True, False);
}


/***********************************************************************\
	putborder_top/middle/bottom () writes menu borders
	update_title is a modified putborder_top
\***********************************************************************/

/*#define menutopmargin menumargin*/
#define menutopmargin 0

local void
	putborder_top (x, y, width, title, scrolled)
		int x, y, width;
		char * title;
		int scrolled;
{
	int i;

	set_cursor (x, y - 1);
	If full_menu_bg
	Then	menubg_on ();
	Fi
	If title != NIL_PTR
	Then
		If use_graphic_borders
		Then	menuborder_on ();

			If scrolled
			Then	putblock ('f');
			Else	putblock ('l');
			Fi

			menuheader_on ();
			putnstr ("", menutopmargin);
		Else	menudisp_on ();
			putnstr ("", 1 + menutopmargin);
		Fi

		If standout_glitch && y == 0
		Then
			For i = 0 While i < width - 2 - 2 * menutopmargin Step i ++
			Do	menuheader_on ();
				putchar ('x');
			Done
			set_cursor (x + horizontal_bar_width + menutopmargin, y - 1);
		Fi
		If full_menu_bg
		Then	menubg_off ();
			menuborder_on ();
			menuheader_on ();
		Fi
		putnstr (title, width - 2 - 2 * menutopmargin);

		If use_graphic_borders
		Then	putnstr ("", menutopmargin);
			menuheader_off ();
			If full_menu_bg
			Then	menubg_on ();
			Fi
			menuborder_on ();

			If scrolled
			Then	putblock ('f');
			Else	putblock ('k');
			Fi
			menuborder_off ();
		Else	putnstr ("", 1 + menutopmargin);
		Fi
	Else
		If use_graphic_borders
		Then
			If horizontal_bar_width == 2
			Then	/* workaround for xterm bold glitch */
				menuheader_off ();
				If full_menu_bg
				Then	menubg_on ();
				Fi
			Fi

			menuborder_on ();

			If scrolled
			Then	putblock ('f');
			Else	putblock ('l');
			Fi

			For i = 2 While i < width Step i += horizontal_bar_width
			Do
				putblock ('q');
			Done

			If scrolled
			Then	putblock ('f');
			Else	putblock ('k');
			Fi

			menuborder_off ();
		Else
			menudisp_on ();
			putnstr ("", width);
		Fi
	Fi
}

local void
	putborder_middle (x, y, width, subtitle)
		int x, y, width;
		char * subtitle;
{
	int i;

	set_cursor (x, y - 1);
	If full_menu_bg
	Then	menubg_on ();
	Fi
	If subtitle != NIL_PTR && * subtitle != '\0'
	Then
		If use_graphic_borders
		Then	putblock ('t');
			menuheader_on ();
		Else	putnstr ("", 1);
		Fi

		If full_menu_bg
		Then	menubg_off ();
			menuborder_on ();
			menuheader_on ();
		Fi
		putnstr_fill (subtitle, width - 2);

		If full_menu_bg
		Then	menubg_on ();
			If use_graphic_borders
			Then	menuborder_on ();
				putblock ('u');
				menuborder_off ();
			Else	putnstr ("", 1);
			Fi
		Elsif use_graphic_borders
		Then	menuheader_off ();
			menuborder_on ();
			putblock ('u');
			menuborder_off ();
		Else	putnstr ("", 1);
		Fi
	Else
		If use_graphic_borders
		Then
			If horizontal_bar_width == 2
			Then	/* workaround for xterm bold glitch */
				menuheader_off ();
				If full_menu_bg
				Then	menubg_on ();
				Fi
			Fi

			menuborder_on ();

			putblock ('t');

			For i = 2 While i < width Step i += horizontal_bar_width
			Do
				putblock ('q');
				If horizontal_bar_width == 2
				Then	/* workaround for xterm font handling bug */
					menuborder_on ();
				Fi
			Done

			putblock ('u');

			menuborder_off ();
		Else
			putnstr ("", width);
		Fi
	Fi
}

local void
	putborder_bottom (x, y, width, scrolled)
		int x, y, width;
		int scrolled;
{
	int i;

	set_cursor (x, y - 1);
	If use_graphic_borders
	Then
		If horizontal_bar_width == 2
		Then	/* workaround for xterm bold glitch */
			menuheader_off ();
		Fi
		If full_menu_bg
		Then	menubg_on ();	/* after workaround above! */
		Fi

		menuborder_on ();

		If scrolled
		Then	putblock ('g');
		Else	putblock ('m');
		Fi

		For i = 2 While i < width Step i += horizontal_bar_width
		Do
			putblock ('q');
			If horizontal_bar_width == 2
			Then	/* workaround for xterm font handling bug */
				menuborder_on ();
			Fi
		Done

		If scrolled
		Then	putblock ('g');
		Else	putblock ('j');
		Fi

		menuborder_off ();
	Else
		If full_menu_bg
		Then	menubg_on ();
		Fi
		putnstr ("", width);
		menudisp_off ();
	Fi
}


local void
	update_title (x, y, width, title, text, scrolled)
		int x, y, width;
		char * title;
		char * text;
		int scrolled;
{
	int i;

	set_cursor (x, y - 1);
	If full_menu_bg
	Then	menubg_on ();
	Fi
	If title != NIL_PTR
	Then
		If use_graphic_borders
		Then	menuborder_on ();

			If scrolled
			Then	putblock ('f');
			Else	putblock ('l');
			Fi

			menuheader_on ();
			putnstr ("", menumargin);
		Else	menudisp_on ();
			putnstr ("", 1 + menumargin);
		Fi

		If standout_glitch && y == 0
		Then	For i = 0 While i < width - 2 - 2 * menumargin Step i ++
			Do	menuheader_on ();
				putchar (' ');
			Done
			set_cursor (x + horizontal_bar_width + menumargin, y - 1);
		Fi
		If full_menu_bg
		Then	menubg_off ();
			menuborder_on ();
			menuheader_on ();
		Fi
		putnstr2 (title, text, width - 2 - 2 * menumargin);

		If use_graphic_borders
		Then	putnstr ("", menumargin);
			menuheader_off ();
			If full_menu_bg
			Then	menubg_on ();
			Fi
			menuborder_on ();

			If scrolled
			Then	putblock ('f');
			Else	putblock ('k');
			Fi
		Else	putnstr ("", 1 + menumargin);
		Fi
	Else
		If use_graphic_borders
		Then	menuborder_on ();

			If horizontal_bar_width == 2
			Then	/* workaround for xterm bold glitch */
				menuheader_off ();
			Fi

			If scrolled
			Then	putblock ('f');
			Else	putblock ('l');
			Fi

			For i = 2 While i < width Step i += horizontal_bar_width
			Do	putblock ('q');
			Done

			If scrolled
			Then	putblock ('f');
			Else	putblock ('k');
			Fi
		Else	menudisp_on ();
			putnstr ("", width);
		Fi
	Fi

	/* fix display mode */
	If use_graphic_borders
	Then	menuborder_off ();
	Else	menudisp_off ();
	Fi
}


/***********************************************************************\
	number_menus () determines the number of pull-down menus
\***********************************************************************/

local int
	number_menus ()
{
	return arrlen (Pulldownmenu);
}


/***********************************************************************\
	calcmenuvalues () determines menu screen positions
	width_of_menu () determines the display width of the menu
\***********************************************************************/

local int
	width_of_menu (menu, menulen, startcol, isflagmenu, minwidth)
		menuitemtype * menu; int menulen;
		int startcol;
		int isflagmenu;
		int minwidth;
{
	int i;
	menuitemtype * item;
	int width = minwidth;
	If minwidth > 0
	Then	width += 1 + 2 * menumargin - isflagmenu + extratag_length;
	Fi
	For i = 0 While i < menulen Step i ++
	Do
		char * iname;
		int itemlen;

		item = & (menu [i]);

		itemlen = 2 + 2 * menumargin;
		If item->itemfu == separator
		Then	itemlen -= isflagmenu;
		Fi

		iname = hop_flag > 0 && item->hopitemname && * item->hopitemname
			? item->hopitemname
			: item->itemname;
		If iname && * iname
		Then	itemlen += utf8_col_count (iname);
		Fi
		/* add display length of extra tag if present */
		If item->extratag
		Then	itemlen += extratag_length;
		Fi

		/* increase maximum if this width is greater */
		If itemlen > width
		Then	width = itemlen;
		Fi
	Done
	width += isflagmenu;
	If width > XMAX
	Then	width = XMAX;
	Fi
	If startcol > 0 && startcol + width > XMAX - 1
	Then	width = XMAX + 1 - startcol;
	Fi
	If cjk_width_data_version && (width & 1)
	Then	width ++;
	Fi
	return width;
}


#define Flagmenu_gap 4
#define menuendgap 0

local void
	calcmenuvalues ()
{
	int flags_gap;

	If width_data_version && cjk_width_data_version
	&& (! use_vt100_block_graphics || xterm_version > 0 || cjk_term)
	Then	horizontal_bar_width = 2;
	Fi

	/* determine positions for menus and flags display */
	pulldownmenu_width = XMAX + 1 - arrlen (Flagmenu) - Flagmenu_gap;
	If pulldownmenu_width <= 0
	Then	pulldownmenu_width = 0;
	Else	pulldownmenu_width /= number_menus ();
	Fi

	popupmenumargin = menumargin;

	If pulldownmenu_width < 10
	Then	menumargin = 0;
	Fi

	/* determine popup menu width: */
	If width_of_menu (Popupmenu, arrlen (Popupmenu), 0, 0, 0) < 15
	Then	popupmenumargin = 0;
	Fi

	/* determine position and length of displayed flags */
	flags_pos = XMAX - arrlen (Flagmenu);
	flags_gap = flags_pos - number_menus () * pulldownmenu_width;
	flags_displayed = arrlen (Flagmenu);
	If flags_gap < 1
	Then	flags_pos += 1 - flags_gap;
		flags_displayed -= 1 - flags_gap;
	Fi
}


/***********************************************************************\
	displaymenuheaders () displays the menu headers
	displaymenuheader () displays one menu header
	cleargap () clears the gap between menu headers and flags
	displayflags () displays the menu flags
	displaymenuline () displays the menu line
\***********************************************************************/

/*#define menuheadermargin menumargin*/
#define menuheadermargin 0

local void
	displaymenuheader (meni)
		int meni;
{
	set_cursor (meni * pulldownmenu_width, -1);
	putchar (' ');
	menudisp_on ();
	putnstr ("", menuheadermargin);
	putnstr (Pulldownmenu [meni].menuname, pulldownmenu_width - 1 - menuendgap - 2 * menuheadermargin);
	putnstr ("", menuheadermargin);
	menudisp_off ();
	/*putchar (' ');*/
	putnstr ("", menuendgap);
}

local void
	displaymenuheaders_from (firsti)
		int firsti;
{
	int i;
	int starti;

	If menuline_dirty
	Then	starti = 0;
		menuline_dirty = False;
	Else	starti = firsti;
	Fi

	calcmenuvalues ();

	If pulldownmenu_width < 3
	Then	return;
	Fi

	If standout_glitch
	Then	set_cursor (starti * pulldownmenu_width, -1);
		clear_eol ();
		disp_normal ();
	Fi

	For i = starti While i < number_menus () Step i ++
	Do
		displaymenuheader (i);
	Done
}

local void
	displaymenuheaders ()
{
	displaymenuheaders_from (0);
}


local void
	cleargap ()
{
	int i;
	int gappos = number_menus () * pulldownmenu_width;

	set_cursor (gappos, -1);
	For i = gappos While i < flags_pos Step i ++
	Do
		putchar (' ');
	Done
}


global void
	displayflags ()
{
	int i;

	If ! MENU
	Then	return;
	Fi

	calcmenuvalues ();

	If pulldownmenu_width < 3
	Then	return;
	Fi

	set_cursor (flags_pos, -1);
	For i = 0 While i < flags_displayed Step i ++
	Do
		char * flags = (* Flagmenu [i].dispflag) ();
		FLAG is_reverse;

		/* attribute indications used in menu (vs. status line)
				unicode indicator (cyan background)
				highlight (red background)
				reverse
				on yellow (unused)
				normal / no highlighting
		 */
		If * flags == ''
		Then	flags ++;
			is_reverse = True;
			combiningdisp_on ();
		Elsif * flags == ''
		Then	flags ++;
			is_reverse = True;
			emph_on ();
			menuheader_on ();
		Elsif * flags == ''
		Then	flags ++;
			is_reverse = True;
			menudisp_on ();
		Elsif * flags == ''
		Then	flags ++;
			is_reverse = True;
			menuitem_on ();
		Elsif * flags == ''
		Then	/* no highlighting explicitly requested (for gap) */
			flags ++;
			is_reverse = False;
		Else
			/*is_reverse = False;*/
			/* apply default highlighting */
			is_reverse = True;
			menuitem_on ();
		Fi

		If (* flags & 0x80) != 0
		Then
			unsigned long unichar;
			int utflen;

			utf8_info (flags, & utflen, & unichar);
			If (cjk_term || mapped_term)
			    && (is_reverse || unichar == 0xE7)
			    && no_char (mappedtermchar (unichar)
			   )
			Then	If unichar == 0xAB	/* « */
				Then	putchar ('<');
				Elsif unichar == 0xBB	/* » */
				Then	putchar ('>');
				Elsif unichar == 0xA6	/* ¦ */
				Then	putchar ('|');
				Elsif unichar == 0xE7	/* ç */
				Then	putchar (';');
				Else	put_unichar (unichar);
				Fi
			Else	put_unichar (unichar);
			Fi
		Elsif * flags != 0
		Then	putchar (* flags);
		Fi
		If is_reverse
		Then	disp_normal ();
		Fi
	Done
	flags_changed = False;
}


local void
	putfilelistborder (margin, rest, inifini)
		int margin;
		int rest;
		int inifini;
{
#ifdef help_attributes
	combiningdisp_on ();	/* on light cyan */
	dispHTML_comment ();	/* on yellow */
	menuitem_on ();		/* red on yellow */
	menuheader_on ();	/* reverse */
	menuborder_on ();	/* red */
#endif
	combiningdisp_on ();
	putnstr ("", margin / 2);

	disp_colour (5, UNSURE);	/* 5 magenta (3 yellow) ((0 bold black)) */
	If use_graphic_borders && horizontal_bar_width == 1
	Then
		menuborder_on ();	/* graphics, red */
#if defined (use_bullets)
# warning restrict to UTF-8
# warning check glyphs available
		If inifini == 0
		Then	put_unichar (0x25D7);	/* ◗ */
		Elsif inifini > 0
		Then	put_unichar (0x25CF);	/* ● */
		Else	put_unichar (0x25D6);	/* ◖ */
		Fi
#elif defined (use_cross_blockchars)
		If inifini == 0
		Then	putblock ('t');		/* ├ */
		Elsif inifini > 0
		Then	putblock ('n');		/* ┼ */
		Else	putblock ('u');		/* ┤ */
		Fi
		menuborder_off ();
#elif defined (use_diamond_vt)
# warning diamond block char not implemented
		putblock ('`');		/* ◆ */
#elif defined (use_diamond_uni)
# warning restrict to UTF-8
# warning check glyph available
		put_unichar (0x25C6);	/* ◆ */
#else
		putblock ('x');		/* │ */
		menuborder_off ();
#endif
		menuborder_off ();
	Else
		If inifini == 0
		Then	put_unichar ('[');
		Elsif inifini > 0
		Then	put_unichar ('|');
		Else	put_unichar (']');
		Fi
	Fi

	combiningdisp_on ();
	If margin > 2
	Then	putnstr ("", (margin - 1) / 2);
	Fi
	If rest > 0
	Then	putnstr ("", rest - margin);
	Fi

	disp_normal ();
}

static FLAG displayingfilelist = False;
static short colourtag = 0;
static short selectedtab = 0;

static
void
	disp_tab_colour ()
{
	If displayingfilelist
	Then
		If selectedtab
		Then
#if 0
			disp_scrollbar_background ();
			dispHTML_comment ();	/* on yellow */
#elif 0
			menuborder_on (); menuheader_on ();
			disp_selected (False, False);
			reverse_on ();	/* yellow on dark blue */
#elif 1
			menuheader_on ();
			disp_colour (5, UNSURE);
			reverse_on ();	/* black on magenta */
#else
			menuborder_on (); menuheader_on ();	/* on red */
#endif
		Elsif colourtag
		Then	disp_scrollbar_background ();
		Else	disp_scrollbar_foreground ();
		Fi
	Fi
}

local void
	displayfilelist (show)
		FLAG show;
{
	short fi;
	short line = 0;
	int col = 0;
	int margin = 1;
	int maxcols = XMAX + 1;
	int maxlines = (YMAX + MENU - 1) / 2;

	short oldMENU = MENU;

	calcmenuvalues ();

	/* indicate filelist displaying mode (for base colour) */
	displayingfilelist = show;

	MENU = 0;
	colourtag = 0;

	For fi = 0
	While fi < filelist_count ()
	Step fi ++
	Do
		char * fn = filelist_get (fi);
		int width = utf8_col_count (fn);

		If fi > 0 && col + width + 2 * margin > maxcols
		Then
			If show
			Then	putfilelistborder (margin, maxcols - col, -1);
			Fi

			line ++;
			col = 0;
			colourtag = line % 2;

			If line >= maxlines
			Then	break;
			Fi
		Fi
		If col + width + 2 * margin > maxcols
		Then	width = maxcols - col - 2 * margin;
		Fi

		filelist_set_coord (line, col + margin, col + margin + width);

		If show
		Then	set_cursor (col, line);
			putfilelistborder (margin, 0, col);
		Fi

		col += margin + width;

		selectedtab = streq (fn, file_name);
		If show
		Then	disp_tab_colour ();
			putnstr (fn, width);
#ifdef alternate_colour_each_file
			colourtag = 1 - colourtag;
#endif
			disp_normal ();
		Fi
	Done
	/* last line: final padding */
	If col > 0
	Then	If show
		Then	putfilelistborder (margin, maxcols - col, -1);
		Fi
		line ++;
	Fi

	/* adjust geometry values */
	MENU = 1 + line;
	If MENU != oldMENU
	Then	YMAX = YMAX + oldMENU - MENU;
		/* if header height changed:
		   notify need to update text on screen;
		   alternatively, could hook into update_file_name
		 */
		text_screen_dirty = True;
	Fi

	/* drop filelist displaying mode */
	displayingfilelist = False;
}

global void
	displaymenuline (show_filelist)
		FLAG show_filelist;
{
	If ! MENU
	Then	return;
	Fi

	If use_file_tabs
	Then	displayfilelist (show_filelist);
	Fi

	displaymenuheaders ();
	cleargap ();
	displayflags ();
	clear_eol ();
	top_line_dirty = False;
}


/***********************************************************************\
	prepare_menuline prepares menu line to prevent 
	right-to-left menu obstruction;
	the menu_line_pointer (pointing to the text line under the 
	menu item) is advanced for the next invocation
\***********************************************************************/

local void
	prepare_menuline (column, line, menu_line_poi)
		int column;
		int line;
		LINE * * menu_line_poi;
{
	char * cp;
	LINE * menu_line = * menu_line_poi;

	If menu_line == NIL_LINE || menu_line == bot_line
	Then	* menu_line_poi = NIL_LINE;
	Else	* menu_line_poi = menu_line->next;
	Fi

	If standout_glitch
	Then	set_cursor (column, line - 1);
		clear_eol ();
		disp_normal ();
	Fi

	If menu_line == tail || menu_line == NIL_LINE
	Then	return;
	Fi

	menu_line->dirty = True;

	If bidi_screen
	Then	For cp = menu_line->text
		While * cp != '\0'
		Step advance_char (& cp)
		Do	If is_right_to_left (unicodevalue (cp))
			Then	break;
			Fi
		Done
		If * cp != '\0'
		Then	set_cursor (0, line - 1);
			clear_eol ();
		Fi
	Fi
}




/***********************************************************************\
	display_menu () displays the given menu, returns menu width
\***********************************************************************/

local void
	display_menu (fullmenu, menulen, menuwidth, column, line, title, isflagmenu, disp_only, minwidth, scroll, maxscroll)
		menuitemtype * fullmenu; int menulen; int menuwidth;
		int column; int line;
		char * title;
		int isflagmenu;
		FLAG disp_only;
		int minwidth;
		int scroll, maxscroll;
{
	int i;
	menuitemtype * item;
	LINE * menu_line;
	menuitemtype * menu = fullmenu + scroll;

	menu_mouse_mode (True);

	/* save redraw values: */
	last_popup_column = column;
	last_popup_line = line;
	last_popup_item = -1;
	last_is_flag_menu = isflagmenu;
	last_minwidth = minwidth;
	last_scroll = scroll;
	last_maxscroll = maxscroll;

	/* save sub-menu reference */
	last_menucolumn = column;

	/* check if menu headers overwritten */
	If line == 0
	Then	menuline_dirty = True;
	Fi

	/* text lines may have to be cleared: */
	menu_line = proceed (top_line, line - 1);

	/* top menu border: */
	prepare_menuline (column, line, & menu_line);
	putborder_top (column, line, menuwidth, title, scroll > 0);
	If standout_glitch && full_menu_bg
	Then	menubg_off ();
		menuborder_on ();
		menuheader_on ();
	Fi

	/* menu items: */
	For i = 0 While i < menulen Step i ++
	Do
#ifdef use_slow_stat_hint_during_menu_display
#warning slow hint during display not necessary and implementation incomplete
	    check_slow_hint ();
#else
	    flush ();	/* in case is_directory()/stat() is slow... */
#endif

	    prepare_menuline (column, line + 1 + i, & menu_line);
	    item = & (menu [i]);
	    If item->itemfu == separator
	    Then
		If hop_flag > 0 && item->hopitemname != NIL_PTR && item->hopitemname [0] != '\0'
		Then	putborder_middle (column, line + 1 + i, menuwidth, item->hopitemname);
		Else	putborder_middle (column, line + 1 + i, menuwidth, item->itemname);
		Fi
	    Else
		set_cursor (column, line + i);
		If full_menu_bg
		Then	menubg_on ();
			If use_graphic_borders
			Then	putblock ('x');
				menuborder_off ();
				menubg_on ();
			Else	putchar (' ');
			Fi
		Elsif use_graphic_borders
		Then	putblock ('x');
			menuborder_off ();
		Else	putchar (' ');
			menudisp_off ();
		Fi
		putnstr ("", popupmenumargin);

		If isflagmenu
		Then	(void) is_directory (item);	/* resolve lazy */
			If * item->itemon &&
			   (* (flagfunc) item->itemon) (item, scroll + i) & 1 /* mask extra flags */
			Then	If is_directory (item)
				Then	put_submenu_marker (True);
				Else	put_menu_marker ();
				Fi
				If full_menu_bg
				Then	menubg_on ();
				Fi
			Else	putchar (' ');
			Fi
		Fi

		If hop_flag > 0 && item->hopitemname != NIL_PTR && item->hopitemname [0] != '\0'
		Then	putnstr_tag (item->hopitemname, item->extratag, 
				menuwidth - isflagmenu - 2 - 2 * popupmenumargin, 
				False);
		Else	putnstr_tag (item->itemname, item->extratag, 
				menuwidth - isflagmenu - 2 - 2 * popupmenumargin, 
				False);
		Fi

		putnstr ("", popupmenumargin);
		If full_menu_bg
		Then	If use_graphic_borders
			Then	putblock ('x');
			Else	putchar (' ');
			Fi
			menubg_off ();
		Elsif use_graphic_borders
		Then	menuborder_on ();
			putblock ('x');
		Else	menudisp_on ();
			putchar (' ');
		Fi
	    Fi
	Done

	/* bottom menu border: */
	prepare_menuline (column, line + 1 + menulen, & menu_line);
	putborder_bottom (column, line + 1 + menulen, menuwidth, scroll < maxscroll);

	If line > 0 && title != NIL_PTR
	Then	/* flag menu */
		set_cursor (column + menuwidth - 2 + horizontal_bar_width, line - 1);
	Else	set_cursor (column, line - 1);
	Fi
	If first_dirty_line < 0 || line - 1 < first_dirty_line
	Then	first_dirty_line = line - 1;
		If first_dirty_line < 0
		Then	first_dirty_line = 0;
		Fi
	Fi
	If line - 1 + menulen + 2 > last_dirty_line
	Then	last_dirty_line = line - 1 + menulen + 2;
	Fi

	last_menulen = menulen;
	last_menuwidth = menuwidth;
	last_disp_only = disp_only;
	last_menutitle = title;

	last_fullmenu = fullmenu;

	popupmenu_active = True;
}


/***********************************************************************\
	popupmenselected () highlights a selected popup menu item
\***********************************************************************/

local void
	popupmenselected (scroll, menu, menu_width, selected, column, line, i, index, isflagmenu)
		int scroll;
		menuitemtype * menu; int menu_width; FLAG selected;
		int column; int line; int i; int index;
		int isflagmenu;
{
	menuitemtype * item;
	FLAG show_info = False;
	char * show_char = NIL_PTR;	/* (avoid -Wmaybe-uninitialized) */
	FLAG coloured = index < 0;
	int unicode_menubar_mode = use_unicode_menubar ();

	/* save sub-menu reference */
	last_menuline = line + i;

	If i < 0
	Then	return;
	Fi

	/* save redraw values: */
	If selected
	Then	last_popup_item = i;
		last_popup_index = index;
	Else	last_popup_item = -1;
	Fi

	set_cursor (column, line + i);

	/* draw left border element */
	If full_menu_bg
	Then	menubg_on ();
	Fi
	If selected && unicode_menubar_mode > 0
	Then	If coloured
		Then	disp_selected (True, True);
		Else	reverse_on ();
		Fi
		If unicode_menubar_mode > 1
		Then	put_unichar (0x258E);	/* ▎ LEFT ONE QUARTER BLOCK */
		Else	put_unichar (0x258D);	/* ▍ LEFT THREE EIGHTHS BLOCK */
		Fi
	Elsif use_graphic_borders
	Then	putblock ('x');
	Else	menudisp_on ();
		putchar (' ');
		menudisp_off ();
	Fi

	/* switch to menu item content */
	If selected && coloured
	Then	disp_selected (True, False);
	Else	If standout_glitch
		Then	menuheader_off ();	/* disp_normal (); ? */
		Fi
		If full_menu_bg
		Then	menubg_on ();
		Fi
	Fi
	putnstr ("", popupmenumargin);

	item = & (menu [i]);

	If isflagmenu
	Then	(void) is_directory (item);	/* resolve lazy */
		If * item->itemon &&
		   (* (flagfunc) item->itemon) (item, scroll + i) & 1 /* mask extra flags */
		Then	If is_directory (item)
			Then	put_submenu_marker (True);
			Else	put_menu_marker ();
			Fi
			If selected && coloured
			Then	disp_selected (True, False);
			Elsif full_menu_bg
			Then	menubg_on ();
			Fi
		Else	putchar (' ');
		Fi
	Fi

	If selected && index >= 0
	Then	show_char = putnstr_mark (item->itemname, menu_width - isflagmenu - 2 - 2 * popupmenumargin, index, False, selected);
		show_info = True;
	Elsif hop_flag > 0 && item->hopitemname != NIL_PTR && item->hopitemname [0] != '\0'
	Then	putnstr_tag (item->hopitemname, item->extratag, 
			menu_width - isflagmenu - 2 - 2 * popupmenumargin, 
			selected);
	Else	putnstr_tag (item->itemname, item->extratag, 
			menu_width - isflagmenu - 2 - 2 * popupmenumargin, 
			selected);
	Fi

	putnstr ("", popupmenumargin);

	/* switch to border */
	If selected && coloured
	Then	disp_normal ();
	Fi
	If full_menu_bg
	Then	menubg_on ();
	Fi
	/* draw right border element */
	If selected && unicode_menubar_mode > 0
	Then	If coloured
		Then	disp_selected (False, True);
		Fi
		If unicode_menubar_mode > 1
		Then	put_unichar (0x258A);	/* ▊ LEFT THREE QUARTERS BLOCK */
		Else	put_unichar (0x258B);	/* ▋ LEFT FIVE EIGHTHS BLOCK */
		Fi
		If full_menu_bg
		Then	menubg_off ();
		Fi
	Elsif use_graphic_borders
	Then	putblock ('x');
	Else	menudisp_on ();
		putchar (' ');
		menudisp_off ();
	Fi

	If show_info
	Then	clear_status ();
		If show_char != NIL_PTR
		Then	display_Han (show_char, True);
		Fi
	Elsif selected
	Then	disp_normal ();
		If ! can_reverse_mode || ! can_hide_cursor
		Then	/* only needed if terminal cannot standout / reverse: */
			set_cursor (column, line + i);
		Fi
	Fi
}


/***********************************************************************\
	clean_menus () wipes menus which are still displayed
	clear_menus () clears menus which are still displayed
\***********************************************************************/

global void
	clean_menus ()
{
	If popupmenu_active
	Then
		LINE * start_line = proceed (top_line, first_dirty_line);
		int ypos = y;

		popupmenu_active = False;

		menu_mouse_mode (False);

		If ypos < first_dirty_line
		Then	/* Han info menu */
			ypos = first_dirty_line;
		Elsif ypos > first_dirty_line + 1 + last_menulen
		Then	/* limit ypos to lower menu border as until 2000.11 */
			ypos = first_dirty_line + 1 + last_menulen;
		Fi

		display (first_dirty_line, start_line, 
			 last_dirty_line - 1 - first_dirty_line, ypos);
		display_flush ();
		first_dirty_line = -1;
		last_dirty_line = 0;
		set_cursor_xy ();
	Fi
	If menuline_dirty
	Then	displaymenuline (True);
	Fi
}

local void
	clear_menus ()
{
	clean_menus ();
}


/***********************************************************************\
	redrawmenu () redraws an open menu after screen refresh
\***********************************************************************/

global void
	redrawmenu ()
{
	If popupmenu_active
	Then
		int item = last_popup_item;
		display_menu (last_fullmenu, last_menulen, last_menuwidth, last_popup_column, last_popup_line, last_menutitle, last_is_flag_menu, last_disp_only, last_minwidth, last_scroll, last_maxscroll);
		If item >= 0
		Then	popupmenselected (last_scroll,
					last_fullmenu + last_scroll, last_menuwidth, True, 
					last_popup_column, last_popup_line, 
					item, last_popup_index, last_is_flag_menu);
		Fi
		If last_disp_only
		Then	menu_mouse_mode (False);
			set_cursor_xy ();
		Fi
	Fi
}


/***********************************************************************\
	is_menu_open () checks if a menu is open
	(used with xterm mouse highlight tracking mode before 2000.16)
\***********************************************************************/

global int
	is_menu_open ()
{
	return popupmenu_active;
}


/***********************************************************************\
	select_item () selects a menu item based on key letter
	select_file () selects a file name based on prefix
\***********************************************************************/

local int
	select_item (menu, menulen, curitemno, key)
		menuitemtype * menu;
		int menulen;
		int curitemno;
		unsigned long key;
{
	int i = curitemno + 1;

	key = case_convert (key, 1);	/* normalise to upper case */

	Dowhile True
	Do
		If i > menulen
		Then	i = 1;
			If curitemno == 0
			Then	return 1;
			Fi
		Fi
		If i == curitemno
		Then	return curitemno;
		Fi
		If menu [i - 1].itemfu != separator
		Then
			char * label;
			unsigned long letter;

			If hop_flag > 0
			Then	label = menu [i - 1].hopitemname;
				If * label == '\0'
				Then	label = menu [i - 1].itemname;
				Fi
			Else	label = menu [i - 1].itemname;
			Fi

			/* look for first letter in menu label */
			letter = utf8value (label);
			Dowhile letter != 0 && ! isLetter (letter)
			Do	advance_utf8 (& label);
				letter = utf8value (label);
			Done

			/* look for further words;
			   check each word whether starting with key */
			Dowhile letter != 0
			Do
				If case_convert (letter, 1) == key
				Then	return i;
				Fi

				/* skip current word */
				Dowhile letter != 0 && isLetter (letter)
				Do	advance_utf8 (& label);
					letter = utf8value (label);
				Done
				/* look for next word */
				Dowhile letter != 0 && ! isLetter (letter)
				Do	advance_utf8 (& label);
					letter = utf8value (label);
				Done
			Done
		Fi
		i ++;
	Done
	/*return curitemno;*/
}

local int
	select_file (menu, menulen, curitemno, prefix)
		menuitemtype * menu;
		int menulen;
		int curitemno;
		char * prefix;
{
	int i = curitemno;
	If i == 0
	Then	i = 1;
	Fi

	Dowhile True
	Do
		If menu [i - 1].itemfu != separator
		Then
			char * label = menu [i - 1].itemname;

			If stringprefix_ (prefix, label)
			Then	return i;
			Fi
		Fi
		i ++;
		If i > menulen
		Then	i = 1;
			If curitemno == 0
			Then	return 0;
			Fi
		Fi
		If i == curitemno
		Then	return 0;
		Fi
	Done
}


/***********************************************************************\
	item_count () counts menu items in a menu line string
\***********************************************************************/

local int
	item_count (line)
		character * line;
{
	character prev = (character) ' ';
	int count = 0;
	Dowhile * line != '\0'
	Do	If (character) * line > (character) ' ' && prev == ' '
		Then	count ++;
		Fi
		prev = * line;
		If prev == (character) '\t'
		Then	prev = (character) ' ';
		Fi
		line ++;
	Done
	return count;
}


/***********************************************************************\
	action_menu () handles given action menu
	popup_menu () handles given action or selection menu
	menu_scroll () moves menu item and, if necessary,
			scrolls a partially displayed menu
\***********************************************************************/

#define dont_debug_menu_scroll

#ifdef debug_menu_scroll
#define trace_menu_scroll(params)	printf params
#else
#define trace_menu_scroll(params)
#endif


/**
   Calculate new menu position and set values.
   Return True if menu has to be scrolled.
 */
local FLAG
	menu_scroll (menu, menulen, delta, fullmenu, scrolloffset, maxscrolloffset, itemno)
		menuitemtype * * menu;
		int menulen;
		int delta;
		menuitemtype * fullmenu;
		int * scrolloffset;
		int maxscrolloffset;
		int * itemno;
{
	int prev_scrolloffset = * scrolloffset;

	If delta == 1
	Then	* itemno += delta;
		If * itemno > menulen
		Then	If * scrolloffset == maxscrolloffset
			Then	/* cycle at end */
				* scrolloffset = 0;
				* itemno = 1;
			Else	/* scroll 1 item */
				* scrolloffset += delta;
				* itemno -= delta;
			Fi
		Fi
		Dowhile fullmenu [* scrolloffset + * itemno - 1].itemfu == separator
		Do	* itemno += delta;
			trace_menu_scroll (("   itemno sep ++ %d + %d\n", * itemno, * scrolloffset));
			If * itemno > menulen
			Then	If * scrolloffset == maxscrolloffset
				Then	/* cycle at end */
					* scrolloffset = 0;
					* itemno = 1;
					trace_menu_scroll (("   itemno sep ++ --> %d + %d\n", * itemno, * scrolloffset));
				Else	/* scroll 1 item */
					* scrolloffset += delta;
					* itemno -= delta;
					trace_menu_scroll (("   itemno sep ++ -> %d + %d\n", * itemno, * scrolloffset));
				Fi
			Fi
		Done
	Elsif delta == -1
	Then	* itemno += delta;
		If * itemno < 1
		Then	If * scrolloffset == 0
			Then	/* cycle at end */
				* scrolloffset = maxscrolloffset;
				* itemno = menulen;
			Else	/* scroll 1 item */
				* scrolloffset += delta;
				* itemno -= delta;
			Fi
		Fi
		Dowhile fullmenu [* scrolloffset + * itemno - 1].itemfu == separator
		Do	* itemno += delta;
			If * itemno < 1
			Then	If * scrolloffset == 0
				Then	/* cycle at end */
					* scrolloffset = maxscrolloffset;
					* itemno = menulen;
				Else	/* scroll 1 item */
					* scrolloffset += delta;
					* itemno -= delta;
				Fi
			Fi
		Done
	Elsif delta > 0
	Then	* itemno += delta;
		trace_menu_scroll (("   itemno %d + %d\n", * itemno, * scrolloffset));
		Dowhile * itemno > menulen
		Do	* scrolloffset += menulen;
			* itemno -= menulen;
			trace_menu_scroll (("-> itemno %d + %d\n", * itemno, * scrolloffset));
		Done

		If * scrolloffset > maxscrolloffset
		Then	* itemno += * scrolloffset - maxscrolloffset;
			* scrolloffset = maxscrolloffset;
			trace_menu_scroll (("=> itemno %d + %d\n", * itemno, * scrolloffset));
			If * scrolloffset == prev_scrolloffset
			Then	/* paging at end: move to menu end item */
				* itemno = menulen;
				trace_menu_scroll (("=> itemno %d + %d\n", * itemno, * scrolloffset));
			Fi
			If * itemno > menulen
			Then	* itemno = menulen;
				trace_menu_scroll (("=> itemno %d + %d\n", * itemno, * scrolloffset));
			Fi
		Fi

		If fullmenu [* scrolloffset + * itemno - 1].itemfu == separator
		Then	If * itemno < menulen
			Then	* itemno += 1;
			Else	* itemno -= 1;
			Fi
			trace_menu_scroll (("!> itemno %d + %d\n", * itemno, * scrolloffset));
		Fi
	Else	* itemno += delta;
		Dowhile * itemno < 1
		Do	* scrolloffset -= menulen;
			* itemno += menulen;
		Done

		If * scrolloffset < 0
		Then	* itemno += * scrolloffset;
			* scrolloffset = 0;
			If * scrolloffset == prev_scrolloffset
			Then	/* paging at end: move to menu end item */
				* itemno = 1;
			Fi
			If * itemno < 1
			Then	* itemno = 1;
			Fi
		Fi

		If fullmenu [* scrolloffset + * itemno - 1].itemfu == separator
		Then	If * itemno > 1
			Then	* itemno -= 1;
			Else	* itemno += 1;
			Fi
		Fi
	Fi

	If * scrolloffset == prev_scrolloffset
	Then	return False;
	Else	* menu = & fullmenu [* scrolloffset];
		return True;
	Fi
}


/* popup_menu token to ignore 1 (initial) mouse release event */
static FLAG ignore_1releasebutton = False;

static char input_result [maxFILENAMElen];

#ifdef vms
#define WILDCARDS "?%*"
#else
#define WILDCARDS "?*["
#endif

global int
	popup_menu (menu, menulen, column, line, title, is_flag_menu, disp_only, select_keys)
		menuitemtype * menu; int menulen;
		int column; int line;
		char * title;
		FLAG is_flag_menu;
		FLAG disp_only;	/* Han info menu */
		char * select_keys;	/* pick list */
{
	FLAG is_file_chooser = select_keys && * select_keys == '*';
	FLAG is_file_selector = column <= 1;
	menuitemtype * fullmenu = menu;
	int fullmenulen = menulen;
	int scrolloffset = 0;
	int maxscrolloffset = 0;

	unsigned long c;
	int startline;
	int minline;
	int startcolumn;
	int itemno;
	int last_menuitemno = 0;
	int mousemove_itemno = -1;
	int mousemove_firstx = -1;	/* avoid -Wmaybe-uninitialized */
	int ret = -1;
	int select_index = -2;
	int sticky_index = -2;
	int minwidth = (disp_only | (select_keys != NIL_PTR)) ? utf8_col_count (title) : 0;
	int menu_width;
	FLAG ignore_releasebutton;
	FLAG continue_menu = True;
	char input_field [maxFILENAMElen];
	char * last_status_msg = NIL_PTR;

	int menu_YMAX = YMAX;
	int menu_XMAX = XMAX;

	is_char_menu = disp_only || (select_keys && * select_keys == '1');

	keyshift = 0;

	input_field [0] = 0;

	If line == -1	/* suppressed display of menu header line */
	Then	line = 0;
	Fi

	/* set token to ignore 1 mouse release event */
	ignore_releasebutton = ignore_1releasebutton;
	/* set token to ignore initial mouse release event */
	ignore_releasebutton = True;
	/* clear token to ignore 1 mouse release event */
	ignore_1releasebutton = False;


	/* calculate general and specific menu geometry */
	calcmenuvalues ();
	If is_file_chooser && ! is_file_selector
	Then	/* provide enough space for file name input field */
		menu_width = XMAX - 2 * column;
	Else	menu_width = width_of_menu (fullmenu, fullmenulen, 0, is_flag_menu, minwidth);
	Fi

	/* adjust menu position and check with screen limits */
	If column >= 0
	Then	startcolumn = column;
	Else	startcolumn = 4 - column - menu_width - 2 * horizontal_bar_width;
	Fi
	If startcolumn > XMAX - menu_width - 2 * horizontal_bar_width + 2
	Then	startcolumn = XMAX - menu_width - 2 * horizontal_bar_width + 2;
	Fi
	If startcolumn < 0 || menu_width < 3
	Then	ring_bell ();
		return ret;
	Fi

	startline = line + 1;
	If startline > YMAX - menulen - 1
	Then	startline = YMAX - menulen - 1;
	Fi
	/* setup partial menu values for menu scrolling */
	If is_file_chooser
	Then	minline = line + 1;
	Elsif is_flag_menu
	Then	minline = 1;
	Else	If MENU
		Then	minline = 0;
		Else	minline = 1;
		Fi
	Fi
	If startline < minline
	Then	maxscrolloffset = minline - startline;
		menulen = menulen - maxscrolloffset;
		startline = minline;
	Fi


	/* initial selection */
	If is_file_chooser
	Then	int i;
		If is_file_selector
		Then	itemno = 0;
			For i = 0 While i < fullmenulen Step i ++
			Do	If streq (menu [i].itemname, file_name)
				Then	/*itemno = i + 1;*/
					(void) menu_scroll (& menu, menulen, scrolloffset + i + 1, fullmenu, & scrolloffset, maxscrolloffset, & itemno);
					break;
				Fi
			Done
		Else	itemno = 1;	/* default position ".." (VMS: []) */
		Fi
	Elsif select_keys == NIL_PTR
	Then	itemno = 0;
	Else	itemno = 1;
		select_index = 0;
		sticky_index = 0;
	Fi

	If menu_reposition
	Then	(void) menu_scroll (& menu, menulen, previous_menuline + previous_scroll + 1 - startline, fullmenu, & scrolloffset, maxscrolloffset, & itemno);
		/* reset repositioning token */
		menu_reposition = False;
	Fi

	/* remember previous menu in case this sub-menu returns to it */
	previous_fullmenu = last_fullmenu;
	previous_menuline = last_menuline;
	previous_scroll = last_scroll;

_stayinmenu:
	continue_menu = True;

	/* initial display */
	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
	/* this also sets menu_mouse_mode (True); */
	If itemno > 0
	Then	popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);
	Fi

	/* for display-only menu, skip interactive part */
	If disp_only
	Then	menu_mouse_mode (False);
		return ret;
	Fi

	mouse_prevxpos = 0;
	mouse_prevypos = 0;

	/* interactive menu handling */
	Dowhile continue_menu
	Do
	    /* let file chooser give context-dependant usage hints */
	    If is_file_chooser && ! is_file_selector
	    Then
		char * this_status_msg = NIL_PTR;
		static char * tab_complete = "Use TAB to complete filename";
		static char * tab_wildcard = "Use TAB to apply wildcard filter";
		static char * tab_directory = "Use TAB to change directory";
		If strpbrk (input_field, WILDCARDS)
		Then	this_status_msg = tab_wildcard;
		Elsif	itemno > 0 &&
			((* input_field && ! streq (input_field, menu [itemno - 1].itemname))
			    || ! is_directory (& (menu [itemno - 1])))
		Then	this_status_msg = tab_complete;
		Elsif	itemno > 0
		Then	this_status_msg = tab_directory;
		Fi
		If ! stat_visible
		Then	last_status_msg = NIL_PTR;
		Fi
		If this_status_msg != last_status_msg
		Then	status_uni (this_status_msg);
			last_status_msg = this_status_msg;
		Fi
	    Fi

	    flush ();
	    c = readcharacter_allbuttons (is_file_chooser);

	    If is_file_chooser &&
		(command (c) == CTRLINS
		|| command (c) == COMPOSE
		|| command (c) == F5
		|| command (c) == F6
		|| command (c) == key_1
		|| command (c) == key_2
		|| command (c) == key_3
		|| command (c) == key_4
		|| command (c) == key_5
		|| command (c) == key_6
		|| command (c) == key_7
		|| command (c) == key_8
		|| command (c) == key_9
		|| command (c) == key_0
		)
	    Then
		/* handle ^V input here to make WINCH handling consistent */
		If command (c) == CTRLINS
		Then	c = CTRLGET (False);
		Else	c = CTRLGET (True);
		Fi
	    Fi

#ifdef debug_mouse
	    If command (c) == MOUSEfunction
	    Then
		trace_mouse ("popup_menu");
	    Fi
#endif

	    /**
	       detect whether screen size has changed;
	       check explicitly because WINCH signal may have been caught already 
	       (in submenu/pick list, or prompt)
	     */
	    If YMAX != menu_YMAX || XMAX != menu_XMAX
	    Then
		/* workaround: simply override read command; menu is closed anyway */
		c = FUNcmd;
		keyproc = RDwin;
	    Fi

	    /**
	       fix display if menu has been overlayed by another menu
	       (esp. a pick list (input method character selection))
	     */
	    If fullmenu != last_fullmenu && command (c) != RDwin
	    Then
		/* refresh menu (also restoring last_* and popupmenu_active) */
		display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
		If itemno > 0
		Then	popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);
		Fi
		/* refresh input field */
		update_title (startcolumn, startline, menu_width, 
				title, input_field, scrolloffset > 0);
	    Fi

	    /**
	       window resize (or explicit refresh)
	     */
	    If command (c) == RDwin
	    Then

#define winchg_quits_menu

#ifdef winchg_quits_menu
		popupmenu_active = False;
		RDwin ();
		clear_menus ();
		menu_mouse_mode (False);
		return ret;
#else
#warning [41m Menu refresh after WINCH not fully implemented [0m
#define winchg_full_reset
#ifdef winchg_full_reset
		int fullitemno = itemno + scrolloffset;
#endif
		RDwin_nomenu ();
		menu_YMAX = YMAX;
		menu_XMAX = XMAX;

		/* recalculate general and specific menu geometry */
		calcmenuvalues ();
		If is_file_chooser
		Then	menu_width = XMAX - 2 * column;
		Else	menu_width = width_of_menu (fullmenu, fullmenulen, 0, is_flag_menu, minwidth);
		Fi

		/* adjust menu position and check with screen limits */
		If column >= 0
		Then	startcolumn = column;
		Else	startcolumn = 4 - column - menu_width - 2 * horizontal_bar_width;
		Fi
		If startcolumn > XMAX - menu_width - 2 * horizontal_bar_width + 2
		Then	startcolumn = XMAX - menu_width - 2 * horizontal_bar_width + 2;
		Fi
		If startcolumn < 0 || menu_width < 3
		Then	ring_bell ();
			clear_menus ();
			return ret;
		Fi

		/* reset menu geometry to full menu for recalculation */
		menulen = fullmenulen;
		maxscrolloffset = 0;

		/* recalculate partial display */
		startline = line + 1;
		If startline > YMAX - menulen - 1
		Then	startline = YMAX - menulen - 1;
		Fi
		/* setup partial menu values for menu scrolling */
		If is_file_chooser
		Then	minline = line + 1;
		Elsif is_flag_menu
		Then	minline = 1;
		Else	If MENU
			Then	minline = 0;
			Else	minline = 1;
			Fi
		Fi
		If startline < minline
		Then	maxscrolloffset = minline - startline;
			menulen = menulen - maxscrolloffset;
			startline = minline;
		Fi
# ifdef debug_resize_open_menus
		printf ("%dx%d start %d len %d max %d\n", YMAX, XMAX, startline, menulen, maxscrolloffset);
# endif

		/* reset and adjust menu scroll */
# ifdef winchg_full_reset
		menu = fullmenu;
		scrolloffset = 0;
		itemno = 0;
		If fullitemno > 0
		Then	(void) menu_scroll (& menu, menulen, fullitemno, fullmenu, & scrolloffset, maxscrolloffset, & itemno);
		Fi
# else
		(void) menu_scroll (& menu, menulen, 0, fullmenu, & scrolloffset, maxscrolloffset, & itemno);
# endif

		/* redisplay */
		display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
		If itemno > 0
		Then	popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);
		Fi
		/* refresh input field */
		update_title (startcolumn, startline, menu_width, 
				title, input_field, scrolloffset > 0);
#endif

	    /**
	       HOP/middle-mouse/space (unless IM menu): toggle HOP
	       file chooser: HOP with edit field active
	       			-> exit; accept wildcard pattern
	     */
	    Elsif (c == ' ' && select_keys == NIL_PTR)
		  || command (c) == HOP
		  || command (c) == GOHOP
		  || (command (c) == MOUSEfunction && mouse_button == middlebutton)
	    Then
		If is_file_chooser
		Then	If input_field [0] != '\0'
			Then	continue_menu = False;
			Fi
		Else	toggle_HOP ();
			clear_menus ();
			/* recalculate x pos to prevent menu display wrap-around */
			menu_width = width_of_menu (fullmenu, fullmenulen, 0, is_flag_menu, 0);
			If column >= 0
			Then	startcolumn = column;
			Else	startcolumn = 4 - column - menu_width - 2 * horizontal_bar_width;
			Fi
			If startcolumn > XMAX - menu_width - 2 * horizontal_bar_width + 2
			Then	startcolumn = XMAX - menu_width - 2 * horizontal_bar_width + 2;
			Fi
			If startline < minline || startcolumn < 0 || menu_width < 3
			Then	ring_bell ();
				return ret;
			Fi
			/* display the menu */
			display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
			popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);
		Fi

	    /**
	       mouse middle release: ignore
	     */
	    Elsif command (c) == MOUSEfunction
		&& mouse_button == releasebutton && mouse_prevbutton == middlebutton
	    Then	/* ignore */

	    /**
	       IM menu: nav. char left
	     */
	    Elsif select_keys != NIL_PTR && ! is_file_chooser && (
		command (c) == LFkey
		|| c == '<'
		)
	    Then
		If select_index > 0
		Then	select_index --;
		Else	int prev_itemno = itemno;
			If menu_scroll (& menu, menulen, - 1, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
			Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
			Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
			Fi

			select_index = item_count (menu [itemno - 1].itemname) - 1;
		Fi
		sticky_index = select_index;
		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       IM menu: nav. char right
	     */
	    Elsif select_keys != NIL_PTR && ! is_file_chooser && (
		command (c) == RTkey
		|| c == '>'
		|| (c == ' ' && selection_space == SPACE_NEXT)
		)
	    Then
		If select_index < item_count (menu [itemno - 1].itemname) - 1
		Then	select_index ++;
		Else	int prev_itemno = itemno;
			If menu_scroll (& menu, menulen, 1, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
			Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
			Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
			Fi

			select_index = 0;
		Fi
		sticky_index = select_index;
		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       IM menu: nav. char first (line)
	     */
	    Elsif select_keys != NIL_PTR && ! is_file_chooser &&
		  (command (c) == HOMEkey || command (c) == smallHOMEkey)
	    Then
		select_index = 0;
		sticky_index = 0;
		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       IM menu: nav. char last (line)
	     */
	    Elsif select_keys != NIL_PTR && ! is_file_chooser &&
		  (command (c) == ENDkey || command (c) == smallENDkey)
	    Then
		select_index = item_count (menu [itemno - 1].itemname) - 1;
		sticky_index = strlen (select_keys) - 1;
		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       navigate top
	     */
	    Elsif command (c) == HOMEkey || command (c) == smallHOMEkey
	    Then
		int prev_itemno = itemno;
		If menu_scroll (& menu, menulen, - scrolloffset - menulen, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
		Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
		Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
		Fi
		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       navigate bottom
	     */
	    Elsif command (c) == ENDkey || command (c) == smallENDkey
	    Then
		int prev_itemno = itemno;
		If menu_scroll (& menu, menulen, maxscrolloffset + menulen, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
		Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
		Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
		Fi
		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       navigate down
	     */
	    Elsif command (c) == PGDNkey
	    Then
		int prev_itemno = itemno;
		If menu_scroll (& menu, menulen, menulen, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
		Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
		Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
		Fi

		If select_keys != NIL_PTR && ! is_file_chooser
		Then	select_index = item_count (menu [itemno - 1].itemname) - 1;
			If sticky_index < select_index
			Then	select_index = sticky_index;
			Fi
		Fi

		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       navigate up
	     */
	    Elsif command (c) == PGUPkey
	    Then
		int prev_itemno = itemno;
		If menu_scroll (& menu, menulen, - menulen, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
		Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
		Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
		Fi

		If select_keys != NIL_PTR && ! is_file_chooser
		Then	select_index = item_count (menu [itemno - 1].itemname) - 1;
			If sticky_index < select_index
			Then	select_index = sticky_index;
			Fi
		Fi

		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       control/shift-mouse scroll: change menu
	     */
	    Elsif command (c) == MOUSEfunction && (mouse_shift & control_button) &&
		  (mouse_button == wheeldown || mouse_button == wheelup)
	    Then	continue_menu = False;

	    /**
	       cursor down/mouse scroll/space (IM menu opt.)
	     */
	    Elsif command (c) == DNkey
		   || (c == ' ' && selection_space == SPACE_NEXTROW)
		   || (command (c) == MOUSEfunction && mouse_button == wheeldown)
	    Then
			int prev_itemno = itemno;
			If menu_scroll (& menu, menulen, 1, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
			Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
			Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
			Fi

			If select_keys != NIL_PTR && ! is_file_chooser
			Then	select_index = item_count (menu [itemno - 1].itemname) - 1;
				If sticky_index < select_index
				Then	select_index = sticky_index;
				Fi
			Fi
			popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       cursor up/mouse scroll
	     */
	    Elsif command (c) == UPkey
		   || (command (c) == MOUSEfunction && mouse_button == wheelup)
	    Then
			int prev_itemno = itemno;
			If menu_scroll (& menu, menulen, - 1, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
			Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
			Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
			Fi

			If select_keys != NIL_PTR && ! is_file_chooser
			Then	select_index = item_count (menu [itemno - 1].itemname) - 1;
				If sticky_index < select_index
				Then	select_index = sticky_index;
				Fi
			Fi
			popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       catch mouse release
	     */
	    Elsif command (c) == MOUSEfunction
			&& mouse_button == releasebutton && ignore_releasebutton
	    Then	ignore_releasebutton = False;

	    /**
	       horizontal mouse move: submenu navigation
	     */
	    Elsif command (c) == MOUSEfunction
			&& mouse_button == movebutton
			&& mouse_ypos == mouse_prevypos
	    Then	/* handle mouse movement to open/close sub-menu */
			If mouse_xpos > mouse_prevxpos
			&& mouse_xpos >= mousemove_firstx + 4
			&& (mouse_xpos >= startcolumn + menu_width + 2
			    || (itemno > 0 && menu [itemno - 1].extratag
				&& strcontains (menu [itemno - 1].extratag, "▶")
			       )
			   )
			Then	c = FUNcmd;
				keyproc = RTkey;
				continue_menu = False;
			Elsif mouse_xpos < mouse_prevxpos
			&& mouse_xpos <= mousemove_firstx - 4
			&& mouse_xpos < startcolumn - 2
			Then	c = FUNcmd;
				keyproc = LFkey;
				continue_menu = False;
			Fi
			If continue_menu && itemno != mousemove_itemno
			Then	mousemove_itemno = itemno;
				mousemove_firstx = mouse_xpos;
			Fi

	    /**
	       mouse navigation (also with button dragged)
	     */
	    Elsif command (c) == MOUSEfunction
			&& (mouse_button == leftbutton
			    || mouse_button == movebutton
			    || mouse_button == rightbutton
			   )
	    Then
		popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, itemno - 1, select_index, is_flag_menu);
		itemno = mouse_ypos - startline + 1;
		If itemno <= 0 || itemno > menulen
		|| menu [itemno - 1].itemfu == separator
		Then	itemno = 0;
		Else	popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);
		Fi
		If mouse_button == leftbutton
		Then	continue_menu = False;
		Fi
		If mouse_button == movebutton
		&& itemno != mousemove_itemno
		Then	mousemove_itemno = itemno;
			mousemove_firstx = mouse_xpos;
		Fi

	    /**
	       release right button: select item
	     */
	    Elsif command (c) == MOUSEfunction
			&& mouse_button == releasebutton && mouse_prevbutton == rightbutton
	    Then	continue_menu = False;

	    /**
	       window focus: ignore
	     */
	    Elsif command (c) == FOCUSin
	    Then	/* ignore */

	    /**
	       toggle IM (keyboard mapping)
	     */
	    Elsif allow_keymap && command (c) == toggleKEYMAP
	    Then
		toggleKEYMAP ();
		If flags_changed
		Then	displayflags ();
		Fi

	    /**
	       file chooser: delete char left
	     */
	    Elsif is_file_chooser && command (c) == DPC
	    Then
		If input_field [0] == '\0'
		Then	ring_bell ();
		Else	/* point to last UTF-8 character */
			char * pp = input_field + strlen (input_field);
			pp --;
			Dowhile pp != input_field && (* pp & 0xC0) == 0x80
			Do	pp --;
			Done
			/* remove it */
			* pp = '\0';
			update_title (startcolumn, startline, menu_width, 
					title, input_field, scrolloffset > 0);
		Fi

	    /**
	       letter navigation
	       file chooser: also enter into input field, accept special input
	     */
	    Elsif ((select_keys == NIL_PTR || is_file_chooser)
			&& c != FUNcmd && c > (character) ' ')
		  || (is_file_chooser && c == ' ')
		  || (is_file_chooser && command (c) == CTRLINS)
		  || (is_file_chooser && command (c) != SNL && c == FUNcmd)
	    Then
		int prev_itemno = itemno;
		int new_itemno;

		If is_file_chooser
		Then	char utfbuf [7];
			int charlen;

			/* obsolete; handled above */
			If command (c) == CTRLINS
			Then	c = CTRLGET (False);
			Elsif c == FUNcmd
			Then	c = CTRLGET (True);
			Fi

			If no_char (c)
			Then	ring_bell ();
			Else
				charlen = utfencode (c, utfbuf);
				If strlen (input_field) + charlen < sizeof (input_field) - 1
				Then	strcat (input_field, utfbuf);
				Else	ring_bell ();
				Fi
			Fi
		Fi

		If is_file_chooser
		Then	new_itemno = select_file (fullmenu, fullmenulen, itemno + scrolloffset, input_field);
		Else	new_itemno = select_item (fullmenu, fullmenulen, itemno + scrolloffset, c);
		Fi

		If new_itemno > 0
		Then	new_itemno -= scrolloffset;
			If menu_scroll (& menu, menulen, new_itemno - prev_itemno, fullmenu, & scrolloffset, maxscrolloffset, & itemno)
			Then	display_menu (fullmenu, menulen, menu_width, startcolumn, startline, title, is_flag_menu, disp_only, minwidth, scrolloffset, maxscrolloffset);
			Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
			Fi
/*		if unselecting, subsequent Enter with input field won't work
		Else	popupmenselected (scrolloffset, menu, menu_width, False, startcolumn, startline, prev_itemno - 1, select_index, is_flag_menu);
			itemno = 0;
*/
		Fi

		If is_file_chooser
		Then	last_menuitemno = itemno + scrolloffset;
			update_title (startcolumn, startline, menu_width, 
				title, input_field, scrolloffset > 0);
		Fi

		popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, itemno - 1, select_index, is_flag_menu);

	    /**
	       file chooser: TAB copies item (~ filename completion),
	       except:
	       on dir with empty input field: navigates
	       on dir with identical input field: navigates
	       with wildcard: activates wildcard filter
	     */
	    Elsif itemno > 0 && is_file_chooser
		&& (c == '\t' || command (c) == STAB)
		&& ((* input_field && ! streq (input_field, menu [itemno - 1].itemname))
		    || ! is_directory (& (menu [itemno - 1]))
		   )
	    Then
		If is_file_selector
		Then	/* ignore */
		Elsif strpbrk (input_field, WILDCARDS)
		Then	c = FUNcmd;
			keyproc = HOP;
			continue_menu = False;
		Else	input_field [0] = '\0';
			strncat (input_field, menu [itemno - 1].itemname, sizeof (input_field) - 1);
			update_title (startcolumn, startline, menu_width, 
					title, input_field, scrolloffset > 0);
		Fi

	    /**
	       other input: leave menu
	     */
	    Else	continue_menu = False;
	    Fi


	    /* file chooser: clear input field after navigation */
	    If is_file_chooser && itemno > 0 && last_menuitemno != itemno + scrolloffset
	    Then
		If * input_field
		Then	* input_field = '\0';
			update_title (startcolumn, startline, menu_width, 
					title, input_field, scrolloffset > 0);
		Fi
	    Fi
	    last_menuitemno = itemno + scrolloffset;

	    If command (c) == MOUSEfunction
	    Then	ignore_releasebutton = False;
			mouse_prevxpos = mouse_xpos;
			mouse_prevypos = mouse_ypos;
	    Fi
	Done

	If command (c) == MOUSEfunction
		&& mouse_button != wheelup
		&& mouse_button != wheeldown
	Then	/* mouse click */
		int menu_xpos = mouse_xpos - startcolumn;
		int menu_ypos = mouse_ypos - startline;

		If menu_ypos + startline == -1
		Then	int meni = mouse_xpos - flags_pos;
			clear_menus ();
			displaymenuheaders ();
			If meni >= 0 && meni < flags_displayed
			/* check double click: 
			   clicked on flag of open menu (rather than any)?
			*/
			   && (Flagmenu [meni].menu == last_fullmenu)
			Then	(* Flagmenu [meni].toggle) (menu_xpos + startcolumn);
			Else	openmenuat (menu_xpos + startcolumn);
			Fi
		Elsif menu_xpos >= 0 && menu_xpos < menu_width
		   && menu_ypos >= 0 && menu_ypos < menulen
		Then
		    If is_file_chooser
		    Then
			ret = scrolloffset + menu_ypos;
		    Elsif select_keys != NIL_PTR
		    Then
			/* determine mouse position in menu line */
			/* used for keyboard mapping selection */
			char * select = menu [menu_ypos].itemname;
			char * cpoi = select;
			int scol = 1;
			ret = 0;
			Dowhile scol < menu_xpos && * cpoi != '\0'
			Do	If * cpoi == ' '
				Then	ret ++;
				Fi
				advance_utf8_scr (& cpoi, & scol, select);
			Done
			If ret < strlen (select_keys)
			Then	ret = (scrolloffset + menu_ypos)
					* strlen (select_keys)
					+ ret;
			Else	ret = -1;
			Fi
		    Elsif menu [menu_ypos].itemfu != separator
		    Then
			popupmenselected (scrolloffset, menu, menu_width, True, startcolumn, startline, menu_ypos, select_index, is_flag_menu);
			menu_mouse_mode (False);

			/* activate menu item */
			(* (menufunc) menu [menu_ypos].itemfu) (& menu [menu_ypos], scrolloffset + menu_ypos);
			displayflags ();

			/* handle STAYINMENU feature */
			If * menu [menu_ypos].itemon
			Then	int on = (* (flagfunc) menu [menu_ypos].itemon) (& menu [menu_ypos], scrolloffset + menu_ypos);
				If on & STAYINMENU
				Then	ignore_releasebutton = True;
					goto _stayinmenu;
				Fi
			Fi
		    Fi
		Fi
	Elsif itemno > 0 && is_file_chooser &&
		(c == '\t' || command (c) == STAB)
	Then	menu_mouse_mode (False);

		menu [itemno - 1].hopitemname = "cd";	/* change directory */
		ret = scrolloffset + itemno - 1;
		If * input_field && ! streq (input_field, menu [itemno - 1].itemname)
		Then	strcpy (input_result, input_field);
			menu [itemno - 1].itemname = input_result;
			menu [itemno - 1].hopitemname = "";
		Fi
	Elsif itemno > 0 && is_file_chooser &&
		(command (c) == HOP
		  || command (c) == GOHOP
		  || (command (c) == MOUSEfunction && mouse_button == middlebutton)
		)
	Then	menu_mouse_mode (False);

		menu [itemno - 1].hopitemname = "*";	/* wildcard filter */
		ret = scrolloffset + itemno - 1;
		strcpy (input_result, input_field);
		menu [itemno - 1].itemname = input_result;
	Elsif itemno > 0 &&
		(c == '\n' || command (c) == SNL 
		 || (c == ' '
			&& select_keys != NIL_PTR
			&& selection_space == SPACE_SELECT)
		 || (command (c) == RTkey
			&& menu [itemno - 1].extratag
			&& strcontains (menu [itemno - 1].extratag, "▶"))
		)
	Then	menu_mouse_mode (False);

		If is_file_chooser
		Then	ret = scrolloffset + itemno - 1;
			If is_file_selector
			Then	/* use menu entry unchanged */
			Elsif * input_field && ! streq (input_field, menu [itemno - 1].itemname)
			Then	strcpy (input_result, input_field);
				menu [itemno - 1].itemname = input_result;
				menu [itemno - 1].hopitemname = "";
			Fi
		Elsif select_keys != NIL_PTR
		Then	ret = (scrolloffset + itemno - 1)
				* strlen (select_keys)
				+ select_index;
		Else	/* activate menu item */
			(* (menufunc) menu [itemno - 1].itemfu) (& menu [itemno - 1], scrolloffset + itemno - 1);
			displayflags ();

			/* handle STAYINMENU feature */
			If * menu [itemno - 1].itemon
			Then	int on = (* (flagfunc) menu [itemno - 1].itemon) (& menu [itemno - 1], scrolloffset + itemno - 1);
				If on & STAYINMENU
				Then	ignore_releasebutton = True;
					goto _stayinmenu;
				Fi
			Fi
		Fi
	Elsif select_keys != NIL_PTR && ! is_file_chooser &&
		c != FUNcmd && c > (character) ' '
	Then
		char * cpoi = strchr (select_keys, c);
		If cpoi != NIL_PTR && * cpoi != '\0'
		Then	ret = (scrolloffset + itemno - 1)
				* strlen (select_keys)
				+ (cpoi - select_keys);
		Fi
	Elsif ! input_active && (
		command (c) == LFkey
		|| (command (c) == MOUSEfunction
		    && mouse_button == wheelup && (mouse_shift & control_button))
		)
	Then	clear_menus ();
		If ! open_prev_menu (fullmenu)
		Then	/* set token to position to previously selected item */
			menu_reposition = True;
			/* find previous menu in list of all menus to invoke */
			that_menu (previous_fullmenu);
		Fi
	Elsif ! input_active && (
		command (c) == RTkey
		|| (command (c) == MOUSEfunction
		    && mouse_button == wheeldown && (mouse_shift & control_button))
		)
	Then	clear_menus ();
		If ! open_next_menu (fullmenu)
		Then	/* find one menu right from previous menu */
			If ! open_next_menu (previous_fullmenu)
			Then	ring_bell ();
				error2 ("Internal error: ", "Menu not found");
			Fi
		Fi
	Fi

	clear_menus ();

	If select_keys != NIL_PTR
	Then	clear_status ();
	Fi

	return ret;
}


local void
	action_menu (menu, menulen, column, line, title, is_flag_menu)
		menuitemtype * menu; int menulen;
		int column; int line;
		char * title;
		FLAG is_flag_menu;
{
	clear_menus ();
	(void) popup_menu (menu, menulen, column, line, title, is_flag_menu, False, NIL_PTR);
}


/***********************************************************************\
	Invoking menus explicitly
	that_menu (m) opens that menu at its native position
	this_menu_here (m) open given menu at given position
	openmenuat (c) opens the menu at screen column c
	openmenu (i) opens the i-th menu
\***********************************************************************/

local void
	that_menu (m)
		menuitemtype * m;
{
	int i;
	For i = 0 While i < arrlen (Pulldownmenu) Step i ++
	Do
		If Pulldownmenu [i].menuitems == m
		Then
			ignore_1releasebutton = True;

			action_menu (Pulldownmenu [i].menuitems, 
				Pulldownmenu [i].menulen, 
				i * pulldownmenu_width, -1, 
				Pulldownmenu [i].menuname, False);
			return;
		Fi
	Done
	For i = 0 While i < arrlen (Flagmenu) Step i ++
	Do
		If Flagmenu [i].menu == m
		Then
			ignore_1releasebutton = True;

			action_menu (Flagmenu [i].menu, 
				Flagmenu [i].menulen, 
				- (flags_pos + i - 1), 0, 
				Flagmenu [i].menutitle, True);
			return;
		Fi
	Done
	If Popupmenu == m
	Then
		ignore_1releasebutton = True;

		QUICKMENU ();
		return;
	Fi
	ring_bell ();
	error2 ("Internal error: ", "Menu not found");
}

local void
	this_menu_here (menu, menulen, menutitle)
		menuitemtype * menu;
		int menulen;
		char * menutitle;
{
	FLAG is_flag_menu = False;
	int i;

	ignore_1releasebutton = True;


	For i = 0 While i < menulen Step i ++
	Do
		If * menu [i].itemon
		Then	is_flag_menu = True;
		Fi
	Done

	(void) popup_menu (menu, menulen, 
		last_menucolumn + 9, last_menuline, 
		menutitle, 
		is_flag_menu, 
		False, NIL_PTR);
}

global void
	openmenuat (column)
		int column;
{
	int meni;

	calcmenuvalues ();

	If pulldownmenu_width < 3
	Then	return;
	Fi
	meni = column / pulldownmenu_width;
	If meni < 0 || meni >= number_menus ()
	Then	/* check if it's a flag toggle position */
		meni = column - flags_pos;
		If meni >= 0 && meni < flags_displayed
		Then
			If mouse_button == middlebutton
			Then	(* Flagmenu [meni].toggle) (column);
			Elsif Flagmenu [meni].menutitle != NIL_PTR
			Then	If * ((* Flagmenu [meni].dispflag) ()) != ' '
				Then	action_menu (
						Flagmenu [meni].menu, 
						Flagmenu [meni].menulen, 
						- (column - 1), 
						0, 
						Flagmenu [meni].menutitle, 
						True);
				Fi
			Fi
			displayflags ();
			set_cursor_xy ();
		Fi
	Else
		If mouse_button == middlebutton && hop_flag == 0
		Then	toggle_HOP ();
		Fi
		that_menu (Pulldownmenu [meni].menuitems);
	Fi
}

local void
	openmenu (i)
		int i;
{
	openmenuat (i * pulldownmenu_width);
}


/***********************************************************************\
	Invoking specific menus
\***********************************************************************/

global void
	FILEMENU ()
{
	openmenu (0);
}

global void
	EDITMENU ()
{
	openmenu (1);
}

global void
	SEARCHMENU ()
{
	openmenu (2);
}

global void
	PARAMENU ()
{
	openmenu (3);
}

global void
	OPTIONSMENU ()
{
	openmenu (4);
	/*that_menu (Optionsmenu);*/
}

global void
	QUICKMENU ()
{
	action_menu (Popupmenu, arrlen (Popupmenu), x, y, NIL_PTR, False);
}

global void
	handleQuotemenu ()
{
	that_menu (Quotemenu);
}


global void
	handleKeymapmenu ()
{
	that_menu (Keymapmenu);
}


global void
	handleEncodingmenu ()
{
	that_menu (encodingmenu);
}


global void
	handleFlagmenus ()
{
	flag_menu (0);
}


local void
	handleInfomenu ()
{
	that_menu (infomenu);
}

local void
	handleBuffermenu ()
{
	that_menu (buffermenu);
}

local void
	handleJustifymenu ()
{
	that_menu (justifymenu);
}

#ifdef unused
local void
	handleParagraphmodemenu ()
{
	that_menu (paragraphmodemenu);
}
#endif


local void
	handleUnlockmenu ()
{
	this_menu_here (Unlockmenu, arrlen (Unlockmenu), "Override file lock");
}

local void
	handleScreensizemenu ()
{
	this_menu_here (Screensizemenu, arrlen (Screensizemenu), "Display size");
}

local void
	handleEmulmenu ()
{
	this_menu_here (Emulmenu, arrlen (Emulmenu), "Emulation");
}

local void
	handleLineendtypemenu ()
{
	this_menu_here (Lineendtypemenu, arrlen (Lineendtypemenu), "Lineend type");
}

local void
	handleFilechoosermenu ()
{
	this_menu_here (Filechoosermenu, arrlen (Filechoosermenu), "File chooser");
}

#ifdef more_menu_handlers
local void
	handleCombiningmenu ()
{
	that_menu (combiningmenu);
}

local void
	handleTextmenu ()
{
	that_menu (textmenu);
}

local void
	handleIndentmenu ()
{
	that_menu (autoindentmenu);
}
#endif


/***********************************************************************\
	End
\***********************************************************************/
