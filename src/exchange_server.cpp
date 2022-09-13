#include "exchange_server.h"
#include "worker.h"
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <sys/socket.h>

namespace exchange_server {

enum class ClientState { Connected, Identified };

struct server::ClientData
{
  ClientState state{ ClientState::Connected };
  std::string name;
  std::vector<char> receiveBuffer;
};

server::server(int port, std::shared_ptr<worker> worker) : _worker{ std::move(worker) }, _listener{ port }
{
  _epoll.add(_listener.get_fd(), EPOLLIN);
}

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

void server::on_connect()
{
  const auto client_fd = _listener.accept();
  if (client_fd) { _epoll.add(*client_fd, EPOLLIN); }
  _clientData[*client_fd] = std::make_shared<ClientData>();
}

void server::on_read(int fd)
{
  auto &clientData = _clientData.at(fd);

  std::array<char, 1024> buffer{};
  auto bytes_read = ::recv(fd, buffer.data(), buffer.size(), 0);
  if (bytes_read < 0) {
    spdlog::error("Error while reading client message");
    return;
  }

  const auto message = std::string_view{ buffer.data(), static_cast<size_t>(bytes_read) };

  spdlog::trace("Received message: {}", message);

  clientData->receiveBuffer.insert(clientData->receiveBuffer.end(), message.begin(), message.end());
}

}