#pragma once

#include <system_error>

namespace exchange_server {
template<class R> struct result
{
  R result{};
  std::error_code err;
};
}