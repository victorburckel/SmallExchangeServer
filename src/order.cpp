#include "order.h"
#include <charconv>

std::optional<exchange_server::order> exchange_server::parse_order(std::string_view message)
{
  // id(4)symbol(8)(+/-)quantity(4)price(8)
  if (message.size() != 4 + 8 + 1 + 4 + 8) { return std::nullopt; }

  const auto id = message.substr(0, 4);
  const auto symbol = message.substr(4, 8);
  const auto way = message.substr(4 + 8, 1);
  const auto quantity = message.substr(4 + 8 + 1, 4);
  const auto price = message.substr(4 + 8 + 1 + 4, 8);

  int parsed_quantity{};
  double parsed_price{};
  std::from_chars(quantity.begin(), quantity.end(), parsed_quantity);
  std::from_chars(price.begin(), price.end(), parsed_price);

  return order{ std::string{ id },
    std::string{ symbol },
    way[0] == '-' ? order_side::sell : order_side::buy,
    static_cast<std::uint64_t>(parsed_quantity),
    parsed_price };
}