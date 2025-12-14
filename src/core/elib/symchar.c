
/* symchar.c */

#include "symchar.h"

int symncmp(const char *s1, const char *s2, unsigned int N, char delim)
{
	if (N == 0)
		return 0;
	do {
		if (*s1 != *s2)
			return (*(unsigned char *)s1 - *(unsigned char *)s2);
		if (*s1 == delim)
			break;
		s1++;
		s2++;
	} while (--N != 0);
	return 0;
}

char *symlf(char *sym)
{
	if (strncmp(sym, "\n", 1) == 0)
		return (char *)"\n";
	return sym;
}

/* EOF */

