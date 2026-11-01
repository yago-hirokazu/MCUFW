
/* qbyte.c */

#include "qbyte.h"

void qbinit(struct qbyte *q, u8 *buf, u16 n)
{
	q->byte = buf;
	q->round = (u16)(n - 1);
}

void qbreset(struct qbyte *q, u8 v)
{
	register u16 n = (u16)(q->round + 1);
	memset(q->byte, v, n);
	q->wp = 0;
	q->rp = 0;
}

u8 qbput(u8 c, struct qbyte *q)
{
	register int n;
	register int rp = q->rp;
	register int wp = q->wp;
	register int round = q->round;;
	u8 cc;

	n = (wp + 1) & round;
	if (rp == n) { /* queue is full */
		cc = 0;
		goto out;
	}

	q->byte[wp] = c;
	wp++;
	wp &= round;
	q->wp = wp;

	cc = c;

out:
	return cc;
}

u8 qbget(struct qbyte *q)
{
	register int rp = q->rp;
	register int wp = q->wp;
	register int round = q->round;
	u8 c = 0;

	if (rp == wp) /* queue is empty */
		c = 0;
	else {
		c = q->byte[rp];
		q->byte[rp] = 0; /* erase old char */
		rp++;
		rp &= round;
		q->rp = rp;
	}

	return c;
}

u8 qbempty(struct qbyte *q)
{
	if (q->rp == q->wp)
		return 1;
	return 0;
}

u16 qbbytes(struct qbyte *q)
{
	register int bytes = 0;
	bytes = (s16)((s16)q->wp - (s16)q->rp);
	return (u16)bytes;
}

/* EOF */

