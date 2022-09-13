#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>

namespace exchange_server {
class worker
{
public:
  void run();
  void post(std::function<void()> work);

private:
  std::mutex _mutex;
  std::condition_variable _condition;
  std::vector<std::function<void()>> _pending;
};
}