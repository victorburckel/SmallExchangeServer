#include "epoll_impl.h"
#include "socket_impl.h"

namespace exchange_server {

class server
{
public:
  explicit server(int port);

  void run();

private:
  void on_connect() const;
  static void on_read(int fd);
  void on_write(int fd);

  exchange_server::listen_socket_impl _listener;
  exchange_server::epoll_impl _epoll;
};

}