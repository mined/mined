	{"Windows Latin", select_encoding, "CP1252", encodingon},
	{"Latin-9", select_encoding, "ISO 8859-15", encodingon},

	{"", separator, ""},
	{"Cyrillic / KOI8-RU", select_encoding, "KOI8-RU", encodingon},

	{label_more_NE_Eurasian, opensubmenu_more_NE_Eurasian, "", onsubmenu_more_NE_Eurasian, extra_more_NE_Eurasian},

	{label_Greek_Semitic, opensubmenu_Greek_Semitic, "", onsubmenu_Greek_Semitic, extra_Greek_Semitic},

	{label_more_Latin, opensubmenu_more_Latin, "", onsubmenu_more_Latin, extra_more_Latin},
#ifdef use_CJKcharmaps

	{"Chinese", separator, ""},
	{"Big5 HK/Traditional", select_encoding, "Big5 HKSCS-2008>~CP950", encodingon},
	{"GB18030/Simplified", select_encoding, "GB18030>CP936>GBK>GB2312/EUC-CN", encodingon},
	{"CNS/TW Traditional", select_encoding, "CNS/EUC-TW", encodingon},

	{"Japanese", separator, ""},
	{"EUC-JP", select_encoding, "EUC-JP", encodingon},
	{"EUC-JIS X 0213", select_encoding, "EUC-JIS-2004 X 0213 (x0213.org)", encodingon},
	{"CP932 'Shift_JIS' ", select_encoding, "CP932>Shift_JIS", encodingon},
	{"Shift_JIS X 0213", select_encoding, "Shift_JIS-2004 X 0213 x0213.org", encodingon},

	{"Korean", separator, ""},
	{"UHC / EUC-KR", select_encoding, "UHC/CP949>EUC-KR", encodingon},
	{"Johab", select_encoding, "Johab", encodingon},
#endif

	{"Vietnamese", separator, ""},
	{"VISCII", select_encoding, "VISCII", encodingon},
	{"TCVN", select_encoding, "TCVN", encodingon},
	{"Windows Vietnamese", select_encoding, "CP1258", encodingon},

	{"Thai", separator, ""},
	{"TIS-620", select_encoding, "CP874>ISO 8859-11>TIS-620", encodingon},
