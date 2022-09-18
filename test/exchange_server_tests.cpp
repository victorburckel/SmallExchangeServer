#include "epoll_impl.h"
#include "exchange_server.h"
#include "socket_impl.h"
#include "worker.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sys/epoll.h>

using ::testing::Return;
using ::testing::StrictMock;
using namespace std::literals;

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

}

namespace {
auto expect_read(std::string_view message)
{
  return [message](std::span<char> buffer) {
    std::copy(message.begin(), message.end(), buffer.begin());
    return exchange_server::result<std::ptrdiff_t>{ .result = static_cast<std::ptrdiff_t>(message.size()) };
  };
}

MATCHER_P(IsMessage, message, "Message matcher")
{
  return std::equal(arg.begin(), arg.end(), message.begin(), message.end());
}
}

class exchange_server_tests : public ::testing::Test
{
protected:
  void SetUp() override
  {
    listen = std::make_shared<StrictMock<mocks::listen_socket>>();
    epoll = std::make_shared<StrictMock<mocks::epoll>>();
    worker = std::make_shared<StrictMock<mocks::worker>>();
    control = std::make_shared<StrictMock<mocks::socket>>();
    client = std::make_shared<StrictMock<mocks::socket>>();

    EXPECT_CALL(*worker, post).WillRepeatedly([](const std::function<void()> &f) { f(); });

    EXPECT_CALL(*listen, get_fd()).WillRepeatedly(Return(100));
    EXPECT_CALL(*control, get_fd()).WillRepeatedly(Return(200));

    EXPECT_CALL(*epoll, add(100, EPOLLIN));
    EXPECT_CALL(*epoll, add(200, EPOLLIN));
  }

  std::shared_ptr<StrictMock<mocks::listen_socket>> listen;
  std::shared_ptr<StrictMock<mocks::epoll>> epoll;
  std::shared_ptr<StrictMock<mocks::worker>> worker;
  std::shared_ptr<StrictMock<mocks::socket>> control;
  std::shared_ptr<StrictMock<mocks::socket>> client;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory)
