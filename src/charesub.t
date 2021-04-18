#define label_more_NE_Eurasian "more NE Eurasian"
#define extra_more_NE_Eurasian "▶"
#define title_more_NE_Eurasian "more NE Eurasian"

local int
	onsubitem_more_NE_Eurasian (item, i)
	menuitemtype * item;
	int i;
{
	return streq (item->hopitemname, get_text_encoding ());
}

local menuitemtype submenu_more_NE_Eurasian [] =
{
	{"ISO Cyrillic", select_encoding, "ISO 8859-5", onsubitem_more_NE_Eurasian},
	{"Windows Cyrillic", select_encoding, "CP1251", onsubitem_more_NE_Eurasian},
	{"Tadjik", select_encoding, "KOI8-T", onsubitem_more_NE_Eurasian},
	{"Kazakh", select_encoding, "PT154", onsubitem_more_NE_Eurasian},

	{"Caucasian", separator, ""},
	{"Georgian-PS", select_encoding, "Georgian-PS", onsubitem_more_NE_Eurasian},
	{"Armenian", select_encoding, "ARMSCII", onsubitem_more_NE_Eurasian},
};

local int
	onsubmenu_more_NE_Eurasian (item, i)
	menuitemtype * item;
	int i;
{
	int k;
	for (k = 0; k < arrlen (submenu_more_NE_Eurasian); k ++) {
		if (streq (submenu_more_NE_Eurasian [k].hopitemname, get_text_encoding ())) {
			return 1;
		}
	}
	return 0;
}

local void
	opensubmenu_more_NE_Eurasian (menu, i)
	menuitemtype * menu;
	int i;
{
	int column = last_popup_column + utf8_col_count (label_more_NE_Eurasian) + 2;
	int line = last_popup_line + i;
	(void) popup_menu (submenu_more_NE_Eurasian, arrlen (submenu_more_NE_Eurasian), column, line, title_more_NE_Eurasian, True, False, NIL_PTR);
}

#define label_Greek_Semitic "Greek/Semitic"
#define extra_Greek_Semitic "▶"
#define title_Greek_Semitic "Greek/Semitic"

local int
	onsubitem_Greek_Semitic (item, i)
	menuitemtype * item;
	int i;
{
	return streq (item->hopitemname, get_text_encoding ());
}

local menuitemtype submenu_Greek_Semitic [] =
{
	{"ISO Greek", select_encoding, "ISO 8859-7", onsubitem_Greek_Semitic},
	{"ISO Arabic", select_encoding, "ISO 8859-6", onsubitem_Greek_Semitic},
	{"Mac Arabic  ", select_encoding, "MacArabic>ISO 8859-6", onsubitem_Greek_Semitic},
	{"ISO Hebrew", select_encoding, "ISO 8859-8", onsubitem_Greek_Semitic},
	{"Windows Hebrew  ", select_encoding, "CP1255>ISO 8859-8", onsubitem_Greek_Semitic},
};

local int
	onsubmenu_Greek_Semitic (item, i)
	menuitemtype * item;
	int i;
{
	int k;
	for (k = 0; k < arrlen (submenu_Greek_Semitic); k ++) {
		if (streq (submenu_Greek_Semitic [k].hopitemname, get_text_encoding ())) {
			return 1;
		}
	}
	return 0;
}

local void
	opensubmenu_Greek_Semitic (menu, i)
	menuitemtype * menu;
	int i;
{
	int column = last_popup_column + utf8_col_count (label_Greek_Semitic) + 2;
	int line = last_popup_line + i;
	(void) popup_menu (submenu_Greek_Semitic, arrlen (submenu_Greek_Semitic), column, line, title_Greek_Semitic, True, False, NIL_PTR);
}

#define label_more_Latin "more Latin"
#define extra_more_Latin "▶"
#define title_more_Latin "more Latin"

local int
	onsubitem_more_Latin (item, i)
	menuitemtype * item;
	int i;
{
	return streq (item->hopitemname, get_text_encoding ());
}

local menuitemtype submenu_more_Latin [] =
{
	{"Mac Roman", select_encoding, "MacRoman", onsubitem_more_Latin},
	{"PC/DOS", select_encoding, "CP437", onsubitem_more_Latin},
	{"PC/Latin", select_encoding, "CP850", onsubitem_more_Latin},

	{"ISO Latin", separator, ""},
	{"Latin-2 CentralEurop", select_encoding, "ISO 8859-2", onsubitem_more_Latin},
	{"Latin-3 SouthEurop", select_encoding, "ISO 8859-3", onsubitem_more_Latin},
	{"Latin-4 (Baltic)", select_encoding, "ISO 8859-4", onsubitem_more_Latin},
	{"Latin-5 Turkish", select_encoding, "ISO 8859-9", onsubitem_more_Latin},
	{"Latin-6 Nordic", select_encoding, "ISO 8859-10", onsubitem_more_Latin},
	{"Latin-7 Baltic", select_encoding, "ISO 8859-13", onsubitem_more_Latin},
	{"Latin-8 Celtic", select_encoding, "ISO 8859-14", onsubitem_more_Latin},
	{"Latin-10 Romanian", select_encoding, "ISO 8859-16", onsubitem_more_Latin},
};

local int
	onsubmenu_more_Latin (item, i)
	menuitemtype * item;
	int i;
{
	int k;
	for (k = 0; k < arrlen (submenu_more_Latin); k ++) {
		if (streq (submenu_more_Latin [k].hopitemname, get_text_encoding ())) {
			return 1;
		}
	}
	return 0;
}

local void
	opensubmenu_more_Latin (menu, i)
	menuitemtype * menu;
	int i;
{
	int column = last_popup_column + utf8_col_count (label_more_Latin) + 2;
	int line = last_popup_line + i;
	(void) popup_menu (submenu_more_Latin, arrlen (submenu_more_Latin), column, line, title_more_Latin, True, False, NIL_PTR);
}

