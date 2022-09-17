#pragma once

#include <cstdint>
#include <span>
#include <vector>

struct epoll_event;

namespace exchange_server {

class epoll_interface
{
public:
  virtual ~epoll_interface() = default;

  virtual void add(int fd, std::uint32_t events) const = 0;
  virtual void modify(int fd, std::uint32_t events) const = 0;
  virtual void remove(int fd) const = 0;

  virtual std::span<epoll_event> wait() = 0;
};

class epoll_impl : public epoll_interface
{
public:
  epoll_impl();
  epoll_impl(const epoll_impl &) = delete;
  epoll_impl(epoll_impl &&) noexcept = delete;
  epoll_impl &operator=(const epoll_impl &) = delete;
  epoll_impl &operator=(epoll_impl &&) noexcept = delete;
  ~epoll_impl();

  void add(int fd, std::uint32_t events) const override;
  void modify(int fd, std::uint32_t events) const override;
  void remove(int fd) const override;

  std::span<epoll_event> wait() override;

private:
  int _fd;
  std::vector<epoll_event> _events;
};

}