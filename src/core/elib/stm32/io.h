
/* io.h */

#ifndef __IO_H__
#define __IO_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "machdep.h"
#include "io_adapt.h"

/*
 * Port A
 */

#define PA0 0x0000
#define PA1 0x0001
#define PA2 0x0002
#define PA3 0x0003
#define PA4 0x0004
#define PA5 0x0005
#define PA6 0x0006
#define PA7 0x0007
#define PA8 0x0008
#define PA9 0x0009
#define PA10 0x000a
#define PA11 0x000b
#define PA12 0x000c
#define PA13 0x000d
#define PA14 0x000e
#define PA15 0x000f

/*
 * Port B
 */

#define PB0 0x0100
#define PB1 0x0101
#define PB2 0x0102
#define PB3 0x0103
#define PB4 0x0104
#define PB5 0x0105
#define PB6 0x0106
#define PB7 0x0107
#define PB8 0x0108
#define PB9 0x0109
#define PB10 0x010a
#define PB11 0x010b
#define PB12 0x010c
#define PB13 0x010d
#define PB14 0x010e
#define PB15 0x010f

/*
 * Port C
 */

#define PC0 0x0200
#define PC1 0x0201
#define PC2 0x0202
#define PC3 0x0203
#define PC4 0x0204
#define PC5 0x0205
#define PC6 0x0206
#define PC7 0x0207
#define PC8 0x0208
#define PC9 0x0209
#define PC10 0x020a
#define PC11 0x020b
#define PC12 0x020c
#define PC13 0x020d
#define PC14 0x020e
#define PC15 0x020f

/*
 * Port D
 */

#define PD0 0x0300
#define PD1 0x0301
#define PD2 0x0302
#define PD3 0x0303
#define PD4 0x0304
#define PD5 0x0305
#define PD6 0x0306
#define PD7 0x0307
#define PD8 0x0308
#define PD9 0x0309
#define PD10 0x030a
#define PD11 0x030b
#define PD12 0x030c
#define PD13 0x030d
#define PD14 0x030e
#define PD15 0x030f

/*
 * Port E
 */

#define PE0 0x0400
#define PE1 0x0401
#define PE2 0x0402
#define PE3 0x0403
#define PE4 0x0404
#define PE5 0x0405
#define PE6 0x0406
#define PE7 0x0407
#define PE8 0x0408
#define PE9 0x0409
#define PE10 0x040a
#define PE11 0x040b
#define PE12 0x040c
#define PE13 0x040d
#define PE14 0x040e
#define PE15 0x040f

/*
 * Port F
 */

#define PF0 0x0500
#define PF1 0x0501
#define PF2 0x0502
#define PF3 0x0503
#define PF4 0x0504
#define PF5 0x0505
#define PF6 0x0506
#define PF7 0x0507
#define PF8 0x0508
#define PF9 0x0509
#define PF10 0x050a
#define PF11 0x050b
#define PF12 0x050c
#define PF13 0x050d
#define PF14 0x050e
#define PF15 0x050f

/*
 * Port G
 */

#define PG0 0x0600
#define PG1 0x0601
#define PG2 0x0602
#define PG3 0x0603
#define PG4 0x0604
#define PG5 0x0605
#define PG6 0x0606
#define PG7 0x0607
#define PG8 0x0608
#define PG9 0x0609
#define PG10 0x060a
#define PG11 0x060b
#define PG12 0x060c
#define PG13 0x060d
#define PG14 0x060e
#define PG15 0x060f

/*
 * Port H
 */

#define PH0 0x0700
#define PH1 0x0701
#define PH2 0x0702
#define PH3 0x0703
#define PH4 0x0704
#define PH5 0x0705
#define PH6 0x0706
#define PH7 0x0707
#define PH8 0x0708
#define PH9 0x0709
#define PH10 0x070a
#define PH11 0x070b
#define PH12 0x070c
#define PH13 0x070d
#define PH14 0x070e
#define PH15 0x070f

/*
 * Port I
 */

#define PI0 0x0800
#define PI1 0x0801
#define PI2 0x0802
#define PI3 0x0803
#define PI4 0x0804
#define PI5 0x0805
#define PI6 0x0806
#define PI7 0x0807
#define PI8 0x0808
#define PI9 0x0809
#define PI10 0x080a
#define PI11 0x080b
#define PI12 0x080c
#define PI13 0x080d
#define PI14 0x080e
#define PI15 0x080f

/*
 * Port J
 */

#define PJ0 0x0900
#define PJ1 0x0901
#define PJ2 0x0902
#define PJ3 0x0903
#define PJ4 0x0904
#define PJ5 0x0905
#define PJ6 0x0906
#define PJ7 0x0907
#define PJ8 0x0908
#define PJ9 0x0909
#define PJ10 0x090a
#define PJ11 0x090b
#define PJ12 0x090c
#define PJ13 0x090d
#define PJ14 0x090e
#define PJ15 0x090f

/*
 * Port K
 */

#define PK0 0x0a00
#define PK1 0x0a01
#define PK2 0x0a02
#define PK3 0x0a03
#define PK4 0x0a04
#define PK5 0x0a05
#define PK6 0x0a06
#define PK7 0x0a07
#define PK8 0x0a08
#define PK9 0x0a09
#define PK10 0x0a0a
#define PK11 0x0a0b
#define PK12 0x0a0c
#define PK13 0x0a0d
#define PK14 0x0a0e
#define PK15 0x0a0f

/*
 * Port config
 */

enum io_tag {
	OPZ = 0, /* Output Push-pull hi-Z */
	OPU,     /* Output Push-pull pull-Up */
	OPD,     /* Output Push-pull pull-Down */
	OOZ,     /* Output Open-drain hi-Z */
	OOU,     /* Output Open-drain pull-Up */
	OOD,     /* Output Open-drain pull-Down */
	IFL,     /* Input FLoating */
	IPU,     /* Input Pull-Up */
	IPD,     /* Input Pull-Down */
	ANA,     /* Analog */
	IO_TAG_LIMIT
};

#define CXX IO_TAG_LIMIT

enum out_tag {
	LOW = 0,
	HIGH,
	OUT_TAG_LIMIT
};

void iop(u16 code, u8 conf);
void out(u16 code, u8 logic);
u8 in(u16 code);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
