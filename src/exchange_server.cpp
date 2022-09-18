#include "exchange_server.h"
#include "epoll_impl.h"
#include "market.h"
#include "order.h"
#include "socket_impl.h"
#include "worker.h"
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unordered_set>

#pragma GCC diagnostic ignored "-Wold-style-cast"

namespace exchange_server {

struct server::state
{
  bool add_symbol(const std::string &symbol)
  {
    std::scoped_lock l{ _mutex };
    return _known_symbols.insert(symbol).second;
  }

  std::vector<std::string> list_symbols() const
  {
    std::scoped_lock l{ _mutex };
    return { _known_symbols.begin(), _known_symbols.end() };
  }

private:
  std::unordered_set<std::string> _known_symbols;
  mutable std::mutex _mutex;
};

enum class client_state { connected, identified };

struct server::client_data
{
  explicit client_data(std::shared_ptr<socket_interface> sock,
    std::weak_ptr<epoll_interface> epoll,
    worker_interface &worker)
    : message_queue{ worker }, _sock{ std::move(sock) }, _epoll{ std::move(epoll) }
  {}


  client_state state{ client_state::connected };
  std::string name{ "unidentified" };

  std::unordered_map<std::string, order> outstanding_orders;
  strand message_queue;


  std::error_code write(std::string_view message)
  {
    bool was_empty = _write_buffer.empty();
    _write_buffer.insert(_write_buffer.end(), message.begin(), message.end());
    if (was_empty || message.empty())
    {
      auto [bytes_written, err] = _sock->write(_write_buffer);

      if (!err) { _write_buffer.erase(_write_buffer.begin(), _write_buffer.begin() + bytes_written); }

      const auto add_write = !_write_buffer.empty() && was_empty;
      const auto remove_write = _write_buffer.empty() && !was_empty;
      if (add_write || remove_write)
      {
        if (const auto epoll = _epoll.lock())
        {
          std::uint32_t flags = EPOLLIN;
          if (add_write) { flags |= EPOLLOUT; }

          epoll->modify(_sock->get_fd(), flags);
        }
      }
    }

    return {};
  }

  result<std::vector<std::string>> read()
  {
    auto [bytes_read, err] = _sock->read(_temp_read_buffer);
    if (err) { return { .err = err }; }
    else if (bytes_read == 0)
    {
      return { .err = std::make_error_code(std::errc::connection_aborted) };
    }

    const auto data = std::string_view{ _temp_read_buffer.data(), static_cast<size_t>(bytes_read) };

    spdlog::trace("Received data: {}", data);

    _read_buffer.insert(_read_buffer.end(), data.begin(), data.end());

    const auto is_eol = [](char c) { return c == '\n' || c == '\r'; };

    std::vector<std::string> result;

    auto last = _read_buffer.begin();
    for (auto previous = _read_buffer.begin(), next = std::find_if(previous, _read_buffer.end(), is_eol);
         next != _read_buffer.end();)
    {
      result.emplace_back(previous, next);
      previous = last = std::find_if(next + 1, _read_buffer.end(), std::not_fn(is_eol));
      next = std::find_if(previous, _read_buffer.end(), is_eol);
    }

    _read_buffer.erase(_read_buffer.begin(), last);

    return { .result = result };
  }

private:
  std::shared_ptr<socket_interface> _sock;
  std::weak_ptr<epoll_interface> _epoll;

  std::vector<char> _write_buffer;

  std::array<char, 1024> _temp_read_buffer{};
  std::vector<char> _read_buffer;
};

server::server(std::shared_ptr<listen_socket_interface> listener,
  std::shared_ptr<epoll_interface> epoll,
  std::shared_ptr<worker_interface> worker,
  std::shared_ptr<socket_interface> control,
  std::shared_ptr<market_interface> market)
  : _worker{ std::move(worker) },
    _listener{ std::move(listener) },
    _epoll{ std::move(epoll) },
    _control{ std::move(control) },
    _market{ std::move(market) },
    _state{ std::make_shared<state>() }
{}

void server::run()
{
  _epoll->add(_listener->get_fd(), EPOLLIN);
  _epoll->add(_control->get_fd(), EPOLLIN);

  for (;;)
  {
    const auto events = _epoll->wait();
    for (const auto &evt : events)
    {
      if ((evt.events & EPOLLERR) != 0U) { throw std::runtime_error{ "Error in epoll::wait" }; }
      else if (evt.data.fd == _control->get_fd())
      {
        on_control();
      }
      else if (evt.data.fd == _listener->get_fd())
      {
        on_connect();
      }
      else if ((evt.events & EPOLLIN) != 0U)
      {
        on_read(evt.data.fd);
      }
      else if ((evt.events & EPOLLOUT) != 0U)
      {
        on_write(evt.data.fd);
      }
      else
      {
        spdlog::error("Unhandled event {}", evt.events);
      }
    }

    if (events.empty() || _should_stop) { break; }
  }
}

void server::on_control()
{
  std::uint64_t evt{};

  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  auto [bytes_read, err] = _control->read(std::span{ reinterpret_cast<char *>(&evt), sizeof(evt) });
  if (err) { throw std::system_error{ err }; }

  _should_stop = evt == 1;
}

void server::on_connect()
{
  auto [client_fd, err] = _listener->accept();
  if (client_fd)
  {
    auto fd = client_fd->get_fd();
    _epoll->add(fd, EPOLLIN);
    _client_data[fd] = std::make_shared<client_data>(std::move(client_fd), _epoll, *_worker);
  }
}

void server::on_read(int fd)
{
  auto client_data_it = _client_data.find(fd);
  if (client_data_it == _client_data.end()) { throw std::runtime_error{ fmt::format("Unknown client {}", fd) }; }

  const auto client_data = client_data_it->second;

  auto [messages, err] = client_data->read();
  if (err && err.value() != static_cast<int>(std::errc::connection_aborted))
  {
    spdlog::error("Error while reading client message");
    return;
  }
  else if (err)
  {
    spdlog::info("Client ({}) disconnected", client_data->name);
    _client_data.erase(fd);
    return;
  }
  else
  {
    for (auto &message : messages)
    {
      client_data->message_queue.post([message = std::move(message), client_data, state = _state, market = _market] {
        on_client_message(message, *client_data, *state, *market);
      });
    }
  }
}

void server::on_write(int fd)
{
  auto client_data_it = _client_data.find(fd);
  if (client_data_it == _client_data.end()) { throw std::runtime_error{ fmt::format("Unknown client {}", fd) }; }

  const auto client_data = client_data_it->second;
  client_data->message_queue.post([client_data] { client_data->write(""); });
}

namespace {
  constexpr std::string_view id_prefix{ "id" };
  constexpr std::string_view order_prefix{ "order" };
  constexpr std::string_view cancel_prefix{ "cancel" };
  constexpr std::string_view list_orders_message{ "listorders" };
  constexpr std::string_view list_symbols_message{ "listsymbols" };

