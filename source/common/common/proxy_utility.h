#pragma once

#include <chrono>

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Common {

class TimeUtil {
public:
  static uint64_t createTimestamp();
};

class StringUtil {
public:
  static std::string escapeForJson(const absl::string_view source);
  static bool md5StringOrNot(const absl::string_view md5_string);
};

} // namespace Common
} // namespace Common
} // namespace Proxy
} // namespace Envoy
