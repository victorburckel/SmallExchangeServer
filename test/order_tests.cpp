#include "order.h"
#include <gtest/gtest.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory)
TEST(order_tests, can_parse_order)
{
  constexpr auto message =
    "1234"
    " BTCUSDT"
    "+"
    "0010"
    "00010000";

  const auto order = exchange_server::parse_order(message);

  ASSERT_TRUE(order.has_value());
  EXPECT_EQ(order->id, "1234");
  EXPECT_EQ(order->symbol, " BTCUSDT");
  EXPECT_EQ(order->way, exchange_server::order_side::buy);
  EXPECT_EQ(order->quantity, 10U);
  EXPECT_EQ(order->price, 10'000);
}