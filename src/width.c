/*======================================================================*\
|	Functions to enquire character width
|	as depending on terminal properties,
|	varying with terminal kind and version,
|	according to different Unicode versions and other features
\*======================================================================*/

#include "termprop.h"


/*======================================================================*\
|	Table structure: tables of character ranges; lookup function
\*======================================================================*/

struct interval {
    unsigned long first;
    unsigned long last;
};

static
struct interval *
lookup (ucs, table, length)
  unsigned long ucs;
  struct interval * table;
  int length;
{
  int min = 0;
  int mid;
  int max = length - 1;

  /* first quick check for Latin-1 etc. characters */
  if (table && ucs < table [0].first) {
	return 0;
  }

  /* binary search */
  while (max >= min) {
	mid = (min + max) / 2;
	if (ucs > table [mid].last) {
		min = mid + 1;
	} else if (ucs < table [mid].first) {
		max = mid - 1;
	} else {
		return & table [mid];
	}
  }

  return 0;
}


/*======================================================================*\
|	Table variables
|	- to be assigned depending on detected Unicode data version
\*======================================================================*/

static struct interval * combining_table = 0;
static int combining_len = -1;
static struct interval * spacing_combining_table = 0;
static int spacing_combining_len = -1;
static struct interval * assigned_table = 0;
static int assigned_len = -1;

#define arrlen(arr)	(sizeof (arr) / sizeof (* arr))


/*======================================================================*\
|	Actual data tables
|	- some explicitly included here
|	- some generated from Unicode data (#include "width.t")
\*======================================================================*/

/* Tables of combining characters, according to different versions 
   of Unicode or wcwidth.c, respectively,
   arranged as sorted lists of non-overlapping intervals.
 */

/**
   Combining characters
   ~ Unicode 3.0 with some exceptions
   xterm < 157
 */
