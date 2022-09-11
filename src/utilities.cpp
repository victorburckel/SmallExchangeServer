#include "utilities.h"
#include <array>
#include <cstring>

std::string exchange_server::get_last_error()
{
  std::array<char, 1024> buffer{};
  return std::string{ strerror_r(errno, buffer.data(), buffer.size()) };
}