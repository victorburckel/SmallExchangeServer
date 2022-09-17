#include "exchange_server.h"
#include "order.h"
#include "worker.h"
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unordered_set>

namespace exchange_server {

struct server::state
{
  std::unordered_set<std::string> known_symbols;
};

enum class client_state { connected, identified };

struct server::client_data
{
  client_state state{ client_state::connected };
  std::string name{ "unidentified" };
  std::vector<char> buffer;
  std::unordered_map<std::string, order> outstanding_orders;
};

server::server(int port, std::shared_ptr<worker> worker)
  : _worker{ std::move(worker) }, _listener{ port }, _state{ std::make_shared<state>() }
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
      else
      {
        spdlog::error("Unhandled event {}", evt.events);
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
  auto client_data_it = _client_data.find(fd);
  if (client_data_it == _client_data.end())
  {
    spdlog::error("Unknown client {}", fd);
    ::close(fd);
  }
  const auto client_data = client_data_it->second;

  std::array<char, 1024> buffer{};
  auto bytes_read = ::recv(fd, buffer.data(), buffer.size(), 0);
  if (bytes_read < 0)
  {
    spdlog::error("Error while reading client message");
    return;
  }
  else if (bytes_read == 0)
  {

    spdlog::info("Client ({}) disconnected", client_data->name);
    ::close(fd);
    _client_data.erase(fd);
    return;
  }

  const auto message = std::string_view{ buffer.data(), static_cast<size_t>(bytes_read) };

  spdlog::trace("Received message: {}", message);

  client_data->buffer.insert(client_data->buffer.end(), message.begin(), message.end());

  const auto is_eol = [](char c) { return c == '\n' || c == '\r'; };
  auto it = std::find_if(client_data->buffer.begin(), client_data->buffer.end(), is_eol);
  if (it != client_data->buffer.end())
  {
    _worker->post([message = std::string{ client_data->buffer.begin(), it }, client_data, state = _state] {
      on_client_message(message, *client_data, *state);
    });

    client_data->buffer.erase(
      client_data->buffer.begin(), std::find_if(++it, client_data->buffer.end(), std::not_fn(is_eol)));
  }
}

namespace {
  constexpr std::string_view id_prefix{ "id" };
  constexpr std::string_view order_prefix{ "order" };
  constexpr std::string_view cancel_prefix{ "cancel" };
}

void server::on_client_message(const std::string &message, client_data &client_data, state &state)
{
  spdlog::trace("Processing message: {}", message);

  if (message.starts_with(id_prefix))
  {
    on_client_id(std::string_view{ message }.substr(id_prefix.size()), client_data);
  }
  else if (message.starts_with(order_prefix))
  {
    on_client_order(std::string_view{ message }.substr(order_prefix.size()), client_data, state);
  }
  else if (message.starts_with(cancel_prefix))
  {
    on_client_cancel(std::string_view{ message }.substr(cancel_prefix.size()), client_data);
  }
  else
  {
    spdlog::error("Unhandled message: {}", message);
  }
}


void server::on_client_id(std::string_view id_message, client_data &client_data)
{
  client_data.name = std::string{ id_message };
  client_data.state = client_state::identified;
  spdlog::info("Client authentified as {}", client_data.name);
}

void server::on_client_order(std::string_view order_message, client_data &client_data, state &state)
{
  if (auto order = parse_order(order_message); order && client_data.state == client_state::identified)
  {
    const auto it = client_data.outstanding_orders.find(order->id);
    if (it == client_data.outstanding_orders.end())
    {
      spdlog::info("Received new order {} from client: {} {}{}@{}",
        order->id,
        magic_enum::enum_name(order->way),
        order->quantity,
        order->symbol,
        order->price);

      if (state.known_symbols.insert(order->symbol).second) { spdlog::info("Added new symbol {}", order->symbol); }
      client_data.outstanding_orders.insert(std::pair{ order->id, std::move(*order) });
    }
    else
    {
      if (order->way != it->second.way || order->symbol != it->second.symbol)
      {
        spdlog::error("Error updating order {} from client: can only update price or quantity", order->id);
      }
      else
      {
        spdlog::info("Received update order {} from client: {} {}{}@{}",
          order->id,
          magic_enum::enum_name(order->way),
          order->quantity,
          order->symbol,
          order->price);

        it->second = *order;
      }
    }
  }
  else if (client_data.state != client_state::identified)
  {
    spdlog::error("Received order \"{}\" from unidentified client", order_message);
  }
  else
  {
    spdlog::error("Received invalid order \"{}\" from {}", order_message, client_data.name);
  }
}

void server::on_client_cancel(std::string_view cancel_message, client_data &client_data)
{
  const auto it = client_data.outstanding_orders.find(std::string{ cancel_message });
  if (it != client_data.outstanding_orders.end())
  {
    spdlog::info("Cancelling order {} from client {}", cancel_message, client_data.name);
    client_data.outstanding_orders.erase(it);
  }
  else
  {
    spdlog::error("Unknwon order {} cancel received from client {}", cancel_message, client_data.name);
  }
}
}