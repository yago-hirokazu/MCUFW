/* script.c */

/**************************************************************************
 * PROJECT-SPECIFIC INCLUDE FILES
 **************************************************************************/
#include "esh.h"
#include "esh_errno.h"
#include "exe.h"
#include "lex.h"
#include "par.h"
#include "script.h"

extern struct parse parse[MAXNESTS];

/**************************************************************************
 * LOCAL VARIABLE DECLARATIONS
 **************************************************************************/
/* Parser and lexer remain singleton; no nested console evaluation */
static struct code script_code;
static struct exe script_exe;

/**************************************************************************
 * FUNCTION DEFINITIONS
 **************************************************************************/
void esh_script_init(void)
{
	code_init(&script_code);
	memset(&script_exe, 0, sizeof(script_exe));
	par_init();
	lex_init();
}

int esh_script_line(char *line)
{
	struct variable_table variables;
	struct tty_arg args;
	struct token token;
	int status;
	int i;

	memset(&args, 0, sizeof(args));
	if (!tokenize(line, &args))
		goto error;
	if (!args.c || !is_endofline(args.v[args.c - 1][0]))
		goto error;
	lex_init();
	status = LEXDONE;
	for (i = 0; i < args.c; i++) {
		status = lex_analyze_token(args.v[i], &token);
		if (status == LEXQUOTE)
			continue;
		if (status == LEXERROR ||
		    par_make_code(&token, &script_code) == PARINVLD)
			goto error;
	}
	if (status == LEXQUOTE)
		goto error;
	if (parse[0].p_state1 != PAR_IDLE && parse[0].p_state1 != PAR_HALT)
		return 1;
	while (script_exe.e_pc < script_code.c_codes)
		if (exe_run_codes(&script_code, &script_exe))
			goto error;
	status = script_exe.e_status;
	variables = script_code.c_vtab;
	esh_script_init();
	script_code.c_vtab = variables;
	return status;
error:
	variables = script_code.c_vtab;
	esh_script_init();
	script_code.c_vtab = variables;
	return -ESH_EINVAL;
}
