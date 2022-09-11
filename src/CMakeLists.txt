find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)

# Generic test that uses conan libs
add_executable(intro main.cpp)
target_link_libraries(
  intro
  PUBLIC exchange_server::project_options exchange_server::project_warnings
  PRIVATE CLI11::CLI11 fmt::fmt spdlog::spdlog)

target_include_directories(intro PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
