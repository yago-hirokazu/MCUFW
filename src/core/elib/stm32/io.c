
/* io.c */

#include "io.h"

#define NULPIN GPIO_PIN_0
#define NULSPD GPIO_SPEED_FREQ_LOW

const u16 pin_tab[] = {
	GPIO_PIN_0,
	GPIO_PIN_1,
	GPIO_PIN_2,
	GPIO_PIN_3,
	GPIO_PIN_4,
	GPIO_PIN_5,
	GPIO_PIN_6,
	GPIO_PIN_7,
	GPIO_PIN_8,
	GPIO_PIN_9,
	GPIO_PIN_10,
	GPIO_PIN_11,
	GPIO_PIN_12,
	GPIO_PIN_13,
	GPIO_PIN_14,
	GPIO_PIN_15
};

/* port config operations */

static void opz(u8 p, u8 b);
static void opu(u8 p, u8 b);
static void opd(u8 p, u8 b);
static void ooz(u8 p, u8 b);
static void oou(u8 p, u8 b);
static void ood(u8 p, u8 b);
static void ifl(u8 p, u8 b);
static void ipu(u8 p, u8 b);
static void ipd(u8 p, u8 b);
static void ana(u8 p, u8 b);

/* port config table */

void (* const iop_tab[])(u8 p, u8 b) = {
	opz, opu, opd,
	ooz, oou, ood,
	ifl, ipu, ipd,
	ana
};

static void code_to_index(u16 code, u8 *port, u8 *pin)
{
	if (port == NULL)
		return;
	if (pin == NULL)
		return;

	*port = (u8)((u16)(code & 0xff00) >> 8);
	*pin  = (u8)((u16)(code & 0x00ff));
}

void iop(u16 code, u8 conf)
{
	u8 port = 0;
	u8 pin = 0;

	code_to_index(code, &port, &pin);

	if (port >= PORT_TAG_LIMIT)
		return;
	if (pin >= PIN_TAG_LIMIT)
		return;
	if (conf >= IO_TAG_LIMIT)
		return;

	iop_tab[conf](port, pin);

	return;
}

void out(u16 code, u8 logic)
{
	GPIO_TypeDef *port = NULL;
	u16 pin = 0;
	u8 iport = 0;
	u8 ipin = 0;

	code_to_index(code, &iport, &ipin);

	if (iport >= PORT_TAG_LIMIT)
		return;
	if (ipin >= PIN_TAG_LIMIT)
		return;
	if (logic >= OUT_TAG_LIMIT)
		return;

	port = (GPIO_TypeDef *)port_tab[iport];
	if (port == NULL)
		return;
	pin = pin_tab[ipin];

	if (logic == HIGH)
		HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);

	return;
}

u8 in(u16 code)
{
	register GPIO_TypeDef *port = NULL;
	GPIO_PinState pstt;
	u16 pin = 0;
	u8 iport = 0;
	u8 ipin = 0;
	u8 logic = 0;

	code_to_index(code, &iport, &ipin);

	if (iport >= PORT_TAG_LIMIT)
		return 0;
	if (ipin >= PIN_TAG_LIMIT)
		return 0;

	port = (GPIO_TypeDef *)port_tab[iport];
	if (port == NULL)
		return 0;
	pin = pin_tab[ipin];

	pstt = HAL_GPIO_ReadPin(port, pin);

	if (pstt == GPIO_PIN_RESET)
		logic = LOW;
	else
		logic = HIGH;

	return logic;
}

static void opz(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_opz = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_OUTPUT_PP, /* Mode */
		.Pull  = GPIO_NOPULL,      /* Pull */
		.Speed = GPIO_SPEED   /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_opz.Pin = pin;
	HAL_GPIO_Init(port, &pconf_opz);

	return;
}

static void opu(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_opu = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_OUTPUT_PP, /* Mode */
		.Pull  = GPIO_PULLUP,      /* Pull */
		.Speed = GPIO_SPEED   /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_opu.Pin = pin;
	HAL_GPIO_Init(port, &pconf_opu);

	return;
}

static void opd(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_opd = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_OUTPUT_PP, /* Mode */
		.Pull  = GPIO_PULLDOWN,    /* Pull */
		.Speed = GPIO_SPEED   /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_opd.Pin = pin;
	HAL_GPIO_Init(port, &pconf_opd);

	return;
}

static void ooz(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_ooz = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_OUTPUT_OD, /* Mode */
		.Pull  = GPIO_NOPULL,      /* Pull */
		.Speed = GPIO_SPEED   /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_ooz.Pin = pin;
	HAL_GPIO_Init(port, &pconf_ooz);

	return;
}

static void oou(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_oou = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_OUTPUT_OD, /* Mode */
		.Pull  = GPIO_PULLUP,      /* Pull */
		.Speed = GPIO_SPEED   /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_oou.Pin = pin;
	HAL_GPIO_Init(port, &pconf_oou);

	return;
}

static void ood(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_ood = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_OUTPUT_OD, /* Mode */
		.Pull  = GPIO_PULLDOWN,    /* Pull */
		.Speed = GPIO_SPEED   /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_ood.Pin = pin;
	HAL_GPIO_Init(port, &pconf_ood);

	return;
}

static void ifl(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_ifl = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_INPUT,  /* Mode */
		.Pull  = GPIO_NOPULL,      /* Pull */
		.Speed = NULSPD            /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_ifl.Pin = pin;
	HAL_GPIO_Init(port, &pconf_ifl);

	return;
}

static void ipu(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_ipu = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_INPUT,  /* Mode */
		.Pull  = GPIO_PULLUP,      /* Pull */
		.Speed = NULSPD            /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_ipu.Pin = pin;
	HAL_GPIO_Init(port, &pconf_ipu);

	return;
}

static void ipd(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_ipd = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_INPUT,  /* Mode */
		.Pull  = GPIO_PULLDOWN,    /* Pull */
		.Speed = NULSPD            /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_ipd.Pin = pin;
	HAL_GPIO_Init(port, &pconf_ipd);

	return;
}

static void ana(u8 p, u8 b)
{
	register GPIO_TypeDef *port = (GPIO_TypeDef *)port_tab[p];
	GPIO_InitTypeDef pconf_ana = {
		.Pin   = NULPIN,           /* Pin */
		.Mode  = GPIO_MODE_ANALOG, /* Mode */
		.Pull  = GPIO_NOPULL,      /* Pull */
		.Speed = NULSPD            /* Speed */
	};
	u16 pin = pin_tab[b];

	pconf_ana.Pin = pin;
	HAL_GPIO_Init(port, &pconf_ana);

	return;
}

/* EOF */

