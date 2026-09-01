#ifndef PACKCONTROLLER_STARTUP_HARDFAULT_RECORD_H
#define PACKCONTROLLER_STARTUP_HARDFAULT_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PACKCONTROLLER_HARDFAULT_RECORD_MAGIC UINT32_C(0x50434652)
#define PACKCONTROLLER_HARDFAULT_RECORD_VERSION UINT32_C(1)

typedef struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
} packcontroller_exception_frame_t;

typedef struct {
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t dfsr;
  uint32_t afsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t shcsr;
} packcontroller_fault_status_t;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t exc_return;
  packcontroller_exception_frame_t frame;
  packcontroller_fault_status_t status;
  uint32_t crc32;
} packcontroller_hardfault_record_t;

uint32_t packcontroller_hardfault_record_crc32(
    const packcontroller_hardfault_record_t* record);
void packcontroller_hardfault_record_write(
    volatile packcontroller_hardfault_record_t* destination,
    const packcontroller_exception_frame_t* frame,
    const packcontroller_fault_status_t* status, uint32_t exc_return);
bool packcontroller_hardfault_record_valid(
    const volatile packcontroller_hardfault_record_t* record);

#ifdef __cplusplus
}
#endif

#endif
