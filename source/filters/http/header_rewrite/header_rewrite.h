
#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "envoy/http/filter.h"
#include "envoy/http/query_params.h"
#include "envoy/server/filter_config.h"

#include "source/common/common/regex.h"
#include "source/common/config/metadata.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/proxy_matcher.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/header_rewrite/v2/header_rewrite.pb.h"
#include "api/proxy/filters/http/header_rewrite/v2/header_rewrite.pb.validate.h"
#include "re2/re2.h"

// clang-format off
#include "nlohmann/json.hpp"
#include "inja/inja.hpp"
// clang-format on

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRewrite {

using ProtoCommonConfig = proxy::filters::http::header_rewrite::v2::ProtoCommonConfig;
using ProtoRouteConfig = proxy::filters::http::header_rewrite::v2::ProtoRouteConfig;
using ProtoHeaderRewrite = proxy::filters::http::header_rewrite::v2::HeaderRewrite;
using ProtoExtractorRewriter = proxy::filters::http::header_rewrite::v2::ExtractorRewriter;
using ProtoExtractor = proxy::filters::http::header_rewrite::v2::Extractor;
using ProtoRewriter = proxy::filters::http::header_rewrite::v2::Rewriter;

using ContextDict = nlohmann::json;
using TemplateEnv = inja::Environment;
using Template = inja::Template;

class ExtractorRewriterConfig : public Logger::Loggable<Logger::Id::filter> {
public:
  ExtractorRewriterConfig(const ProtoExtractorRewriter& proto_config);
  ExtractorRewriterConfig() = default;

  class Extractor {
  public:
    using ExtractorF = std::function<std::string(const Http::HeaderMap&, const std::string&,
                                                 const Http::Utility::QueryParams&)>;

    Extractor(const ProtoExtractor& proto_config);

    std::string extract(const Http::HeaderMap& h, const std::string& p,
                        const Http::Utility::QueryParams& q);

  private:
    ExtractorF extractor_func_;

    // For regex.
    uint32_t group_{0};
    std::unique_ptr<re2::RE2> regex_;
  };
  using ExtractorPtr = std::unique_ptr<Extractor>;

  class Rewriter {
  public:
    using RewriterF = std::function<void(Http::HeaderMap&, std::string&,
                                         Http::Utility::QueryParams&, const ContextDict&)>;

    Rewriter(const ProtoRewriter& proto_config, ExtractorRewriterConfig& parent);

    void rewrite(Http::HeaderMap& h, std::string& p, Http::Utility::QueryParams& q,
                 const ContextDict& c);

  private:
    bool enableRewrite(const Http::HeaderMap& headers);

    ExtractorRewriterConfig& parent_;

    RewriterF rewriter_func_;

    absl::optional<Template> template_;

    std::unique_ptr<Proxy::Common::Http::CommonMatcher> matcher_;
  };
  using RewriterPtr = std::unique_ptr<Rewriter>;
  friend class Rewriter;

  void extractContext(Http::HeaderMap& h, std::string& p, Http::Utility::QueryParams& q,
                      ContextDict& c) const;

  void rewriteHeaders(Http::HeaderMap& h, std::string& p, Http::Utility::QueryParams& q,
                      ContextDict& c) const;

  bool wouldUpdatePath() const { return would_update_path_; }

  mutable bool would_update_path_{false};

  TemplateEnv template_env_;

  std::vector<std::pair<std::string, ExtractorPtr>> extractors_;
  std::vector<RewriterPtr> rewriters_;
};
using ExtractorRewriterConfigPtr = std::unique_ptr<ExtractorRewriterConfig>;

class HeaderRewriteConfig : public Logger::Loggable<Logger::Id::filter> {
public:
  HeaderRewriteConfig(const ProtoHeaderRewrite& proto_config)
      : clear_route_cache_(proto_config.clear_route_cache()) {
    decoder_config_ = std::make_unique<ExtractorRewriterConfig>(proto_config.decoder_rewriters());
    encoder_config_ = std::make_unique<ExtractorRewriterConfig>(proto_config.encoder_rewriters());

    if (encoder_config_->wouldUpdatePath()) {
      throw EnvoyException(
          fmt::format("Illegal target: only header target can be set for encode rewrite"));
    }
  }

  const ExtractorRewriterConfig& decodeRewritersConfig() const { return *decoder_config_; }
  const ExtractorRewriterConfig& encodeRewritersConfig() const { return *encoder_config_; }
  bool clearRouteCache() const { return clear_route_cache_; }

private:
  ExtractorRewriterConfigPtr decoder_config_;
  ExtractorRewriterConfigPtr encoder_config_;
  const bool clear_route_cache_{false};
};

class RouteConfig : public Router::RouteSpecificFilterConfig,
                    public Logger::Loggable<Logger::Id::filter>,
                    public Config::TypedMetadata::Object {
public:
  RouteConfig(const ProtoRouteConfig& proto_config)
      : config_(proto_config.config()), disabled_(proto_config.disabled()) {}

  bool disabled() const { return disabled_; }

  HeaderRewriteConfig config_;

private:
  const bool disabled_{false};
};

using RouteConfigSharedPtr = std::shared_ptr<RouteConfig>;

class CommonConfig : Logger::Loggable<Logger::Id::filter> {
public:
  CommonConfig(const ProtoCommonConfig& proto_config) : config_(proto_config.config()) {}

  HeaderRewriteConfig config_;
};

using CommonConfigSharedPtr = std::shared_ptr<CommonConfig>;

/*
 * Filter inherits from PassthroughFilter instead of StreamFilter because PassthroughFilter
 * implements all the StreamFilter's virtual mehotds by default. This reduces a lot of unnecessary
 * code.
 */
class Filter : public Envoy::Http::PassThroughFilter, Logger::Loggable<Logger::Id::filter> {
public:
  Filter(CommonConfig* config, const std::string& name) : config_(config), name_(name){};

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;

private:
  ContextDict render_context_;

  std::string path_;
  Http::Utility::QueryParams parameter_;

  CommonConfig* config_{nullptr};
  const std::string name_;

  const RouteConfig* route_config_{nullptr};
  // const RouteConfig* cluster_config_{nullptr};
};

using FilterPtr = std::unique_ptr<Filter>;

} // namespace HeaderRewrite
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
