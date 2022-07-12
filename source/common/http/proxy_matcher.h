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

class Matcher {
public:
  virtual ~Matcher() = default;
  virtual bool match(const Envoy::Http::HeaderMap& headers,
                     const Envoy::Http::Utility::QueryParams& params) const PURE;
};
using MatcherPtr = std::unique_ptr<Matcher>;

class CommonMatcher {
public:
  CommonMatcher(const CommonMatcherProto& config);
  bool match(const Envoy::Http::HeaderMap& headers,
             const Envoy::Http::Utility::QueryParams& params) const;

private:
  std::vector<MatcherPtr> matchers_;
};

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
