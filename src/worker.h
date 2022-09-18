#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace exchange_server {

class worker_interface
{
public:
  virtual ~worker_interface() = default;

  virtual void post(std::function<void()> work) = 0;
};

class worker : public worker_interface
{
public:
  worker() = default;
  worker(const worker &) = delete;
  worker(worker &&) noexcept = delete;
  worker &operator=(const worker &) = delete;
  worker &operator=(worker &&) noexcept = delete;
  ~worker();

  void run();
  void post(std::function<void()> work) override;
  void stop();

private:
  std::mutex _mutex;
  std::condition_variable _condition;
  std::queue<std::function<void()>> _pending;
  bool _stop_requested{};
};

class strand : public worker_interface
{
public:
  explicit strand(worker_interface &worker) : _worker{ worker } {}
  void post(std::function<void()> work) override;

private:
  void post_next();
  void do_post(std::function<void()> work);

  worker_interface &_worker;

  std::mutex _mutex;
  std::queue<std::function<void()>> _pending;
};
}