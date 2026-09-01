#ifndef PACKCONTROLLER_PLATFORM_IO_H
#define PACKCONTROLLER_PLATFORM_IO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  bool danger_voltage_clear_n;
  bool por_state_n;
  bool sc_latched;
  bool precharge_actual;
  bool air_p_actual;
  bool air_n_actual;
  bool dcdc_actual;
} packcontroller_safety_inputs_t;

typedef struct {
  bool air_n;
  bool precharge;
  bool air_p;
  bool dcdc;
} packcontroller_switch_outputs_t;

packcontroller_safety_inputs_t packcontroller_platform_read_safety_inputs(void);
void packcontroller_platform_commit_switch_outputs(
    packcontroller_switch_outputs_t outputs);

#ifdef __cplusplus
}
#endif

#endif
