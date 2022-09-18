#pragma once

#include "order.h"
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace exchange_server {

class market_interface
{
public:
  virtual ~market_interface() = default;

  virtual void add_order(const order &order, std::function<void()> callback) = 0;
  virtual bool update_order(const order &order) = 0;
  virtual bool cancel_order(const std::string &id) = 0;
};

class market : public market_interface
{
public:
  void run();
  void stop();

  void add_order(const order &order, std::function<void()> callback) override;
  bool update_order(const order &order) override;
  bool cancel_order(const std::string &id) override;

private:
  bool _stop_requested{ false };
  std::mutex _mutex;
  std::vector<std::pair<order, std::function<void()>>> _pending_orders;
  std::unordered_map<std::string, size_t> _mapping;
};
}