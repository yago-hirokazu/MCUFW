
/* esh_adapt.c */

#include "esh.h"
#include "esh_adapt.h"
#include "tty_adapt.h"

void eshinit(void)
{
	ttyinit();
	prompt();
}

/* EOF */

