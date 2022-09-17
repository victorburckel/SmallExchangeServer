#pragma once

#include <optional>
#include <utility>

namespace exchange_server {
template<class T> class scope_exit
{
public:
  explicit scope_exit(T t) : _t{ std::move(t) } {}
  scope_exit(const scope_exit &) = delete;
  scope_exit(scope_exit &&other) noexcept : _t{ std::exchange(other._t, std::nullopt) } {}
  scope_exit &operator=(const scope_exit &) = delete;
  scope_exit &operator=(scope_exit &&other) noexcept
  {
    _t = std::exchange(other._t, std::nullopt);
    return *this;
  }
  ~scope_exit()
  {
    try {
      if (_t) { (*_t)(); }
    } catch (...) {
    }
  }

private:
  std::optional<T> _t;
};
}