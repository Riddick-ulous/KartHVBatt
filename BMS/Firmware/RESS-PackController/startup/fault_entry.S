.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.section .text.HardFault_Handler,"ax",%progbits
.global HardFault_Handler
.type HardFault_Handler, %function
.thumb_func
HardFault_Handler:
  tst lr, #4
  ite eq
  mrseq r0, msp
  mrsne r0, psp
  mov r1, lr
  tst r1, #0x10
  it eq
  addeq r0, r0, #0x48
  b packcontroller_hardfault_capture
.size HardFault_Handler, .-HardFault_Handler
