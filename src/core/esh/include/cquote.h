
/* cquote.h */

#ifndef __CQUOTE_H__
#define __CQUOTE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "esh_adapt.h"

int is_quotestart(char *str, char quote);
int is_quoteend(char *str, char quote, int *ctrace);
int is_closequote(char *str, char quote, int *ctrace);
int qcatargv(char quote, int *argc, char **argV);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
