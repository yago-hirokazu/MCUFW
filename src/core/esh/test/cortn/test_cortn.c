/* test_cortn.c */

#include <stdint.h>
#include <stdio.h>

#include "cortn.h"

#define TEST_COROUTINES	2
#define ROUND_COUNT	3

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			       #condition); \
			failures++; \
		} \
	} while (0)

struct basic_data {
	int step;
	int value;
};

struct round_data {
	int id;
};

struct wait_data {
	enum cortn_yield_reason reason;
	int resumed;
};

struct simd_value {
	uint64_t low;
	uint64_t high;
};

static int failures;
static int order[TEST_COROUTINES * ROUND_COUNT];
static int order_count;
static unsigned char basic_stack[CORTN_WINDOWS_STACK_SIZE]
	__attribute__((aligned(16)));
static unsigned char wait_stack[CORTN_WINDOWS_STACK_SIZE]
	__attribute__((aligned(16)));
static unsigned char simd_stack[CORTN_WINDOWS_STACK_SIZE]
	__attribute__((aligned(16)));
static unsigned char round_stack[TEST_COROUTINES][CORTN_WINDOWS_STACK_SIZE]
	__attribute__((aligned(16)));

static void nested_yield(struct cortn *self, struct basic_data *data);
static void basic_entry(struct cortn *self, void *arg);
static void wait_entry(struct cortn *self, void *arg);
static void round_entry(struct cortn *self, void *arg);
static void simd_entry(struct cortn *self, void *arg);
static void test_basic(void);
static void test_wait(void);
static void test_round_robin(void);
static void test_stack_guard(void);
static void test_simd_and_fpu(void);

