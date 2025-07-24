
/* shell.h */

#ifndef __SHELL__
#define __SHELL__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "machdep.h"
#include "eshdb.h"
#include "esh_adapt.h"
#include "stdio_adapt.h"


struct applet {
	const char *name;
	int (* const main)(int argc, char **argv);
};

extern const struct applet usercmds[];
extern const struct applet builtins[];
extern void (* const tasks[])(void);
extern void (* const helps[])(void);
extern struct applet *find_applet(char *name, const struct applet *ap, int *k);
extern int run_applet(int argc, char **argv);
extern void run_tasks(void);
extern void ttyinit(void);
extern void ttytask(int c);
extern void eshinit(void);
extern void esh(void);

extern int return_from_applet;


#define PROTOTYPES
#include "ucmd.h"
#undef PROTOTYPES

#define PROTOTYPES
#include "bltn.h"
#undef PROTOTYPES


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
