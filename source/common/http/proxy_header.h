#pragma once

#include "envoy/http/message.h"

#include "source/common/http/header_map_impl.h"
#include "source/common/http/headers.h"
#include "source/common/http/message_impl.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

// 一个对Http::Message的最简易实现，本身不持有任何数据，仅仅保持一个对headers的引用
class WeakHeaderOnlyMessage : public Envoy::Http::RequestMessage {
public:
  WeakHeaderOnlyMessage(Envoy::Http::RequestHeaderMap& headers);

  Envoy::Http::RequestHeaderMap& headers() override { return headers_; }
  Buffer::Instance& body() override { return FAKE_BODY; }
  Envoy::Http::RequestTrailerMap* trailers() override { return nullptr; }
  void trailers(Envoy::Http::RequestTrailerMapPtr&&) override { ; }
  std::string bodyAsString() const override { return ""; }

private:
  Envoy::Http::RequestHeaderMap& headers_;
  static Envoy::Buffer::OwnedImpl FAKE_BODY;
};

class HeaderUtility {
public:
  static void replaceHeaders(Envoy::Http::HeaderMap& headers,
                             const Envoy::Http::HeaderMap& replace_headers);

  static absl::string_view getHeaderValue(const Envoy::Http::LowerCaseString& key,
                                          const Envoy::Http::HeaderMap& headers,
                                          absl::string_view default_value = "");
};

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