static struct interval
combining_old [] = {
  { 0x0300, 0x034E }, { 0x0360, 0x0362 }, { 0x0483, 0x0486 },
  { 0x0488, 0x0489 }, { 0x0591, 0x05A1 }, { 0x05A3, 0x05B9 },
  { 0x05BB, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
  { 0x05C4, 0x05C4 }, { 0x064B, 0x0655 }, { 0x0670, 0x0670 },
  { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
  { 0x0711, 0x0711 }, { 0x0730, 0x074A }, { 0x07A6, 0x07B0 },
  { 0x0901, 0x0902 }, { 0x093C, 0x093C }, { 0x0941, 0x0948 },
  { 0x094D, 0x094D }, { 0x0951, 0x0954 }, { 0x0962, 0x0963 },
  { 0x0981, 0x0981 }, { 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 },
  { 0x09CD, 0x09CD }, { 0x09E2, 0x09E3 }, { 0x0A02, 0x0A02 },
  { 0x0A3C, 0x0A3C }, { 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 },
  { 0x0A4B, 0x0A4D }, { 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 },
  { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 },
  { 0x0ACD, 0x0ACD }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
  { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 }, { 0x0B4D, 0x0B4D },
  { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 }, { 0x0BC0, 0x0BC0 },
  { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 }, { 0x0C46, 0x0C48 },
  { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 }, { 0x0CBF, 0x0CBF },
  { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD }, { 0x0D41, 0x0D43 },
  { 0x0D4D, 0x0D4D }, { 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 },
  { 0x0DD6, 0x0DD6 }, { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A },
  { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 },
  { 0x0EBB, 0x0EBC }, { 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 },
  { 0x0F35, 0x0F35 }, { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 },
  { 0x0F71, 0x0F7E }, { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 },
  { 0x0F90, 0x0F97 }, { 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 },
  { 0x102D, 0x1030 }, { 0x1032, 0x1032 }, { 0x1036, 0x1037 },
  { 0x1039, 0x1039 }, { 0x1058, 0x1059 }, { 0x17B7, 0x17BD },
  { 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x18A9, 0x18A9 },
  { 0x20D0, 0x20E3 }, { 0x302A, 0x302F }, { 0x3099, 0x309A },
  { 0xFB1E, 0xFB1E }, { 0xFE20, 0xFE23 }
};

/**
   Combining characters
   Unicode 3.0
   wcwidth 2001-01-12
   xterm 157...166
 */
/*static struct interval combining_300 [] = { defined in width.t };*/

/**
   Combining characters
   Unicode 3.2
   wcwidth.c 2002-05-08
   xterm 167..179
 */
/*static struct interval combining_320 [] = { defined in width.t };*/

/**
   Combining characters
   Unicode 4.0
   wcwidth.c 2003-05-20
   xterm 180...201
 */
/*static struct interval combining_400 [] = { defined in width.t };*/

/**
   Combining characters
   Unicode 4.1
   xterm 202...
 */
/*static struct interval combining_410 [] = { defined in width.t };*/

/**
   Combining characters
   Unicode 5.0
 */
/*static struct interval combining_500 [] = { defined in width.t };*/


/* Tables of specific width sets.
 */

/**
   Poderosa terminal emulator in Latin-1 mode.
 */
static struct interval
wide_poderosa [] = {
    { 0x00A7, 0x00A8 }, { 0x00B0, 0x00B1 },
    { 0x00B4, 0x00B4 }, { 0x00B6, 0x00B6 }, { 0x00D7, 0x00D7 }, { 0x00F7, 0x00F7 }
};


/* Tables of ambiguous width characters, according to different versions 
   of Unicode or wcwidth.c, respectively,
   arranged as sorted lists of non-overlapping intervals.
 */

/**
   Ambiguous width characters
   Unicode 3.2 without Private Use range plus extended end/non-BMP ranges
   wcwidth.c 2002-05-08 plus end/non-BMP ranges
   xterm 167...179 (different version in earlier xterm releases was unused)
 */
static struct interval
ambiguous_old [] = {
    { 0x00A1, 0x00A1 }, { 0x00A4, 0x00A4 }, { 0x00A7, 0x00A8 },
    { 0x00AA, 0x00AA }, { 0x00AD, 0x00AE }, { 0x00B0, 0x00B4 },
    { 0x00B6, 0x00BA }, { 0x00BC, 0x00BF }, { 0x00C6, 0x00C6 },
    { 0x00D0, 0x00D0 }, { 0x00D7, 0x00D8 }, { 0x00DE, 0x00E1 },
    { 0x00E6, 0x00E6 }, { 0x00E8, 0x00EA }, { 0x00EC, 0x00ED },
    { 0x00F0, 0x00F0 }, { 0x00F2, 0x00F3 }, { 0x00F7, 0x00FA },
    { 0x00FC, 0x00FC }, { 0x00FE, 0x00FE }, { 0x0101, 0x0101 },
    { 0x0111, 0x0111 }, { 0x0113, 0x0113 }, { 0x011B, 0x011B },
    { 0x0126, 0x0127 }, { 0x012B, 0x012B }, { 0x0131, 0x0133 },
    { 0x0138, 0x0138 }, { 0x013F, 0x0142 }, { 0x0144, 0x0144 },
    { 0x0148, 0x014B }, { 0x014D, 0x014D }, { 0x0152, 0x0153 },
    { 0x0166, 0x0167 }, { 0x016B, 0x016B }, { 0x01CE, 0x01CE },
    { 0x01D0, 0x01D0 }, { 0x01D2, 0x01D2 }, { 0x01D4, 0x01D4 },
    { 0x01D6, 0x01D6 }, { 0x01D8, 0x01D8 }, { 0x01DA, 0x01DA },
    { 0x01DC, 0x01DC }, { 0x0251, 0x0251 }, { 0x0261, 0x0261 },
    { 0x02C4, 0x02C4 }, { 0x02C7, 0x02C7 }, { 0x02C9, 0x02CB },
    { 0x02CD, 0x02CD }, { 0x02D0, 0x02D0 }, { 0x02D8, 0x02DB },
    { 0x02DD, 0x02DD }, { 0x02DF, 0x02DF }, { 0x0391, 0x03A1 },
    { 0x03A3, 0x03A9 }, { 0x03B1, 0x03C1 }, { 0x03C3, 0x03C9 },
    { 0x0401, 0x0401 }, { 0x0410, 0x044F }, { 0x0451, 0x0451 },
    { 0x2010, 0x2010 }, { 0x2013, 0x2016 }, { 0x2018, 0x2019 },
    { 0x201C, 0x201D }, { 0x2020, 0x2022 }, { 0x2024, 0x2027 },
    { 0x2030, 0x2030 }, { 0x2032, 0x2033 }, { 0x2035, 0x2035 },
    { 0x203B, 0x203B }, { 0x203E, 0x203E }, { 0x2074, 0x2074 },
    { 0x207F, 0x207F }, { 0x2081, 0x2084 }, { 0x20AC, 0x20AC },
    { 0x2103, 0x2103 }, { 0x2105, 0x2105 }, { 0x2109, 0x2109 },
    { 0x2113, 0x2113 }, { 0x2116, 0x2116 }, { 0x2121, 0x2122 },
    { 0x2126, 0x2126 }, { 0x212B, 0x212B }, { 0x2153, 0x2154 },
    { 0x215B, 0x215E }, { 0x2160, 0x216B }, { 0x2170, 0x2179 },
    { 0x2190, 0x2199 }, { 0x21B8, 0x21B9 }, { 0x21D2, 0x21D2 },
    { 0x21D4, 0x21D4 }, { 0x21E7, 0x21E7 }, { 0x2200, 0x2200 },
    { 0x2202, 0x2203 }, { 0x2207, 0x2208 }, { 0x220B, 0x220B },
    { 0x220F, 0x220F }, { 0x2211, 0x2211 }, { 0x2215, 0x2215 },
    { 0x221A, 0x221A }, { 0x221D, 0x2220 }, { 0x2223, 0x2223 },
    { 0x2225, 0x2225 }, { 0x2227, 0x222C }, { 0x222E, 0x222E },
    { 0x2234, 0x2237 }, { 0x223C, 0x223D }, { 0x2248, 0x2248 },
    { 0x224C, 0x224C }, { 0x2252, 0x2252 }, { 0x2260, 0x2261 },
    { 0x2264, 0x2267 }, { 0x226A, 0x226B }, { 0x226E, 0x226F },
    { 0x2282, 0x2283 }, { 0x2286, 0x2287 }, { 0x2295, 0x2295 },
    { 0x2299, 0x2299 }, { 0x22A5, 0x22A5 }, { 0x22BF, 0x22BF },
    { 0x2312, 0x2312 }, { 0x2460, 0x24E9 }, { 0x24EB, 0x24FE },
    { 0x2500, 0x254B }, { 0x2550, 0x2573 }, { 0x2580, 0x258F },
    { 0x2592, 0x2595 }, { 0x25A0, 0x25A1 }, { 0x25A3, 0x25A9 },
    { 0x25B2, 0x25B3 }, { 0x25B6, 0x25B7 }, { 0x25BC, 0x25BD },
    { 0x25C0, 0x25C1 }, { 0x25C6, 0x25C8 }, { 0x25CB, 0x25CB },
    { 0x25CE, 0x25D1 }, { 0x25E2, 0x25E5 }, { 0x25EF, 0x25EF },
    { 0x2605, 0x2606 }, { 0x2609, 0x2609 }, { 0x260E, 0x260F },
    { 0x261C, 0x261C }, { 0x261E, 0x261E }, { 0x2640, 0x2640 },
    { 0x2642, 0x2642 }, { 0x2660, 0x2661 }, { 0x2663, 0x2665 },
    { 0x2667, 0x266A }, { 0x266C, 0x266D }, { 0x266F, 0x266F },
    { 0x273D, 0x273D }, { 0x2776, 0x277F }, 
    { 0xFFFD, 0xFFFF }, { 0xF0000, 0x7FFFFFFF }
};

/**
   Ambiguous width characters
   Unicode 4.0 plus extended end/non-BMP ranges
   wcwidth.c 2003-05-20 plus extended end/non-BMP ranges
   wcwidth.c 2007-05-25 plus extended end/non-BMP ranges
   xterm 180...
 */
static struct interval
ambiguous_400 [] = {
    { 0x00A1, 0x00A1 }, { 0x00A4, 0x00A4 }, { 0x00A7, 0x00A8 },
    { 0x00AA, 0x00AA }, { 0x00AE, 0x00AE }, { 0x00B0, 0x00B4 },
    { 0x00B6, 0x00BA }, { 0x00BC, 0x00BF }, { 0x00C6, 0x00C6 },
    { 0x00D0, 0x00D0 }, { 0x00D7, 0x00D8 }, { 0x00DE, 0x00E1 },
    { 0x00E6, 0x00E6 }, { 0x00E8, 0x00EA }, { 0x00EC, 0x00ED },
    { 0x00F0, 0x00F0 }, { 0x00F2, 0x00F3 }, { 0x00F7, 0x00FA },
    { 0x00FC, 0x00FC }, { 0x00FE, 0x00FE }, { 0x0101, 0x0101 },
    { 0x0111, 0x0111 }, { 0x0113, 0x0113 }, { 0x011B, 0x011B },
    { 0x0126, 0x0127 }, { 0x012B, 0x012B }, { 0x0131, 0x0133 },
    { 0x0138, 0x0138 }, { 0x013F, 0x0142 }, { 0x0144, 0x0144 },
    { 0x0148, 0x014B }, { 0x014D, 0x014D }, { 0x0152, 0x0153 },
    { 0x0166, 0x0167 }, { 0x016B, 0x016B }, { 0x01CE, 0x01CE },
    { 0x01D0, 0x01D0 }, { 0x01D2, 0x01D2 }, { 0x01D4, 0x01D4 },
    { 0x01D6, 0x01D6 }, { 0x01D8, 0x01D8 }, { 0x01DA, 0x01DA },
    { 0x01DC, 0x01DC }, { 0x0251, 0x0251 }, { 0x0261, 0x0261 },
    { 0x02C4, 0x02C4 }, { 0x02C7, 0x02C7 }, { 0x02C9, 0x02CB },
    { 0x02CD, 0x02CD }, { 0x02D0, 0x02D0 }, { 0x02D8, 0x02DB },
    { 0x02DD, 0x02DD }, { 0x02DF, 0x02DF }, { 0x0391, 0x03A1 },
    { 0x03A3, 0x03A9 }, { 0x03B1, 0x03C1 }, { 0x03C3, 0x03C9 },
    { 0x0401, 0x0401 }, { 0x0410, 0x044F }, { 0x0451, 0x0451 },
    { 0x2010, 0x2010 }, { 0x2013, 0x2016 }, { 0x2018, 0x2019 },
    { 0x201C, 0x201D }, { 0x2020, 0x2022 }, { 0x2024, 0x2027 },
    { 0x2030, 0x2030 }, { 0x2032, 0x2033 }, { 0x2035, 0x2035 },
    { 0x203B, 0x203B }, { 0x203E, 0x203E }, { 0x2074, 0x2074 },
    { 0x207F, 0x207F }, { 0x2081, 0x2084 }, { 0x20AC, 0x20AC },
    { 0x2103, 0x2103 }, { 0x2105, 0x2105 }, { 0x2109, 0x2109 },
    { 0x2113, 0x2113 }, { 0x2116, 0x2116 }, { 0x2121, 0x2122 },
    { 0x2126, 0x2126 }, { 0x212B, 0x212B }, { 0x2153, 0x2154 },
    { 0x215B, 0x215E }, { 0x2160, 0x216B }, { 0x2170, 0x2179 },
    { 0x2190, 0x2199 }, { 0x21B8, 0x21B9 }, { 0x21D2, 0x21D2 },
    { 0x21D4, 0x21D4 }, { 0x21E7, 0x21E7 }, { 0x2200, 0x2200 },
    { 0x2202, 0x2203 }, { 0x2207, 0x2208 }, { 0x220B, 0x220B },
    { 0x220F, 0x220F }, { 0x2211, 0x2211 }, { 0x2215, 0x2215 },
    { 0x221A, 0x221A }, { 0x221D, 0x2220 }, { 0x2223, 0x2223 },
    { 0x2225, 0x2225 }, { 0x2227, 0x222C }, { 0x222E, 0x222E },
    { 0x2234, 0x2237 }, { 0x223C, 0x223D }, { 0x2248, 0x2248 },
    { 0x224C, 0x224C }, { 0x2252, 0x2252 }, { 0x2260, 0x2261 },
    { 0x2264, 0x2267 }, { 0x226A, 0x226B }, { 0x226E, 0x226F },
    { 0x2282, 0x2283 }, { 0x2286, 0x2287 }, { 0x2295, 0x2295 },
    { 0x2299, 0x2299 }, { 0x22A5, 0x22A5 }, { 0x22BF, 0x22BF },
    { 0x2312, 0x2312 }, { 0x2460, 0x24E9 }, { 0x24EB, 0x254B },
    { 0x2550, 0x2573 }, { 0x2580, 0x258F }, { 0x2592, 0x2595 },
    { 0x25A0, 0x25A1 }, { 0x25A3, 0x25A9 }, { 0x25B2, 0x25B3 },
    { 0x25B6, 0x25B7 }, { 0x25BC, 0x25BD }, { 0x25C0, 0x25C1 },
    { 0x25C6, 0x25C8 }, { 0x25CB, 0x25CB }, { 0x25CE, 0x25D1 },
    { 0x25E2, 0x25E5 }, { 0x25EF, 0x25EF }, { 0x2605, 0x2606 },
    { 0x2609, 0x2609 }, { 0x260E, 0x260F }, { 0x2614, 0x2615 },
    { 0x261C, 0x261C }, { 0x261E, 0x261E }, { 0x2640, 0x2640 },
    { 0x2642, 0x2642 }, { 0x2660, 0x2661 }, { 0x2663, 0x2665 },
    { 0x2667, 0x266A }, { 0x266C, 0x266D }, { 0x266F, 0x266F },
    { 0x273D, 0x273D }, { 0x2776, 0x277F }, { 0xE000, 0xF8FF },
    { 0xFFFD, 0xFFFF }, { 0xF0000, 0x7FFFFFFF }
};

static struct interval
ambiguous_520 [] = {
  { 0x00A1, 0x00A1 }, { 0x00A4, 0x00A4 }, { 0x00A7, 0x00A8 },
  { 0x00AA, 0x00AA }, { 0x00AE, 0x00AE }, { 0x00B0, 0x00B4 },
  { 0x00B6, 0x00BA }, { 0x00BC, 0x00BF }, { 0x00C6, 0x00C6 },
  { 0x00D0, 0x00D0 }, { 0x00D7, 0x00D8 }, { 0x00DE, 0x00E1 },
  { 0x00E6, 0x00E6 }, { 0x00E8, 0x00EA }, { 0x00EC, 0x00ED },
  { 0x00F0, 0x00F0 }, { 0x00F2, 0x00F3 }, { 0x00F7, 0x00FA },
  { 0x00FC, 0x00FC }, { 0x00FE, 0x00FE }, { 0x0101, 0x0101 },
  { 0x0111, 0x0111 }, { 0x0113, 0x0113 }, { 0x011B, 0x011B },
  { 0x0126, 0x0127 }, { 0x012B, 0x012B }, { 0x0131, 0x0133 },
  { 0x0138, 0x0138 }, { 0x013F, 0x0142 }, { 0x0144, 0x0144 },
  { 0x0148, 0x014B }, { 0x014D, 0x014D }, { 0x0152, 0x0153 },
  { 0x0166, 0x0167 }, { 0x016B, 0x016B }, { 0x01CE, 0x01CE },
  { 0x01D0, 0x01D0 }, { 0x01D2, 0x01D2 }, { 0x01D4, 0x01D4 },
  { 0x01D6, 0x01D6 }, { 0x01D8, 0x01D8 }, { 0x01DA, 0x01DA },
  { 0x01DC, 0x01DC }, { 0x0251, 0x0251 }, { 0x0261, 0x0261 },
  { 0x02C4, 0x02C4 }, { 0x02C7, 0x02C7 }, { 0x02C9, 0x02CB },
  { 0x02CD, 0x02CD }, { 0x02D0, 0x02D0 }, { 0x02D8, 0x02DB },
  { 0x02DD, 0x02DD }, { 0x02DF, 0x02DF }, { 0x0391, 0x03A1 },
  { 0x03A3, 0x03A9 }, { 0x03B1, 0x03C1 }, { 0x03C3, 0x03C9 },
  { 0x0401, 0x0401 }, { 0x0410, 0x044F }, { 0x0451, 0x0451 },
  { 0x2010, 0x2010 }, { 0x2013, 0x2016 }, { 0x2018, 0x2019 },
  { 0x201C, 0x201D }, { 0x2020, 0x2022 }, { 0x2024, 0x2027 },
  { 0x2030, 0x2030 }, { 0x2032, 0x2033 }, { 0x2035, 0x2035 },
  { 0x203B, 0x203B }, { 0x203E, 0x203E }, { 0x2074, 0x2074 },
  { 0x207F, 0x207F }, { 0x2081, 0x2084 }, { 0x20AC, 0x20AC },
  { 0x2103, 0x2103 }, { 0x2105, 0x2105 }, { 0x2109, 0x2109 },
  { 0x2113, 0x2113 }, { 0x2116, 0x2116 }, { 0x2121, 0x2122 },
  { 0x2126, 0x2126 }, { 0x212B, 0x212B }, { 0x2153, 0x2154 },
  { 0x215B, 0x215E }, { 0x2160, 0x216B }, { 0x2170, 0x2179 },
  { 0x2189, 0x2189 }, { 0x2190, 0x2199 }, { 0x21B8, 0x21B9 },
  { 0x21D2, 0x21D2 }, { 0x21D4, 0x21D4 }, { 0x21E7, 0x21E7 },
  { 0x2200, 0x2200 }, { 0x2202, 0x2203 }, { 0x2207, 0x2208 },
  { 0x220B, 0x220B }, { 0x220F, 0x220F }, { 0x2211, 0x2211 },
  { 0x2215, 0x2215 }, { 0x221A, 0x221A }, { 0x221D, 0x2220 },
  { 0x2223, 0x2223 }, { 0x2225, 0x2225 }, { 0x2227, 0x222C },
  { 0x222E, 0x222E }, { 0x2234, 0x2237 }, { 0x223C, 0x223D },
  { 0x2248, 0x2248 }, { 0x224C, 0x224C }, { 0x2252, 0x2252 },
  { 0x2260, 0x2261 }, { 0x2264, 0x2267 }, { 0x226A, 0x226B },
  { 0x226E, 0x226F }, { 0x2282, 0x2283 }, { 0x2286, 0x2287 },
  { 0x2295, 0x2295 }, { 0x2299, 0x2299 }, { 0x22A5, 0x22A5 },
  { 0x22BF, 0x22BF }, { 0x2312, 0x2312 }, { 0x2460, 0x24E9 },
  { 0x24EB, 0x254B }, { 0x2550, 0x2573 }, { 0x2580, 0x258F },
  { 0x2592, 0x2595 }, { 0x25A0, 0x25A1 }, { 0x25A3, 0x25A9 },
  { 0x25B2, 0x25B3 }, { 0x25B6, 0x25B7 }, { 0x25BC, 0x25BD },
  { 0x25C0, 0x25C1 }, { 0x25C6, 0x25C8 }, { 0x25CB, 0x25CB },
  { 0x25CE, 0x25D1 }, { 0x25E2, 0x25E5 }, { 0x25EF, 0x25EF },
  { 0x2605, 0x2606 }, { 0x2609, 0x2609 }, { 0x260E, 0x260F },
  { 0x2614, 0x2615 }, { 0x261C, 0x261C }, { 0x261E, 0x261E },
  { 0x2640, 0x2640 }, { 0x2642, 0x2642 }, { 0x2660, 0x2661 },
  { 0x2663, 0x2665 }, { 0x2667, 0x266A }, { 0x266C, 0x266D },
  { 0x266F, 0x266F }, { 0x269E, 0x269F }, { 0x26BE, 0x26BF },
  { 0x26C4, 0x26CD }, { 0x26CF, 0x26E1 }, { 0x26E3, 0x26E3 },
  { 0x26E8, 0x26FF }, { 0x273D, 0x273D }, { 0x2757, 0x2757 },
  { 0x2776, 0x277F }, { 0x2B55, 0x2B59 }, { 0x3248, 0x324F },
  { 0xE000, 0xF8FF }, { 0xFFFD, 0xFFFD }, { 0x1F100, 0x1F10A },
  { 0x1F110, 0x1F12D }, { 0x1F131, 0x1F131 }, { 0x1F13D, 0x1F13D },
  { 0x1F13F, 0x1F13F }, { 0x1F142, 0x1F142 }, { 0x1F146, 0x1F146 },
  { 0x1F14A, 0x1F14E }, { 0x1F157, 0x1F157 }, { 0x1F15F, 0x1F15F },
  { 0x1F179, 0x1F179 }, { 0x1F17B, 0x1F17C }, { 0x1F17F, 0x1F17F },
  { 0x1F18A, 0x1F18D }, { 0x1F190, 0x1F190 }, { 0xF0000, 0xFFFFD },
  { 0x100000, 0x10FFFD }
};


#ifdef __TURBOC__
static struct interval
spacing_combining_300 [] = { {0, 0} };
static struct interval
assigned_300 [] = { {0, 0} };
#else
#include "width.t"
#endif


/*======================================================================*\
|	Width enquiry functions; and table setup
\*======================================================================*/

static int configured_combining_data_version = -1;

static
void
term_setup_data ()
{
#ifndef __TURBOC__
  if (combining_data_version >= U700) {
	combining_table = combining_700;
	combining_len = arrlen (combining_700);
	spacing_combining_table = spacing_combining_700;
	spacing_combining_len = arrlen (spacing_combining_700);
	assigned_table = assigned_700;
	assigned_len = arrlen (assigned_700);
  } else if (combining_data_version >= U630) {
	combining_table = combining_630;
	combining_len = arrlen (combining_630);
	spacing_combining_table = spacing_combining_630;
	spacing_combining_len = arrlen (spacing_combining_630);
	assigned_table = assigned_630;
	assigned_len = arrlen (assigned_630);
  } else if (combining_data_version >= U620) {
	combining_table = combining_620;
	combining_len = arrlen (combining_620);
	spacing_combining_table = spacing_combining_620;
	spacing_combining_len = arrlen (spacing_combining_620);
	assigned_table = assigned_620;
	assigned_len = arrlen (assigned_620);
  } else if (combining_data_version >= U600) {
	combining_table = combining_600;
	combining_len = arrlen (combining_600);
	spacing_combining_table = spacing_combining_600;
	spacing_combining_len = arrlen (spacing_combining_600);
	assigned_table = assigned_600;
	assigned_len = arrlen (assigned_600);
  } else if (combining_data_version >= U520) {
	combining_table = combining_520;
	combining_len = arrlen (combining_520);
	spacing_combining_table = spacing_combining_520;
	spacing_combining_len = arrlen (spacing_combining_520);
	assigned_table = assigned_520;
	assigned_len = arrlen (assigned_520);
  } else if (combining_data_version >= U510) {
	combining_table = combining_510;
	combining_len = arrlen (combining_510);
	spacing_combining_table = spacing_combining_510;
	spacing_combining_len = arrlen (spacing_combining_510);
	assigned_table = assigned_510;
	assigned_len = arrlen (assigned_510);
  } else if (combining_data_version >= U500) {
	combining_table = combining_500;
	combining_len = arrlen (combining_500);
	spacing_combining_table = spacing_combining_500;
	spacing_combining_len = arrlen (spacing_combining_500);
	assigned_table = assigned_500;
	assigned_len = arrlen (assigned_500);
  } else if (combining_data_version == U410) {
	combining_table = combining_410;
	combining_len = arrlen (combining_410);
	spacing_combining_table = spacing_combining_410;
	spacing_combining_len = arrlen (spacing_combining_410);
	assigned_table = assigned_410;
	assigned_len = arrlen (assigned_410);
  } else if (combining_data_version == U400) {
	combining_table = combining_400;
	combining_len = arrlen (combining_400);
	spacing_combining_table = spacing_combining_400;
	spacing_combining_len = arrlen (spacing_combining_400);
	assigned_table = assigned_400;
	assigned_len = arrlen (assigned_400);
  } else if (combining_data_version == U320) {
	combining_table = combining_320;
	combining_len = arrlen (combining_320);
	spacing_combining_table = spacing_combining_320;
	spacing_combining_len = arrlen (spacing_combining_320);
	assigned_table = assigned_320;
	assigned_len = arrlen (assigned_320);
  } else if (combining_data_version == U300) {
	combining_table = combining_300;
	combining_len = arrlen (combining_300);
	spacing_combining_table = spacing_combining_300;
	spacing_combining_len = arrlen (spacing_combining_300);
	assigned_table = assigned_300;
	assigned_len = arrlen (assigned_300);
  } else
#endif
  {
	combining_table = combining_old;
	combining_len = arrlen (combining_old);
	spacing_combining_table = spacing_combining_300;
	spacing_combining_len = arrlen (spacing_combining_300);
	assigned_table = assigned_300;
	assigned_len = arrlen (assigned_300);
  }
  configured_combining_data_version = combining_data_version;
}

static
int
iscombining_listed (ucs)
  unsigned long ucs;
{
  if (configured_combining_data_version != combining_data_version) {
	term_setup_data ();
  }

  if (printable_bidi_controls) {	/* since xterm 230 */
	/*
	‎ U+200E;LEFT-TO-RIGHT MARK
	‏ U+200F;RIGHT-TO-LEFT MARK
	‪ U+202A;LEFT-TO-RIGHT EMBEDDING
	‫ U+202B;RIGHT-TO-LEFT EMBEDDING
	‬ U+202C;POP DIRECTIONAL FORMATTING
	‭ U+202D;LEFT-TO-RIGHT OVERRIDE
	‮ U+202E;RIGHT-TO-LEFT OVERRIDE
	*/
	if (ucs >= 0x200E && ucs <= 0x202E && (ucs <= 0x200F || ucs >= 0x202A)) {
		return 0;
	}
  }

#ifdef wide_combining_not_combining
	/* workaround for bug in MinTTY 0.4 beta */
	if (ucs >= 0x302A && ucs <= 0x309A && (ucs <= 0x302F || ucs >= 0x3099)) {
		return 0;
	}
#endif

  if (lookup (ucs, combining_table, combining_len)
   || (spacing_combining && lookup (ucs, spacing_combining_table, spacing_combining_len))
     ) {
	if (ucs < 0x10000) {
		return 1;
	} else if (suppress_non_BMP) {
		return 0;
	} else if (ucs >= 0xE0000) {
		return plane_14_combining;
	} else {
		return plane_1_combining;
	}
  } else {
	return 0;
  }
}

/* Check whether a Unicode code is a valid (assigned) Unicode character.
 */
int
term_isassigned (ucs)
  unsigned long ucs;
{
  if (configured_combining_data_version != combining_data_version) {
	term_setup_data ();
  }

  if (lookup (ucs, assigned_table, assigned_len)) {
	return 1;
  } else {
	return 0;
  }
}

/* Check whether a Unicode character is a combining character, based on its 
   Unicode general category being Mn (Mark Nonspacing), Me (Mark Enclosing), 
   Cf (Other Format),
   or (depending on terminal features) Mc (Mark Spacing Combining).
 */
int
term_iscombining (ucs)
  unsigned long ucs;
{
  if (unassigned_single_width) {
	if (rxvt_version > 0) {
		/* handle weird mapping of non-Unicode ranges */
		if (ucs < 0x80000000) {
			ucs &= 0x1FFFFF;
		}
	}
	if (! bidi_screen) {
		/* special case of rxvt, not mlterm */
		if (ucs >= 0xF8F0 && ucs <= 0xF8FF) {
			return 1;
		}
	}
	return iscombining_listed (ucs) && term_isassigned (ucs);
  } else if (ucs >= 0xD7B0 && ucs <= 0xD7FF) {
	return hangul_jamo_extended;
  } else if (konsole_version > 0 && ucs >= 0x200000) {
	return 1;
  } else {
	return iscombining_listed (ucs);
  }
}

#define dont_debug_width
#define dont_debug_width_all

#ifdef debug_width
#include <stdio.h>
#define do_trace(c)	if (ucs == c) _do_trace = 1;
#define trace_width(tag, res)	if (_do_trace) printf ("iswide (%04lX) [%s]: %d\n", ucs, tag, res);
#else
#define trace_width(tag, res)	
#endif

/* Check whether a Unicode character is a wide character, based on its 
   Unicode category being East Asian Wide (W) or East Asian FullWidth (F) 
   as defined in Unicode Technical Report #11, East Asian Ambiguous (A) 
   if the terminal is running in CJK compatibility mode (xterm -cjk_width).
   Data taken from different versions of
   http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */
int
term_iswide (ucs)
  unsigned long ucs;
{
#ifdef debug_width
  int _do_trace = 0;
#ifdef debug_width_all
  if (ucs >= 0x80) {
	_do_trace = 1;
  }
#endif

  do_trace (0x4DC0);
  do_trace (0xFE19);
  do_trace (0x232A);
  do_trace (0x3099);
  do_trace (0x302A);
#endif

  if (width_data_version == 0) {
	trace_width ("none", 0);
	return 0;
  }

  if (ucs >= 0x4DC0 && ucs <= 0x4DFF) {
	trace_width ("hexagram", wide_Yijing_hexagrams);
	return wide_Yijing_hexagrams;
  }

  if (unassigned_single_width) {
	if (rxvt_version > 0) {
		/* handle weird mapping of non-Unicode ranges */
		if (ucs < 0x80000000) {
			ucs &= 0x1FFFFF;
		}
	}
	if (! term_isassigned (ucs)) {
		trace_width ("unassigned_single_width", 0);
		return 0;
	}
  }

  if (cjk_currency_width == 2) {
	if (ucs == 0xA2 || ucs == 0xA3 || ucs == 0xA5) {
		return 1;
	}
  }

  /* handle xterm -cjk_width */
  if (cjk_width_data_version) {
    if (utf8_screen || ucs >= 0x80 || cjk_wide_latin1) {
	/* look up ambiguous character */

	if (cjk_width_data_version >= U520) {
		if (lookup (ucs, ambiguous_520, arrlen (ambiguous_520))) {
			trace_width ("cjk 520", 1);
			return 1;
		}
	} else if (cjk_width_data_version >= U400) {
		if (lookup (ucs, ambiguous_400, arrlen (ambiguous_400))) {
			trace_width ("cjk 400", 1);
			return 1;
		}
	} else if (cjk_width_data_version >= U320beta) {
		if (lookup (ucs, ambiguous_old, arrlen (ambiguous_old))) {
			trace_width ("cjk 320", 1);
			return 1;
		}
	}
    }

    if (cjk_width_data_version == U300beta
        && (lookup (ucs, wide_poderosa, arrlen (wide_poderosa))
            || ucs >= 0x100
           )
       )
    {
      trace_width ("wide poderosa", 1);
      return 1;
    }

    /* Surrogates are also displayed wide by xterm -cjk_width */
    if (ucs >= 0xD800 && ucs <= 0xDFFF) {
      trace_width ("cjk surrogates", 1);
      return 1;
    }

    /* Non-BMP are also displayed wide by xterm -cjk_width */
    if (ucs >= 0x10000 && plane_2_double_width) {
      trace_width ("cjk non-bmp", 1);
      return 1;
    }
  }

  /* first quick check for Latin-1 etc. characters */
  if (ucs < 0x1100) {
      trace_width ("low", 0);
      return 0;
  }

  if (bidi_screen && mintty_version <= 0) {
    /* handle mlterm deviations */
    if (ucs == 0x2329 || ucs == 0x232A || /* angle brackets */
        (ucs >= 0xA000 && ucs <= 0xA4C6) /* Yi */
       ) {
      trace_width ("bidi", 0);
      return 0;
    }
  }

  if (width_data_version <= U300) {
   trace_width ("<=300", -1);
   return
    /* wide character ranges
       Unicode 3.0
       wcwidth 2001-01-12
       xterm 157...166
     */
    (ucs >= 0x1100 && ucs <= 0x115f) || /* Hangul Jamo */
    (ucs >= 0x2e80 && ucs <= 0xa4cf 
     && (ucs & (unsigned long) ~0x0011) != 0x300a
     && ucs != 0x303f) ||                  /* CJK ... Yi */
    (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
    (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
    (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
    (ucs >= 0xff00 && ucs <= 0xff5f) || /* Fullwidth Forms */
    (ucs >= 0xffe0 && ucs <= 0xffe6)
    || (ucs >= 0x20000 && ucs <= 0x2ffff && plane_2_double_width) /* missing before xterm 157 */
    ;
  } else {
   trace_width (">300", -1);
   return
    /* wide character ranges
       Unicode 3.2 - without 0x3???? range:
        wcwidth 2002-05-08
        xterm 167...179
       Unicode 4.0 - including 0x3???? range:
        wcwidth 2003-05-20
        xterm 180...225
     */
    /* wide character ranges
       Unicode 4.1
        wcwidth 2007-05-25
        xterm 226...
     */
    (ucs >= 0x1100 &&
     (ucs <= 0x115f ||                  /* Hangul Jamo init. consonants */
      ucs == 0x2329 || ucs == 0x232a || /* angle brackets */
      (ucs >= 0x2e80 && ucs <= 0xa4cf && ucs != 0x303f) || /* CJK ... Yi */
      (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
      (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
      (width_data_version >= U410 && ucs >= 0xfe10 && ucs <= 0xfe19) || /* Vertical forms */
      (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
      (ucs >= 0xff00 && ucs <= 0xff60) || /* Fullwidth Forms */
      (ucs >= 0xffe0 && ucs <= 0xffe6) ||
      (width_data_version >= U520 && ucs >= 0xA960 && ucs <= 0xA97F) || /* Hangul Jamo Extended-A */
      (width_data_version >= U600 && ucs >= 0x1B000 && ucs <= 0x1B0FF) || /* Kana Supplement */
      (width_data_version >= U520 && ucs >= 0x1F200 && ucs <= 0x1F2FF) || /* Enclosed Ideographic Supplement */
      (plane_2_double_width && ucs >= 0x20000 && ucs <= 0x3ffff)
      ));
  }
}


/*======================================================================*\
|	End
\*======================================================================*/
