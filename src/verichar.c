#include "charprop.c"

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

FLAG suppress_non_BMP = 0;


int
check_cases (unsigned long u)
{
	char buf [7];
	int ulen = utfencode (u, buf);
	int clen;
	unsigned long lower = case_convert (u, -1);
	unsigned long upper = case_convert (u, 1);
	unsigned long title = case_convert (u, 2);
	if (lower != u && ulen != (clen = utfencode (lower, buf))) {
		printf ("U+%04lX %d lower U+%04lX %d\n", u, ulen, lower, clen);
	}
	if (upper != u && ulen != (clen = utfencode (upper, buf))) {
		printf ("U+%04lX %d upper U+%04lX %d\n", u, ulen, upper, clen);
	}
	if (title != u && ulen != (clen = utfencode (title, buf))) {
		printf ("U+%04lX %d title U+%04lX %d\n", u, ulen, title, clen);
	}
}

int
main ()
{
	unsigned long u;
	for (u = 0; u < 0x30000; u ++) {
		check_cases (u);
	}
}