int main(void)
{
	test_basic();
	test_wait();
	test_round_robin();
	test_stack_guard();
	test_simd_and_fpu();
	if (failures) {
		printf("%d tests failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}

static void nested_yield(struct cortn *self, struct basic_data *data)
{
	int local;

	local = 41;
	data->step = 1;
	CHECK(cortn_yield(self, CORTN_YIELD_VOLUNTARY) == CORTN_SUCCESS);
	CHECK(local == 41);
	data->value = local + 1;
	data->step = 2;
}

static void basic_entry(struct cortn *self, void *arg)
{
	struct basic_data *data;

	data = arg;
	nested_yield(self, data);
}

static void wait_entry(struct cortn *self, void *arg)
{
	struct wait_data *data;

	data = arg;
	CHECK(cortn_yield(self, data->reason) == CORTN_SUCCESS);
	data->resumed = 1;
}

static void round_entry(struct cortn *self, void *arg)
{
	struct round_data *data;
	int i;

	data = arg;
	for (i = 0; i < ROUND_COUNT; i++) {
		order[order_count++] = data->id;
		CHECK(cortn_yield(self, CORTN_YIELD_VOLUNTARY) ==
		      CORTN_SUCCESS);
	}
}

static void simd_entry(struct cortn *self, void *arg)
{
	struct simd_value *result;
	struct simd_value expected;
	uint32_t mxcsr;
	uint32_t changed_mxcsr;
	uint16_t control;
	uint16_t changed_control;

	result = arg;
	expected.low = UINT64_C(0x0123456789abcdef);
	expected.high = UINT64_C(0xfedcba9876543210);
	__asm__ volatile ("movdqu %0, %%xmm6" : : "m" (expected) : "xmm6");
	__asm__ volatile ("stmxcsr %0" : "=m" (mxcsr));
	__asm__ volatile ("fnstcw %0" : "=m" (control));
	changed_mxcsr = mxcsr ^ UINT32_C(0x6000);
	changed_control = control ^ UINT16_C(0x0c00);
	__asm__ volatile ("ldmxcsr %0" : : "m" (changed_mxcsr));
	__asm__ volatile ("fldcw %0" : : "m" (changed_control));

	CHECK(cortn_yield(self, CORTN_YIELD_VOLUNTARY) == CORTN_SUCCESS);

	__asm__ volatile ("movdqu %%xmm6, %0" : "=m" (*result));
	__asm__ volatile ("stmxcsr %0" : "=m" (mxcsr));
	__asm__ volatile ("fnstcw %0" : "=m" (control));
	CHECK(result->low == expected.low);
	CHECK(result->high == expected.high);
	CHECK(mxcsr == changed_mxcsr);
	CHECK(control == changed_control);
}

static void test_basic(void)
{
	struct cortn cortn;
	struct basic_data data;

	data.step = 0;
	data.value = 0;
	CHECK(cortn_init(&cortn, basic_stack, sizeof(basic_stack),
			 basic_entry, &data) == CORTN_SUCCESS);
	CHECK(cortn.c_state == CORTN_RUNNABLE);
	CHECK(cortn_resume(&cortn) == CORTN_SUCCESS);
	CHECK(cortn.c_state == CORTN_RUNNABLE);
	CHECK(cortn.c_reason == CORTN_YIELD_VOLUNTARY);
	CHECK(data.step == 1);
	CHECK(cortn_resume(&cortn) == CORTN_SUCCESS);
	CHECK(cortn.c_state == CORTN_FINISHED);
	CHECK(cortn.c_reason == CORTN_YIELD_FINISHED);
	CHECK(data.step == 2);
	CHECK(data.value == 42);
	CHECK(cortn_stack_used(&cortn) > 0);
	CHECK(cortn_resume(&cortn) == CORTN_ERR_STATE);
}

static void test_wait_reason(enum cortn_yield_reason reason,
			     enum cortn_state state)
{
	struct cortn cortn;
	struct wait_data data;

	data.reason = reason;
	data.resumed = 0;
	CHECK(cortn_init(&cortn, wait_stack, sizeof(wait_stack),
			 wait_entry, &data) == CORTN_SUCCESS);
	CHECK(cortn_resume(&cortn) == CORTN_SUCCESS);
	CHECK(cortn.c_state == state);
	CHECK(cortn_resume(&cortn) == CORTN_ERR_STATE);
	CHECK(cortn_wake(&cortn) == CORTN_SUCCESS);
	CHECK(cortn_resume(&cortn) == CORTN_SUCCESS);
	CHECK(cortn.c_state == CORTN_FINISHED);
	CHECK(data.resumed == 1);
}

static void test_wait(void)
{
	test_wait_reason(CORTN_YIELD_WAIT_READ, CORTN_WAIT_READ);
	test_wait_reason(CORTN_YIELD_WAIT_WRITE, CORTN_WAIT_WRITE);
}

static void test_round_robin(void)
{
	struct cortn cortn[TEST_COROUTINES];
	struct round_data data[TEST_COROUTINES];
	int i;
	int round;

	order_count = 0;
	for (i = 0; i < TEST_COROUTINES; i++) {
		data[i].id = i + 1;
		CHECK(cortn_init(&cortn[i], round_stack[i],
				 sizeof(round_stack[i]), round_entry,
				 &data[i]) == CORTN_SUCCESS);
	}
	for (round = 0; round <= ROUND_COUNT; round++) {
		for (i = 0; i < TEST_COROUTINES; i++)
			CHECK(cortn_resume(&cortn[i]) == CORTN_SUCCESS);
	}
	CHECK(order_count == TEST_COROUTINES * ROUND_COUNT);
	for (i = 0; i < order_count; i++)
		CHECK(order[i] == i % TEST_COROUTINES + 1);
}

static void test_stack_guard(void)
{
	struct cortn cortn;
	struct basic_data data;

	CHECK(cortn_init(&cortn, basic_stack, sizeof(basic_stack),
			 basic_entry, &data) == CORTN_SUCCESS);
	basic_stack[0] = 0;
	CHECK(cortn_stack_check(&cortn) == CORTN_ERR_STACK);
	CHECK(cortn_resume(&cortn) == CORTN_ERR_STACK);
	CHECK(cortn.c_state == CORTN_ERROR);

	CHECK(cortn_init(&cortn, basic_stack, sizeof(basic_stack),
			 basic_entry, &data) == CORTN_SUCCESS);
	basic_stack[sizeof(basic_stack) - 1] = 0;
	CHECK(cortn_stack_check(&cortn) == CORTN_ERR_STACK);
}

static void test_simd_and_fpu(void)
{
	struct cortn cortn;
	struct simd_value result;

	result.low = 0;
	result.high = 0;
	CHECK(cortn_init(&cortn, simd_stack, sizeof(simd_stack),
			 simd_entry, &result) == CORTN_SUCCESS);
	CHECK(cortn_resume(&cortn) == CORTN_SUCCESS);
	CHECK(cortn_resume(&cortn) == CORTN_SUCCESS);
	CHECK(cortn.c_state == CORTN_FINISHED);
}
