add_library(packcontroller_core STATIC
  src/faults.cpp
  src/scheduler.cpp)

target_include_directories(packcontroller_core PUBLIC include)
target_link_libraries(packcontroller_core PRIVATE packcontroller_project_options)
