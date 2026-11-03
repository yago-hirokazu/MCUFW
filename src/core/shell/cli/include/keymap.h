
/* keymap.h */

#ifndef __KEYMAP__
#define __KEYMAP__

#define ASCII_NUL ((char)0x00)
#define ASCII_EOT ((char)0x04)
#if defined(__CYGWIN__)
#define ASCII_BS ((char)0x7f)
#else
#define ASCII_BS ((char)0x08)
#endif
#define ASCII_HT ((char)0x09)
#define ASCII_ESC ((char)0x1b)
#if !defined(__CYGWIN__)
#define ASCII_DEL ((char)0x7f)
#endif

/*
 * cygwin
 * BS : 0x7f
 * DEL: 0x1b 0x5b 0x33 0x7e
 */

#endif
