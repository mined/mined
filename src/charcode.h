/*======================================================================*\
|*		Editor mined, definitions				*|
|*		CJK character set <-> Unicode mapping tables		*|
\*======================================================================*/

/* disable CJK encodings on VAX (VMS) and MSDOS */
#if defined (msdos) || defined (VAX)
#define noCJKcharmaps
#endif


#define use_CJKcharmaps

#if defined (noCJKcharmaps) || defined (NOCJK)
#undef use_CJKcharmaps
#endif


/*======================================================================*\
	constants
\*======================================================================*/

#define arrlen(arr)	(sizeof (arr) / sizeof (* arr))

/**
   1 byte tags for 2 Unicode character mappings of certain CJK characters;
   check consistency with:
   * list uni2_accents in charcode.c
   * special handling in function iswide
 */
#define uni2tag_shift	16
#define jis2_0x309A	0x80 << uni2tag_shift
#define jis2_0x0300	0x81 << uni2tag_shift
#define jis2_0x0301	0x82 << uni2tag_shift
#define jis2_0x02E5	0x83 << uni2tag_shift
#define jis2_0x02E9	0x84 << uni2tag_shift
#define hkscs2_0x0304	0x85 << uni2tag_shift
#define hkscs2_0x030C	0x86 << uni2tag_shift
#define ambiguous	0x88 << uni2tag_shift
#undef ambiguous	/* not yet implemented */
#define ambiguous	0


#define split_map_entries


/*======================================================================*\
	table entry type
\*======================================================================*/

struct encoding_table_entry {
#ifdef split_map_entries
	unsigned char cjk_ext;
	unsigned char unicode_high;
	unsigned short cjk_base;
	unsigned short unicode_low;
#else
	unsigned int cjk;
	unsigned int unicode;
#endif
};


/*======================================================================*\
	table entry macros
\*======================================================================*/

#ifdef split_map_entries
/**
   For compact representation of encoding mapping tables, the following 
   mappings are used:
	Unicode       table entry
	XXYYZZ        XX, YYZZ

	2 * Unicode   special table entry (for JIS)
	00YYZZ 00yyzz xx, YYZZ (xx >= 80, indicates 2nd char - one of 
				0x309A, 0x0300, 0x0301, 0x02E5, 0x02E9)

	CJK (GB18030) table entry
	    XXYY      FF, XXYY
	XX31YY32      12, YYXX

	CJK (other)   table entry
	    XXYY      00, XXYY
	  8FXXYY      8F, XXYY
	8EXXYYZZ      XX, YYZZ (XX >= 90)
 */
#define map(cjk, unicode)	{(cjk >> 16) & 0xFF, (unicode) >> 16, cjk & 0xFFFF, (unicode) & 0xFFFF},
#define mapgbshort(cjk, unicode)	{0xFF, (unicode) >> 16, cjk, (unicode) & 0xFFFF},
#define mapgblong(cjk, unicode)	{((cjk >> 12) & 0xF0) | (cjk & 0x0F), (unicode) >> 16, (cjk & 0xFF00) | (cjk >> 24), (unicode) & 0xFFFF},
#else
#define map(cjk, unicode)	{cjk, unicode},
#define mapgb(cjk, unicode)	{cjk, unicode},
#endif


/*======================================================================*\
|*		end							*|
\*======================================================================*/
