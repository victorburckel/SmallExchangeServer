#pragma once

#include <utility>

namespace exchange_server {
template<class T> class scope_exit
{
public:
  explicit scope_exit(T t) : _t{ std::move(t) } {}
  scope_exit(const scope_exit &) = delete;
  scope_exit(scope_exit &&) noexcept = default;
  scope_exit &operator=(const scope_exit &) = delete;
  scope_exit &operator=(scope_exit &&) noexcept = default;
  ~scope_exit()
  {
    try {
      _t();
    } catch (...) {
    }
  }

private:
  T _t;
};
}