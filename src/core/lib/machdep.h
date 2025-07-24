
/* machdep.h */

#ifndef __MACHDEP_H__
#define __MACHDEP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#if defined(STM32F030x8) || defined(STM32F031x6) || defined(STM32F048xx) || defined(STM32F072xB) || defined(STM32F070xB)
#include "stm32f0xx.h"
#include "stm32f0xx_hal.h"
#include "stm32f0xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(STM32F401xE) || defined(STM32F407xx) || defined(STM32F411xE) || defined(STM32F412Zx) || defined(STM32F413xx) || defined(STM32F429xx) || defined(STM32F446xx)
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(STM32L011xx) || defined(STM32L031xx)
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"
#include "stm32l0xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(STM32L476xx) || defined(STM32L496xx) || defined(STM32L4A6xx)
#include "stm32l4xx.h"
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(STM32G031xx)
#include "stm32g0xx.h"
#include "stm32g0xx_hal.h"
#include "stm32g0xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(STM32H743xx) || defined(STM32H7B0xx) || defined(STM32H7B3xxQ) || defined(STM32H723xx) || defined(STM32H735xx)
#include "stm32h7xx.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(STM32U575xx) || defined(STM32U585xx)
#include "stm32u5xx.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(STM32C031xx)
#include "stm32c0xx.h"
#include "stm32c0xx_hal.h"
#include "stm32c0xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(STM32H563xx)
#include "stm32h5xx.h"
#include "stm32h5xx_hal.h"
#include "stm32h5xx_hal_conf.h"
#include "misra_c_simple_types.h"
#elif defined(__CYGWIN__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <stdint.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include "misra_c_simple_types.h"
#elif defined(_WIN32)
#include <stdint.h>
#include <conio.h>
#include <windows.h>
#include "stdio_adapt.h"
#include "misra_c_simple_types.h"
#define __weak __attribute__((weak))
#endif

#if defined(PRKERNELV4)
#include "bios.h"
#include "itron.h"
#include "kernel_id.h"
#include "fault.h"

/* To use interrupt numbers defined by OS */
#include "../OS/parts/include/bios_ep/chip/stm32f4xx/stm32f4xx.h"
#endif

#define __FNAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
