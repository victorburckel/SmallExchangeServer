#include "mocks.h"
#include <gtest/gtest.h>

using ::testing::StrictMock;
using testing::MockFunction;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory)
TEST(strand_tests, strand_serializes_messages)
{
  StrictMock<mocks::worker> worker{};
  exchange_server::strand strand{ worker };

  MockFunction<void(void)> work1;
  MockFunction<void(void)> work2;

  std::function<void()> delayed;

  EXPECT_CALL(worker, post).WillOnce([&delayed](std::function<void()> f) { delayed = std::move(f); });
  strand.post(work1.AsStdFunction());
  strand.post(work2.AsStdFunction());

  EXPECT_CALL(work1, Call);
  EXPECT_CALL(worker, post).WillOnce([&delayed](std::function<void()> f) { delayed = std::move(f); });
  delayed();

  EXPECT_CALL(work2, Call);
  delayed();
}