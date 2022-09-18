#include "socket_impl.h"
#include "utilities.h"
#include <fcntl.h>
#include <fmt/core.h>
#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace {

void log_client_address(const struct sockaddr_in &client_addr, socklen_t len)
{
  std::array<char, NI_MAXHOST> hostname{};
  std::array<char, NI_MAXSERV> port{};

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,clang-diagnostic-old-style-cast)
  if (getnameinfo((struct sockaddr *)&client_addr, len, hostname.data(), hostname.size(), port.data(), port.size(), 0)
      == 0)
  {
    spdlog::info("Client {}:{} connected", std::string_view{ hostname.data() }, std::string_view{ port.data() });
  }
  else
  {
    spdlog::info("Unknown client connected");
  }
}
}

namespace exchange_server {

socket_impl_base::socket_impl_base(int fd) : _fd{ fd }
{
  if (_fd < 0) { throw std::system_error{ get_last_error() }; }
}

socket_impl_base::~socket_impl_base()
{
  if (_fd >= 0) { ::close(_fd); }
}

std::error_code socket_impl_base::make_non_blocking() const
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg,hicpp-signed-bitwise)
  if (fcntl(_fd, F_SETFL, fcntl(_fd, F_GETFL, 0) | O_NONBLOCK) < 0) { return get_last_error(); }

  return {};
}

result<std::ptrdiff_t> socket_impl::read(std::span<char> buffer)
{
  const auto bytes_read = ::recv(_fd, buffer.data(), buffer.size(), 0);
  if (bytes_read < 0) { return { .err = std::make_error_code(static_cast<std::errc>(errno)) }; }

  return { .result = bytes_read };
}

result<std::ptrdiff_t> socket_impl::write(std::span<const char> buffer)
{
  const auto bytes_written = ::send(_fd, buffer.data(), buffer.size(), 0);
  if (bytes_written < 0) { return { .err = std::make_error_code(static_cast<std::errc>(errno)) }; }

  return { .result = bytes_written };
}

listen_socket_impl::listen_socket_impl(int port) : socket_impl_base{ ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0) }
{
  const int value = 1;
  if (::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0)
  {
    throw std::system_error{ get_last_error() };
  }

  struct sockaddr_in serv_addr = { .sin_family = AF_INET, .sin_port = htons(static_cast<uint16_t>(port)) };
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,clang-diagnostic-old-style-cast)
  if (::bind(_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    throw std::system_error{ get_last_error() };
  }

  if (::listen(_fd, _max_connections) < 0) { throw std::system_error{ get_last_error() }; }
}

result<std::shared_ptr<socket_interface>> listen_socket_impl::accept() const
{
  struct sockaddr_in client_addr = {};
  auto len = socklen_t{ sizeof(client_addr) };

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast,clang-diagnostic-old-style-cast)
  const auto fd = ::accept(_fd, (struct sockaddr *)&client_addr, &len);
  if (fd < 0) { return { .err = get_last_error() }; }

  auto result = std::make_shared<socket_impl>(fd);
  if (const auto err = result->make_non_blocking()) { return { .err = err }; }

  log_client_address(client_addr, len);

  return { .result = std::move(result) };
}
}