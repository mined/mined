/**
   These functions try to determine from environment variables 
   the user's configuration for terminal encoding as well as 
   text encoding preferences.
   It first finds the relevant locale environment variable, then inspects it.
   For the locale environment, it is the first one in the sequence 
   LC_ALL, LC_CTYPE, LANG that has a non-empty value; this is used for 
   detection of terminal encoding.
   For text encoding preferences, the mined-defined application-specific 
   variable TEXTLANG takes precedence over the others.

   If no explicit encoding suffix can be found, the following substitute 
   information is returned:
   * a variation (starting with "@") if present, otherwise:
   * the country code (separated with "_") (now obsolete).
   This information can be used as a substitute indication of the 
   encoding likely to be desired.
   For clear indication, the beginning "@" or "_" character is included 
   in the return value in these cases.

   In either case, the language code (with appended country code and 
   encoding if present) and the country code (with leading "_" and 
   appended encoding if present) are left in the global variables 
   language_code and country_code as a secondary indication of the 
   desired encoding.
   This enables the application a distinction if the suffix is just 
   ".euc" or ".EUC" which is ambiguous.
   It also facilitates common recognition of Arabic.

   Examples:
   Value of locale variable (checked in order as defined by entry function)
   		Encoding indication returned
   			country_code
   en_US.UTF-8	UTF-8	_US.UTF-8
   ja_JP.eucjp	eucjp	_JP.eucjp
   vi_VN.tcvn	tcvn	_VN.tcvn
   de_DE@euro	@euro	_DE@euro
   th_TH	_TH	_TH
   zh_TW.EUC	EUC	_TW.EUC
*/

#include "encoding.h"

#include <string.h>
#include <stdlib.h>

extern char * getenv ();

char * language_code;
char * country_code;
int language_preference = 0;


static
char *
locale_encoding (var1, var2, var3)
  char * var1;
  char * var2;
  char * var3;
{
  char * locale = getenv (var1);
  if (! locale || ! * locale) {
	locale = getenv (var2);
	if (! locale || ! * locale) {
		locale = getenv (var3);
	}
  }

  if (locale) {
	char * encoding;

	language_code = locale;

	country_code = (char *) strchr (locale, '_');
	if (! country_code) {
		country_code = "";
	}

	encoding = (char *) strchr (locale, '.');
	if (encoding) {
		/* ".UTF-8" */
		encoding ++;
		/* "UTF-8" */
	} else {
		encoding = (char *) strchr (locale, '@');
		if (encoding) {
			/* "@euro" */
		} else {
			/* "_CC" */
			encoding = "";
		}
	}
	return encoding;
  } else {
	language_code = "";
	country_code = "";

	return "";
  }
}

char *
locale_terminal_encoding ()
{
  return locale_encoding ("LC_ALL", "LC_CTYPE", "LANG");
}

char *
locale_text_encoding ()
{
/*
  return locale_encoding ("LANGUAGE", "TEXTLANG", "LC_ALL", "LC_CTYPE", "LANG");
*/
  char * ret = locale_encoding ("LC_ALL", "LC_CTYPE", "LANG");

  char * lang = getenv ("LANGUAGE");
  if (! lang || ! * lang) {
	lang = getenv ("TEXTLANG");
  }
  if (lang && * lang) {
	char * encoding;

	language_code = lang;

	country_code = (char *) strchr (lang, '_');
	if (! country_code) {
		country_code = "";
	}

	language_preference = 1;

	encoding = (char *) strchr (lang, '.');
	if (encoding) {
		/* ".UTF-8" */
		encoding ++;
		ret = encoding;
	}
  }

  return ret;
}