  constexpr std::string_view ok_message{ "ok\n" };
  constexpr std::string_view reject_message{ "rejected\n" };
}

void server::on_client_message(const std::string &message,
  client_data &client_data,
  state &state,
  market_interface &market)
{
  spdlog::trace("Processing message: {}", message);

  if (message.starts_with(id_prefix))
  {
    on_client_id(std::string_view{ message }.substr(id_prefix.size()), client_data);
  }
  else if (message.starts_with(order_prefix))
  {
    on_client_order(std::string_view{ message }.substr(order_prefix.size()), client_data, state, market);
  }
  else if (message.starts_with(cancel_prefix))
  {
    on_client_cancel(std::string_view{ message }.substr(cancel_prefix.size()), client_data, market);
  }
  else if (message == list_orders_message)
  {
    on_client_list_orders(client_data);
  }
  else if (message == list_symbols_message)
  {
    on_client_list_symbols(client_data, state);
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

void server::on_client_order(std::string_view order_message,
  client_data &client_data,
  state &state,
  market_interface &market)
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

      if (state.add_symbol(order->symbol)) { spdlog::info("Added new symbol {}", order->symbol); }

      market.add_order(*order, [&client_data, id = order->id]() {
        client_data.message_queue.post([&client_data, id]() {
          client_data.write(fmt::format("exec{}\n", id));
          client_data.outstanding_orders.erase(id);
        });
      });

      client_data.outstanding_orders.insert(std::pair{ order->id, std::move(*order) });

      client_data.write(ok_message);
    }
    else
    {
      if (order->way != it->second.way || order->symbol != it->second.symbol)
      {
        spdlog::error("Error updating order {} from client: can only update price or quantity", order->id);

        client_data.write(reject_message);
      }
      else if (market.update_order(*order))
      {
        spdlog::info("Received update order {} from client: {} {}{}@{}",
          order->id,
          magic_enum::enum_name(order->way),
          order->quantity,
          order->symbol,
          order->price);

        it->second = *order;

        client_data.write(ok_message);
      }
      else
      {
        spdlog::error("Error updating order {} from client: rejected by market", order->id);
        client_data.write(reject_message);
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

void server::on_client_cancel(std::string_view cancel_message, client_data &client_data, market_interface &market)
{
  const auto it = client_data.outstanding_orders.find(std::string{ cancel_message });
  if (it != client_data.outstanding_orders.end())
  {
    if (market.cancel_order(it->second.id))
    {
      spdlog::info("Cancelling order {} from client {}", cancel_message, client_data.name);
      client_data.outstanding_orders.erase(it);

      client_data.write(ok_message);
    }
    else
    {
      spdlog::info("Error cancelling order {} from client {}: rejected by market", cancel_message, client_data.name);
      client_data.write(reject_message);
    }
  }
  else
  {
    spdlog::error("Unknwon order {} cancel received from client {}", cancel_message, client_data.name);
  }
}

void server::on_client_list_orders(client_data &client_data)
{
  spdlog::info("Received orderlist request from {}", client_data.name);

  std::ostringstream oss;
  std::transform(client_data.outstanding_orders.begin(),
    client_data.outstanding_orders.end(),
    std::ostream_iterator<std::string>{ oss, "\n" },
    [](const auto &element) {
      const auto &order = element.second;
      return fmt::format("{: >4}{: >8}{}{:0>4}{:0>8.0f}",
        order.id,
        order.symbol,
        order.way == order_side::buy ? '+' : '-',
        order.quantity,
        order.price);
    });

  const auto message = oss.str();

  spdlog::trace("Sending orderlist response: {}", message);
  client_data.write(message);
}

void server::on_client_list_symbols(client_data &client_data, const state &state)
{
  spdlog::info("Received symbollist request from {}", client_data.name);

  std::ostringstream oss;
  auto known_symbols = state.list_symbols();
  std::copy(known_symbols.begin(), known_symbols.end(), std::ostream_iterator<std::string>{ oss, "\n" });

  const auto message = oss.str();

  spdlog::trace("Sending symbollist response: {}", message);
  client_data.write(message);
}
}