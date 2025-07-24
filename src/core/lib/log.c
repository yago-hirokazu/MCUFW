
/* log.c */

#include "log.h"

char *tick_to_time(uint32_t tick)
{
	static char tstr[32];
	register int hh, mm, ss, ms, _tick = tick;
	hh = _tick / 3600000;
	_tick %= 3600000;

	mm = _tick / 60000;
	_tick %= 60000;

	ss = _tick / 1000;
	_tick %= 1000;

	ms = _tick;

	memset(tstr, 0, sizeof(tstr));
	sprintf(tstr,"%02d:%02d:%02d:%03d",hh,mm,ss,ms);
	return tstr;
}

void hexdump(uint8_t *buf, uint32_t len)
{
	hexdumpa(buf, len, 0);
}

void hexdumpa(uint8_t *buf, uint32_t len, uint32_t addr)
{
	int k;
	int _len = len;
	for (k = 0; k < _len; k++) {
		if (k % 16 == 0)
			printf("%08X| ", k+addr);
		printf("%02X", buf[k]);
		printf(((k+1) % 16 == 0) ? "\n" : " ");
	}
	printf((_len % 16 != 0) ? "\n" : "");
}

void hexdumpna(uint8_t *buf, uint32_t len)
{
	int k;
	int _len = len;
	for (k = 0; k < _len; k++) {
		printf("%02X", buf[k]);
		printf(((k+1) % 16 == 0) ? "\n" : " ");
	}
	printf((_len % 16 != 0) ? "\n" : "");
}

/* EOF */

