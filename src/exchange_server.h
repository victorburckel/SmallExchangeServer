#pragma once

#include "epoll_impl.h"
#include "socket_impl.h"
#include <memory>
#include <unordered_map>

namespace exchange_server {

class worker;

class server
{
public:
  explicit server(int port, std::shared_ptr<worker> worker);

  void run();

private:
  void on_connect();
  void on_read(int fd);
  void on_write(int fd);

  struct client_data;

  static void on_client_message(const std::string &message, client_data &);

  std::shared_ptr<worker> _worker;
  exchange_server::listen_socket_impl _listener;
  exchange_server::epoll_impl _epoll;
  std::unordered_map<int, std::shared_ptr<client_data>> _client_data;
};

}