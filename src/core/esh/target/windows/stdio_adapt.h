
/* stdio_adapt.h */

#ifndef __STDIO_ADAPT_H__
#define __STDIO_ADAPT_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


extern int win_getchar(void);
extern int win_putchar(int c);
extern int win_printf(const char *fmt, ...);
extern int win_scanf(char *fmt, ...);

#undef getchar
#undef putchar
#undef printf
#undef scanf
#define getchar win_getchar
#define putchar win_putchar
#define printf  win_printf
#define scanf   win_scanf


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
