
/* exe.c */

#include "esh.h"
#include "esh_debug.h"
#include "exe.h"
#include "test.h"


#define EP(fmt, ...)\
do {\
	fprintf(stdout,"error [%s|%s()|L%3d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)

#if DEBUG_EXE
#define DP(fmt, ...)\
do {\
	fprintf(stdout, "debug [%s|%s()|L%3d] " fmt "\n",\
	       __FNAME__,__func__,__LINE__,\
	       ##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)
#else
#define DP(...)
#endif


extern void set_symbol_to_variable(char var_p, char *val, struct code *code);
extern char *get_symbol_from_variable(char var_p, struct code *code);
extern char *get_string(char id, struct code *code);

static void expand_quoted_arg(char *dst, char *src, struct code *code);

int exe_run_codes(struct code *code, struct exe *exe)
{
	int c=0, cid=0, sid=0, vid=0, ret=0, label_p=0;
	int pc = exe->e_pc;
	int sp = exe->e_sp;
	char str[SYMCHARS];
	struct proc *proc;

	DP("pc = %04d, sp = %04d", pc, sp);
	DP("code->c_code[pc+0 = %4d] = %02X (opcode = %s)",
	   pc, code->c_code[pc], opcode_str[code->c_code[pc] & ~0x80]);

	switch (code->c_code[pc]) {
	case OP_USER_CMD:
		proc = &exe->e_proc[0];
		c = proc->p_argc;
		cid = code->c_code[pc+1];
		proc->p_cmd = (struct applet *)&usercmds[cid];

		proc->p_argv[c++] = (char *)proc->p_cmd->name;
		proc->p_argc = c;
		exe->e_procs++;

		DP("code->c_code[pc+1 = %4d] = %02X: User Cmd ID",pc+1,cid);
		DP("proc->p_argv[%2d] = %s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc = %d",proc->p_argc);

		pc += 2;
		break;
	case OP_BLTN_CMD:
		proc = &exe->e_proc[0];
		c = proc->p_argc;
		cid = code->c_code[pc+1];
		proc->p_cmd = (struct applet *)&builtins[cid];

		proc->p_argv[c++] = (char *)proc->p_cmd->name;
		proc->p_argc = c;
		exe->e_procs++;

		DP("code->c_code[pc+1 = %4d] = %02X: Bltn Cmd ID",pc+1,cid);
		DP("proc->p_argv[%2d] = %s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc = %d",proc->p_argc);

		pc += 2;
		break;
	case OP_CMD_ARG:
		proc = &exe->e_proc[0];
		c = proc->p_argc;
		sid = code->c_code[pc+1];
		proc->p_argv[c++] = get_string(sid, code);
		proc->p_argc = c;

		DP("code->c_code[pc+1 = %4d] = %02X: String ID",pc+1,sid);
		DP("proc->p_argv[%2d] = %s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc = %d",proc->p_argc);

		pc += 2;
		break;
	case OP_CMD_VARG:
		proc = &exe->e_proc[0];
		c = proc->p_argc;
		vid = code->c_code[pc+1];
		proc->p_argv[c++] = get_symbol_from_variable(vid, code);
		proc->p_argc = c;

		DP("code->c_code[pc+1 = %4d] = %02X: Variable ID",pc+1,vid);
		DP("proc->p_argv[%2d] = %s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc = %d",proc->p_argc);

		pc += 2;
		break;
	case OP_CMD_QARG:
		proc = &exe->e_proc[0];
		c = proc->p_argc;
		sid = code->c_code[pc+1];
		expand_quoted_arg(proc->p_argbuf[c], get_string(sid, code), code);
		proc->p_argv[c] = proc->p_argbuf[c];
		proc->p_argc = ++c;

		pc += 2;
		break;
	case OP_CMD_EXE:
		proc = &exe->e_proc[0];
		ret = proc->p_cmd->main(proc->p_argc, proc->p_argv);
		exe->e_procs--;
		exe->e_proc->p_argc=0;

		DP("Command: %s", proc->p_cmd->name);

#if TODO_BREAK_WITH_ARGUMENT
		if (0 == strcmp(proc->p_argv[0], "break") && ret >= 0)
			pc = code->c_ltab.l_addr[ret];
		else
#endif
		pc += 1;
		break;
	case OP_CMD_EXE_THEN_STACK:
		proc = &exe->e_proc[0];
		ret = proc->p_cmd->main(proc->p_argc, proc->p_argv);
		exe->e_stack[sp] = ret;
		exe->e_procs--;
		exe->e_proc->p_argc=0;

		DP("Command: %s --> ret = %d", proc->p_cmd->name, ret);
		DP("ret --> stack[%d]", ret, sp);

		sp += 1;
		pc += 1;
		break;
	case OP_CMD_EXE_THEN_ASSIGN:
		proc = &exe->e_proc[0];
		ret = proc->p_cmd->main(proc->p_argc, proc->p_argv);
		exe->e_procs--;
		exe->e_proc->p_argc=0;
		sprintf(str, "%d", ret);
		vid = code->c_code[pc+1];
		set_symbol_to_variable(vid, str, code);

		DP("Command: %s --> ret = %d", proc->p_cmd->name, ret);
		DP("assigned to the variable ID = %d (%s)",
		   vid,
		   (code->c_vtab.v_var[vid * SYMCHARS]==0)
		   ? "(null)"
		   : &code->c_vtab.v_var[vid * SYMCHARS]);

		pc += 2;
		break;
	case OP_JUMP_IF_FALSE:
		if (exe->e_stack[sp-1] == TEST_FALSE) {
			label_p = code->c_code[pc+1];
			pc = code->c_ltab.l_addr[label_p];

			DP("TEST_FALSE then jump to pc=%d", pc);
		} else {
			pc += 2;

			DP("TEST_TRUE  them move to pc=%d", pc);
		}

		sp -= 1;
		break;
	case OP_JUMP:
		DP("jump from pc=%d to pc=%d",
		   pc, code->c_ltab.l_addr[label_p]);

		label_p = code->c_code[pc+1];
		pc = code->c_ltab.l_addr[label_p];
		break;
#if !TODO_BREAK_WITH_ARGUMENT
	case OP_BREAK_LOOP:
		DP("OP_BREAK_LOOP");
		DP("jump from pc=%d to pc=%d",
		   pc, code->c_ltab.l_addr[label_p]);

		label_p = code->c_code[pc+1];
		pc = code->c_ltab.l_addr[label_p];
		break;
#endif
	case OP_PUSH_SYMBOL:
		sid = code->c_code[pc+1];
		exe->e_stack[sp] = sid;

		DP("String ID = %d (%s) --> stack[%d]",
		   sid,
		   (code->c_stab.s_str[sid * SYMCHARS]==0)
		   ? "(null)"
		   : &code->c_stab.s_str[sid * SYMCHARS]);

		sp += 1;
		pc += 2;
		break;
	case OP_POP_SYMBOL_THEN_ASSIGN:
		sid = exe->e_stack[sp-1];
		char *sym = get_string(sid, code);
		vid = code->c_code[pc+1];
		set_symbol_to_variable(vid, sym, code);

		DP("String ID = %d (%s) --> Variable ID = %d (%s)",
		   sid, sym, vid,
		   (code->c_vtab.v_val[vid * SYMCHARS]==0)
		   ? "(null)"
		   : &code->c_vtab.v_val[vid * SYMCHARS]);

		sp -= 1;
		pc += 2;
		break;
	case OP_NOP:
	default:
		break;
	}

	exe->e_pc = pc;
	exe->e_sp = sp;

	return EXIT_SUCCESS;
}

static void expand_quoted_arg(char *dst, char *src, struct code *code)
{
	int d = 0;

	while (*src != '\0' && d < SYMCHARS - 1) {
		char name[SYMCHARS];
		char *value;
		int n = 0;
		int v = 0;
		int var;

		/* Check whether src points to start of variable */
		if (*src == '$' &&
		    (isalpha((unsigned char)src[1]) || src[1] == '_')) {
			/* Expand variable below */
		} else {
			dst[d++] = *src++;
			continue;
		}

		src++; /* Skip '$' */

		/* Extract variable name */
		while (isalnum((unsigned char)*src) || *src == '_') {
			if (n < SYMCHARS - 1)
				name[n++] = *src;
			src++;
		}
		name[n] = '\0';

		/* Search target variable from table by name */
		for (var = 0; var < code->c_vtab.v_num; var++) {
			if (strncmp(name,
				    &code->c_vtab.v_var[var * SYMCHARS],
				    SYMCHARS) == 0)
				break;
		}

		/* Skip undefined variable */
		if (var >= code->c_vtab.v_num)
			continue;

		value = get_symbol_from_variable(var, code);

		/* Skip unset variable */
		if (*value == '\n')
			continue;

		/* Copy variable value */
		while (v < SYMCHARS && value[v] != '\0' &&
		       d < SYMCHARS - 1) {
			dst[d++] = value[v++];
		}
	}
	dst[d] = '\0';
}

/* EOF */

