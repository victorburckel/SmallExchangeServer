#include "worker.h"
#include <thread>

namespace exchange_server {

void worker::run()
{
  for (;;) {

    decltype(_pending) pending;

    {
      std::unique_lock l{ _mutex };
      _condition.wait(l);
      pending.swap(_pending);
    }

    for (const auto &work : pending) { work(); }
  }
}

void worker::post(std::function<void()> work)
{
  std::scoped_lock l{ _mutex };
  _pending.push_back(std::move(work));
  _condition.notify_all();
}
}