find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

add_library(
  server_lib SHARED
  epoll_impl.cpp
  epoll_impl.h
  exchange_server.cpp
  exchange_server.h
  order.cpp
  order.h
  result.cpp
  result.h
  scope_exit.cpp
  scope_exit.h
  socket_impl.cpp
  socket_impl.h
  utilities.cpp
  utilities.h
  worker.cpp
  worker.h)

target_include_directories(server_lib PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

target_link_libraries(
  server_lib
  PUBLIC exchange_server::project_options exchange_server::project_warnings
  PRIVATE CLI11::CLI11
          fmt::fmt
          spdlog::spdlog
          magic_enum::magic_enum)

add_library(exchange_server::server_lib ALIAS server_lib)

add_executable(server main.cpp)

target_link_libraries(
  server
  PUBLIC exchange_server::server_lib exchange_server::project_options exchange_server::project_warnings
  PRIVATE CLI11::CLI11 fmt::fmt spdlog::spdlog)

target_include_directories(server PRIVATE "${CMAKE_BINARY_DIR}/configured_files/include")
