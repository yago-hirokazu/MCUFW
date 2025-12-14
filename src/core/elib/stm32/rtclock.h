
/* rtclock.h */

#ifndef __RTCLOCK_H__
#define __RTCLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"

void mktstr(RTC_TimeTypeDef *time, uint8_t *str);
void mkdstr(RTC_DateTypeDef *date, uint8_t *str);
void get_date_and_time(uint8_t *dstr, uint8_t *tstr);
void run_RTC_alarm(uint32_t hours, uint32_t minutes, uint32_t seconds,
                   uint32_t subseconds);
void halt_RTC_alarm(void);
int reset_backup_domain(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
