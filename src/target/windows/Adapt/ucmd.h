

/* ucmd.h */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#undef UCMD
#undef TASK
#undef HELP

/* Applet */

#if defined(PROTOTYPES)

#define UCMD(name) extern int name##_main(int argc, char **argv);

#else

const struct applet usercmds[] = {
#define UCMD(name) {#name, name##_main},

#endif

	UCMD(sam1)
	UCMD(sam2)
	UCMD(sam3)

#if !defined(PROTOTYPES)
	{NULL, NULL}
};
#endif

/* Task */

#if defined(PROTOTYPES)

#define TASK(name) extern void task_##name(void);

#else

void (* const tasks[])(void) = {
#define TASK(name) task_##name,

#endif

	/* add here */

#if !defined(PROTOTYPES)
	NULL
};
#endif

/* Help */

#if defined(PROTOTYPES)

#define HELP(name) extern void help_##name(void);

#else

void (* const helps[])(void) = {
#define HELP(name) help_##name,

#endif

	HELP(sam)

#if !defined(PROTOTYPES)
	NULL
};
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
