/*======================================================================*\
	Check function valid_cjk with all mapping entries
	of all CJK character code mapping tables.
\*======================================================================*/

#include "charcode.c"


/*======================================================================*\
	Dummies for charcode.c
\*======================================================================*/

/**
   Providing dummy variables for charcode.c
 */
FLAG utf8_text = 0;
FLAG utf16_file = 0;
FLAG utf16_little_endian = 0;
FLAG cjk_text = 0;
FLAG mapped_text = 0;
FLAG ebcdic_text = 0;
FLAG ebcdic_file = 0;


FLAG
iscombining_unichar (u)
  unsigned long u;
{
  return term_iscombining (u);
}


/*======================================================================*\
	Dummies for width.c
\*======================================================================*/

FLAG suppress_non_BMP = 0;


/*======================================================================*\
	Checking function
\*======================================================================*/

static
unsigned long
decode_uni (struct encoding_table_entry * tp)
{
#ifdef split_map_entries
	if (tp->unicode_high & 0x80) {
		return 0x80000000 | (uni2_accents [tp->unicode_high & 0x7F] << 16) | (tp->unicode_low);
	} else {
		return (((unsigned long) tp->unicode_high) << 16) | (tp->unicode_low);
	}
#else
	if (tp->unicode & 0x800000) {
		return 0x80000000 | (uni2_accents [(tp->unicode >> 16) & 0x7F] << 16) | (tp->unicode & 0xFFFF);
	} else {
		return tp->unicode;
	}
#endif
}

/**
   Return # character codes marked non-valid.
 */
static
unsigned long
check (char tag)
{
	unsigned int i = 0;
	unsigned long cjkchar;
	unsigned long unichar;
	unsigned long ok = 0;
	unsigned long non = 0;
	unsigned long comb = 0;
	struct encoding_table_entry * cjk_table_poi;

	(void) set_text_encoding (NIL_PTR, tag, "validcjk");

	cjk_table_poi = text_table;
	while (i ++ < text_table_len) {
#ifdef split_map_entries
		cjkchar = decode_cjk (cjk_table_poi, text_table);
#else
		cjkchar = cjk_table_poi->cjk;
#endif
		unichar = decode_uni (cjk_table_poi);
		if (valid_cjk (cjkchar, NIL_PTR)) {
			ok ++;
		} else {
			printf ("%c %04lX\n", text_encoding_tag, cjkchar);
			non ++;
		}
		if (iscombining_unichar (unichar)) {
			comb ++;
		}
		cjk_table_poi ++;
	}
	printf ("%c: valid %ld, not valid %ld, combining %ld\n", text_encoding_tag, ok, non, comb);
	return non;
}

int
main ()
{
	unsigned long non = 0;
	non += check ('B');
	non += check ('G');
	non += check ('C');
	non += check ('J');
	non += check ('X');
	non += check ('S');
	non += check ('x');
	non += check ('K');
	non += check ('H');

	return non > 0;
}


/*======================================================================*\
	end
\*======================================================================*/
