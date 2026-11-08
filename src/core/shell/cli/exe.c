
/*
 * par.c => bytecode => exe_run_codes() => command / exe_run_pipeline()
 *   => variable table / string table => expanded command arguments
 */

#include "custom_assert.h"
#include "code_file.h"
#include "esh.h"
#include "esh_debug.h"
#include "exe.h"
#include "file.h"
#include "pipe.h"
#include "sys_error.h"
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

extern int exe_run_pipeline(struct exe_proc *proc, int count);

static void set_symbol_to_variable(char var_p, char *val, struct code *code);
static char *get_symbol_from_variable(char var_p, struct code *code);
static char *get_string(char id, struct code *code);

static void expand_quote(char *dst, char *src, struct code *code);
static void proc_entry(void *arg);
static void close_proc_fds(struct exe_proc *proc, int count);
static void cleanup_pipeline(struct exe_proc *proc, int pipe_count,
			     int prepared);
static int connect_proc(struct exe_proc *writer, struct exe_proc *reader);
static int execute_commands(struct exe *exe);
static void reset_commands(struct exe *exe);

static int execute_commands(struct exe *exe)
{
	struct exe_proc *proc;
	struct proc *previous;
	int result;
	int i;

	assert_warm(exe->e_procs > 0, FILE_CLI_EXE, -SYS_EINVAL);
	assert_warm(exe->e_procs <= PROCS, FILE_CLI_EXE, -SYS_EINVAL);
	for (i = 0; i < exe->e_procs; i++) {
		assert_warm(exe->e_proc[i].p_cmd != NULL,
			    FILE_CLI_EXE, -SYS_EINVAL);
		assert_warm(exe->e_proc[i].p_cmd->main != NULL,
			    FILE_CLI_EXE, -SYS_EINVAL);
	}
	if (exe->e_procs > 1)
		return exe_run_pipeline(exe->e_proc, exe->e_procs);
	proc = &exe->e_proc[0];
	init_proc(&proc->p_exec);
	previous = current_proc();
	sched_set_current_proc(&proc->p_exec);
	result = proc->p_cmd->main(proc->p_argc, proc->p_argv);
	sys_lock();
	close_all(&proc->p_exec);
	sys_unlock();
	sched_set_current_proc(previous);
	return result;
}

static void reset_commands(struct exe *exe)
{
	int i;

	for (i = 0; i < exe->e_procs; i++) {
		exe->e_proc[i].p_cmd = NULL;
		exe->e_proc[i].p_argc = 0;
		memset(exe->e_proc[i].p_argv, 0,
		       sizeof(exe->e_proc[i].p_argv));
		memset(exe->e_proc[i].p_args, 0,
		       sizeof(exe->e_proc[i].p_args));
	}
	exe->e_procs = 0;
}

