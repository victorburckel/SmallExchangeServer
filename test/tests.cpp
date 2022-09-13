#include "order.h"
#include <catch2/catch.hpp>

TEST_CASE("Can parse order", "[orders]")
{
  constexpr auto message =
    "1234"
    " BTCUSDT"
    "+"
    "0010"
    "00010000";

  const auto order = exchange_server::parse_order(message);

  REQUIRE(order.has_value());
  CHECK(order->id == "1234");
  CHECK(order->symbol == " BTCUSDT");
  CHECK(order->way == exchange_server::order_side::buy);
  CHECK(order->quantity == 10U);
  CHECK(order->price == 10'000);
}