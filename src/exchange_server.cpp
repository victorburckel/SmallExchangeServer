#include "exchange_server.h"
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <sys/socket.h>

namespace exchange_server {

server::server(int port) : _listener{ port } { _epoll.add(_listener.get_fd(), EPOLLIN); }

void server::run()
{
  for (;;) {
    const auto events = _epoll.wait();
    for (const auto &evt : events) {
      if ((evt.events & EPOLLERR) != 0U) {
        throw std::runtime_error{ "Error in epoll::wait" };
      } else if (evt.data.fd == _listener.get_fd()) {
        on_connect();
      } else if ((evt.events & EPOLLIN) != 0U) {
        on_read(evt.data.fd);
      }
    }
  }
}

void server::on_connect() const
{
  const auto client_fd = _listener.accept();
  if (client_fd) { _epoll.add(*client_fd, EPOLLIN); }
}

void server::on_read(int fd)
{
  std::array<char, 10> buffer{};
  auto bytes_read = ::recv(fd, buffer.data(), buffer.size(), 0);
  if (bytes_read < 0) {
    spdlog::error("Error while reading client message");
    return;
  }

  spdlog::info("Received message: {}", std::string_view{ buffer.data(), static_cast<size_t>(bytes_read) });
}

}