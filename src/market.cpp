#include "market.h"
#include <chrono>
#include <random>
#include <spdlog/spdlog.h>
#include <thread>

namespace exchange_server {

void market::run()
{
  std::random_device r;
  std::default_random_engine e{ r() };
  std::uniform_int_distribution<int> delay_dist{ 1, 10 };
  std::uniform_real_distribution<double> index_dist{ 0, 1 };


  for (;;)
  {
    auto delay = delay_dist(e);
    // Sleep for makes clang-tidy crash because of spaceship operator
    // https://bugs.llvm.org/show_bug.cgi?id=47511
    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds{ delay });

    std::optional<decltype(_pending_orders)::value_type> execution;
    std::unique_lock l{ _mutex };
    if (_stop_requested) { break; }
    if (!_pending_orders.empty())
    {
      auto it = _pending_orders.begin() + static_cast<int>(index_dist(e) * static_cast<double>(_pending_orders.size()));
      execution = *it;
      _pending_orders.erase(it);
      _mapping.erase(execution->first.id);
    }

    l.unlock();

    if (execution)
    {
      const auto &[order, callback] = *execution;
      spdlog::info("Executing order {}", order.id);
      callback();
    }
  }
}

void market::stop()
{
  std::scoped_lock l{ _mutex };
  _stop_requested = true;
}

void market::add_order(const order &order, std::function<void()> callback)
{
  std::scoped_lock l{ _mutex };
  auto index = _pending_orders.size();
  _pending_orders.emplace_back(order, std::move(callback));
  _mapping.insert(std::pair{ order.id, index });
}

bool market::update_order(const order &order)
{
  std::scoped_lock l{ _mutex };
  if (const auto it = _mapping.find(order.id); it != _mapping.end())
  {
    _pending_orders[it->second].first = order;
    return true;
  }

  return false;
}

bool market::cancel_order(const std::string &id)
{
  std::scoped_lock l{ _mutex };
  if (const auto it = _mapping.find(id); it != _mapping.end())
  {
    _pending_orders.erase(_pending_orders.begin() + static_cast<std::ptrdiff_t>(it->second));
    _mapping.erase(it);
    return true;
  }

  return false;
}
}