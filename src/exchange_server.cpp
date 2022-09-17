#include "exchange_server.h"
#include "order.h"
#include "worker.h"
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <sys/socket.h>

namespace exchange_server {

enum class client_state { connected, identified };

struct server::client_data
{
  client_state state{ client_state::connected };
  std::string name;
  std::vector<char> buffer;
};

server::server(int port, std::shared_ptr<worker> worker) : _worker{ std::move(worker) }, _listener{ port }
{
  spdlog::info("Starting server on port {}", port);
  _epoll.add(_listener.get_fd(), EPOLLIN);
}

void server::run()
{
  for (;;)
  {
    const auto events = _epoll.wait();
    for (const auto &evt : events)
    {
      if ((evt.events & EPOLLERR) != 0U) { throw std::runtime_error{ "Error in epoll::wait" }; }
      else if (evt.data.fd == _listener.get_fd())
      {
        on_connect();
      }
      else if ((evt.events & EPOLLIN) != 0U)
      {
        on_read(evt.data.fd);
      }
    }
  }
}

void server::on_connect()
{
  const auto client_fd = _listener.accept();
  if (client_fd) { _epoll.add(*client_fd, EPOLLIN); }
  _client_data[*client_fd] = std::make_shared<client_data>();
}

void server::on_read(int fd)
{
  auto client_data = _client_data.at(fd);

  std::array<char, 1024> buffer{};
  auto bytes_read = ::recv(fd, buffer.data(), buffer.size(), 0);
  if (bytes_read < 0)
  {
    spdlog::error("Error while reading client message");
    return;
  }

  const auto message = std::string_view{ buffer.data(), static_cast<size_t>(bytes_read) };

  spdlog::trace("Received message: {}", message);

  client_data->buffer.insert(client_data->buffer.end(), message.begin(), message.end());

  auto it = std::find(client_data->buffer.begin(), client_data->buffer.end(), '\n');
  if (it != client_data->buffer.end())
  {
    ++it;
    _worker->post([this, message = std::string{ client_data->buffer.begin(), it }, client_data] {
      on_client_message(message, *client_data);
    });

    client_data->buffer.erase(client_data->buffer.begin(), it);
  }
}

namespace {

  constexpr std::string_view order_prefix{ "order" };
}

void server::on_client_message(const std::string &message, client_data &client_data)
{
  spdlog::trace("Processing message: {}", message);

  if (message.starts_with("id"))
  {
    client_data.name = message.substr(2);
    client_data.state = client_state::identified;
    spdlog::info("Client authentified as {}", client_data.name);
  }
  else if (message.starts_with(order_prefix))
  {
    if (client_data.state == client_state::identified)
    {
      auto order = parse_order(std::string_view{ message }.substr(order_prefix.size()));
    }
  }
}

}