#ifndef PACKCONTROLLER_PLATFORM_ADC_H
#define PACKCONTROLLER_PLATFORM_ADC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK = 10,
  PACKCONTROLLER_ADC_CHANNEL_COUNT = 12
};

typedef enum {
  PACKCONTROLLER_ADC_RLEAK1 = 0,
  PACKCONTROLLER_ADC_VVEHI = 1,
  PACKCONTROLLER_ADC_TNTC4 = 2,
  PACKCONTROLLER_ADC_VREFINT = 3,
  PACKCONTROLLER_ADC_RLEAK2 = 4,
  PACKCONTROLLER_ADC_TNTC1 = 5,
  PACKCONTROLLER_ADC_TNTC5 = 6,
  PACKCONTROLLER_ADC_VBATT = 7,
  PACKCONTROLLER_ADC_VACCU = 8,
  PACKCONTROLLER_ADC_VDCDC = 9,
  PACKCONTROLLER_ADC_TNTC3 = 10,
  PACKCONTROLLER_ADC_TNTC2 = 11
} packcontroller_adc_channel_t;

typedef struct {
  uint16_t raw[PACKCONTROLLER_ADC_SAMPLES_PER_BLOCK]
              [PACKCONTROLLER_ADC_CHANNEL_COUNT];
  uint32_t sequence;
  uint32_t timestamp_ms;
  bool coherent;
} packcontroller_adc_block_t;

typedef struct {
  uint16_t dma_error_count;
  uint16_t dropped_block_count;
  uint16_t frame_error_count;
  bool started;
} packcontroller_adc_diagnostics_t;

bool packcontroller_platform_adc_init(void);
bool packcontroller_platform_adc_receive(packcontroller_adc_block_t *block);
packcontroller_adc_diagnostics_t packcontroller_platform_adc_diagnostics(void);
uint16_t packcontroller_platform_adc_vref_calibration_raw(void);
uint16_t packcontroller_platform_adc_vref_calibration_mv(void);

#ifdef __cplusplus
}
#endif

#endif
