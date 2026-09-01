add_subdirectory(can)

if(CMAKE_CROSSCOMPILING)
  add_subdirectory(stm32cube)
endif()
