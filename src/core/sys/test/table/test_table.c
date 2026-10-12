/* test_table.c */

#include <stdio.h>
#include <string.h>

#include "file.h"
#include "inode.h"
#include "param.h"

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

_Static_assert(NFILE == 12, "NFILE value");
_Static_assert(NINODE == 3, "NINODE value");
_Static_assert(NOFILE == 12, "NOFILE value");

static int failures;

static int file_is_unused(const struct file *fp);
static int inode_is_unused(const struct inode *ip);
static void reset_tables(void);
static void test_initial_state(void);
static void test_file_boundaries(void);
static void test_inode_boundaries(void);

int main(void)
{
	test_initial_state();
	test_file_boundaries();
	test_inode_boundaries();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}

static int file_is_unused(const struct file *fp)
{
	return fp->f_flag == 0 && fp->f_count == 0 &&
	       fp->f_inode == NULL && fp->f_offset == 0;
}

static int inode_is_unused(const struct inode *ip)
{
	return ip->i_flag == 0 && ip->i_count == 0 && ip->i_buf == NULL &&
	       ip->i_size == 0 && ip->i_lock_chan.wc_head == NULL &&
	       ip->i_lock_chan.wc_tail == NULL &&
	       ip->i_read_chan.wc_head == NULL &&
	       ip->i_read_chan.wc_tail == NULL &&
	       ip->i_write_chan.wc_head == NULL &&
	       ip->i_write_chan.wc_tail == NULL;
}

static void reset_tables(void)
{
	memset(file, 0, NFILE * sizeof(file[0]));
	memset(inode, 0, NINODE * sizeof(inode[0]));
}

static void test_initial_state(void)
{
	int i;

	for (i = 0; i < NFILE; i++)
		CHECK(file_is_unused(&file[i]));
	for (i = 0; i < NINODE; i++)
		CHECK(inode_is_unused(&inode[i]));
}

static void test_file_boundaries(void)
{
	int i;

	CHECK(&file[NFILE - 1] - &file[0] == NFILE - 1);
	for (i = 0; i < NFILE; i++) {
		CHECK(file_is_unused(&file[i]));
		file[i].f_count = 1;
	}
	CHECK(file[0].f_count == 1);
	CHECK(file[NFILE - 1].f_count == 1);
	reset_tables();
}

static void test_inode_boundaries(void)
{
	int i;

	CHECK(&inode[NINODE - 1] - &inode[0] == NINODE - 1);
	for (i = 0; i < NINODE; i++) {
		CHECK(inode_is_unused(&inode[i]));
		inode[i].i_count = 1;
	}
	CHECK(inode[0].i_count == 1);
	CHECK(inode[NINODE - 1].i_count == 1);
	reset_tables();
}
