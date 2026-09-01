add_library(packcontroller_app STATIC
  src/build_profile.cpp
  src/developer_session.cpp
  src/measurements.cpp
  src/nvm.cpp
  src/runtime.cpp)

target_include_directories(packcontroller_app PUBLIC include)
target_link_libraries(packcontroller_app
  PUBLIC
    packcontroller_core
    packcontroller_services
    packcontroller_platform
    packcontroller_hardfault_record
  PRIVATE packcontroller_project_options)
