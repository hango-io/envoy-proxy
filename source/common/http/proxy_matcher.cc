#include "source/common/http/proxy_matcher.h"

#include "source/common/http/path_utility.h"
#include "source/common/http/utility.h"

#include "absl/types/optional.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Http {

absl::string_view HttpCommonMatcherContext::getPath() const {
  if (path_.empty()) {
    const auto typed_headers = dynamic_cast<const Envoy::Http::RequestHeaderMap*>(&headers_);
    if (typed_headers != nullptr) {
      absl::string_view path_with_query = typed_headers->getPathValue();
      path_ = Envoy::Http::PathUtil::removeQueryAndFragment(path_with_query);
    }
  }
  return path_;
}

const Envoy::Http::Utility::QueryParams& HttpCommonMatcherContext::getQueryParams() const {
  if (!parameters_.has_value()) {
    const auto typed_headers = dynamic_cast<const Envoy::Http::RequestHeaderMap*>(&headers_);
    if (typed_headers != nullptr) {
      auto path_with_query = typed_headers->getPathValue();
      parameters_ = Envoy::Http::Utility::parseQueryString(path_with_query);
    } else {
      parameters_.emplace(Envoy::Http::Utility::QueryParams{});
    }
  }
  return parameters_.value();
}

const CookiesMap& HttpCommonMatcherContext::getCookies() const {
  if (!cookies_.has_value()) {
    const auto typed_headers = static_cast<const Envoy::Http::RequestHeaderMap*>(&headers_);
    if (typed_headers != nullptr) {
      cookies_ = Envoy::Http::Utility::parseCookies(*typed_headers);
    } else {
      cookies_.emplace(CookiesMap{});
    }
  }
  return cookies_.value();
}

namespace {

using ProtoHeadersMatcher = Protobuf::RepeatedPtrField<envoy::config::route::v3::HeaderMatcher>;
using ProtoParametersMatcher =
    Protobuf::RepeatedPtrField<envoy::config::route::v3::QueryParameterMatcher>;
using ProtoCookiesMatcher = Protobuf::RepeatedPtrField<proxy::common::matcher::v3::CookieMatcher>;

class PathMatcher : public Matcher {
public:
  PathMatcher(const envoy::type::matcher::v3::StringMatcher& matcher) : matcher_(matcher) {}

  bool match(const HttpCommonMatcherContext& context) const override {
    return matcher_.match(context.getPath());
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

  bool match(const HttpCommonMatcherContext& context) const override {
    return Envoy::Http::HeaderUtility::matchHeaders(context.getHeaders(), headers_matcher_);
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

  bool match(const HttpCommonMatcherContext& context) const override {
    return Envoy::Router::ConfigUtility::matchQueryParams(context.getQueryParams(),
                                                          parameters_matcher_);
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

  bool match(const HttpCommonMatcherContext& context) const override {
    CookiesMap request_cookies = context.getCookies();
    for (const auto& cookie : cookies_matcher_) {
      bool find_key = false;
      if (request_cookies.find(cookie.first) != request_cookies.end()) {
        find_key = true;
      }

      if (!cookie.second.has_value() && !find_key) {
        // Present match but cookie not present.
        return false;
      }

      if (cookie.second.has_value() &&
          (!find_key || !cookie.second.value().match(request_cookies.at(cookie.first)))) {
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

bool CommonMatcher::match(const HttpCommonMatcherContext& context) const {
  for (const auto& matcher : matchers_) {
    if (!matcher->match(context)) {
      return false;
    }
  }
  return true;
}

} // namespace Http
} // namespace Common
} // namespace Proxy
} // namespace Envoy
