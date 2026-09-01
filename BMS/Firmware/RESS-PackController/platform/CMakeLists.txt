add_library(packcontroller_platform STATIC
  src/build_contract.c)

if(CMAKE_CROSSCOMPILING)
  target_sources(packcontroller_platform PRIVATE
    src/stm32/adc_dma_stm32.c
    src/stm32/eeprom_stm32.c
    src/stm32/fdcan_stm32.c)
  target_include_directories(packcontroller_platform PRIVATE
    "${CMAKE_SOURCE_DIR}/generated/can"
    "${CMAKE_SOURCE_DIR}/generated/stm32cube/PackController/Core/Inc"
    "${CMAKE_SOURCE_DIR}/generated/stm32cube/PackController/Drivers/STM32G4xx_HAL_Driver/Inc")
  target_include_directories(packcontroller_platform SYSTEM PRIVATE
    "${CMAKE_SOURCE_DIR}/generated/stm32cube/PackController/Drivers/CMSIS/Device/ST/STM32G4xx/Include"
    "${CMAKE_SOURCE_DIR}/generated/stm32cube/PackController/Drivers/CMSIS/Include")
  target_compile_definitions(packcontroller_platform PRIVATE
    USE_HAL_DRIVER
    STM32G483xx)
endif()

target_include_directories(packcontroller_platform PUBLIC include)
target_link_libraries(packcontroller_platform PRIVATE packcontroller_project_options)
