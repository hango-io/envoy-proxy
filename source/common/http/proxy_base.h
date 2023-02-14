#pragma once
#include <memory>
#include <string>

#include "source/common/http/message_impl.h"

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

struct HttpUri;
using HttpUriPtr = std::unique_ptr<HttpUri>;

struct HttpUri {
  std::string scheme;
  std::string host;
  std::string path;
  static HttpUriPtr makeHttpUri(const absl::string_view uri);
};

enum class Direction {
  DECODE,
  ENCODE,
};

// 一些工具函数
Envoy::Http::ResponseMessagePtr makeMessageCopy(const Envoy::Http::ResponseMessagePtr& message);
Envoy::Http::RequestMessagePtr makeMessageCopy(const Envoy::Http::RequestMessagePtr& message);

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
