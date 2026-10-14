
/* esh_adapt.c */

#include "buf.h"
#include "esh.h"
#include "esh_adapt.h"
#include "file.h"
#include "tty_adapt.h"

void eshinit(void)
{
	binit();
	finit();
	ttyinit();
	prompt();
}

/* EOF */

