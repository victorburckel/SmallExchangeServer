#pragma once

#include "result.h"

#include <memory>
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

  std::error_code make_non_blocking() const;

protected:
  int _fd;
};

class socket_interface
{
public:
  virtual ~socket_interface() = default;

  virtual result<std::ptrdiff_t> read(std::span<char> buffer) = 0;
  virtual result<std::ptrdiff_t> write(std::span<const char> buffer) = 0;

  virtual int get_fd() const = 0;
};

class socket_impl
  : public socket_impl_base
  , public socket_interface
{
public:
  using socket_impl_base::socket_impl_base;

  result<std::ptrdiff_t> read(std::span<char> buffer) override;
  result<std::ptrdiff_t> write(std::span<const char> buffer) override;

  int get_fd() const override { return _fd; }
};

class listen_socket_interface
{
public:
  virtual ~listen_socket_interface() = default;

  virtual result<std::shared_ptr<socket_interface>> accept() const = 0;

  virtual int get_fd() const = 0;
};

class listen_socket_impl
  : public socket_impl_base
  , public listen_socket_interface
{
public:
  explicit listen_socket_impl(int port);

  result<std::shared_ptr<socket_interface>> accept() const override;

  int get_fd() const override { return _fd; }

private:
  static constexpr int _max_connections{ 128 };
};


}