TEST_F(exchange_server_tests, can_process_order)
{
  // Events setup
  std::array events{ epoll_event{ .data = { .fd = 100 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } } };
  EXPECT_CALL(*epoll, wait()).WillOnce(Return(std::span{ events })).WillOnce(Return(std::span<epoll_event>{}));

  // Client connects
  EXPECT_CALL(*listen, accept())
    .WillOnce(Return(exchange_server::result<std::shared_ptr<exchange_server::socket_interface>>{ .result = client }));
  EXPECT_CALL(*client, get_fd()).WillRepeatedly(Return(300));
  EXPECT_CALL(*epoll, add(300, EPOLLIN));

  // Client identifies and places order
  EXPECT_CALL(*client, read)
    .WillOnce(expect_read("idclient_id\n"))
    .WillOnce(expect_read("order1234 BTCUSDT+001000010000\n"));

  EXPECT_CALL(*client, write(IsMessage("ok\n"sv)))
    .WillOnce(Return(exchange_server::result<std::ptrdiff_t>{ .result = 3 }));

  exchange_server::server server{ listen, epoll, worker, control };
  server.run();
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory)
TEST_F(exchange_server_tests, handles_client_disconnection)
{
  // Events setup
  std::array events{ epoll_event{ .data = { .fd = 100 } }, epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } } };
  EXPECT_CALL(*epoll, wait()).WillOnce(Return(std::span{ events })).WillOnce(Return(std::span<epoll_event>{}));

  // Client connects
  EXPECT_CALL(*listen, accept())
    .WillOnce(Return(exchange_server::result<std::shared_ptr<exchange_server::socket_interface>>{ .result = client }));
  EXPECT_CALL(*client, get_fd()).WillRepeatedly(Return(300));
  EXPECT_CALL(*epoll, add(300, EPOLLIN));

  // Client disconnects
  EXPECT_CALL(*client, read).WillOnce(expect_read(""));

  exchange_server::server server{ listen, epoll, worker, control };
  server.run();
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory)
TEST_F(exchange_server_tests, handles_write_buffer_full)
{
  // Events setup
  std::array events{ epoll_event{ .data = { .fd = 100 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } },
    epoll_event{ .events = EPOLLOUT, .data = { .fd = 300 } } };
  EXPECT_CALL(*epoll, wait()).WillOnce(Return(std::span{ events })).WillOnce(Return(std::span<epoll_event>{}));

  // Client connects
  EXPECT_CALL(*listen, accept())
    .WillOnce(Return(exchange_server::result<std::shared_ptr<exchange_server::socket_interface>>{ .result = client }));
  EXPECT_CALL(*client, get_fd()).WillRepeatedly(Return(300));
  EXPECT_CALL(*epoll, add(300, EPOLLIN));

  // Client identifies and places order
  EXPECT_CALL(*client, read)
    .WillOnce(expect_read("idclient_id\n"))
    .WillOnce(expect_read("order1234 BTCUSDT+001000010000\n"));

  // Response is partially written
  EXPECT_CALL(*client, write(IsMessage("ok\n"sv)))
    .WillOnce(Return(exchange_server::result<std::ptrdiff_t>{ .result = 1 }));
  EXPECT_CALL(*epoll, modify(300, EPOLLIN | EPOLLOUT));

  // Remaining is sent once ready
  EXPECT_CALL(*client, write(IsMessage("k\n"sv)))
    .WillOnce(Return(exchange_server::result<std::ptrdiff_t>{ .result = 2 }));
  EXPECT_CALL(*epoll, modify(300, EPOLLIN));

  exchange_server::server server{ listen, epoll, worker, control };
  server.run();
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory)
TEST_F(exchange_server_tests, handle_multiple_messages_in_match)
{
  // Events setup
  std::array events{ epoll_event{ .data = { .fd = 100 } }, epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } } };
  EXPECT_CALL(*epoll, wait()).WillOnce(Return(std::span{ events })).WillOnce(Return(std::span<epoll_event>{}));

  // Client connects
  EXPECT_CALL(*listen, accept())
    .WillOnce(Return(exchange_server::result<std::shared_ptr<exchange_server::socket_interface>>{ .result = client }));
  EXPECT_CALL(*client, get_fd()).WillRepeatedly(Return(300));
  EXPECT_CALL(*epoll, add(300, EPOLLIN));

  // Client identifies and places order
  EXPECT_CALL(*client, read).WillOnce(expect_read("idclient_id\norder1234 BTCUSDT+001000010000\n"));

  EXPECT_CALL(*client, write(IsMessage("ok\n"sv)))
    .WillOnce(Return(exchange_server::result<std::ptrdiff_t>{ .result = 3 }));

  exchange_server::server server{ listen, epoll, worker, control };
  server.run();
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory)
TEST_F(exchange_server_tests, can_list_orders)
{
  // Events setup
  std::array events{ epoll_event{ .data = { .fd = 100 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } } };
  EXPECT_CALL(*epoll, wait()).WillOnce(Return(std::span{ events })).WillOnce(Return(std::span<epoll_event>{}));

  // Client connects
  EXPECT_CALL(*listen, accept())
    .WillOnce(Return(exchange_server::result<std::shared_ptr<exchange_server::socket_interface>>{ .result = client }));
  EXPECT_CALL(*client, get_fd()).WillRepeatedly(Return(300));
  EXPECT_CALL(*epoll, add(300, EPOLLIN));

  // Client identifies, places order, and lists orders
  EXPECT_CALL(*client, read)
    .WillOnce(expect_read("idclient_id\n"))
    .WillOnce(expect_read("order1234 BTCUSDT+001000010000\n"))
    .WillOnce(expect_read("listorders\n"));

  EXPECT_CALL(*client, write(IsMessage("ok\n"sv)))
    .WillOnce(Return(exchange_server::result<std::ptrdiff_t>{ .result = 3 }));

  EXPECT_CALL(*client, write(IsMessage("1234 BTCUSDT+001000010000\n"sv)))
    .WillOnce(Return(exchange_server::result<std::ptrdiff_t>{ .result = 26 }));


  exchange_server::server server{ listen, epoll, worker, control };
  server.run();
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory)
TEST_F(exchange_server_tests, can_list_symbols)
{
  // Events setup
  std::array events{ epoll_event{ .data = { .fd = 100 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } },
    epoll_event{ .events = EPOLLIN, .data = { .fd = 300 } } };
  EXPECT_CALL(*epoll, wait()).WillOnce(Return(std::span{ events })).WillOnce(Return(std::span<epoll_event>{}));

  // Client connects
  EXPECT_CALL(*listen, accept())
    .WillOnce(Return(exchange_server::result<std::shared_ptr<exchange_server::socket_interface>>{ .result = client }));
  EXPECT_CALL(*client, get_fd()).WillRepeatedly(Return(300));
  EXPECT_CALL(*epoll, add(300, EPOLLIN));

  // Client identifies, places order, and lists symbols
  EXPECT_CALL(*client, read)
    .WillOnce(expect_read("idclient_id\n"))
    .WillOnce(expect_read("order1234 BTCUSDT+001000010000\n"))
    .WillOnce(expect_read("listsymbols\n"));

  EXPECT_CALL(*client, write(IsMessage("ok\n"sv)))
    .WillOnce(Return(exchange_server::result<std::ptrdiff_t>{ .result = 3 }));

  EXPECT_CALL(*client, write(IsMessage(" BTCUSDT\n"sv)))
    .WillOnce(Return(exchange_server::result<std::ptrdiff_t>{ .result = 9 }));


  exchange_server::server server{ listen, epoll, worker, control };
  server.run();
}