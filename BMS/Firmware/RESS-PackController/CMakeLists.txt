cmake_minimum_required(VERSION 3.24)

project(PackController VERSION 0.0.0 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PACKCONTROLLER_BUILD_PROFILE "BOARD_BRINGUP" CACHE STRING
    "Compile-time PackController profile")
set_property(CACHE PACKCONTROLLER_BUILD_PROFILE PROPERTY STRINGS
             BOARD_BRINGUP PACK_162S2P)

if(NOT PACKCONTROLLER_BUILD_PROFILE STREQUAL "BOARD_BRINGUP" AND
   NOT PACKCONTROLLER_BUILD_PROFILE STREQUAL "PACK_162S2P")
  message(FATAL_ERROR
          "PACKCONTROLLER_BUILD_PROFILE must be BOARD_BRINGUP or PACK_162S2P")
endif()

add_library(packcontroller_project_options INTERFACE)
target_compile_features(packcontroller_project_options INTERFACE c_std_11 cxx_std_17)

if(PACKCONTROLLER_BUILD_PROFILE STREQUAL "BOARD_BRINGUP")
  target_compile_definitions(packcontroller_project_options INTERFACE
                             PACKCONTROLLER_PROFILE_BOARD_BRINGUP=1)
else()
  target_compile_definitions(packcontroller_project_options INTERFACE
                             PACKCONTROLLER_PROFILE_PACK_162S2P=1)
endif()

if(MSVC)
  string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  target_compile_options(packcontroller_project_options INTERFACE
    /W4
    /WX
    $<$<COMPILE_LANGUAGE:CXX>:/EHs-c->
    $<$<COMPILE_LANGUAGE:CXX>:/GR->)
  target_compile_definitions(packcontroller_project_options INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:_HAS_EXCEPTIONS=0>)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR
       CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(packcontroller_project_options INTERFACE
    -Wall
    -Wextra
    -Wconversion
    -Wshadow
    -Werror
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
else()
  message(FATAL_ERROR "Unsupported compiler: warning policy is not defined")
endif()

# cantools==41.3.0 declares Python >=3.10; fail at configure time instead of
# producing a misleading missing-package error in the contract/codegen gates.
find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)

add_custom_target(generate-dbc
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_dbc.py"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Generating checked-in CAN codecs from pack_controller.dbc"
  VERBATIM)

add_custom_target(check-dbc-generated
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_dbc.py" --check
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Checking generated CAN codec drift"
  VERBATIM)

add_subdirectory(platform)
add_subdirectory(drivers)
add_subdirectory(generated)
add_subdirectory(startup)
add_subdirectory(core)
add_subdirectory(services)
add_subdirectory(app)

include(CTest)
if(BUILD_TESTING)
  if(CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Host tests cannot be enabled for a cross build")
  endif()
  add_subdirectory(tests)
endif()
