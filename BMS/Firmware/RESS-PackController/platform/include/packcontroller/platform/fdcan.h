#ifndef PACKCONTROLLER_PLATFORM_FDCAN_H
#define PACKCONTROLLER_PLATFORM_FDCAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PACKCONTROLLER_CAN_BUS_1 = 1,
  PACKCONTROLLER_CAN_BUS_2 = 2
} packcontroller_can_bus_t;

typedef struct {
  uint32_t identifier;
  uint32_t timestamp_us;
  uint8_t data[64];
  uint8_t length;
  uint8_t bus;
  bool is_fd;
  bool bit_rate_switch;
  bool is_extended;
  bool high_priority;
} packcontroller_can_frame_t;

typedef struct {
  uint16_t rx_dropped;
  uint16_t tx_dropped;
  uint8_t bus_off_count;
  bool bus_off;
  bool error_passive;
  bool started;
} packcontroller_can_bus_diagnostics_t;

bool packcontroller_platform_can_init(void);
bool packcontroller_platform_can_receive(packcontroller_can_frame_t *frame);
bool packcontroller_platform_can_transmit(
    const packcontroller_can_frame_t *frame);
void packcontroller_platform_can_service(void);
packcontroller_can_bus_diagnostics_t packcontroller_platform_can_diagnostics(
    packcontroller_can_bus_t bus);

#ifdef __cplusplus
}
#endif

#endif
