#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>

namespace exchange_server {
class worker
{
public:
  worker() = default;
  worker(const worker &) = delete;
  worker(worker &&) noexcept = delete;
  worker &operator=(const worker &) = delete;
  worker &operator=(worker &&) noexcept = delete;
  ~worker();

  void run();
  void post(std::function<void()> work);
  void stop();

private:
  std::mutex _mutex;
  std::condition_variable _condition;
  std::vector<std::function<void()>> _pending;
  bool _stop_requested{};
};
}