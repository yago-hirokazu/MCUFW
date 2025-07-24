
/* par.c */

#include "par.h"
#include "tok.h"
#include "code.h"
#include "eshdb.h"
#include "esh.h"


#if DEBUG_PAR
#define DP(fmt, ...) do {					\
		fprintf(stdout, "[%s|%s()|L%d] " fmt "\n",	\
			__FNAME__,__func__,__LINE__,		\
			##__VA_ARGS__				\
			);					\
		fflush(stdout);					\
	} while (0);
#else
#define DP(...)
#endif


enum par_state1
{
	PAR_HALT = 0,
	PAR_IDLE,
	PAR_CMDLINE,
	PAR_ASSIGN,
	PAR_WHILE,
};

enum par_state2
{
	PAR_UNDEF = 0,
	PAR_CMDLINE_ARG,
	PAR_ASSIGN_EQUAL,
	PAR_ASSIGN_RVALUE,
	PAR_ASSIGN_CMDLINE,
	PAR_WHILE_LBRACKET,
	PAR_WHILE_COND,
	PAR_WHILE_EOL,
	PAR_WHILE_DO,
	PAR_WHILE_BLOCK,
};

struct parse
{
	enum par_state1 p_state1;
	enum par_state2 p_state2;
	char p_nest;
	char p_varid;
	struct label
	{
		int l_loop;
		int l_end;
	} p_label;
} par[PARNESTS];
int pars;

char * const par_state1_str[] = {
	"PAR_HALT",
	"PAR_IDLE",
	"PAR_LP_CMDLINE",
	"PAR_ASSIGN",
	"PAR_WHILE"
};

char * const par_state2_str[] = {
	"PAR_UNDEF",
	"PAR_CMDLINE_ARG",
	"PAR_ASSIGN_EQUAL",
	"PAR_ASSIGN_RVALUE",
	"PAR_ASSIGN_CMDLINE",
	"PAR_WHILE_LBRACKET",
	"PAR_WHILE_COND",
	"PAR_WHILE_EOL",
	"PAR_WHILE_DO",
	"PAR_WHILE_BLOCK",
};

static void add_code(unsigned char byte, struct code *code);
static char find_symbol(char *var, char sym[SYMBOLS][SYMCHARS]);
static char make_symbol(char *var, char sym[SYMBOLS][SYMCHARS], char *sym_num);
extern void set_value_to_variable(char var_p, char *val, struct code *code);
extern char *get_value_from_variable(char var_p, struct code *code);
static char make_label(struct code *code);
static char set_code_addr_to_label(char label_p, int code_addr, struct code *code);
extern char *get_string(char id, struct code *code);
static int find_cmd(struct token *tok, const struct applet *cmd, enum opcode opcmd, struct code *code);

static char parse_cmdarg(struct token *token, struct code *code);
static char parse_cmdline(struct token *token, struct code *code);

static char _par_make_code(struct parse *par, struct token *tok, struct code *code);
static char parse_idle(struct parse *par, struct token *tok, struct code *code);
static char parse_assign(struct parse *par, struct token *tok, struct code *code);
static char parse_while(struct parse *par, struct token *tok, struct code *code);


char par_make_code(struct token *tok, struct code *code)
{
	if (par[0].p_state1 == PAR_HALT) {
		DP("par[0].p_state1 == PAR_HALT");
		memset(&par[0], 0, sizeof(struct parse));
		par[0].p_state1 = PAR_IDLE;
		par[0].p_nest = 0;
	}
	return _par_make_code(&par[0], tok, code);
}

static void add_code(unsigned char byte, struct code *code)
{
	int c = code->c_codes;
	code->c_code[c++] = byte;
	code->c_codes = c;
}

static char find_symbol(char *var, char sym[SYMBOLS][SYMCHARS])
{
	int k;
	for (k=0; k<SYMBOLS; k++) {
		if (0 == strncmp(&sym[k][0], var, SYMCHARS)) {
			return k;
		}
	}
	return -1;
}

static char make_symbol(char *var, char sym[SYMBOLS][SYMCHARS], char *sym_num)
{
	char n1 = *sym_num;
	char n2 = n1;
	strncpy(&sym[n1++][0], var, SYMCHARS);
	*sym_num = n1;
	return n2;
}

extern void set_value_to_variable(char var_p, char *val, struct code *code)
{
	struct variable_table *v = &code->c_vtab;
	strncpy(v->v_val[var_p], val, SYMCHARS);
}

extern char *get_value_from_variable(char var_p, struct code *code)
{
	struct variable_table *v = &code->c_vtab;
	return (char *)v->v_val[var_p];
}

