#pragma once

#include <cstdint>
#include <span>
#include <vector>

struct epoll_event;

namespace exchange_server {

class epoll_impl
{
public:
  epoll_impl();
  epoll_impl(const epoll_impl &) = delete;
  epoll_impl(epoll_impl &&) noexcept = default;
  epoll_impl &operator=(const epoll_impl &) = delete;
  epoll_impl &operator=(epoll_impl &&) noexcept = default;
  ~epoll_impl();

  void add(int fd, std::uint32_t events) const;
  void remove(int fd) const;

  std::span<epoll_event> wait();

private:
  int _fd;
  std::vector<epoll_event> _events;
};

}