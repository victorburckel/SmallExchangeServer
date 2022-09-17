#pragma once

#include <optional>
#include <system_error>

namespace exchange_server {
template<class R> struct result
{
  std::optional<R> result;
  std::error_code err;
};
}