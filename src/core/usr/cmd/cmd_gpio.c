
/* cmd_gpio.c */

#include "machdep.h"
#include "getopt.h"

static char * const _usage[] = {
  "gpo\n",
  NULL
};

void help_gpio(void)
{
  for (int i=0; _usage[i]!=NULL; i++)
    printf("%s", _usage[i]);
}

static GPIO_TypeDef *parse_gpio(char *str, uint16_t *pin)
{
  GPIO_TypeDef *gpio = NULL;
  char c[128];
  if (pin == NULL)
    return NULL;

  memset(c, 0, sizeof(c));
  strncpy(c, str, 4);
  c[4] = '\0';

  if (c[0] != 'p')
    return NULL;

  switch (c[1])
  {
  case 'a':
  case 'A':
    gpio = GPIOA;
    break;
  case 'b':
  case 'B':
    gpio = GPIOB;
    break;
  case 'c':
  case 'C':
    gpio = GPIOC;
    break;
  case 'd':
  case 'D':
    gpio = GPIOD;
    break;
  case 'e':
  case 'E':
    gpio = GPIOE;
    break;
  case 'f':
  case 'F':
    gpio = GPIOF;
    break;
  case 'g':
  case 'G':
    gpio = GPIOG;
    break;
  default:
    return NULL;
  }

  switch (c[2])
  {
  case '0':
    *pin = GPIO_PIN_0;
    break;
  case '1':
    if      (c[3] == '0') *pin = GPIO_PIN_10;
    else if (c[3] == '1') *pin = GPIO_PIN_11;
    else if (c[3] == '2') *pin = GPIO_PIN_12;
    else if (c[3] == '3') *pin = GPIO_PIN_13;
    else if (c[3] == '4') *pin = GPIO_PIN_14;
    else if (c[3] == '5') *pin = GPIO_PIN_15;
    else                  *pin = GPIO_PIN_1;
    break;
  case '2':
    *pin = GPIO_PIN_2;
    break;
  case '3':
    *pin = GPIO_PIN_3;
    break;
  case '4':
    *pin = GPIO_PIN_4;
    break;
  case '5':
    *pin = GPIO_PIN_5;
    break;
  case '6':
    *pin = GPIO_PIN_6;
    break;
  case '7':
    *pin = GPIO_PIN_7;
    break;
  case '8':
    *pin = GPIO_PIN_8;
    break;
  case '9':
    *pin = GPIO_PIN_9;
    break;
  default:
    return NULL;
  }

  return gpio;
}

int gpo_main(int argc, char **argv)
{
  GPIO_TypeDef *gpio = NULL;
  GPIO_PinState H_or_L = GPIO_PIN_RESET;
  uint16_t pin = 0;

  static char * const help[] = {
    "gpo\tGeneral purpose output\n",
    "\t-p\tPort & Pin, e.g. PE0\n",
    "\t-l\toutput Logic 1=High 0=Low\n",
    NULL
  };

  int opt;
  optind = 1; // initialize index nto parent argv vector
  opterr = 0; // disable error messages of getopt()

  if (argc < 2)
    goto noopt;

  while ((opt = getopt(argc, argv, "hp:l:")) != -1) {
    if (opt == 'h') {
      for (int i=0; help[i]!=NULL; i++)
        printf("%s", help[i]);
      goto out;
    } else if (opt == 'p') {
      gpio = parse_gpio(optarg, &pin);
    } else if (opt == 'l') {
      H_or_L = (GPIO_PinState)strtol(optarg,NULL,0);
    } else {
      printf("invalid option: -%c\n", opt);
      return EXIT_FAILURE;
    }
  }
  goto exec;

noopt:

exec:

  if (gpio == NULL)
    return EXIT_FAILURE;

  HAL_GPIO_WritePin(gpio, pin, H_or_L);

out:
  return EXIT_SUCCESS;
}


/* EOF */

