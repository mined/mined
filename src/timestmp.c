#if defined (__DATE__) && defined (__TIME__)

char __date__ [] = __DATE__;
char * __time__ = __TIME__;

#include <stdio.h>

char *
buildstamp ()
{
	static char bs [21];

	__date__ [6] = '\0';
	__date__ [3] = '-';
	if (__date__ [4] == ' ') {
		__date__ [4] = '0';
	}
	sprintf (bs, "%s-%s %s", & __date__ [7], __date__, __time__);
	return bs;
}

#else

char *
buildstamp ()
{
	return 0;
}

#endif
