#pragma once

#include <optional>
#include <string>

namespace exchange_server {
enum class order_side { buy, sell };

struct order
{
  std::string id;
  std::string symbol;
  order_side way{};
  std::uint64_t quantity{};
  double price{};
};

std::optional<order> parse_order(std::string_view message);
}