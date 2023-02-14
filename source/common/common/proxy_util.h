#pragma once

#include <cstdint>

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Common {

// https://gist.github.com/underscorediscovery/81308642d0325fd386237cfa3b44785c

class Util {
public:
  static constexpr uint32_t val_32_const = 0x811c9dc5;
  static constexpr uint32_t prime_32_const = 0x1000193;
  static constexpr uint64_t val_64_const = 0xcbf29ce484222325;
  static constexpr uint64_t prime_64_const = 0x100000001b3;

  static constexpr uint32_t xxHash32(const char* const str,
                                     const uint32_t value = val_32_const) noexcept {
    return (str[0] == '\0') ? value
                            : xxHash32(&str[1], (value ^ uint32_t(str[0])) * prime_32_const);
  }

  static constexpr uint64_t xxHash64(const char* const str,
                                     const uint64_t value = val_64_const) noexcept {
    return (str[0] == '\0') ? value
                            : xxHash64(&str[1], (value ^ uint64_t(str[0])) * prime_64_const);
  }
};

} // namespace Common
} // namespace Common
} // namespace Proxy
} // namespace Envoy
