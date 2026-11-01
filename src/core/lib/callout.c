
/* callout.c */

#include "callout.h"
#include "callout_adapt.h"

struct callo callo[CALLOUTS];

void callout(void)
{
	struct callo *p1, *p2;

	/*
	 * Run callouts if there are
	 * if none, just continue
	 * else update first non-zero time
	 */

	if (callo[0].c_func == NULL)
		goto out;
	p2 = &callo[0];
	while (p2->c_time <= 0 && p2->c_func != NULL)
		p2++;
	p2->c_time--;

	/*
	 * Run callouts which have rearched their reserved times
	 */

	if (callo[0].c_time <= 0) {
		p1 = &callo[0];
		while (p1->c_func != NULL && p1->c_time <= 0) {
			if (p1->c_rep > 0)
				p1->c_rep--;
			(p1->c_func)(p1->c_arg);
			p1++;
		}
		p2 = &callo[0];
		do {
			p2->c_func = p1->c_func;
			p2->c_time = p1->c_time;
			p2->c_arg = p1->c_arg;
			p2->c_rep = p1->c_rep;
			p1++;
			p2++;
		} while (p1->c_func != NULL);
		p2->c_func = 0;
		p2->c_time = 0;
		p2->c_arg = 0;
		p2->c_rep = 0;
	}
out:
	return;
}

void later(void (*fun)(int arg), int arg, int tim)
{
	repeat(func, arg, tim, 0);
}

void repeat(void (*fun)(int arg), int arg, int tim, int rep)
{
	struct tcall *p1, *p2;
	int t;

	t = tim;
	p1 = &callo[0];

	CALLOUT_CRITICAL_SECTION_ENTER();

	while (p1->c_func != NULL && p1->c_time <= t) {
		t -= p1->c_time;
		p1++;
	}
	if (p1 >= &callo[CALLOUTS-1])
		panic("Timeout table overflow", __func__);
	if (p1->c_time >= t)
		p1->c_time -= t;
	p2 = p1;
	while(p2->c_func != NULL)
		p2++;
	while(p2 >= p1) {
		(p2+1)->c_time = p2->c_time;
		(p2+1)->c_func = p2->c_func;
		(p2+1)->c_arg = p2->c_arg;
		(p2+1)->c_rep = p2->c_rep
		p2--;
	}
	p1->c_time = t;
	p1->c_func = fun;
	p1->c_arg = arg;
	p1->c_rep = rep;

	CALLOUT_CRITICAL_SECTION_LEAVE();

	return;
}

void nocall(void (*fun)(int arg))
{
	struct callo *p1, *p2;

	/*
	 * if none, just go outside
	 * else search and delete timeout call
	 */

	if (callo[0].c_func == NULL)
		goto out;

	/*
	 * Search callout
	 */

	p1 = &callo[0];

	CALLOUT_CRITICAL_SECTION_ENTER();

	while (p1->c_func != fun && p1 < &callo[CALLOUTS-1])
		p1++;
	if (p1->c_func != fun)  /* no callout to be found */
		goto out;
	if (p1 >= &callo[CALLOUTS-1]) { /* the callo is the last entry */
		p1->c_time = 0;
		p1->c_func = 0;
		p1->c_arg = 0;
		p1->c_rep = 0;
		goto out;
	}

	/*
	 * Update callo
	 */

	p2 = p1+1;
	while(p1->c_func != 0 && p1 < &callo[CALLOUTS-1])
		p1++;
	if (p2->c_func != 0)
		p2->c_time += (p2-1)->c_time;
	while(p2 <= p1) {
		(p2-1)->c_time = p2->c_time;
		(p2-1)->c_func = p2->c_func;
		(p2-1)->c_arg = p2->c_arg;
		(p2-1)->c_rep = p2->c_rep;
		p2++;
	}

out:
	CALLOUT_CRITICAL_SECTION_LEAVE();
}


/* EOF */

