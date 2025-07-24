
/* bltn.h */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "esh_adapt.h"

#undef BLTN

#if defined(PROTOTYPES)

extern const char C_HELP;
#if USE_STACKMACH
extern const char C_TEST;
extern const char C_EXPR;
extern const char C_ECHO;
#endif

#define BLTN(name) extern int name(int argc, char **argv);

#else

const char C_HELP = 0;
#if USE_STACKMACH
const char C_TEST = 1;
const char C_EXPR = 2;
const char C_ECHO = 3;
#endif

const struct applet builtins[] = {
#define BLTN(name) {#name, name},

#endif

	BLTN(help)
#if USE_STACKMACH
	BLTN(test)
	BLTN(expr)
	BLTN(echo)
#endif

#if !defined(PROTOTYPES)
	{NULL, NULL}
};
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */
