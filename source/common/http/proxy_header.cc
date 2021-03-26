#include "common/http/proxy_header.h"

#include "common/http/header_utility.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

void HeaderUtility::replaceHeaders(Envoy::Http::HeaderMap& headers,
                                   const Envoy::Http::HeaderMap& replace_headers) {
  headers.clear();
  Envoy::Http::HeaderUtility::addHeaders(headers, replace_headers);
}

absl::string_view HeaderUtility::getHeaderValue(const Envoy::Http::LowerCaseString& key,
                                                const Envoy::Http::HeaderMap& headers,
                                                const absl::string_view default_value) {
  auto result = headers.get(key);
  if (!result.empty()) {
    return result[0]->value().getStringView();
  }
  return default_value;
}

Envoy::Buffer::OwnedImpl WeakHeaderOnlyMessage::FAKE_BODY;

WeakHeaderOnlyMessage::WeakHeaderOnlyMessage(Envoy::Http::RequestHeaderMap& headers)
    : headers_(headers) {}

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
