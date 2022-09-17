#pragma once

#include <errno.h>
#include <system_error>

namespace exchange_server {

inline std::error_code get_last_error() { return std::make_error_code(static_cast<std::errc>(errno)); }

}