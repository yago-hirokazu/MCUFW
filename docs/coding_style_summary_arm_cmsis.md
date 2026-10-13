# Arm CMSIS Coding Rules

https://arm-software.github.io/CMSIS_6/latest/General/index.html

The CMSIS uses the following essential coding rules and conventions:

- Compliant with ANSI C (C99) and C++ (C++03).
- Uses ANSI C standard data types defined in **<stdint.h>**.
- Variables and parameters have a complete data type.
- Expressions for `#define` constants are enclosed in parenthesis.
- Conforms to MISRA 2012 (but does not claim MISRA compliance). MISRA rule violations are documented.

In addition, the CMSIS recommends the following conventions for identifiers:

- **CAPITAL** names to identify Core Registers, Peripheral Registers, and CPU Instructions.
- **CamelCase** names to identify function names and interrupt functions.
- **Namespace_** prefixes avoid clashes with user identifiers and provide functional groups (i.e. for peripherals, RTOS, or DSP Library).

The CMSIS is documented within the source files with:

- Comments that use the C or C++ style.
- Doxygen compliant function comments that provide:
- brief function overview.
- detailed description of the function.
- detailed parameter explanation.
- detailed information about return values.
- Doxygen comment example:
- 
```c
/**
 * @brief  Enable Interrupt in NVIC Interrupt Controller
 * @param  IRQn  interrupt number that specifies the interrupt
 * @return none.
 * Enable the specified interrupt in the NVIC Interrupt Controller.
 * Other settings of the interrupt such as priority are not affected.
 */
```
