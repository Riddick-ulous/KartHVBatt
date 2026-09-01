#include "packcontroller/startup/hardfault_record.h"

#include <stddef.h>

static uint32_t crc32_bytes(const uint8_t* data, size_t size) {
  uint32_t crc = UINT32_C(0xFFFFFFFF);
  for (size_t index = 0U; index < size; ++index) {
    crc ^= data[index];
    for (uint32_t bit = 0U; bit < 8U; ++bit) {
      const uint32_t mask = UINT32_C(0) - (crc & UINT32_C(1));
      crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
    }
  }
  return ~crc;
}

uint32_t packcontroller_hardfault_record_crc32(
    const packcontroller_hardfault_record_t* record) {
  return crc32_bytes((const uint8_t*)record,
                     offsetof(packcontroller_hardfault_record_t, crc32));
}

void packcontroller_hardfault_record_write(
    volatile packcontroller_hardfault_record_t* destination,
    const packcontroller_exception_frame_t* frame,
    const packcontroller_fault_status_t* status, uint32_t exc_return) {
  destination->magic = PACKCONTROLLER_HARDFAULT_RECORD_MAGIC;
  destination->version = PACKCONTROLLER_HARDFAULT_RECORD_VERSION;
  destination->exc_return = exc_return;
  destination->frame = *frame;
  destination->status = *status;
  destination->crc32 = packcontroller_hardfault_record_crc32(
      (const packcontroller_hardfault_record_t*)destination);
}

bool packcontroller_hardfault_record_valid(
    const volatile packcontroller_hardfault_record_t* record) {
  const packcontroller_hardfault_record_t snapshot = *record;
  return (snapshot.magic == PACKCONTROLLER_HARDFAULT_RECORD_MAGIC) &&
         (snapshot.version == PACKCONTROLLER_HARDFAULT_RECORD_VERSION) &&
         (snapshot.crc32 == packcontroller_hardfault_record_crc32(&snapshot));
}
