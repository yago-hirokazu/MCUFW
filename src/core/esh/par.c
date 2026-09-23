
/* par.c */

#include "esh.h"
#include "esh_debug.h"
#include "par.h"


#define EP(fmt, ...)\
do {\
	fprintf(stdout, "error [%s|%s()|L%d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)

#if DEBUG_PAR
#define DP(fmt, ...)\
do {\
	fprintf(stdout, "debug [%s|%s()|L%d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)
#define P printf
#define DUMPCX dump_context
#else
#define DP(...)
#define P(...)
#define DUMPCX(...)
#endif


#define NO_SYMBOL -1
#define NULL_SYMBOL -2

struct parse parse[MAXNESTS];
int nest_p, maxnests;
int loop_at[MAXNESTS], loops;
int cond_at[MAXNESTS], conds;


static void add_code(int byte, struct code *code);
static char find_symbol(char *sym, char sym_tab[]);
static char make_symbol(char *sym, char sym_tab[], char *sym_num);
extern void set_symbol_to_variable(char var_p, char *sym, struct code *code);
extern char *get_symbol_from_variable(char var_p, struct code *code);
static char make_label(struct code *code);
static char set_code_addr_to_label(char label_p, int code_addr, struct code *code);
extern char *get_string(char id, struct code *code);
static int find_cmd(struct token *tok, const struct applet *cmd);

static char parse_cmdarg(struct token *token, struct code *code);
static char parse_cmdline(struct token *token, struct code *code);

static char _par_make_code(struct parse *par, struct token *tok, struct code *code);
static char parse_idle(struct parse *par, struct token *tok, struct code *code);
static char parse_assign(struct parse *par, struct token *tok, struct code *code);
static char parse_while(struct parse *par, struct token *tok, struct code *code);
static char parse_if(struct parse *par, struct token *tok, struct code *code);

static void nest_enter(int *nest_p, int *maxnests);
static void nest_leave(int *nest_p, int *maxnests);
static void nest_restore_if_needed(int *nest_p, int maxnests);

#if !TODO_BREAK_WITH_ARGUMENT
static int break_label(struct parse *par, int *loop_at, int loop_p);
#endif

static void dump_context(const char *s, struct token *tok);


char par_make_code(struct token *tok, struct code *code)
{
	if (parse[0].p_state1 == PAR_HALT) {
		DP("parse[nest_p=%d].p_state1 == PAR_HALT",nest_p);
		memset(&parse[0], 0, sizeof(struct parse));
		parse[0].p_state1 = PAR_IDLE;
	}
	nest_p = 0;
	return _par_make_code(&parse[nest_p], tok, code);
}

static void add_code(int byte, struct code *code)
{
	int c = code->c_codes;
	code->c_code[c++] = byte;
	code->c_codes = c;
}

static char find_symbol(char *sym, char sym_tab[])
{
	char k;

	if (*sym == 0)
		return NULL_SYMBOL;

	for (k=0; k<SYMBOLS; k++) {
		char ch = sym_tab[k * SYMCHARS];
		char *str = &sym_tab[k * SYMCHARS];

		P("sym_tab[%d * SYMCHARS]=\'%c\' (%d)\n", k, ch, ch);
		P("sym_tab[%d * SYMCHARS]=\"%s\"\n",
		  k, (ch == 0) ? "(null)" : str);

		if (ch == 0)
			continue;

		if (0 == strncmp(str, sym, SYMCHARS))
			return k;
	}

	return NO_SYMBOL;
}

static char make_symbol(char *sym, char sym_tab[], char *sym_num)
{
	char n1 = *sym_num;
	char n2 = n1;

	assert(sym != NULL);
	assert(*sym != 0);
	assert(sym_num != NULL);

	P("sym = \"%s\"\n", (*sym == 0) ? "(null)" : sym);
	P("*sym_num = %d\n", *sym_num);

	memset((void *)&sym_tab[n1 * SYMCHARS], '\0', SYMCHARS);
	strncpy(&sym_tab[n1 * SYMCHARS], sym, SYMCHARS);

	P("sym_tab[%d * SYMCHARS]=\'%c\' (%d)\n",
	  n1, sym_tab[n1 * SYMCHARS], sym_tab[n1 * SYMCHARS]);
	P("sym_tab[%d * SYMCHARS]=\"%s\"\n",
	  n1,
	  (sym_tab[n1 * SYMCHARS] == 0) ? "(null)" : &sym_tab[n1 * SYMCHARS]);

	n1++;
	*sym_num = n1;
	return n2;
}

extern void set_symbol_to_variable(char var_p, char *sym, struct code *code)
{
	struct variable_table *v = &code->c_vtab;
	strncpy(&v->v_val[var_p * SYMCHARS], sym, SYMCHARS);
}

extern char *get_symbol_from_variable(char var_p, struct code *code)
{
	struct variable_table *v = &code->c_vtab;
	return (char *)&v->v_val[var_p * SYMCHARS];
}

static char make_label(struct code *code)
{
	char label_p = code->c_ltab.l_num;
	DP("make label = %3d", label_p);
	code->c_ltab.l_num++;
	return label_p;
}

static char set_code_addr_to_label(char label_p, int code_addr, struct code *code)
{
	DP("l_addr[%d] = %3d", label_p, code_addr);
	code->c_ltab.l_addr[label_p] = code_addr;
}

extern char *get_string(char id, struct code *code)
{
	if (id >= SYMBOLS)
		return NULL;
	return (char *)&code->c_stab.s_str[id * SYMCHARS];
}

static int find_cmd(struct token *tok, const struct applet *cmd)
{
	int k=0;
	struct applet *ap = find_applet(tok->t_symbol, cmd, &k);
	if (ap == NULL || ap->name == NULL) {
		DP("ap = 0x%08X", (uint32_t)ap);
		DP("ap->name = 0x%08X", (uint32_t)ap->name);
		return -1;
	}
	return k;
}

static char
parse_cmdarg(struct token *tok, struct code *code)
{
	DUMPCX(__func__, tok);

	if (tok->t_type == T_CMD_ARG) {
		char strid = find_symbol(tok->t_symbol, code->c_stab.s_str);
		DP("Str: %d = find_symbol(%s,...)",strid,
		   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
		if (strid == NO_SYMBOL) {
			strid = make_symbol(tok->t_symbol, code->c_stab.s_str, &(code->c_stab.s_num));
			DP("Str: %d = make_symbol(%s,...)",strid,
			   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
		}
		add_code(OP_CMD_ARG, code);
		add_code(strid, code);
		return PARONGO;
	} else if (tok->t_type == T_CMD_VARG) {
		char varid = find_symbol(tok->t_symbol, code->c_vtab.v_var);
		DP("Var: %d = find_symbol(%s,...)=%d",varid,
		   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
		if (varid == NO_SYMBOL) {
			varid = make_symbol(tok->t_symbol, code->c_vtab.v_var, &(code->c_vtab.v_num));
			DP("Var: %d = make_symbol(%s,...)=%d",varid,
			   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
		}
		add_code(OP_CMD_VARG, code);
		add_code(varid, code);
		return PARONGO;
	}
	return PARINVLD;
}

static char
parse_cmdline(struct token *tok, struct code *code)
{
	DUMPCX(__func__, tok);

	int ret = 0;

	if (tok->t_type == T_USER_CMD) {
		ret = find_cmd(tok, usercmds);
		DP("%d = find_cmd(...)", ret);
		if (ret < 0)
			goto error;
		add_code(OP_USER_CMD, code);
		add_code(ret, code);
		return PARONGO;
	} else if (tok->t_type == T_BLTN_CMD) {
		ret = find_cmd(tok, builtins);
		DP("%d = find_cmd(...)", ret);
		if (ret < 0)
			goto error;
		add_code(OP_BLTN_CMD, code);
		add_code(ret, code);
		if (0 == strcmp(tok->t_symbol, "break")) {
			add_code(OP_CMD_ARG, code);
			add_code(loops-1, code);
		}
		return PARONGO;
	} else if (tok->t_type == T_CMD_ARG || tok->t_type == T_CMD_VARG) {
		ret = parse_cmdarg(tok, code);
		DP("%d = parse_cmdarg(...)", ret);
		return ret;
	} else if (tok->t_type == T_EOL) {
		add_code(OP_CMD_EXE, code);
		return PARDONE;
	}

error:
	return PARINVLD;
}

static char
_par_make_code(struct parse *par, struct token *tok, struct code *code)
{
	DUMPCX(__func__, tok);

	char ret = PARONGO;

	DP("par=0x%08X", par);
	DP("par->p_state1=%s(%d)",par_state1_str[par->p_state1],par->p_state1);
	switch (par->p_state1) {
	case PAR_HALT:
		DP("par->p_state1 = PAR_HALT");
		par->p_state1 = PAR_IDLE;
		ret = PARONGO;
		/* break; */
	case PAR_IDLE:
		DP("par->p_state1 = PAR_IDLE");
		ret = parse_idle(par, tok, code);
		DP("%d = parse_idle(...)", ret);
		break;
	case PAR_CMDLINE:
		ret = parse_cmdline(tok, code);
		DP("%d = parse_cmdline(...)", ret);
		if (ret == PARDONE) {
			par->p_state1 = PAR_HALT;
			par->p_state2 = PAR_UNDEF;
		}
		break;
	case PAR_ASSIGN:
		ret = parse_assign(par, tok, code);
		DP("%d = parse_assign(...)", ret);
		break;
	case PAR_WHILE:
		ret = parse_while(par, tok, code);
		DP("%d = parse_while(...)", ret);
		break;
	case PAR_IF:
		ret = parse_if(par, tok, code);
		DP("%d = parse_if(...)", ret);
		break;
	default:
		break;
	}

	return ret;
}

static char
parse_idle(struct parse *par, struct token *tok, struct code *code)
{
	DUMPCX(__func__, tok);

	char ret=0, varid=0;
	switch (tok->t_type) {
	case T_USER_CMD:
	case T_BLTN_CMD:
		ret = parse_cmdline(tok, code);
		DP("%d = parse_cmdline(...)",ret);
		if (ret == PARINVLD)
			return PARINVLD;
		par->p_state1 = PAR_CMDLINE;
		par->p_state2 = PAR_CMDLINE_ARG;
		return PARONGO;
	case T_VARIABLE:
		varid = find_symbol(tok->t_symbol, code->c_vtab.v_var);
		DP("Var: %d = find_symbol(%s,...)", varid,
		   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
		if (varid == NO_SYMBOL) {
			varid = make_symbol(tok->t_symbol, code->c_vtab.v_var, &(code->c_vtab.v_num));
			DP("Var: %d = make_symbol(%s,...)", varid,
			   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
		}
		par->p_varid = varid;
		par->p_state1 = PAR_ASSIGN;
		par->p_state2 = PAR_ASSIGN_EQUAL;
		return PARONGO;
	case T_WHILE:
		DP("loop_at[loops=%d] = nest_p = %d",loops,nest_p);
		loop_at[loops] = nest_p;
		loops++;
		par->p_state1 = PAR_WHILE;
		par->p_state2 = PAR_WHILE_LBRACKET;
		return PARONGO;
	case T_IF:
		DP("cond_at[conds=%d] = nest_p = %d",conds,nest_p);
		cond_at[conds] = nest_p;
		conds++;
		par->p_label.l_end = make_label(code);
		par->p_state1 = PAR_IF;
		par->p_state2 = PAR_IF_LBRACKET;
		return PARONGO;
#if !TODO_BREAK_WITH_ARGUMENT
	case T_BREAK:
		DP("break statement");

		int label = break_label(&parse[0], &loop_at[0], loops-1);
		add_code(OP_BREAK_LOOP, code);
		add_code(label, code);
		return PARDONE;
#endif
	case T_EOL:
	case T_COMMENT:
		return PARDONE;
	default:
		break;
	}

	return PARINVLD;
}

static char
parse_assign(struct parse *par, struct token *tok, struct code *code)
{
	DUMPCX(__func__, tok);

	switch (par->p_state2) {
	case PAR_ASSIGN_EQUAL:
		if (0 == strcmp(tok->t_symbol, "=")) {
			par->p_state2 = PAR_ASSIGN_SYMBOL;
			return PARONGO;
		}
		goto error;
	case PAR_ASSIGN_SYMBOL:
		DP("PAR_ASSIGN_SYMBOL w/ %s",token_type_str[tok->t_type]);
		if (tok->t_type == T_VALUE ||
		    tok->t_type == T_STRING
			) {
			int id=0;
			id = find_symbol(tok->t_symbol, code->c_stab.s_str);
			DP("Str: %d = find_symbol(%s,...)",id,
			   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
			if (id == NO_SYMBOL) {
				id = make_symbol(tok->t_symbol, code->c_stab.s_str, &(code->c_stab.s_num));
				DP("Str: %d = make_symbol(%s,...)",id,
				   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
			}
			add_code(OP_PUSH_SYMBOL, code);
			add_code(id, code);
			add_code(OP_POP_SYMBOL_THEN_ASSIGN, code);
			add_code(par->p_varid, code);
			par->p_state1 = PAR_HALT;
			par->p_state2 = PAR_UNDEF;
			return PARDONE;
		}
		else if (tok->t_type == T_LBACKQUOTE) {
			par->p_state2 = PAR_ASSIGN_CMDRESULT;
			return PARONGO;
		}
		goto error;
	case PAR_ASSIGN_CMDRESULT:
		if (tok->t_type == T_RBACKQUOTE) {
			DP("tok->t_type == T_RBACKQUOTE");
			add_code(OP_CMD_EXE_THEN_ASSIGN, code);
			add_code(par->p_varid, code);
			par->p_state1 = PAR_HALT;
			par->p_state2 = PAR_UNDEF;
			return PARDONE;
		}

		int ret = parse_cmdline(tok, code);
		DP("%d = parse_cmdline(tok, code)", ret);
		if (ret != PARINVLD)
			return PARONGO;
		goto error;
	default:
		break;
	}

error:
	par->p_state1 = PAR_HALT;
	par->p_state2 = PAR_UNDEF;
	return PARINVLD;
}

static char
parse_while(struct parse *par, struct token *tok, struct code *code)
{
	DUMPCX(__func__, tok);

	int caddr=0;
	char label=0, status=PARONGO;

	switch (par->p_state2) {
	case PAR_WHILE_LBRACKET:
		if (tok->t_type != T_LBRACKET) {
			EP("while statement syntax error: no [");
			goto error;
		}
		label = make_label(code);
		caddr = code->c_codes;
		set_code_addr_to_label(label, caddr, code);
		par->p_label.l_loop = label;
		add_code(OP_BLTN_CMD, code);
		add_code(C_TEST, code);
		par->p_state2 = PAR_WHILE_CONDITION;
		return PARONGO;
	case PAR_WHILE_CONDITION:
		if (tok->t_type == T_RBRACKET) {
			add_code(OP_CMD_EXE_THEN_STACK, code);
			par->p_state2 = PAR_WHILE_EOL;
			return PARONGO;
		}
		else if (parse_cmdarg(tok, code) == PARINVLD) {
			EP("while statement syntax error: invalid condition statement");
			goto error;
		}
		else if (tok->t_type == T_EOL) {
			EP("while statement syntax error: no ]");
			goto error;
		}
		return PARONGO;
	case PAR_WHILE_EOL:
		if (tok->t_type != T_EOL && tok->t_type != T_SEMICOLON) {
			EP("while statement syntax error: no semicolon");
			goto error;
		}
		par->p_state2 = PAR_WHILE_DO;
		return PARONGO;
	case PAR_WHILE_DO:
		if (0 != strcmp(tok->t_symbol, "do")) {
			EP("while statement syntax error: no do");
			goto error;
		}
		if (nest_p >= MAXNESTS) {
			EP("Exceed max nests");
			goto error;
		}
		label = make_label(code);
		add_code(OP_JUMP_IF_FALSE, code);
		add_code(label, code);
		par->p_label.l_end = label;
		nest_enter(&nest_p, &maxnests);
		memset(&parse[nest_p], 0, sizeof(parse[nest_p]));
		par->p_state2 = PAR_WHILE_BLOCK;
		return PARONGO;
	case PAR_WHILE_BLOCK:
		nest_restore_if_needed(&nest_p, maxnests);
		if (tok->t_type == T_DONE) {
			DP("done statement");
			int loop_p = loops-1;
			int loopat = loop_at[loop_p];

			DP("loops=%d, loopat=%d, (nest_p-1)=%d",
			   loops, loopat, nest_p);
			DP("loop_at[loop_p=%d]=%d, loops=%d",
			   loop_p, loopat, loops);
			DP("parse[loopat=%d].p_state1=%s",
			   loopat,par_state1_str[parse[loopat].p_state1]);
			DP("parse[loopat=%d].p_state2=%s",
			   loopat,par_state2_str[parse[loopat].p_state2]);

			if (!(loops > 0 && loopat == (nest_p-1))) {
				DP("must be \"done\" of nested while statement");
				goto doneskip;
			}

			DP("Accept \"done\"");
			par->p_state2 = PAR_WHILE_DONE;
			return PARONGO;
		}

doneskip:
		DP("_par_make_code(&parse[%d], ...) in PAR_WHILE_BLOCK", nest_p);
		status = _par_make_code(&parse[nest_p], tok, code);
		DP("%d = _par_make_code(&parse[%d], ...)", status, nest_p);
		if (status == PARINVLD) {
			EP("while statement syntax error: invalid grammer in while blok");
			goto error;
		}
		return status;
	case PAR_WHILE_DONE:
		DP("End of while statement");
		nest_leave(&nest_p, &maxnests);
		if (loops > 0)
			loops--;
		label = par->p_label.l_loop;
		add_code(OP_JUMP, code);
		add_code(label, code);
		label = par->p_label.l_end;
		caddr = code->c_codes;
		set_code_addr_to_label(label, caddr, code);
		par->p_state1 = PAR_IDLE;
		par->p_state2 = PAR_UNDEF;
		return PARDONE;
	default:
		break;
	}

error:
	par->p_state1 = PAR_HALT;
	par->p_state2 = PAR_UNDEF;
	return PARINVLD;

}

static char
parse_if(struct parse *par, struct token *tok, struct code *code)
{
	DUMPCX(__func__, tok);

	int label=0, caddr=0, elses=0, status=PARONGO;

	switch (par->p_state2) {
	case PAR_IF_LBRACKET:
		if (tok->t_type != T_LBRACKET) {
			goto error;
		}
		add_code(OP_BLTN_CMD, code);
		add_code(C_TEST, code);
		par->p_state2 = PAR_IF_CONDITION;
		return PARONGO;
	case PAR_IF_CONDITION:
		if (tok->t_type == T_RBRACKET) {
			add_code(OP_CMD_EXE_THEN_STACK, code);
			par->p_state2 = PAR_IF_EOL;
			return PARONGO;
		}
		else if (parse_cmdarg(tok, code) == PARINVLD) {
			EP("if statement syntax error: invalid condition statement");
			goto error;
		}
		return PARONGO;
	case PAR_IF_EOL:
		if (tok->t_type != T_EOL && tok->t_type != T_SEMICOLON) {
			EP("if statement syntax error: invalid condition statement");
			goto error;
		}
		par->p_state2 = PAR_IF_THEN;
		return PARONGO;
	case PAR_IF_THEN:
		if (0 != strcmp(tok->t_symbol, "then")) {
			EP("if statement syntax error: no then");
			goto error;
		}
		if (nest_p >= MAXNESTS) {
			EP("Exceed max nests");
			goto error;
		}
		elses = par->p_elses;
		if (par->p_elses >= ELSES) {
			EP("Exceed max elif else statements");
			goto error;
		}
		label = make_label(code);
		add_code(OP_JUMP_IF_FALSE, code);
		add_code(label, code);
		par->p_label.l_else[par->p_elses] = label;
		nest_enter(&nest_p, &maxnests);
		memset(&parse[nest_p], 0, sizeof(struct parse));
		par->p_state2 = PAR_IF_BLOCK;
		return PARONGO;
	case PAR_IF_BLOCK:
		nest_restore_if_needed(&nest_p, maxnests);
		if (0 == strcmp(tok->t_symbol, "elif")) {
			DP("elfi statement");
			nest_leave(&nest_p, &maxnests);
			add_code(OP_JUMP, code);
			add_code(par->p_label.l_end, code);
			label = par->p_label.l_else[par->p_elses];
			caddr = code->c_codes;
			set_code_addr_to_label(label, caddr, code);
			par->p_elses++;
			par->p_state2 = PAR_IF_LBRACKET;
			return PARONGO;
		} else if (0 == strcmp(tok->t_symbol, "else")) {
			DP("else statement");
			add_code(OP_JUMP, code);
			add_code(par->p_label.l_end, code);
			label = par->p_label.l_else[par->p_elses];
			caddr = code->c_codes;
			set_code_addr_to_label(label, caddr, code);
			par->p_elses++;
			memset(&parse[nest_p], 0, sizeof(struct parse));
			par->p_state2 = PAR_IF_BLOCK;
			return PARONGO;
		} else if (0 == strcmp(tok->t_symbol, "fi")) {
			DP("fi statement");
			int cond_p = conds-1;
			int condat = cond_at[cond_p];

			DP("conds=%d, conat=%d, (nest_p-1)=%d",
			   conds, condat, (nest_p-1));
			DP("cond_at[cond_p=%d]=%d, conds=%d",
			   cond_p, condat, conds);
			DP("parse[condat=%d].p_state1=%s",
			   condat,par_state1_str[parse[condat].p_state1]);
			DP("parse[condat=%d].p_state2=%s",
			   condat,par_state2_str[parse[condat].p_state2]);

			if (!(conds > 0 && condat == (nest_p-1))) {
				DP("must be \"fi\" of nested if statement");
				goto fiskip;
			}

			DP("Accept \"fi\"");
			nest_leave(&nest_p, &maxnests);
			if (conds > 0)
				conds--;
			caddr = code->c_codes;
			if (par->p_elses == 0) {
				label = par->p_label.l_else[par->p_elses];
				set_code_addr_to_label(label, caddr, code);
			}
			label = par->p_label.l_end;
			set_code_addr_to_label(label, caddr, code);
			par->p_state1 = PAR_IDLE;
			par->p_state2 = PAR_UNDEF;
			return PARDONE;
		}

fiskip:
		DP("_par_make_code(&parse[%d], ...) in PAR_IF_BLOCK", nest_p);
		status = _par_make_code(&parse[nest_p], tok, code);
		DP("%d = _par_make_code(&parse[%d], ...)", status, nest_p);
		if (status == PARINVLD) {
			EP("if statement syntax error: invalid grammer in while blok");
			goto error;
		}
		return status;
	default:
		break;
	}

error:
	par->p_state1 = PAR_HALT;
	par->p_state2 = PAR_UNDEF;
	return PARINVLD;
}


static void nest_enter(int *nest_p, int *maxnests)
{
	int nest = *nest_p;

	DP("BEFORE: nest_p=%d, maxnests=%d",*nest_p,*maxnests);

	nest++;
	*nest_p = nest;
	*maxnests = nest;

	DP("AFTER : nest_p=%d, maxnests=%d",*nest_p,*maxnests);
}

static void nest_leave(int *nest_p, int *maxnests)
{
	int n = *nest_p;
	int m = *maxnests;

	DP("BEFORE: nest_p=%d, maxnests=%d",*nest_p,*maxnests);

	if (m > 0)
		m--;
	*maxnests = m;

	if (n > 0 && n > m)
		n--;
	*nest_p = n;

	DP("AFTER : nest_p=%d, maxnests=%d",*nest_p,*maxnests);
}

static void nest_restore_if_needed(int *nest_p, int maxnests)
{
	int nest = *nest_p;

	DP("BEFORE: nest_p=%d, maxnests=%d",*nest_p,maxnests);

	if (nest < maxnests)
		nest++;
	*nest_p = nest;

	DP("AFTER : nest_p=%d, maxnests=%d",*nest_p,maxnests);


}

#if !TODO_BREAK_WITH_ARGUMENT
static int break_label(struct parse *par, int *loop_at, int loop_p)
{
	int label;
	int loopat = loop_at[loop_p];

	label = par[loopat].p_label.l_end;
	DP("label = parse[loop_at[loop_p=%d]=%d].p_label.l_end = %d",
	   loop_p,loop_at[loop_p],label);
	return label;
}
#endif

static void dump_context(const char *s, struct token *tok)
{
	DP("called @ %s", s);
	DP("tok->t_type=%s",token_type_str[tok->t_type]);
	DP("tok->t_symbol = \"%s\"",
	   (tok->t_symbol[0]==0) ? "(null)" : tok->t_symbol);
	DP("nest_p=%d, maxnests=%d",nest_p,maxnests);
	for (int k=0; k<=maxnests; k++) {
		DP("&parse[%d]=0x%08X",k,(int)&parse[k]);
	}
	for (int k=0; k<=maxnests; k++) {
		if (k == nest_p) {
			DP("parse[%d].p_state1=%s(%2d) <-- nest_p",
			   k,
			   par_state1_str[parse[k].p_state1],
			   parse[k].p_state1);
			DP("parse[%d].p_state2=%s(%2d) <-- nest_p",
			   k,
			   par_state2_str[parse[k].p_state2],
			   parse[k].p_state2);
		} else {
			DP("parse[%d].p_state1=%s(%2d)",
			   k,
			   par_state1_str[parse[k].p_state1],
			   parse[k].p_state1);
			DP("parse[%d].p_state2=%s(%2d)",
			   k,
			   par_state2_str[parse[k].p_state2],
			   parse[k].p_state2);
		}
	}
}

/* EOF */

