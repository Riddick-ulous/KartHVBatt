set(PACKCONTROLLER_CUBE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/PackController")
set(PACKCONTROLLER_CUBE_CORE "${PACKCONTROLLER_CUBE_ROOT}/Core")
set(PACKCONTROLLER_CUBE_DRIVERS "${PACKCONTROLLER_CUBE_ROOT}/Drivers")

file(GLOB PACKCONTROLLER_HAL_SOURCES CONFIGURE_DEPENDS
  "${PACKCONTROLLER_CUBE_DRIVERS}/STM32G4xx_HAL_Driver/Src/*.c")

# STM32CubeG4 1.5.2 contains two intentional integer expressions in this
# vendor source that trigger GCC's -Wconversion. Keep every other warning,
# including -Werror, active for the complete firmware build.
set_source_files_properties(
  "${PACKCONTROLLER_CUBE_DRIVERS}/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart_ex.c"
  "${PACKCONTROLLER_CUBE_CORE}/Src/syscalls.c"
  PROPERTIES COMPILE_OPTIONS -Wno-conversion)

# The Cube interrupt file contains a generated HardFault loop. Rename only that
# symbol at compile time so the architecture-defined assembly veneer owns the
# vector without editing generated interrupt code outside USER CODE sections.
set_source_files_properties(
  "${PACKCONTROLLER_CUBE_CORE}/Src/stm32g4xx_it.c"
  PROPERTIES COMPILE_DEFINITIONS
             "HardFault_Handler=Cube_Generated_HardFault_Handler")

add_executable(packcontroller_firmware
  "${CMAKE_SOURCE_DIR}/startup/fault_entry.S"
  "${CMAKE_SOURCE_DIR}/startup/fault_entry.c"
  "${PACKCONTROLLER_CUBE_CORE}/Startup/startup_stm32g483vetx.s"
  "${PACKCONTROLLER_CUBE_CORE}/Src/main.c"
  "${PACKCONTROLLER_CUBE_CORE}/Src/stm32g4xx_hal_msp.c"
  "${PACKCONTROLLER_CUBE_CORE}/Src/stm32g4xx_it.c"
  "${PACKCONTROLLER_CUBE_CORE}/Src/syscalls.c"
  "${PACKCONTROLLER_CUBE_CORE}/Src/system_stm32g4xx.c"
  ${PACKCONTROLLER_HAL_SOURCES})

target_include_directories(packcontroller_firmware PRIVATE
  "${CMAKE_SOURCE_DIR}/platform/include"
  "${CMAKE_SOURCE_DIR}/startup/include"
  "${PACKCONTROLLER_CUBE_CORE}/Inc"
  "${PACKCONTROLLER_CUBE_DRIVERS}/STM32G4xx_HAL_Driver/Inc"
  "${PACKCONTROLLER_CUBE_DRIVERS}/STM32G4xx_HAL_Driver/Inc/Legacy")

# CMSIS 5.9.0 is supplied verbatim by STM32CubeG4 1.5.2. Treat these vendor
# headers as system headers so the project's -Werror policy applies to our
# code and HAL translation units without failing on upstream inline helpers.
target_include_directories(packcontroller_firmware SYSTEM PRIVATE
  "${PACKCONTROLLER_CUBE_DRIVERS}/CMSIS/Device/ST/STM32G4xx/Include"
  "${PACKCONTROLLER_CUBE_DRIVERS}/CMSIS/Include")

target_compile_definitions(packcontroller_firmware PRIVATE
  USE_HAL_DRIVER
  STM32G483xx)

target_link_libraries(packcontroller_firmware PRIVATE
  packcontroller_app
  packcontroller_hardfault_record
  packcontroller_project_options)

set(PACKCONTROLLER_LINKER_SCRIPT
  "${PACKCONTROLLER_CUBE_ROOT}/STM32CubeIDE/STM32G483VETX_FLASH.ld")
set(PACKCONTROLLER_MAP_FILE
  "${CMAKE_CURRENT_BINARY_DIR}/packcontroller_firmware.map")

target_link_options(packcontroller_firmware PRIVATE
  "-T${PACKCONTROLLER_LINKER_SCRIPT}"
  --specs=nano.specs
  --specs=nosys.specs
  "-Wl,-Map=${PACKCONTROLLER_MAP_FILE}"
  -Wl,--cref
  -Wl,--gc-sections
  -Wl,--print-memory-usage)

set_target_properties(packcontroller_firmware PROPERTIES
  LINKER_LANGUAGE CXX
  OUTPUT_NAME packcontroller
  SUFFIX .elf)

find_program(PACKCONTROLLER_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(PACKCONTROLLER_SIZE arm-none-eabi-size REQUIRED)

add_custom_command(TARGET packcontroller_firmware POST_BUILD
  COMMAND "${PACKCONTROLLER_OBJCOPY}" -O ihex
          "$<TARGET_FILE:packcontroller_firmware>"
          "${CMAKE_CURRENT_BINARY_DIR}/packcontroller.hex"
  COMMAND "${PACKCONTROLLER_OBJCOPY}" -O binary
          "$<TARGET_FILE:packcontroller_firmware>"
          "${CMAKE_CURRENT_BINARY_DIR}/packcontroller.bin"
  COMMAND "${PACKCONTROLLER_SIZE}" "$<TARGET_FILE:packcontroller_firmware>"
  COMMENT "Creating flash images and reporting firmware size"
  VERBATIM)