static char make_label(struct code *code)
{
	char label_p = code->c_ltab.l_num;
	code->c_ltab.l_num++;
	return label_p;
}

static char set_code_addr_to_label(char label_p, int code_addr, struct code *code)
{
	code->c_ltab.l_addr[label_p] = code_addr;
}

extern char *get_string(char id, struct code *code)
{
	if (id >= SYMBOLS)
		return NULL;
	return (char *)code->c_stab.s_str[id];
}

static int find_cmd(struct token *tok, const struct applet *cmd, enum opcode opcmd, struct code *code)
{
	int k=0;
	struct applet *ap = find_applet(tok->t_symbol, cmd, &k);
	if (ap == NULL || ap->name == NULL)
		return PARINVLD;
	add_code(opcmd, code);
	add_code(k, code);
	return PARONGO;
}

static char
parse_cmdarg(struct token *tok, struct code *code)
{
	if (tok->t_type == T_CMD_ARG) {
		char strid = find_symbol(tok->t_symbol, code->c_stab.s_str);
		DP("find_symbol(%s,...)=%d",tok->t_symbol,strid);
		if (strid == -1) {
			strid = make_symbol(tok->t_symbol, code->c_stab.s_str, &(code->c_stab.s_num));
			DP("make_symbol(%s,...)=%d",tok->t_symbol,strid);
		}
		add_code(OP_CMD_ARG, code);
		add_code(strid, code);
		return PARONGO;
	} else if (tok->t_type == T_CMD_VARG) {
		char varid = find_symbol(tok->t_symbol, code->c_vtab.v_var);
		DP("find_symbol(%s,...)=%d",tok->t_symbol,varid);
		if (varid == -1) {
			varid = make_symbol(tok->t_symbol, code->c_vtab.v_var, &(code->c_vtab.v_num));
			DP("make_symbol(%s,...)=%d",tok->t_symbol,varid);
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
	if (tok->t_type == T_USER_CMD)
		return find_cmd(tok, usercmds, OP_USER_CMD, code);
	else if (tok->t_type == T_BLTN_CMD)
		return find_cmd(tok, builtins, OP_BLTN_CMD, code);
	else if (tok->t_type == T_CMD_ARG || tok->t_type == T_CMD_VARG)
		return parse_cmdarg(tok, code);
	else if (tok->t_type == T_EOL) {
		add_code(OP_CMD_EXE, code);
		return PARDONE;
	}
	return PARINVLD;
}

static char
_par_make_code(struct parse *par, struct token *tok, struct code *code)
{
	char ret;

	DP("par[%d].p_state1=%s",par->p_nest,par_state1_str[par->p_state1]);
	DP("par[%d].p_state2=%s",par->p_nest,par_state2_str[par->p_state2]);
	DP("tok->t_type=%s",token_type_str[tok->t_type]);
	DP("tok->t_symbol=%s",tok->t_symbol);

	switch (par->p_state1) {
	case PAR_HALT:
		par->p_state1 = PAR_IDLE;
	case PAR_IDLE:
		ret = parse_idle(par, tok, code);
		break;
	case PAR_CMDLINE:
		ret = parse_cmdline(tok, code);
		DP("parse_cmdline()=%d",ret);
		if (ret == PARDONE) {
			par->p_state1 = PAR_HALT;
			par->p_state2 = PAR_UNDEF;
		}
		break;
	case PAR_ASSIGN:
		ret = parse_assign(par, tok, code);
		break;
	case PAR_WHILE:
		ret = parse_while(par, tok, code);
		break;
	default:
		break;
	}

	return ret;
}

static char
parse_idle(struct parse *par, struct token *tok, struct code *code)
{
	DP("tok->t_type=%s",token_type_str[tok->t_type]);

	char ret=0, varid=0;
	switch (tok->t_type) {
	case T_USER_CMD:
	case T_BLTN_CMD:
		ret = parse_cmdline(tok, code);
		DP("parse_cmdline()=%d",ret);
		if (ret == PARINVLD)
			return PARINVLD;
		par->p_state1 = PAR_CMDLINE;
		par->p_state2 = PAR_CMDLINE_ARG;
		return PARONGO;
	case T_SYMBOL:
		varid = find_symbol(tok->t_symbol, code->c_vtab.v_var);
		DP("find_symbol(%s,...)=%d",tok->t_symbol,varid);
		if (varid == -1) {
			varid = make_symbol(tok->t_symbol, code->c_vtab.v_var, &(code->c_vtab.v_num));
			DP("make_symbol(%s,...)=%d",tok->t_symbol,varid);
		}
		par->p_varid = varid;
		par->p_state1 = PAR_ASSIGN;
		par->p_state2 = PAR_ASSIGN_EQUAL;
		return PARONGO;
	case T_WHILE:
		par->p_state1 = PAR_WHILE;
		par->p_state2 = PAR_WHILE_LBRACKET;
		return PARONGO;
	case T_EOL:
		return PARDONE;
	default:
		break;
	}

	return PARINVLD;
}

static char
parse_assign(struct parse *par, struct token *tok, struct code *code)
{
	switch (par->p_state2) {
	case PAR_ASSIGN_EQUAL:
		if (0 == strcmp(tok->t_symbol, "=")) {
			par->p_state2 = PAR_ASSIGN_RVALUE;
			return PARONGO;
		}
		goto error;
	case PAR_ASSIGN_RVALUE:
		DP("PAR_ASSIGN_RVALUE w/ %s",token_type_str[tok->t_type]);
		if (tok->t_type == T_VALUE) {
			DP("varid=%d, symbol=%s",par->p_varid,tok->t_symbol);
			set_value_to_variable(par->p_varid, tok->t_symbol, code);
			par->p_state1 = PAR_HALT;
			par->p_state2 = PAR_UNDEF;
			return PARDONE;
		}
		else if (tok->t_type == T_LBACKQUOTE) {
			par->p_state2 = PAR_ASSIGN_CMDLINE;
			return PARONGO;
		}
		goto error;
	case PAR_ASSIGN_CMDLINE:
		if (tok->t_type == T_RBACKQUOTE) {
			add_code(OP_CMD_EXE_THEN_ASSIGN, code);
			add_code(par->p_varid, code);
			par->p_state1 = PAR_HALT;
			par->p_state2 = PAR_UNDEF;
			return PARDONE;
		}
		else if (parse_cmdline(tok, code) == EXIT_SUCCESS) {
			return PARONGO;
		}
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
	DP("par[%d].p_state1=%s",par->p_nest,par_state1_str[par->p_state1]);
	DP("par[%d].p_state2=%s",par->p_nest,par_state2_str[par->p_state2]);
	DP("tok->t_type=%s",token_type_str[tok->t_type]);
	DP("tok->t_symbol=%s",tok->t_symbol);

	int label=0, caddr=0;

	switch (par->p_state2) {
	case PAR_WHILE_LBRACKET:
		if (tok->t_type != T_LBRACKET)
			goto error;
		label = make_label(code);
		caddr = code->c_codes;
		set_code_addr_to_label(label, caddr, code);
		par->p_label.l_loop = label;
		add_code(OP_BLTN_CMD, code);
		add_code(C_TEST, code);
		par->p_state2 = PAR_WHILE_COND;
		return PARONGO;
	case PAR_WHILE_COND:
		if (tok->t_type == T_RBRACKET) {
			add_code(OP_CMD_EXE_THEN_STACK, code);
			par->p_state2 = PAR_WHILE_EOL;
			return PARONGO;
		}
		else if (parse_cmdarg(tok, code) == PARINVLD) {
			goto error;
		}
		return PARONGO;
	case PAR_WHILE_EOL:
		if (tok->t_type != T_EOL && tok->t_type != T_SEMICOLON)
			goto error;
		par->p_state2 = PAR_WHILE_DO;
		return PARONGO;
	case PAR_WHILE_DO:
		if (0 != strcmp(tok->t_symbol, "do"))
			goto error;
		if (pars >= PARNESTS)
			goto error;
		label = make_label(code);
		add_code(OP_JUMP_IF_FALSE, code);
		add_code(label, code);
		par->p_label.l_end = label;
		DP("BEFORE INCREMENT: pars=%d",pars);
		pars++;
		DP("AFTER  INCREMENT: pars=%d",pars);
		memset(&par[pars], 0, sizeof(par[pars]));
		par[pars].p_nest = pars;
		par->p_state2 = PAR_WHILE_BLOCK;
		return PARONGO;
	case PAR_WHILE_BLOCK:
		if (0 == strcmp(tok->t_symbol, "done")) {
			DP("End of while statement");
			pars--;
			label = par->p_label.l_loop;
			add_code(OP_JUMP, code);
			add_code(label, code);
			label = par->p_label.l_end;
			caddr = code->c_codes;
			set_code_addr_to_label(label, caddr, code);
			par->p_state1 = PAR_IDLE;
			par->p_state2 = PAR_UNDEF;
			return PARDONE;
		}
		return _par_make_code(&par[pars], tok, code);
	default:
		break;
	}

error:
	par->p_state1 = PAR_HALT;
	par->p_state2 = PAR_UNDEF;
	return PARINVLD;

}

/* EOF */

