
/* symchar.h */

#ifndef __SYMCHAR_H__
#define __SYMCHAR_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"

int symncmp(const char *s1, const char *s2, unsigned int N, char delim);
char *symlf(char *sym);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
