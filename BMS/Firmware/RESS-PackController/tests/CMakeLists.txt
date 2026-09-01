include(FetchContent)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  # GoogleTest v1.14.0, pinned to the immutable commit rather than a tag.
  GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571
  GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(host)

add_test(
  NAME packcontroller.ioc_contract
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/validate_ioc.py")
