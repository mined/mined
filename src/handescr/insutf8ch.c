#include <string.h>
#include <ctype.h>
#include <stdio.h>

void
printutf8 (unichar)
	unsigned long unichar;
{
	/* this is literal output, not C string preparation;
	   so in contrast to the mk* scripts, do not apply \ escaping
	 */

	if (unichar < 0x80) {
		printf ("%c", (unsigned char)(unichar));
	} else if (unichar < 0x800) {
		printf ("%c", (unsigned char)(0xC0 | (unichar >> 6)));
		printf ("%c", (unsigned char)(0x80 | (unichar & 0x3F)));
	} else if (unichar < 0x10000) {
		printf ("%c", (unsigned char)(0xE0 | (unichar >> 12)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 6) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | (unichar & 0x3F)));
	} else if (unichar < 0x200000) {
		printf ("%c", (unsigned char)(0xF0 | (unichar >> 18)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 12) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 6) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | (unichar & 0x3F)));
	} else if (unichar < 0x4000000) {
		printf ("%c", (unsigned char)(0xF8 | (unichar >> 24)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 18) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 12) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 6) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | (unichar & 0x3F)));
	} else if (unichar < 0x80000000) {
		printf ("%c", (unsigned char)(0xFC | (unichar >> 30)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 24) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 18) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 12) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | ((unichar >> 6) & 0x3F)));
		printf ("%c", (unsigned char)(0x80 | (unichar & 0x3F)));
	}
}

int
prio (unichar)
	unsigned long unichar;
{
/* prioritize according to range names in Blocks.txt
4E00..9FFF; CJK Unified Ideographs
3400..4DBF; CJK Unified Ideographs Extension A
20000..2A6DF; CJK Unified Ideographs Extension B
2A700..2B73F; CJK Unified Ideographs Extension C
2B740..2B81F; CJK Unified Ideographs Extension D
2B820..2CEAF; CJK Unified Ideographs Extension E
2CEB0..2EBEF; CJK Unified Ideographs Extension F
30000..3134F; CJK Unified Ideographs Extension G
2E80..2EFF; CJK Radicals Supplement
31C0..31EF; CJK Strokes
F900..FAFF; CJK Compatibility Ideographs
2F800..2FA1F; CJK Compatibility Ideographs Supplement
3300..33FF; CJK Compatibility
FE30..FE4F; CJK Compatibility Forms
3000..303F; CJK Symbols and Punctuation
3200..32FF; Enclosed CJK Letters and Months
*/
	if (unichar >= 0x4E00 && unichar <= 0x9FFF) {
		/* CJK Unified Ideographs */
		return 1;
	} else if (unichar >= 0x3400 && unichar <= 0x4DBF) {
		/* CJK Unified Ideographs Extension A */
		return 2;
	} else if (unichar >= 0x20000 && unichar <= 0x2A6DF) {
		/* CJK Unified Ideographs Extension B */
		return 3;
	} else if (unichar >= 0x2A700 && unichar <= 0x2B73F) {
		/* CJK Unified Ideographs Extension C */
		return 4;
	} else if (unichar >= 0x2B740 && unichar <= 0x2B81F) {
		/* CJK Unified Ideographs Extension D */
		return 5;
	} else if (unichar >= 0x2B820 && unichar <= 0x2CEAF) {
		/* CJK Unified Ideographs Extension E */
		return 6;
	} else if (unichar >= 0x2CEB0 && unichar <= 0x2EBEF) {
		/* CJK Unified Ideographs Extension F */
		return 7;
	} else if (unichar >= 0x30000 && unichar <= 0x3134F) {
		/* CJK Unified Ideographs Extension G */
		return 8;
	} else if (unichar >= 0x2E80 && unichar <= 0x2EFF) {
		/* CJK Radicals Supplement */
		return 11;
	} else if (unichar >= 0x31C0 && unichar <= 0x31EF) {
		/* CJK Strokes */
		return 12;
	} else if (unichar >= 0xF900 && unichar <= 0xFAFF) {
		/* CJK Compatibility Ideographs */
		return 21;
	} else if (unichar >= 0x2F800 && unichar <= 0x2FA1F) {
		/* CJK Compatibility Ideographs Supplement */
		return 22;
	} else if (unichar >= 0x3300 && unichar <= 0x33FF) {
		/* CJK Compatibility */
		return 23;
	} else if (unichar >= 0xFE30 && unichar <= 0xFE4F) {
		/* CJK Compatibility Forms */
		return 31;
	} else if (unichar >= 0x3000 && unichar <= 0x303F) {
		/* CJK Symbols and Punctuation */
		return 41;
	} else if (unichar >= 0x3200 && unichar <= 0x32FF) {
		/* Enclosed CJK Letters and Months */
		return 42;
	} else {
		return 99;
	}
}

int
main (argc, argv)
	int argc;
	char * * argv;
{
	char dec = 0;
	char buf[9999];

	if (argc > 1 && * argv[1] == '-')
		if (argv[1][1] == 'd')
			dec = 1;

	while (fgets (buf, sizeof buf, stdin)) {
		char * U_ = strstr (buf, "U+");
		if (U_) {
			char * s = U_ + 2;
			long u = 0;

			* U_ = 0;
			printf ("%s", buf);
			while (isxdigit (* s)) {
				if (* s <= '9')
					u = u * 16 + * s - '0';
				else if (* s <= 'F')
					u = u * 16 + * s - 'A' + 10;
				else if (* s <= 'f')
					u = u * 16 + * s - 'a' + 10;
				s ++;
			}
			if (dec) {
				printf ("%02d %ld ", prio (u), u);
			}
			printutf8 (u);
			* U_ = 'U';
			printf (" %s\n", U_);
		}
		else
			printf ("%s\n", buf);
	}
}
