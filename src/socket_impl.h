#pragma once

#include <optional>
#include <span>

namespace exchange_server {

class socket_impl_base
{
public:
  explicit socket_impl_base(int fd);
  socket_impl_base(const socket_impl_base &) = delete;
  socket_impl_base(socket_impl_base &&other) noexcept : _fd{ std::exchange(other._fd, -1) } {}
  socket_impl_base &operator=(const socket_impl_base &) = delete;
  socket_impl_base &operator=(socket_impl_base &&other) noexcept
  {
    _fd = std::exchange(other._fd, -1);
    return *this;
  }
  ~socket_impl_base();

  bool make_non_blocking() const;

  auto get_fd() const { return _fd; }

protected:
  int _fd;
};

class socket_impl : public socket_impl_base
{
public:
  using socket_impl_base::socket_impl_base;

  std::ptrdiff_t read(std::span<char> buffer);
};

class listen_socket_impl : public socket_impl_base
{
public:
  explicit listen_socket_impl(int port);

  std::optional<socket_impl> accept() const;

private:
  static constexpr int _max_connections{ 128 };
};


}