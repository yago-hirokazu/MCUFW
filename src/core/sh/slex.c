
/* slex.c */

#include "machdep.h"
#include "lex.h"
#include "tok.h"
#include "eshdb.h"
#include "esh.h"


#if DEBUG_SLEX
#define DP(fmt, ...) do {					\
		fprintf(stdout,"[%s|%s()|L%d] " fmt "\n",	\
			__FNAME__,__func__,__LINE__,		\
			##__VA_ARGS__				\
			);					\
		fflush(stdout);					\
	} while (0);
#else
#define DP(...)
#endif

enum lex_state {
	LEX_IDLE=0,
	LEX_CMD,
	LEX_CMD_ARG,
};

char * const lex_state_str[] = {
	"LEX_IDLE",
	"LEX_CMD",
	"LEX_CMD_ARG",
	"LEX_WHILE",
};

#define	WHILE_COND	0x01
#define	WHILE_DO	0x02
#define	WHILE_DONE	0x04

struct slex {
	enum lex_state l_state;	/* State of lexical analysis */
	char l_backq;		/* BACKQute */
	char l_brack;		/* BRACKet */
	char l_while;		/* While statement */
} lex;

struct reserved_token {
	enum token_type t_type;
	char *t_symbol;
};

static const struct reserved_token kwtab[] = {
	{T_WHILE, "while"},
	{T_DO   , "do"   },
	{T_DONE , "done" }
};

static const struct reserved_token optab[] = {
	{T_ASSIGN   , "="},
	{T_SEMICOLON, ";"},
};

enum {NKWTAB = sizeof(kwtab) / sizeof(struct reserved_token)};
enum {NOPTAB = sizeof(optab) / sizeof(struct reserved_token)};

static char is_reserved(char *s, const struct reserved_token *rsvtab, char ntab);
static char is_comment(char c);
static char is_command(char *s, const struct applet *ap);
static char is_variable(char c);
static char is_endofline(char c);
static char is_operator(char *str);
static char is_comparison(char *str);
static char is_cmdarg(char *str);
static enum token_type get_token_type(char *s, const struct reserved_token *rsvtab, char ntab);

void slex_init(void)
{
	memset(&lex, 0, sizeof(lex));
	lex.l_state = LEX_IDLE;
}

int slex_make_token(char *str, struct token *token)
{
	int ret = LEXONGO;
	enum lex_state state = lex.l_state;
	enum token_type *type = &token->t_type;
	char *sym = &token->t_symbol[0];
	char c = 0;

	DP("state=%s, str=%s",lex_state_str[state],str);
	switch (state) {
	case LEX_IDLE:
		tok_init(token);
		c = str[0];
		if (c == '`') {
			DP("Left Backquote: %s",str);
			lex.l_backq = 1;
			*type = T_LBACKQUOTE;
			sym[0] = c;
			state = LEX_CMD;
		} else if (c == '[') {
			DP("Left Bracket: %s",str);
			lex.l_brack = 1;
			*type = T_LBRACKET;
			sym[0] = c;
			state = LEX_CMD_ARG;
		} else if (isdigit(c)) {
			DP("Value: %s",str);
			strncpy(sym, str, SYMCHARS);
			*type = T_VALUE;
			goto done;
		} else if (isalpha(c) || c == '_') {
			DP("Symbol: %s",str);
			if (is_command(str, usercmds)) {
				*type = T_USER_CMD;
				strncpy(sym, str, SYMCHARS);
				state = LEX_CMD_ARG;
			} else if (is_command(str, builtins)) {
				*type = T_BLTN_CMD;
				strncpy(sym, str, SYMCHARS);
				state = LEX_CMD_ARG;
			} else if (is_reserved(str, kwtab, NKWTAB)) {
				*type = get_token_type(str, kwtab, NKWTAB);
				strncpy(sym, str, SYMCHARS);
				goto done;
			} else {
				strncpy(sym, str, SYMCHARS);
				*type = T_SYMBOL;
				goto done;
			}
		} else if (is_reserved(str, optab, NOPTAB)) {
			DP("Operator: %s",str);
			*type = get_token_type(str, optab, NOPTAB);
			strncpy(sym, str, SYMCHARS);
			goto done;
		} else if (is_comment(c)) {
			DP("Comment: %s",str);
			*type = T_COMMENT;
			goto done;
		} else if (is_endofline(c)) {
			DP("EOL: 0x%02X",str[0]);
			*type = T_EOL;
			goto done;
		} else {
			DP("Unknown: %s",str);
			*type = T_UNKNOWN;
			ret = LEXERROR;
		}
		break;
	case LEX_CMD:
		if (is_command(str, usercmds)) {
			DP("User commands: %s",str);
			*type = T_USER_CMD;
			strncpy(sym, str, SYMCHARS);
			state = LEX_CMD_ARG;
		} else if (is_command(str, builtins)) {
			DP("Builtin commands: %s",str);
			*type = T_BLTN_CMD;
			strncpy(sym, str, SYMCHARS);
			state = LEX_CMD_ARG;
		} else {
			DP("Invalid symbol: %s",str);
			*type = T_UNKNOWN;
			ret = LEXERROR;
		}
		state = LEX_CMD_ARG;
		break;
	case LEX_CMD_ARG:
		c = str[0];
		if (c == ' ' || c == '\0') {
			DP("Null symbol",0);
			*type = T_UNKNOWN;
			ret = LEXERROR;
		} else if (c == '$') {
			DP("VARG: %s",str);
			strncpy(sym, str+1, SYMCHARS);
			*type = T_CMD_VARG;
		} else if (is_cmdarg(str)) {
			DP("is_cmdarg: %s",str);
			strncpy(sym, str, SYMCHARS);
			*type = T_CMD_ARG;
		} else if (is_operator(str)) {
			DP("is_operator: %s",str);
			strncpy(sym, str, SYMCHARS);
			*type = T_CMD_ARG;
		} else if (is_comparison(str)) {
			DP("is_comparison: %s",str);
			strncpy(sym, str, SYMCHARS);
			*type = T_CMD_ARG;
		} else if (lex.l_backq == 1 && c == '`') {
			DP("Right Backquote: %s", str);
			lex.l_backq = 0;
			*type = T_RBACKQUOTE;
			sym[0] = c;
		} else if (lex.l_brack == 1 && c == ']') {
			DP("Right Bracket: %s",str);
			lex.l_brack = 0;
			*type = T_RBRACKET;
			sym[0] = c;
		} else if (is_endofline(c)) {
			DP("EOL: 0x%02X",c);
			*type = T_EOL;
			goto done;
		} else if (c == ';') {
			DP("SEMICOLON: %c",c);
			*type = T_SEMICOLON;
			sym[0] = c;
			goto done;
		}
		break;
	default:
		DP("%s","slex is now in unknown state");
		*type = T_UNKNOWN;
		ret = LEXERROR;
		break;
	}

	lex.l_state = state;
	return ret;

done:
	lex.l_state = LEX_IDLE;
	ret = LEXDONE;

	return ret;
}

