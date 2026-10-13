
/* esh_adapt.c */

#include "esh.h"
#include "esh_adapt.h"
#include "file.h"
#include "tty_adapt.h"

void eshinit(void)
{
	file_system_init();
	ttyinit();
	prompt();
}

/* EOF */

