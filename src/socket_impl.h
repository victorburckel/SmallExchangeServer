#pragma once

#include <optional>

namespace exchange_server {

class listen_socket_impl
{
public:
  explicit listen_socket_impl(int port);
  listen_socket_impl(const listen_socket_impl &) = delete;
  listen_socket_impl(listen_socket_impl &&) noexcept = default;
  listen_socket_impl &operator=(const listen_socket_impl &) = delete;
  listen_socket_impl &operator=(listen_socket_impl &&) noexcept = default;
  ~listen_socket_impl();

  std::optional<int> accept() const;

  auto get_fd() const { return _fd; }

private:
  int _fd;
  static constexpr int _max_connections{ 128 };
};

}