
/* retarget.c */

#include <rt_misc.h>

#pragma import(__use_no_semihosting_swi)

void _ttywrch(int ch) {
  // Not used (No Output)
}

void _sys_exit(int return_code) {
label:  goto label;  /* endless loop */
}

/* EOF */

