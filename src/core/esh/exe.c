
/* exe.c */

#include "esh.h"
#include "esh_debug.h"
#include "esh_errno.h"
#include "esh_file.h"
#include "esh_pipe.h"
#include "exe.h"
#include "test.h"


#define EP(fmt, ...)\
do {\
	fprintf(stderr,"error [%s|%s()|L%3d] " fmt "\n",\
		__FNAME__,__func__,__LINE__,\
		##__VA_ARGS__\
		);\
	fflush(stdout);\
} while (0)

#if DEBUG_EXE
#define DP(fmt, ...)\
do {\
	fprintf(stderr, "debug [%s|%s()|L%3d] " fmt "\n",\
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
static void exe_proc_entry(void *arg);
static void exe_close_proc_fds(struct proc *proc, int count);
static void exe_cleanup_pipeline(struct proc *proc, int pipe_count,
				 int prepared);
static int exe_connect_proc(struct proc *writer, struct proc *reader);
static int exe_execute_commands(struct exe *exe);
static void exe_reset_commands(struct exe *exe);

static int exe_valid_instruction(struct code *code, struct exe *exe)
{
	const struct applet *table;
	int op;
	int operand;
	int width;
	int i;

	if (!code || !exe || code->c_codes < 0 || code->c_codes > CODES ||
	    exe->e_pc < 0 || exe->e_pc >= code->c_codes ||
	    exe->e_sp < 0 || exe->e_sp > STACKS ||
	    exe->e_procs < 0 || exe->e_procs > PROCS ||
	    code->c_stab.s_num < 0 || code->c_stab.s_num > SYMBOLS ||
	    code->c_vtab.v_num < 0 || code->c_vtab.v_num > SYMBOLS ||
	    code->c_ltab.l_num < 0 || code->c_ltab.l_num > LABELS)
		return 0;
	op = code->c_code[exe->e_pc];
	if (op < OP_NOP || op > OP_PIPE_NEXT)
		return 0;
	width = (op == OP_NOP || op == OP_CMD_EXE ||
		 op == OP_CMD_EXE_THEN_STACK || op == OP_PIPE_NEXT) ? 1 : 2;
	if (exe->e_pc > code->c_codes - width)
		return 0;
	operand = width == 2 ? code->c_code[exe->e_pc + 1] : 0;
	if (width == 2 && operand < 0)
		return 0;
	switch (op) {
	case OP_USER_CMD:
	case OP_BLTN_CMD:
		table = op == OP_USER_CMD ? usercmds : builtins;
		for (i = 0; table[i].name; i++)
			if (i == operand)
				return table[i].main && exe->e_procs < PROCS;
		return 0;
	case OP_CMD_ARG:
	case OP_CMD_QARG:
		if (operand >= code->c_stab.s_num ||
		    !memchr(&code->c_stab.s_str[operand * SYMCHARS],
			    0, SYMCHARS))
			return 0;
		break;
	case OP_CMD_VARG:
	case OP_CMD_EXE_THEN_ASSIGN:
	case OP_POP_SYMBOL_THEN_ASSIGN:
		if (operand >= code->c_vtab.v_num)
			return 0;
		if (op == OP_CMD_VARG &&
		    !memchr(&code->c_vtab.v_val[operand * SYMCHARS],
			    0, SYMCHARS))
			return 0;
		break;
	case OP_JUMP:
	case OP_JUMP_IF_FALSE:
	case OP_BREAK_LOOP:
		if (operand >= code->c_ltab.l_num ||
		    code->c_ltab.l_addr[operand] < 0 ||
		    code->c_ltab.l_addr[operand] > code->c_codes)
			return 0;
		break;
	case OP_PUSH_SYMBOL:
		if (operand >= code->c_stab.s_num ||
		    !memchr(&code->c_stab.s_str[operand * SYMCHARS],
			    0, SYMCHARS))
			return 0;
		break;
	default:
		break;
	}
	if ((op == OP_PUSH_SYMBOL || op == OP_CMD_EXE_THEN_STACK) &&
	    exe->e_sp == STACKS)
		return 0;
	if ((op == OP_JUMP_IF_FALSE || op == OP_POP_SYMBOL_THEN_ASSIGN) &&
	    !exe->e_sp)
		return 0;
	if (op == OP_POP_SYMBOL_THEN_ASSIGN &&
	    (exe->e_stack[exe->e_sp - 1] < 0 ||
	     exe->e_stack[exe->e_sp - 1] >= code->c_stab.s_num))
		return 0;
	if (op >= OP_CMD_ARG && op <= OP_CMD_EXE_THEN_ASSIGN) {
		struct proc *proc;

		if (!exe->e_procs)
			return 0;
		proc = &exe->e_proc[exe->e_procs - 1];
		if (!proc->p_cmd || !proc->p_cmd->main ||
		    proc->p_argc < 1 || proc->p_argc > TTY_NARG ||
		    !proc->p_argv[0])
			return 0;
	}
	return 1;
}

static void exe_proc_entry(void *arg)
{
	struct proc *proc;

	proc = arg;
	proc->p_exec.ep_status = proc->p_cmd->main(proc->p_argc,
						     proc->p_argv);
	esh_kernel_lock();
	esh_close_all(&proc->p_exec.ep_fd_table);
	esh_kernel_unlock();
}

static void exe_close_proc_fds(struct proc *proc, int count)
{
	int i;

	esh_kernel_lock();
	for (i = 0; i < count; i++)
		esh_close_all(&proc[i].p_exec.ep_fd_table);
	esh_kernel_unlock();
}

static void exe_cleanup_pipeline(struct proc *proc, int pipe_count,
				 int prepared)
{
	int i;

	exe_close_proc_fds(proc, pipe_count + 1);
	for (i = 0; i < prepared; i++)
		esh_sched_release(&proc[i].p_exec);
}

static int exe_connect_proc(struct proc *writer, struct proc *reader)
{
	struct esh_proc owner;
	struct esh_file *read_file;
	struct esh_file *write_file;
	int fd[2];
	int result;

	esh_proc_init(&owner);
	result = esh_pipe_for_proc(&owner, fd);
	if (result < 0)
		return result;
	read_file = esh_fd_get(&owner.ep_fd_table, fd[0]);
	write_file = esh_fd_get(&owner.ep_fd_table, fd[1]);
	result = esh_fd_install_at(&writer->p_exec.ep_fd_table,
				   ESH_FD_STDOUT, write_file);
	if (result < 0)
		goto out;
	result = esh_fd_install_at(&reader->p_exec.ep_fd_table,
				   ESH_FD_STDIN, read_file);
	if (result < 0)
		esh_fd_close(&writer->p_exec.ep_fd_table,
			     ESH_FD_STDOUT);
out:
	esh_close_all(&owner.ep_fd_table);
	return result < 0 ? result : 0;
}

int exe_run_pipeline(struct proc *proc, int count)
{
	int finished;
	int pipe_count;
	int prepared;
	int progress;
	int result;
	int i;

	if (!proc || count < 2 || count > PROCS)
		return -ESH_EINVAL;
	for (i = 0; i < count; i++) {
		if (!proc[i].p_cmd || !proc[i].p_cmd->main ||
		    !proc[i].p_cmd->name || proc[i].p_argc < 1 ||
		    proc[i].p_argc > TTY_NARG || !proc[i].p_argv[0])
			return -ESH_EINVAL;
	}
	for (i = 0; i < count; i++) {
		esh_proc_init(&proc[i].p_exec);
	}
	pipe_count = 0;
	prepared = 0;
	for (i = 0; i < count - 1; i++) {
		result = exe_connect_proc(&proc[i], &proc[i + 1]);
		if (result < 0)
			goto out;
		pipe_count++;
	}
	for (i = 0; i < count; i++) {
		result = esh_sched_prepare(&proc[i].p_exec, exe_proc_entry,
					   &proc[i]);
		if (result < 0)
			goto out;
		prepared++;
	}

	result = -ESH_EINVAL;
	for (;;) {
		finished = 0;
		progress = 0;
		for (i = 0; i < count; i++) {
			if (proc[i].p_exec.ep_state == ESH_PROC_FINISHED) {
				finished++;
				continue;
			}
			if (proc[i].p_exec.ep_state == ESH_PROC_ERROR)
				goto out;
			if (proc[i].p_exec.ep_state != ESH_PROC_RUNNABLE)
				continue;
			if (esh_sched_resume(&proc[i].p_exec) < 0)
				goto out;
			progress = 1;
		}
		if (finished == count) {
			result = proc[count - 1].p_exec.ep_status;
			break;
		}
		if (!progress)
			break;
	}

out:
	exe_cleanup_pipeline(proc, pipe_count, prepared);
	return result;
}

static int exe_execute_commands(struct exe *exe)
{
	struct proc *proc;
	int i;

	if (exe->e_procs <= 0 || exe->e_procs > PROCS)
		return -ESH_EINVAL;
	for (i = 0; i < exe->e_procs; i++)
		if (!exe->e_proc[i].p_cmd || !exe->e_proc[i].p_cmd->main)
			return -ESH_EINVAL;
	if (exe->e_procs > 1)
		return exe_run_pipeline(exe->e_proc, exe->e_procs);
	proc = &exe->e_proc[0];
	return proc->p_cmd->main(proc->p_argc, proc->p_argv);
}

static void exe_reset_commands(struct exe *exe)
{
	int i;

	for (i = 0; i < exe->e_procs; i++) {
		exe->e_proc[i].p_cmd = NULL;
		exe->e_proc[i].p_argc = 0;
		memset(exe->e_proc[i].p_argv, 0,
		       sizeof(exe->e_proc[i].p_argv));
		memset(exe->e_proc[i].p_argbuf, 0,
		       sizeof(exe->e_proc[i].p_argbuf));
	}
	exe->e_procs = 0;
}

int exe_run_codes(struct code *code, struct exe *exe)
{
	int c=0, cid=0, sid=0, vid=0, ret=0, label_p=0;
	int pc;
	int sp;
	char str[SYMCHARS];
	struct proc *proc;

	if (!exe_valid_instruction(code, exe))
		return EXIT_FAILURE;
	pc = exe->e_pc;
	sp = exe->e_sp;

	DP("pc = %04d, sp = %04d", pc, sp);
	DP("code->c_code[pc+0 = %4d] = %02X (opcode = %s)",
	   pc, code->c_code[pc], opcode_str[code->c_code[pc] & ~0x80]);

	switch (code->c_code[pc]) {
	case OP_USER_CMD:
		if (exe->e_procs >= PROCS)
			return EXIT_FAILURE;
		proc = &exe->e_proc[(int)exe->e_procs];
		memset(proc, 0, sizeof(*proc));
		c = proc->p_argc;
		if (c >= TTY_NARG)
			return EXIT_FAILURE;
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
		if (exe->e_procs >= PROCS)
			return EXIT_FAILURE;
		proc = &exe->e_proc[(int)exe->e_procs];
		memset(proc, 0, sizeof(*proc));
		c = proc->p_argc;
		if (c >= TTY_NARG)
			return EXIT_FAILURE;
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
		if (exe->e_procs <= 0)
			return EXIT_FAILURE;
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		c = proc->p_argc;
		if (c >= TTY_NARG)
			return EXIT_FAILURE;
		sid = code->c_code[pc+1];
		proc->p_argv[c++] = get_string(sid, code);
		proc->p_argc = c;

		DP("code->c_code[pc+1 = %4d] = %02X: String ID",pc+1,sid);
		DP("proc->p_argv[%2d] = %s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc = %d",proc->p_argc);

		pc += 2;
		break;
	case OP_CMD_VARG:
		if (exe->e_procs <= 0)
			return EXIT_FAILURE;
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		c = proc->p_argc;
		if (c >= TTY_NARG)
			return EXIT_FAILURE;
		vid = code->c_code[pc+1];
		proc->p_argv[c++] = get_symbol_from_variable(vid, code);
		proc->p_argc = c;

		DP("code->c_code[pc+1 = %4d] = %02X: Variable ID",pc+1,vid);
		DP("proc->p_argv[%2d] = %s",c-1,proc->p_argv[c-1]);
		DP("proc->p_argc = %d",proc->p_argc);

		pc += 2;
		break;
	case OP_CMD_QARG:
		if (exe->e_procs <= 0)
			return EXIT_FAILURE;
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		c = proc->p_argc;
		if (c >= TTY_NARG)
			return EXIT_FAILURE;
		sid = code->c_code[pc+1];
		expand_quoted_arg(proc->p_argbuf[c], get_string(sid, code), code);
		proc->p_argv[c] = proc->p_argbuf[c];
		proc->p_argc = ++c;

		pc += 2;
		break;
	case OP_PIPE_NEXT:
		if (exe->e_procs <= 0 || exe->e_procs >= PROCS)
			return EXIT_FAILURE;
		pc += 1;
		break;
	case OP_CMD_EXE:
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		ret = exe_execute_commands(exe);
		exe->e_status = ret;

		DP("Command: %s", proc->p_cmd->name);

#if TODO_BREAK_WITH_ARGUMENT
		if (0 == strcmp(proc->p_argv[0], "break") && ret >= 0 &&
		    ret < code->c_ltab.l_num)
			pc = code->c_ltab.l_addr[ret];
		else
#endif
		pc += 1;
		exe_reset_commands(exe);
		break;
	case OP_CMD_EXE_THEN_STACK:
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		ret = exe_execute_commands(exe);
		exe->e_status = ret;
		exe->e_stack[sp] = ret;

		DP("Command: %s --> ret = %d", proc->p_cmd->name, ret);
		DP("ret --> stack[%d]", ret, sp);

		sp += 1;
		pc += 1;
		exe_reset_commands(exe);
		break;
	case OP_CMD_EXE_THEN_ASSIGN:
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		ret = exe_execute_commands(exe);
		exe->e_status = ret;
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
		exe_reset_commands(exe);
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
		pc++;
		break;
	default:
		return EXIT_FAILURE;
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

