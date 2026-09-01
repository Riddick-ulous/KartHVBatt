#ifndef PACKCONTROLLER_PLATFORM_RUNTIME_H
#define PACKCONTROLLER_PLATFORM_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t packcontroller_platform_time_us(void);
void packcontroller_platform_toggle_wdbeat(void);
void packcontroller_platform_toggle_heartbeat(void);
void packcontroller_platform_set_error_led(bool enabled);
bool packcontroller_platform_stored_hardfault_valid(void);
void packcontroller_platform_emergency_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
