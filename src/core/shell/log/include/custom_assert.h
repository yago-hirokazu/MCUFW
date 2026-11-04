#ifndef __CUSTOM_ASSERT_H__
#define __CUSTOM_ASSERT_H__

#include "log_error.h"

#define assert_warm(_cond_, _err_, _ret_) \
	do { \
		if ((_cond_) == 0) { \
			error_record((_err_), __LINE__); \
			return (_ret_); \
		} \
	} while (0)

#define assert_warm_void(_cond_, _err_) \
	do { \
		if ((_cond_) == 0) { \
			error_record((_err_), __LINE__); \
			return; \
		} \
	} while (0)

#endif
