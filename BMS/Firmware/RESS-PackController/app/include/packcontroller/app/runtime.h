#ifndef PACKCONTROLLER_APP_RUNTIME_H
#define PACKCONTROLLER_APP_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  PACKCONTROLLER_RUNTIME_TASK_COUNT = 5,
  PACKCONTROLLER_RUNTIME_ADC_CHANNEL_COUNT = 12
};

typedef struct {
  uint32_t run_count[PACKCONTROLLER_RUNTIME_TASK_COUNT];
  uint32_t last_runtime_us[PACKCONTROLLER_RUNTIME_TASK_COUNT];
  uint32_t max_runtime_us[PACKCONTROLLER_RUNTIME_TASK_COUNT];
  uint32_t max_start_lateness_us[PACKCONTROLLER_RUNTIME_TASK_COUNT];
  uint32_t deadline_misses[PACKCONTROLLER_RUNTIME_TASK_COUNT];
  uint32_t consecutive_deadline_misses[PACKCONTROLLER_RUNTIME_TASK_COUNT];
  uint32_t overrun_limit_violations[PACKCONTROLLER_RUNTIME_TASK_COUNT];
  uint32_t skipped_releases[PACKCONTROLLER_RUNTIME_TASK_COUNT];
  uint32_t watchdog_edges;
  uint32_t heartbeat_edges;
  uint32_t loop_count;
  uint32_t idle_iterations;
  uint32_t last_loop_gap_us;
  uint32_t max_loop_gap_us;
  uint64_t fault_active_low;
  uint64_t fault_active_high;
  uint64_t fault_latched_low;
  uint64_t fault_latched_high;
  uint16_t adc_raw[PACKCONTROLLER_RUNTIME_ADC_CHANNEL_COUNT];
  float adc_physical[PACKCONTROLLER_RUNTIME_ADC_CHANNEL_COUNT];
  uint8_t adc_quality[PACKCONTROLLER_RUNTIME_ADC_CHANNEL_COUNT];
  uint32_t adc_sample_counter;
  uint32_t adc_timestamp_ms;
  uint32_t runtime_seconds;
  uint16_t adc_dma_error_count;
  uint16_t adc_dropped_block_count;
  uint16_t eeprom_error_count;
  uint32_t nvm_sequence;
  uint8_t nvm_initialized;
  uint8_t adc_overall_quality;
  uint8_t adc_coherent;
  uint8_t scheduler_healthy;
  uint8_t stored_hardfault;
} packcontroller_runtime_diagnostics_t;

extern volatile uint32_t g_packcontroller_debug_stall_scheduler;
extern volatile packcontroller_runtime_diagnostics_t
    g_packcontroller_runtime_diagnostics;

void packcontroller_runtime_init(void);
void packcontroller_runtime_poll(void);

#ifdef __cplusplus
}
#endif

#endif
