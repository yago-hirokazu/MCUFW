
/* log.h */

#ifndef __LOG_H__
#define __LOG_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"


#define DEBUGLOG 0

#define debuglog(fmt, ...)				\
	printf("[%s] " fmt,				\
	tick_to_time(HAL_GetTick()), ##__VA_ARGS__)	\

extern char *tick_to_time(uint32_t tick);
extern void hexdump(uint8_t *buf, uint32_t len);
extern void hexdumpa(uint8_t *buf, uint32_t len, uint32_t addr);
extern void hexdumpna(uint8_t *buf, uint32_t len);

#if DEBUGLOG
#define LOG		debuglog
#define LOGDUMP		hexdump
#define DEBUG_ASSERT(mustBeTrue)					\
	{								\
		if (!(mustBeTrue))					\
		{							\
			LOG("ASSERT@ " __FNAME__ "%d\n", __LINE__);	\
			while (1);					\
		}							\
	}
#else
#define LOG(...)
#define LOGDUMP(...)
#define DEBUG_ASSERT(mustBeTrue) {}
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
