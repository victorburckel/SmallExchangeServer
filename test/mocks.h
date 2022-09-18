#include "epoll_impl.h"
#include "exchange_server.h"
#include "market.h"
#include "socket_impl.h"
#include "worker.h"

#include <gmock/gmock.h>
#include <sys/epoll.h>

namespace mocks {

class listen_socket : public exchange_server::listen_socket_interface
{
public:
  MOCK_METHOD(exchange_server::result<std::shared_ptr<exchange_server::socket_interface>>,
    accept,
    (),
    (const, override));

  MOCK_METHOD(int, get_fd, (), (const, override));
};

class epoll : public exchange_server::epoll_interface
{
public:
  MOCK_METHOD(void, add, (int fd, std::uint32_t events), (const, override));
  MOCK_METHOD(void, modify, (int fd, std::uint32_t events), (const, override));
  MOCK_METHOD(void, remove, (int fd), (const, override));

  MOCK_METHOD(std::span<epoll_event>, wait, (), (override));
};

class worker : public exchange_server::worker_interface
{
public:
  MOCK_METHOD(void, post, (std::function<void()> work), (override));
};

class socket : public exchange_server::socket_interface
{
public:
  MOCK_METHOD(exchange_server::result<std::ptrdiff_t>, read, (std::span<char> buffer), (override));
  MOCK_METHOD(exchange_server::result<std::ptrdiff_t>, write, (std::span<const char> buffer), (override));

  MOCK_METHOD(int, get_fd, (), (const, override));
};

class market : public exchange_server::market_interface
{
public:
  MOCK_METHOD(void, add_order, (const exchange_server::order &order, std::function<void()> callback), (override));
  MOCK_METHOD(bool, update_order, (const exchange_server::order &order), (override));
  MOCK_METHOD(bool, cancel_order, (const std::string &id), (override));
};

}