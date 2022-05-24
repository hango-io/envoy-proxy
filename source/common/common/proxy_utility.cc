
#include "source/common/common/proxy_utility.h"
#include <sstream>
#include <string>
#include <iomanip>

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Common {

uint64_t TimeUtil::createTimestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string StringUtil::escapeForJson(const absl::string_view source) {
  std::ostringstream o;
  for (auto c : source) {
    switch (c) {
    case '"':
      o << "\\\"";
      break;
    case '\\':
      o << "\\\\";
      break;
    case '\b':
      o << "\\b";
      break;
    case '\f':
      o << "\\f";
      break;
    case '\n':
      o << "\\n";
      break;
    case '\r':
      o << "\\r";
      break;
    case '\t':
      o << "\\t";
      break;
    default:
      if ('\x00' <= c && c <= '\x1f') {
        o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
      } else {
        o << c;
      }
    }
  }
  return o.str();
}

bool StringUtil::md5StringOrNot(const absl::string_view md5_string) {
  if (md5_string.size() != 32) {
    return false;
  }
  for (size_t i = 0; i < md5_string.size(); i++) {
    char ch = md5_string[i];
    if (!(('0' <= ch && ch <= '9') || ('a' <= ch && ch <= 'f') || ('A' <= ch && ch <= 'F'))) {
      return false;
    }
  }
  return true;
}

} // namespace Common
} // namespace Common
} // namespace Proxy
} // namespace Envoy
