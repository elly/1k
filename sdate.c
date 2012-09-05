/* sdate.c */

#include <stdio.h>
#include <time.h>

int main(void) {
	time_t t;
	struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);

	printf("%u.%03u.%u\n", 1900 + tm->tm_year, tm->tm_yday,
	       tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
	return 0;
}
