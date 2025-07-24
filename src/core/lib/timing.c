
/* timing.h */

#include "timing.h"

int time_elapsed(uint32_t now, uint32_t future)
{
	volatile int32_t sub = (int32_t)(now) - (int32_t)(future);
	return (sub >= 0);
}

/* EOF */