static char is_reserved(char *s, const struct reserved_token *rsvtab, char ntab)
{
	int k=0;
	for (k=0; k<ntab; k++) {
		if (0 == strcmp(s, rsvtab[k].t_symbol))
			return 1;
	}
	return 0;
}

static char is_command(char *s, const struct applet *ap)
{
	for (; ap->name!=NULL; ap++) {
		if (0 == strcmp(s, ap->name)) {
			return 1;
		}
	}
	return 0;
}

static char is_comment(char c)
{
	if (c == '#')
		return 1;
	return 0;
}

static char is_variable(char c)
{
	if (c == '$')
		return 1;
	return 0;
}

static char is_endofline(char c)
{
	if (c == '\r' || c == '\n')
		return 1;
	return 0;
}

static char is_operator(char *s)
{
	if (strcmp(s, "|") == 0 ||
	    strcmp(s, "&") == 0 ||
	    strcmp(s, "=") == 0 ||
	    strcmp(s, "+") == 0 ||
	    strcmp(s, "-") == 0 ||
	    strcmp(s, "*") == 0 ||
	    strcmp(s, "/") == 0 ||
	    strcmp(s, "%") == 0)
		return 1;
	return 0;
}

static char is_comparison(char *s)
{
	if (strcmp(s, "<") == 0 ||
	    strcmp(s, ">") == 0 ||
	    strcmp(s, "!") == 0 ||
	    strcmp(s, "=") == 0 ||
	    strcmp(s, "<=") == 0 ||
	    strcmp(s, ">=") == 0)
		return 1;
	return 0;
}

static char is_cmdarg(char *s)
{
	char *c=s, k=0;

	if (isdigit(*c)) {
		DP("isdigit: %c",*c);
		c++; k=0;
		while (*c != '\0') {
			if (k >= SYMCHARS)
				return 0;
			if (!isdigit(*c))
				return 0;
			c++;
			k++;
		}
		goto out;
	}

	if (isalpha(*c)) {
		DP("isalpha: %c",*c);
		c++;
		goto parse;
	}

	if (*c == '-') {
		DP("hyphen: %c",*c);
		c++;
		goto parse;
	}

parse:
	k=0;
	while (*c != '\0') {
		DP("parse: %c",*c);
		if (k >= SYMCHARS)
			return 0;
		if (!(isalpha(*c) || isdigit(*c)))
			return 0;
		c++;
		k++;
	}

out:
	return 1;
}

static enum token_type
get_token_type(char *str, const struct reserved_token *rsvtab, char ntab)
{
	int k=0;
	for (k=0; k<ntab; k++) {
		DP("str = \"%s\"",str);
		DP("rsvtab[%d].t_symbol=%s",k,rsvtab[k].t_symbol);
		DP("rsvtab[%d].t_type=%d",k,rsvtab[k].t_type);
		if (0 == strcmp(str, rsvtab[k].t_symbol)) {
			return rsvtab[k].t_type;
		}
	}
	return T_UNDEF;
}

/* EOF */

