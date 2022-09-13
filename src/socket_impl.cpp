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

namespace {

void log_client_address(const struct sockaddr_in &client_addr, socklen_t len)
{
  std::array<char, NI_MAXHOST> hostname{};
  std::array<char, NI_MAXSERV> port{};

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
  if (getnameinfo((struct sockaddr *)&client_addr, len, hostname.data(), hostname.size(), port.data(), port.size(), 0)
      == 0) {
    spdlog::info("Client {}:{} connected", std::string_view{ hostname.data() }, std::string_view{ port.data() });
  } else {
    spdlog::info("Unknown client connected");
  }
}

}

namespace exchange_server {

listen_socket_impl::listen_socket_impl(int port) : _fd{ ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0) }
{
  if (_fd < 0) { throw std::runtime_error{ fmt::format("Error opening socket: {}", get_last_error()) }; }

  const int value = 1;
  if (::setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
    throw std::runtime_error{ fmt::format("Error setting socket options: {}", get_last_error()) };
  }

  struct sockaddr_in serv_addr = { .sin_family = AF_INET, .sin_port = htons(port) };
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
  if (::bind(_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    throw std::runtime_error{ fmt::format("Error binding socket: {}", get_last_error()) };
  }

  if (::listen(_fd, _max_connections) < 0) {
    throw std::runtime_error{ fmt::format("Error listening: {}", get_last_error()) };
  }
}

listen_socket_impl::~listen_socket_impl() { ::close(_fd); }

std::optional<int> listen_socket_impl::accept() const
{
  struct sockaddr_in client_addr = {};
  auto len = socklen_t{ sizeof(client_addr) };

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
  const auto fd = ::accept(_fd, (struct sockaddr *)&client_addr, &len);
  if (fd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) { spdlog::info("Error in accept: {}", get_last_error()); }

    return std::nullopt;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg,hicpp-signed-bitwise)
  if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
    spdlog::error("Error updating socket to non blocking: {}", get_last_error());
    ::close(fd);
    return std::nullopt;
  }

  log_client_address(client_addr, len);

  return fd;
}

}