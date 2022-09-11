#include "epoll_impl.h"
#include "utilities.h"
#include <fmt/core.h>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>

namespace exchange_server {

epoll_impl::epoll_impl() : _fd{ ::epoll_create1(0) }, _events{ 1024 }
{
  if (_fd < 0) { throw std::runtime_error{ fmt::format("Error creating epoll: {}", get_last_error()) }; }
}

epoll_impl::~epoll_impl() { ::close(_fd); }

void epoll_impl::add(int fd, std::uint32_t events) const
{
  struct epoll_event evt
  {
    .events = events, .data = {.fd = fd }
  };

  if (epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &evt) < 0) {
    throw std::runtime_error{ fmt::format("Error adding file descriptor to epoll: {}", get_last_error()) };
  }
}

void epoll_impl::remove(int fd) const
{
  if (epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
    throw std::runtime_error{ fmt::format("Error removing file descriptor from epoll: {}", get_last_error()) };
  }
}

std::span<epoll_event> epoll_impl::wait()
{
  int result = epoll_wait(_fd, _events.data(), static_cast<int>(_events.size()), -1);
  if (result < 0) {
    throw std::runtime_error{ fmt::format("Error removing file descriptor from epoll: {}", get_last_error()) };
  }

  return std::span{ _events }.subspan(0, static_cast<size_t>(result));
}

}