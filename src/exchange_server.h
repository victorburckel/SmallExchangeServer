#pragma once

#include <memory>
#include <unordered_map>

namespace exchange_server {

class listen_socket_interface;
class epoll_interface;
class worker_interface;

class server
{
public:
  explicit server(std::shared_ptr<listen_socket_interface> listener,
    std::shared_ptr<epoll_interface> epoll,
    std::shared_ptr<worker_interface> worker);

  void run();

private:
  void on_connect();
  void on_read(int fd);
  void on_write(int fd);

  struct client_data;
  struct state;

  static void on_client_message(const std::string &message, client_data &client_data, state &state);
  static void on_client_id(std::string_view id_message, client_data &client_data);
  static void on_client_order(std::string_view order_message, client_data &client_data, state &state);
  static void on_client_cancel(std::string_view cancel_message, client_data &client_data);

  std::shared_ptr<worker_interface> _worker;
  std::shared_ptr<exchange_server::listen_socket_interface> _listener;
  std::shared_ptr<exchange_server::epoll_interface> _epoll;
  std::unordered_map<int, std::shared_ptr<client_data>> _client_data;
  std::shared_ptr<state> _state;
};

}