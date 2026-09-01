add_executable(packcontroller_host_smoke
  can_service_test.cpp
  core_test.cpp
  developer_session_test.cpp
  hardfault_record_test.cpp
  measurements_test.cpp
  nvm_test.cpp
  smoke_test.cpp)
target_link_libraries(packcontroller_host_smoke PRIVATE
  packcontroller_app
  packcontroller_core
  packcontroller_hardfault_record
  packcontroller_project_options
  GTest::gtest_main)

add_test(NAME packcontroller.host_smoke COMMAND packcontroller_host_smoke)
