#include "worker.h"
#include <spdlog/spdlog.h>
#include <thread>

namespace exchange_server {

worker::~worker() { stop(); }

void worker::run()
{
  spdlog::info("Starting worker");

  for (;;)
  {
    decltype(_pending) pending;
    bool stop_requested{};

    {
      std::unique_lock l{ _mutex };
      _condition.wait(l, [this]() { return !_pending.empty() || _stop_requested; });
      pending.swap(_pending);
      stop_requested = _stop_requested;
    }

    if (stop_requested) { break; }

    for (const auto &work : pending) { work(); }
  }
}

void worker::post(std::function<void()> work)
{
  std::scoped_lock l{ _mutex };
  _pending.push_back(std::move(work));
  _condition.notify_all();
}

void worker::stop()
{
  spdlog::info("Stoping worker");

  std::scoped_lock l{ _mutex };
  _stop_requested = true;
}
}