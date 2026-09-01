# STM32Cube generated sources

CubeMX-generated HAL/LL, CMSIS, startup, and linker files live exclusively in
this directory. `PackController` was generated with STM32CubeMX 6.9.2 and
STM32CubeG4 1.5.2 for Increment 1. Regenerate it only through
`tools/generate_stm32cube.ps1`, validate it with `tools/validate_ioc.py`, and
review the complete generated diff before committing it.

Only code inside CubeMX `USER CODE` sections may be maintained manually here.
The current additions establish safe PWM GPIO levels, start the 1 MHz TIM5
free-running counter, start the platform-owned FDCAN integration and bridge the
HAL-free runtime to Heartbeat, WDBeat and ErrorLED GPIOs. The FDCAN rings and
callbacks themselves remain in `platform/src/stm32`; generated `main.c` only
calls their public initialization boundary from a `USER CODE` section.