int exe_run_codes(struct code *code, struct exe *exe)
{
	int c=0, cid=0, sid=0, vid=0, ret=0, label_p=0;
	int pc;
	int sp;
	char str[SYMCHARS];
	struct exe_proc *proc;

	pc = exe->e_pc;
	sp = exe->e_sp;

	DP("pc = %04d, sp = %04d", pc, sp);
	DP("code->c_code[pc+0 = %4d] = %02X (opcode = %s)",
	   pc, code->c_code[pc], opcode_str[code->c_code[pc] & ~0x80]);

	switch (code->c_code[pc]) {
	case OP_USER_CMD:
		proc = &exe->e_proc[(int)exe->e_procs];
		memset(proc, 0, sizeof(*proc));
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
		proc = &exe->e_proc[(int)exe->e_procs];
		memset(proc, 0, sizeof(*proc));
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
		proc = &exe->e_proc[(int)exe->e_procs - 1];
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
		proc = &exe->e_proc[(int)exe->e_procs - 1];
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
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		c = proc->p_argc;
		sid = code->c_code[pc+1];
		expand_quote(proc->p_args[c], get_string(sid, code), code);
		proc->p_argv[c] = proc->p_args[c];
		proc->p_argc = ++c;

		pc += 2;
		break;
	case OP_PIPE_NEXT:
		pc += 1;
		break;
	case OP_CMD_EXE:
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		ret = execute_commands(exe);
		exe->e_status = ret;

		DP("Command: %s", proc->p_cmd->name);

#if TODO_BREAK_WITH_ARGUMENT
		if (0 == strcmp(proc->p_argv[0], "break") && ret >= 0 &&
		    ret < code->c_ltab.l_num)
			pc = code->c_ltab.l_addr[ret];
		else
#endif
		pc += 1;
		reset_commands(exe);
		break;
	case OP_CMD_EXE_THEN_STACK:
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		ret = execute_commands(exe);
		exe->e_status = ret;
		exe->e_stack[sp] = ret;

		DP("Command: %s --> ret = %d", proc->p_cmd->name, ret);
		DP("ret --> stack[%d]", ret, sp);

		sp += 1;
		pc += 1;
		reset_commands(exe);
		break;
	case OP_CMD_EXE_THEN_ASSIGN:
		proc = &exe->e_proc[(int)exe->e_procs - 1];
		ret = execute_commands(exe);
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
		reset_commands(exe);
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

int exe_run_pipeline(struct exe_proc *proc, int count)
{
	int finished;
	int pipe_count;
	int prepared;
	int progress;
	int result;
	int i;

	assert_warm(proc != NULL, FILE_CLI_EXE, -SYS_EINVAL);
	assert_warm(count >= 2, FILE_CLI_EXE, -SYS_EINVAL);
	assert_warm(count <= PROCS, FILE_CLI_EXE, -SYS_EINVAL);
	for (i = 0; i < count; i++) {
		assert_warm(proc[i].p_cmd != NULL, FILE_CLI_EXE, -SYS_EINVAL);
		assert_warm(proc[i].p_cmd->main != NULL, FILE_CLI_EXE,
			    -SYS_EINVAL);
		assert_warm(proc[i].p_cmd->name != NULL, FILE_CLI_EXE,
			    -SYS_EINVAL);
		assert_warm(proc[i].p_argc >= 1, FILE_CLI_EXE, -SYS_EINVAL);
		assert_warm(proc[i].p_argc <= TTY_NARG, FILE_CLI_EXE,
			    -SYS_EINVAL);
		assert_warm(proc[i].p_argv[0] != NULL, FILE_CLI_EXE,
			    -SYS_EINVAL);
	}
	for (i = 0; i < count; i++) {
		init_proc(&proc[i].p_exec);
	}
	pipe_count = 0;
	prepared = 0;
	for (i = 0; i < count - 1; i++) {
		result = connect_proc(&proc[i], &proc[i + 1]);
		if (result < 0)
			goto out;
		pipe_count++;
	}
	for (i = 0; i < count; i++) {
		result = sched_prepare(&proc[i].p_exec, proc_entry,
				       &proc[i]);
		if (result < 0)
			goto out;
		prepared++;
	}

	result = -SYS_EINVAL;
	for (;;) {
		finished = 0;
		progress = 0;
		for (i = 0; i < count; i++) {
			if (proc[i].p_exec.p_state == PROC_FINISHED) {
				finished++;
				continue;
			}
			if (proc[i].p_exec.p_state == PROC_ERROR)
				goto out;
			if (proc[i].p_exec.p_state != PROC_RUNNABLE)
				continue;
			if (sched_resume(&proc[i].p_exec) < 0)
				goto out;
			progress = 1;
		}
		if (finished == count) {
			result = proc[count - 1].p_exec.p_ret;
			break;
		}
		if (!progress)
			break;
	}

out:
	cleanup_pipeline(proc, pipe_count, prepared);
	return result;
}

/*
 * Report invalid assignment and select log file code
 */
static uint8_t invalid_assignment(void)
{
	EP("Invalid variable assignment");
	return FILE_CLI_EXE;
}

/*
 * Store symbol in variable table
 */
static void set_symbol_to_variable(char var_p, char *sym, struct code *code)
{
	struct variable_table *v;

	assert_void(code != NULL, invalid_assignment());
	assert_void(sym != NULL, invalid_assignment());
	assert_void(var_p >= 0, invalid_assignment());
	assert_void(var_p < SYMBOLS, invalid_assignment());
	v = &code->c_vtab;
	strncpy(&v->v_val[var_p * SYMCHARS], sym, SYMCHARS - 1);
	v->v_val[var_p * SYMCHARS + SYMCHARS - 1] = 0;
}

/*
 * Fetch value from variable table
 */
static char *get_symbol_from_variable(char var_p, struct code *code)
{
	struct variable_table *v;

	assert_warm(code != NULL, FILE_CLI_EXE, NULL);
	assert_warm(var_p >= 0, FILE_CLI_EXE, NULL);
	assert_warm(var_p < SYMBOLS, FILE_CLI_EXE, NULL);
	v = &code->c_vtab;
	return (char *)&v->v_val[var_p * SYMCHARS];
}


/*
 * Fetch string from bytecode string table
 */
static char *get_string(char id, struct code *code)
{
	assert_warm(code != NULL, FILE_CLI_EXE, NULL);
	assert_warm(id >= 0, FILE_CLI_EXE, NULL);
	assert_warm(id < SYMBOLS, FILE_CLI_EXE, NULL);
	return (char *)&code->c_stab.s_str[id * SYMCHARS];
}

/*
 * Expand quoted argument into command buffer
 */
static void expand_quote(char *dst, char *src, struct code *code)
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


static void proc_entry(void *arg)
{
	struct exe_proc *proc;

	proc = arg;
	proc->p_exec.p_ret = proc->p_cmd->main(proc->p_argc,
						    proc->p_argv);
	sys_lock();
	close_all(&proc->p_exec);
	sys_unlock();
}

static void close_proc_fds(struct exe_proc *proc, int count)
{
	int i;

	sys_lock();
	for (i = 0; i < count; i++)
		close_all(&proc[i].p_exec);
	sys_unlock();
}

static void cleanup_pipeline(struct exe_proc *proc, int pipe_count,
			     int prepared)
{
	int i;

	close_proc_fds(proc, pipe_count + 1);
	for (i = 0; i < prepared; i++)
		sched_release(&proc[i].p_exec);
}

static int connect_proc(struct exe_proc *writer, struct exe_proc *reader)
{
	struct proc owner;
	struct file *read_file;
	struct file *write_file;
	int fd[2];
	int result;

	init_proc(&owner);
	sys_lock();
	result = pipe_for_proc(&owner, fd);
	if (result < 0)
		goto out;
	read_file = getf(&owner, fd[0]);
	write_file = getf(&owner, fd[1]);
	result = fd_install_at(&writer->p_exec, FD_STDOUT, write_file);
	if (result < 0)
		goto out;
	result = fd_install_at(&reader->p_exec, FD_STDIN, read_file);
	if (result < 0)
		fd_close(&writer->p_exec, FD_STDOUT);
out:
	close_all(&owner);
	sys_unlock();
	return result < 0 ? result : 0;
}


/* EOF */

