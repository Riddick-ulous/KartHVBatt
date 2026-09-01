#ifndef PACKCONTROLLER_PLATFORM_EEPROM_H
#define PACKCONTROLLER_PLATFORM_EEPROM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  PACKCONTROLLER_EEPROM_SIZE_BYTES = 32768U,
  PACKCONTROLLER_EEPROM_PAGE_SIZE = 64U,
  PACKCONTROLLER_EEPROM_I2C_ADDRESS_7BIT = 0x50U,
};

typedef enum {
  PACKCONTROLLER_EEPROM_IO_IDLE = 0,
  PACKCONTROLLER_EEPROM_IO_BUSY = 1,
  PACKCONTROLLER_EEPROM_IO_COMPLETE = 2,
  PACKCONTROLLER_EEPROM_IO_NOT_READY = 3,
  PACKCONTROLLER_EEPROM_IO_ERROR = 4,
} packcontroller_eeprom_io_status_t;

typedef struct {
  uint16_t communication_error_count;
  uint16_t completed_read_count;
  uint16_t completed_write_count;
  uint16_t ack_poll_count;
  bool write_protected;
} packcontroller_eeprom_diagnostics_t;

bool packcontroller_platform_eeprom_init(void);
bool packcontroller_platform_eeprom_start_read(uint16_t address,
                                               uint8_t *data,
                                               uint16_t length);
bool packcontroller_platform_eeprom_start_page_write(uint16_t address,
                                                     const uint8_t *data,
                                                     uint8_t length);
bool packcontroller_platform_eeprom_start_ack_poll(void);
packcontroller_eeprom_io_status_t packcontroller_platform_eeprom_status(void);
void packcontroller_platform_eeprom_clear_result(void);
void packcontroller_platform_eeprom_set_write_protected(bool enabled);
packcontroller_eeprom_diagnostics_t
packcontroller_platform_eeprom_diagnostics(void);

#ifdef __cplusplus
}
#endif

#endif
