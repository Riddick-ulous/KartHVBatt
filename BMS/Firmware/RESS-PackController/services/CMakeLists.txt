add_library(packcontroller_services STATIC
  src/can_service.cpp
  src/eeprom_driver.cpp)
target_include_directories(packcontroller_services PUBLIC include)
target_link_libraries(packcontroller_services
  PUBLIC
    packcontroller_can
    packcontroller_platform
  PRIVATE
    packcontroller_drivers
    packcontroller_project_options)
