
/* qbyte.h */

#ifndef __QBYTE_H__
#define __QBYTE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"

struct	qbyte {
	u8 *byte;
	u16 round;
	u16 wp;
	u16 rp;
};

void	qbinit(struct qbyte *q, u8 *buf, u16 n);
void	qbreset(struct qbyte *q, u8 v);
u8	qbput(u8 c, struct qbyte *q);
u8	qbget(struct qbyte *q);
u8	qbempty(struct qbyte *q);
u16	qbbytes(struct qbyte *q);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* #ifndef __QBYTE_H__ */
