
/* code.c */

#include "code.h"

void code_init(struct code *code)
{
	int k;

	memset(code, 0, sizeof(struct code));

	for (k=0; k<SYMBOLS; k++) {
		code->c_vtab.v_var[k * SYMCHARS] = '\n';
		code->c_vtab.v_val[k * SYMCHARS] = '\n';
		code->c_stab.s_str[k * SYMCHARS] = '\n';
	}

	return;
}

/* EOF */

