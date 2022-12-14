cmake_minimum_required(VERSION 3.15...3.23)

project(CmakeConfigPackageTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
  enable_testing()

  find_package(exchange_server CONFIG REQUIRED) # for intro, project_options, ...

  if(NOT TARGET exchange_server::project_options)
    message(FATAL_ERROR "Requiered config package not found!")
    return() # be strictly paranoid for Template Janitor github action! CK
  endif()
endif()

# ---- Dependencies ----

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

# Provide a simple smoke test to make sure that the CLI works and can display a --help message
add_test(NAME cli.has_help COMMAND server --help)

# Provide a test to verify that the version being reported from the application
# matches the version given to CMake. This will be important once you package
# your program. Real world shows that this is the kind of simple mistake that is easy
# to make, but also easy to test for.
add_test(NAME cli.version_matches COMMAND server --version)
set_tests_properties(cli.version_matches PROPERTIES PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}")

add_executable(
  tests
  exchange_server_tests.cpp
  mocks.cpp
  mocks.h
  order_tests.cpp
  strand_tests.cpp)
target_link_libraries(
  tests
  PRIVATE exchange_server::server_lib
          exchange_server::project_warnings
          exchange_server::project_options
          GTest::gtest
          GTest::gtest_main
          GTest::gmock)

# automatically discover tests that are defined in catch based test files you can modify the unittests. Set TEST_PREFIX
# to whatever you want, or use different for different binaries
gtest_discover_tests(tests TEST_PREFIX "unittests." XML_OUTPUT_DIR .)
