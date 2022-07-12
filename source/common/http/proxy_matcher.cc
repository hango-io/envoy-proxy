#include "source/common/http/proxy_matcher.h"

#include "source/common/http/path_utility.h"
#include "source/common/http/utility.h"

#include "absl/types/optional.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

namespace {

using ProtoHeadersMatcher = Protobuf::RepeatedPtrField<envoy::config::route::v3::HeaderMatcher>;
using ProtoParametersMatcher =
    Protobuf::RepeatedPtrField<envoy::config::route::v3::QueryParameterMatcher>;
using ProtoCookiesMatcher = Protobuf::RepeatedPtrField<proxy::common::matcher::v3::CookieMatcher>;

class PathMatcher : public Matcher {
public:
  PathMatcher(const envoy::type::matcher::v3::StringMatcher& matcher) : matcher_(matcher) {}

  bool match(const Envoy::Http::HeaderMap& headers,
             const Envoy::Http::Utility::QueryParams&) const override {
    ASSERT(dynamic_cast<const Envoy::Http::RequestHeaderMap*>(&headers) != nullptr);

    const auto typed_headers = static_cast<const Envoy::Http::RequestHeaderMap*>(&headers);

    absl::string_view path_with_query = typed_headers->getPathValue();

    absl::string_view path = Envoy::Http::PathUtil::removeQueryAndFragment(path_with_query);
    return matcher_.match(path);
  }

private:
  Matchers::StringMatcherImpl<envoy::type::matcher::v3::StringMatcher> matcher_;
};

class HeadersMatcher : public Matcher {
public:
  HeadersMatcher(const ProtoHeadersMatcher& proto_headers_matcher) {
    for (const auto& header_data : proto_headers_matcher) {
      headers_matcher_.push_back(
          std::make_unique<Envoy::Http::HeaderUtility::HeaderData>(header_data));
    }
  }

  bool match(const Envoy::Http::HeaderMap& headers,
             const Envoy::Http::Utility::QueryParams&) const override {
    return Envoy::Http::HeaderUtility::matchHeaders(headers, headers_matcher_);
  }

private:
  std::vector<Envoy::Http::HeaderUtility::HeaderDataPtr> headers_matcher_;
};

class ParametersMatcher : public Matcher {
public:
  ParametersMatcher(const ProtoParametersMatcher& proto_parameters_matcher) {
    for (const auto& parameter : proto_parameters_matcher) {
      parameters_matcher_.push_back(
          std::make_unique<Router::ConfigUtility::QueryParameterMatcher>(parameter));
    }
  }

  bool match(const Envoy::Http::HeaderMap&,
             const Envoy::Http::Utility::QueryParams& params) const override {
    return Envoy::Router::ConfigUtility::matchQueryParams(params, parameters_matcher_);
  }

private:
  std::vector<Envoy::Router::ConfigUtility::QueryParameterMatcherPtr> parameters_matcher_;
};

class CookiesMatcher : public Matcher {
public:
  CookiesMatcher(const ProtoCookiesMatcher& proto_cookies_matcher) {
    for (const auto& cookie : proto_cookies_matcher) {
      cookies_matcher_.push_back(
          {cookie.name(),
           cookie.has_string_match()
               ? absl::make_optional<
                     Matchers::StringMatcherImpl<envoy::type::matcher::v3::StringMatcher>>(
                     cookie.string_match())
               : absl::nullopt});
    }
  }

  bool match(const Envoy::Http::HeaderMap& headers,
             const Envoy::Http::Utility::QueryParams&) const override {
    for (const auto& cookie : cookies_matcher_) {
      auto value = Envoy::Http::Utility::parseCookieValue(headers, cookie.first);

      if (!cookie.second.has_value() && value.empty()) {
        // Present match but cookie not present.
        return false;
      }

      if (cookie.second.has_value() && !cookie.second.value().match(value)) {
        // String match but cookie not match.
        return false;
      }
    }
    return true;
  }

private:
  using CookieMatcher = std::pair<
      std::string,
      absl::optional<Matchers::StringMatcherImpl<envoy::type::matcher::v3::StringMatcher>>>;
  std::vector<CookieMatcher> cookies_matcher_;
};

} // namespace

CommonMatcher::CommonMatcher(const CommonMatcherProto& config) {
  if (config.has_path()) {
    matchers_.push_back(std::make_unique<PathMatcher>(config.path()));
  }

  if (config.headers().size() > 0) {
    matchers_.push_back(std::make_unique<HeadersMatcher>(config.headers()));
  }

  if (config.parameters().size() > 0) {
    matchers_.push_back(std::make_unique<ParametersMatcher>(config.parameters()));
  }

  if (config.cookies().size() > 0) {
    matchers_.push_back(std::make_unique<CookiesMatcher>(config.cookies()));
  }
}

bool CommonMatcher::match(const Envoy::Http::HeaderMap& headers,
                          const Envoy::Http::Utility::QueryParams& params) const {
  bool result = true;
  for (const auto& matcher : matchers_) {
    result = result && matcher->match(headers, params);
  }
  return result;
}

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
