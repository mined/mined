/*======================================================================*\
|*		Editor mined						*|
|*		Interface						*|
|*		Character properties					*|
\*======================================================================*/

/* Han character descriptions table */

struct hanentry {
	unsigned long unicode;
	char * Mandarin;
	char * Cantonese;
	char * Japanese;
	char * Sino_Japanese;
	char * Hangul;
	char * Korean;
	char * Vietnamese;
	char * HanyuPinlu;
	char * HanyuPinyin;
	char * XHCHanyuPinyin;
	char * Tang;
	char * Definition;
};

extern struct hanentry * lookup_handescr _((unsigned long unichar));


/* Character name information */

extern char * charname _((unsigned long ucs));
extern char * charseqname _((unsigned long ucs, char * follow, int * seqlen, unsigned long * * seq));


/* Script information */

struct scriptentry {
	unsigned long first, last;
	int scriptname;
	int categoryname;
};

extern struct scriptentry * scriptinfo _((unsigned long ucs));

extern char * category_names [];


/* Case conversion conditions */

#define U_cond_Final_Sigma		0x01
#define U_cond_After_I			0x02
#define U_cond_After_Soft_Dotted	0x04
#define U_cond_More_Above		0x08
#define U_cond_Not_Before_Dot		0x10
#define U_cond_tr			0x20
#define U_cond_lt			0x40
#define U_cond_az			0x80
#define U_conds_lang	(U_cond_tr | U_cond_lt | U_cond_az)

/* Case conversion table */

struct caseconv_entry {
	unsigned long base;
	int toupper, tolower;
	unsigned long title;
};

extern struct caseconv_entry caseconv_table [];

typedef struct {unsigned short u1, u2, u3;} uniseq;

struct caseconv_special_entry {
	unsigned long base;
	uniseq lower, title, upper;
	short condition;
};

extern struct caseconv_special_entry caseconv_special [];

extern int lookup_caseconv _((unsigned long basechar));
extern int lookup_caseconv_special _((unsigned long basechar, short langcond));
extern unsigned long case_convert _((unsigned long unichar, int dir));


/* Various character properties */

extern int soft_dotted _((unsigned long ucs));
extern int iscombining_notabove _((unsigned long unichar));
extern int iscombining_above _((unsigned long unichar));
extern char * script _((unsigned long ucs));
extern unsigned long decomposition_base _((unsigned long ucs));
extern char * category _((unsigned long ucs));
extern FLAG isLetter _((unsigned long ucs));
extern FLAG is_wideunichar _((unsigned long ucs));
extern FLAG iscombining_unichar _((unsigned long ucs));
extern FLAG isspacingcombining_unichar _((unsigned long ucs));


/* Character decomposition information */

extern char * decomposition_string _((unsigned long ucs));


/*======================================================================*\
|*			from charcode.c					*|
\*======================================================================*/

extern unsigned char code_SPACE;
extern unsigned char code_TAB;
extern unsigned long code_LF;
extern unsigned long code_NL;

extern int iscontrol _((unsigned long));
extern int iswhitespace _((unsigned long));
extern int isquotationmark _((unsigned long));
extern int isdash _((unsigned long));
extern int is_right_to_left _((unsigned long ucs));
extern FLAG is_bullet_or_dash _((unsigned long unich));
extern int isopeningparenthesis _((unsigned long));
extern character controlchar _((character));

extern int utfencode _((unsigned long, character *));
extern int cjkencode _((unsigned long, character *));
extern int cjkencode_char _((FLAG term, unsigned long, character *));
extern char * encode_char _((unsigned long));
extern FLAG valid_cjk _((unsigned long cjkchar, /* opt */ character * cjkbytes));
extern FLAG valid_cjkchar _((FLAG term, unsigned long cjkchar, /* opt */ character * cjkbytes));
extern unsigned long isolated_alef _((unsigned long));
extern unsigned long ligature_lam_alef _((unsigned long));
extern FLAG no_char _((unsigned long c));
extern FLAG no_unichar _((unsigned long u));

extern unsigned long mappedtermchar _((unsigned long));
extern unsigned long lookup_mappedtermchar _((unsigned long));
extern FLAG remapping_chars _((void));
extern unsigned long encodedchar _((unsigned long));
extern unsigned long encodedchar2 _((unsigned long, unsigned long));
extern unsigned long lookup_encodedchar _((unsigned long));
extern unsigned long max_char_value _((void));
extern FLAG encoding_has_combining _((void));
extern char * get_text_encoding _((void));
extern char * get_term_encoding _((void));
extern FLAG set_text_encoding _((char * charmap, char tag, char * debug_tag));
extern FLAG set_term_encoding _((char * charmap, char tag));
extern char text_encoding_tag;
extern char * text_encoding_flag;
extern char term_encoding_tag;


/*======================================================================*\
|*				End					*|
\*======================================================================*/
