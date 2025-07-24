
/* exe.c */

#include "exe.h"
#include "test.h"

#if DEBUG_EXE
#define DP(fmt, ...) \
	printf("[%s|%s()|L%d] " fmt "\n",		\
	       __FNAME__,__func__,__LINE__,		\
	       ##__VA_ARGS__				\
		)
#else
#define DP(...)
#endif

extern void set_value_to_variable(char var_p, char *val, struct code *code);
extern char *get_value_from_variable(char var_p, struct code *code);
extern char *get_string(char id, struct code *code);

int exe_run_codes(struct code *code, struct exe *exe)
{
	int b=0, c=0, ret=0, label_p=0;
	int pc = exe->e_pc;
	int sp = exe->e_sp;
	char str[SYMCHARS];
	struct proc *proc;

	DP("pc=%03d",pc);
	DP("sp=%02d",sp);
	DP("code->c_code[pc=%03d]=%02X",pc,code->c_code[pc]);

	switch (code->c_code[pc]) {
	case OP_USER_CMD:
		DP("OP_USER_CMD",0);

		proc = &exe->e_proc[0];
		c = proc->p_argc;
		b = code->c_code[pc+1];
		proc->p_cmd = (struct applet *)&usercmds[b];

		proc->p_argv[c++] = (char *)proc->p_cmd->name;
		proc->p_argc = c;
		exe->e_procs++;

		DP("code->c_code[pc+1=%d]=%02X",pc+1,b);
		DP("proc->p_argv[%d]=%s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc=%d",proc->p_argc);

		pc += 2;
		break;
	case OP_BLTN_CMD:
		DP("OP_BLTN_CMD",0);

		proc = &exe->e_proc[0];
		c = proc->p_argc;
		b = code->c_code[pc+1];
		proc->p_cmd = (struct applet *)&builtins[b];

		proc->p_argv[c++] = (char *)proc->p_cmd->name;
		proc->p_argc = c;
		exe->e_procs++;

		DP("code->c_code[pc+1=%d]=%02X",pc+1,b);
		DP("proc->p_argv[%d]=%s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc=%d",proc->p_argc);

		pc += 2;
		break;
	case OP_CMD_ARG:
		DP("OP_CMD_ARG",0);

		proc = &exe->e_proc[0];
		c = proc->p_argc;
		b = code->c_code[pc+1];
		proc->p_argv[c++] = get_string(b, code);
		proc->p_argc = c;
		pc += 2;

		DP("proc->p_argv[%d]=%s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc=%d",proc->p_argc);
		break;
	case OP_CMD_VARG:
		DP("OP_CMD_VARG",0);

		proc = &exe->e_proc[0];
		c = proc->p_argc;
		b = code->c_code[pc+1];
		proc->p_argv[c++] = get_value_from_variable(b, code);
		proc->p_argc = c;
		pc += 2;

		DP("proc->p_argv[%d]=%s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc=%d",proc->p_argc);
		break;
	case OP_CMD_EXE:
		DP("OP_CMD_EXE",0);
		proc = &exe->e_proc[0];
		ret = proc->p_cmd->main(proc->p_argc, proc->p_argv);
		exe->e_procs--;
		exe->e_proc->p_argc=0;
		pc++;
		break;
	case OP_CMD_EXE_THEN_STACK:
		DP("OP_CMD_EXE_THEN_STACK",0);
		proc = &exe->e_proc[0];
		ret = proc->p_cmd->main(proc->p_argc, proc->p_argv);
		DP("ret=%d",ret);
		exe->e_stack[sp++] = ret;
		exe->e_procs--;
		exe->e_proc->p_argc=0;
		pc++;
		break;
	case OP_CMD_EXE_THEN_ASSIGN:
		DP("OP_CMD_EXE_THEN_ASSIGN",0);
		proc = &exe->e_proc[0];
		ret = proc->p_cmd->main(proc->p_argc, proc->p_argv);
		DP("ret=%d",ret);
		exe->e_procs--;
		exe->e_proc->p_argc=0;
		sprintf(str, "%d", ret);
		set_value_to_variable(code->c_code[pc+1], str, code);
		pc += 2;
		break;
	case OP_JUMP_IF_FALSE:
		DP("OP_JUMP_IF_FALSE",0);
		if (exe->e_stack[sp-1] == TEST_FALSE) {
			label_p = code->c_code[pc+1];
			pc = code->c_ltab.l_addr[label_p];
		} else {
			pc += 2;
		}
		sp--;
		break;
	case OP_JUMP:
		DP("OP_JUMP",0);
		label_p = code->c_code[pc+1];
		pc = code->c_ltab.l_addr[label_p];
		break;
	case OP_NOP:
	default:
		break;
	}

	exe->e_pc = pc;
	exe->e_sp = sp;

	return EXIT_SUCCESS;
}

/* EOF */

