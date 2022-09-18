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
  _condition.notify_all();
}

void strand::post(std::function<void()> work)
{
  decltype(work) to_post;

  std::unique_lock l{ _mutex };
  if (_pending.empty()) { to_post = work; }
  _pending.push(std::move(work));

  l.unlock();

  if (to_post) { do_post(std::move(to_post)); }
}

void strand::post_next()
{
  decltype(_pending)::value_type work;

  std::unique_lock l{ _mutex };
  _pending.pop();
  if (!_pending.empty()) { work = _pending.front(); }

  l.unlock();

  if (work) { do_post(std::move(work)); }
}

void strand::do_post(std::function<void()> work)
{
  // TODO use std::enable_shared_from_this to be able to check if underlying strand is still alive
  _worker.post([this, work = std::move(work)]() {
    work();
    post_next();
  });
}
}