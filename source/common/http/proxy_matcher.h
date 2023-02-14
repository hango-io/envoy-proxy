#pragma once

#include "source/common/common/matchers.h"
#include "source/common/http/header_utility.h"
#include "source/common/router/config_utility.h"

#include "api/proxy/common/matcher/v3/matcher.pb.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

using CommonMatcherProto = proxy::common::matcher::v3::CommonMatcher;
using CookiesMap = absl::flat_hash_map<std::string, std::string>;

class HttpCommonMatcherContext {
public:
  explicit HttpCommonMatcherContext(const Envoy::Http::HeaderMap& headers) : headers_(headers) {}

  absl::string_view getPath() const;

  const Envoy::Http::HeaderMap& getHeaders() const { return headers_; }

  const Envoy::Http::Utility::QueryParams& getQueryParams() const;

  const CookiesMap& getCookies() const;

private:
  const Envoy::Http::HeaderMap& headers_;
  mutable absl::optional<Envoy::Http::Utility::QueryParams> parameters_;
  mutable absl::string_view path_;
  mutable absl::optional<CookiesMap> cookies_;
};

class Matcher {
public:
  virtual ~Matcher() = default;
  virtual bool match(const HttpCommonMatcherContext& context) const PURE;
};
using MatcherPtr = std::unique_ptr<Matcher>;

class CommonMatcher {
public:
  CommonMatcher(const CommonMatcherProto& config);

  bool match(const HttpCommonMatcherContext& context) const;

private:
  std::vector<MatcherPtr> matchers_;
};

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
