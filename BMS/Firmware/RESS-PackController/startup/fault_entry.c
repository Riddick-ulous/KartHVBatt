#include "packcontroller/startup/hardfault_record.h"

#include "packcontroller/platform/runtime.h"
#include "stm32g4xx.h"

volatile packcontroller_hardfault_record_t g_packcontroller_hardfault_record
    __attribute__((section(".noinit.hardfault"), aligned(8), used));

__attribute__((noreturn)) void packcontroller_hardfault_capture(
    const packcontroller_exception_frame_t* frame, uint32_t exc_return) {
  const packcontroller_fault_status_t status = {
      SCB->CFSR,  SCB->HFSR, SCB->DFSR, SCB->AFSR,
      SCB->MMFAR, SCB->BFAR, SCB->SHCSR};

  packcontroller_hardfault_record_write(&g_packcontroller_hardfault_record,
                                        frame, &status, exc_return);
  __DSB();
  packcontroller_platform_emergency_shutdown();
  __DSB();

  for (;;) {
    __WFI();
  }
}
