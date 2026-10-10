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
 * LOCAL FUNCTION PROTOTYPES
 **************************************************************************/
static void script_reset_preserving_variables(void);

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
	struct tty_arg args;
	struct token token;
	int lex_status;
	int result;
	int i;

	memset(&args, 0, sizeof(args));
	if (!tokenize(line, &args))
		goto error;
	if (!args.c || !is_endofline(args.v[args.c - 1][0]))
		goto error;
	lex_init();
	lex_status = LEXDONE;
	for (i = 0; i < args.c; i++) {
		lex_status = lex_analyze_token(args.v[i], &token);
		if (lex_status == LEXQUOTE)
			continue;
		if (lex_status == LEXERROR ||
		    par_make_code(&token, &script_code) == PARINVLD)
			goto error;
	}
	if (lex_status == LEXQUOTE)
		goto error;
	if (parse[0].p_state1 != PAR_IDLE && parse[0].p_state1 != PAR_HALT)
		return 1;
	/* Execute before reset so break can read parser loop metadata */
	while (script_exe.e_pc < script_code.c_codes)
		if (exe_run_codes(&script_code, &script_exe))
			goto error;
	result = script_exe.e_status;
	goto out;
error:
	result = -ESH_EINVAL;
out:
	script_reset_preserving_variables();
	return result;
}

static void script_reset_preserving_variables(void)
{
	struct variable_table variables;

	variables = script_code.c_vtab;
	esh_script_init();
	script_code.c_vtab = variables;
}
