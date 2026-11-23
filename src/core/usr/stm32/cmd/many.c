
/* cmd_many.c */

#include "machdep.h"
#include "getopt.h"

static char * const _usage[] = {
  "rst\n",
  "busy\n",
  "hsi\n",
  NULL
};

void help_many(void)
{
  for (int i=0; _usage[i]!=NULL; i++)
    printf("%s", _usage[i]);
}

int rst_main(int argc, char **argv)
{
  static char * const help[] = {
    "rst\tReset MCU by means of system reset\n",
    NULL
  };

  int opt;
  optind = 1; // initialize index nto parent argv vector
  opterr = 0; // disable error messages of getopt()

  if (argc < 2)
    goto noopt;

  while ((opt = getopt(argc, argv, "h")) != -1) {
    if (opt == 'h') {
      for (int i=0; help[i]!=NULL; i++)
        printf("%s", help[i]);
      goto out;
    } else {
      printf("invalid option: -%c\n", opt);
      return EXIT_FAILURE;
    }
  }
  goto exec;

noopt:

exec:

  NVIC_SystemReset();

out:
  return EXIT_SUCCESS;
}

int busy_main(int argc, char **argv)
{
  static char * const help[] = {
    "busy\tBusy loop\n",
    NULL
  };

  int opt;
  optind = 1; // initialize index nto parent argv vector
  opterr = 0; // disable error messages of getopt()

  if (argc < 2)
    goto noopt;

  while ((opt = getopt(argc, argv, "h")) != -1) {
    if (opt == 'h') {
      for (int i=0; help[i]!=NULL; i++)
        printf("%s", help[i]);
      goto out;
    } else {
      printf("invalid option: -%c\n", opt);
      return EXIT_FAILURE;
    }
  }
  goto exec;

noopt:

exec:

  while (1) {}

out:
  return EXIT_SUCCESS;
}

int hsi_main(int argc, char **argv)
{
  RCC_OscInitTypeDef oscinit;
  uint32_t trim=0;

  static char * const help[] = {
    "hsi\tRead/adjust HSI trimming value.\n",
    "\tNo option reads the currenct HSI trimming value.\n",
    "\t-a [val]\tAdjust HSI trimming value.\n",
    NULL
  };

  int opt;
  optind = 1; // initialize index nto parent argv vector
  opterr = 0; // disable error messages of getopt()

  if (argc < 2)
    goto noopt;

  while ((opt = getopt(argc, argv, "ha:")) != -1) {
    if (opt == 'h') {
      for (int i=0; help[i]!=NULL; i++)
        printf("%s", help[i]);
      goto out;
    } else if (opt == 'a') {
      trim = strtol(optarg,NULL,0);
      if (trim > 127)
        trim = 127;
    } else {
      printf("invalid option: -%c\n", opt);
      return EXIT_FAILURE;
    }
  }
  goto exec;

noopt:

  memset(&oscinit, 0, sizeof(oscinit));
  HAL_RCC_GetOscConfig(&oscinit);
  printf("HSITRIM = %d\n", oscinit.HSICalibrationValue);
  goto out;

exec:

  __HAL_RCC_HSI_CALIBRATIONVALUE_ADJUST(trim);

out:
  return EXIT_SUCCESS;
}


/* EOF */

