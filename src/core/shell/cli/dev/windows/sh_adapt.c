
/* sh_adapt.c */

#include "buf.h"
#include "sh.h"
#include "sh_adapt.h"
#include "file.h"
#include "ramdisk.h"
#include "tty_adapt.h"

void shinit(void)
{
	/* Cold start only; no live users or unsaved dirty buffers */
	init_ramdisk();
	init_bufc();
	init_fsys();
	init_tty();
	prompt();
}

/* EOF */

