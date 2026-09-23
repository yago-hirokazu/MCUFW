
/* qevent.c */

#include "machdep.h"
#include "qbyte.h"
#include "qevent.h"


struct qbyte eventq = {0,};
u8 eventbuf[EVENTBUFS] = {0};


// ====== machine dependent part [ENTER] ======

__STATIC_INLINE void CRITICAL_SECTION_ENTER(void)
{
	__disable_irq();
}

__STATIC_INLINE void CRITICAL_SECTION_LEAVE(void)
{
	__enable_irq();
}


static void wakeup(void)
{
	return;
}

static void iwakeup(void)
{
	return;
}

#define WAKEUPME() wakeup()
#define IWAKEUPIT() iwakeup()

// ====== machine dependent part [ENTER] ======

void
qeventinit(void)
{
	qbinit(&eventq, &eventbuf[0], EVENTBUFS);
}

enum event
gete(void)
{
	return (enum event)qbget(&eventq);
}

static int evenq(enum event e)
{
	if (e == E_NONE)
		return 0;

	CRITICAL_SECTION_ENTER();

	qbput(e, &eventq);

	CRITICAL_SECTION_LEAVE();

	return 1;
}

void
pute(enum event e)
{
	if (evenq(e))
		WAKEUPME();
}

void
ipute(enum event e)
{
	if (evenq(e))
		IWAKEUPIT();
}

/* EOF */

