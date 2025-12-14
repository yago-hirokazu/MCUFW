
/* rtclock.c */

#include "rtclock.h"
#include "rtc.h"

void mktstr(RTC_TimeTypeDef *time, uint8_t *str)
{
  if (time==NULL || str==NULL)
    return;
  // Display time Format : hh:mm:ss
  sprintf((char *)str, "%.2d:%.2d:%.2d", time->Hours, time->Minutes, time->Seconds);
}

void mkdstr(RTC_DateTypeDef *date, uint8_t *str)
{
  if (date==NULL || str==NULL)
    return;
  // Display date Format : mm-dd
  sprintf((char*)str, "20%.2d-%.2d-%.2d", date->Year, date->Month, date->Date);
}

void get_date_and_time(uint8_t *dstr, uint8_t *tstr)
{
  RTC_DateTypeDef date;
  RTC_TimeTypeDef time;

  // You must call HAL_RTC_GetDate() after HAL_RTC_GetTime()
  // to unlock the values in the higher-order calendar shadow registers
  // to ensure consistency between the time and date values.
  // Reading RTC current time locks the values in calendar shadow registers
  // until Current date is read.
  HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);

  if (tstr != NULL)
    mktstr(&time, tstr);

  if (dstr != NULL)
    mkdstr(&date, dstr);
}

void run_RTC_alarm(uint32_t hours, uint32_t minutes, uint32_t seconds,
              uint32_t subseconds)
{
  HAL_StatusTypeDef stat=HAL_OK;
  RTC_TimeTypeDef time;
  RTC_AlarmTypeDef alarm;
  memset(&time, 0, sizeof(time));
  memset(&alarm, 0, sizeof(alarm));
  uint32_t h, m, s;

  HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
  h = time.Hours;
  m = time.Minutes;
  s = time.Seconds + seconds;
  if (s > 60)
  {
    m += 1;
    s %= 60;
  }
  if (m > 60)
  {
    h += 1;
    m %= 60;
  }

  alarm.Alarm = RTC_ALARM_A;
  alarm.AlarmDateWeekDay = RTC_WEEKDAY_MONDAY;
  alarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_WEEKDAY;
  alarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
  alarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_NONE;
  alarm.AlarmTime.TimeFormat = time.TimeFormat;
  alarm.AlarmTime.Hours = h;
  alarm.AlarmTime.Minutes = m;
  alarm.AlarmTime.Seconds = s;
  alarm.AlarmTime.SubSeconds = 0;

  stat = HAL_RTC_SetAlarm_IT(&hrtc, &alarm, RTC_FORMAT_BIN);
  if (stat != HAL_OK)
  {
    printf("HAL_RTC_SetAlarm_IT()=%d\n",stat);
    return;
  }
}

void halt_RTC_alarm(void)
{
  HAL_StatusTypeDef stat=HAL_OK;

  stat=HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
  if (stat != HAL_OK)
  {
    printf("HAL_NVIC_DeactivateAlarm()=%d\n",stat);
  }
}

int reset_backup_domain(void)
{
	uint32_t temp_reg=0, tickstart=0;

  /* Store the content of BDCR register before the reset of Backup Domain */
  temp_reg = (RCC->BDCR & ~(RCC_BDCR_RTCSEL));
  /* RTC Clock selection can be changed only if the Backup Domain is reset */
  __HAL_RCC_BACKUPRESET_FORCE();
  __HAL_RCC_BACKUPRESET_RELEASE();
  /* Restore the Content of BDCR register */
  RCC->BDCR = temp_reg;

  /* Wait for LSERDY if LSE was enabled */
  if (HAL_IS_BIT_SET(temp_reg, RCC_BDCR_LSEON))
  {
    /* Get Start Tick */
    tickstart = HAL_GetTick();

    /* Wait till LSE is ready */
    while(__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET)
    {
      if((HAL_GetTick() - tickstart) > RCC_LSE_TIMEOUT_VALUE)
      {
        return EXIT_FAILURE;
      }
    }
  }
  return EXIT_SUCCESS;
}

/* EOF */

