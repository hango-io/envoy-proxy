#include "source/filters/http/header_rewrite/header_rewrite.h"

#include "envoy/upstream/upstream.h"

#include "source/common/common/empty_string.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/path_utility.h"
#include "source/common/http/utility.h"
#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/utility.h"

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRewrite {

// class TypedMetadataFactory : public Envoy::Upstream::ClusterTypedMetadataFactory {
// public:
//   std::string name() const override { return Filter::name(); }

//   std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
//   parse(const ProtobufWkt::Struct& data) const override {
//     ProtoRouteConfig cluster_config;
//     MessageUtil::jsonConvert(data, ProtobufMessage::getStrictValidationVisitor(),
//     cluster_config);

//     return std::make_unique<RouteConfig>(cluster_config);
//   }
// };

// static Registry::RegisterFactory<TypedMetadataFactory,
// Envoy::Upstream::ClusterTypedMetadataFactory>
//     register_;

ExtractorRewriterConfig::Extractor::Extractor(const ProtoExtractor& proto_config) {
  if (!proto_config.regex().empty() && proto_config.group() != 0) {
    regex_ = std::make_unique<re2::RE2>(proto_config.regex(), re2::RE2::Quiet);

    if (!regex_ || !regex_->ok()) {
      throw EnvoyException(regex_->error());
    }
    group_ = proto_config.group();

    size_t mark_count = regex_->NumberOfCapturingGroups();

    if (mark_count < group_) {
      throw EnvoyException(fmt::format("Group is {} but pattern only has {}", group_,
                                       regex_->NumberOfCapturingGroups()));
    }
  }

  // Only One of decoder_header/encoder_header/parameter/environment has value and others must be
  // empty string.
  std::string name =
      proto_config.header_name() + proto_config.parameter() + proto_config.environment();
  auto header_name = Http::LowerCaseString(name);

  std::string env_value = std::getenv(name.c_str()) ? std::getenv(name.c_str()) : "";

  switch (proto_config.source_case()) {
  case ProtoExtractor::kHeaderName:
    extractor_func_ = [header_name](const Http::HeaderMap& h, const std::string&,
                                    const Http::Utility::QueryParams&) {
      const auto result = h.get(header_name);
      return !result.empty() ? std::string(result[0]->value().getStringView()) : EMPTY_STRING;
    };
    break;
  case ProtoExtractor::kParameter:
    extractor_func_ = [name](const Http::HeaderMap&, const std::string&,
                             const Http::Utility::QueryParams& q) {
      return q.find(name) != q.end() ? q.at(name) : EMPTY_STRING;
    };
    break;
  case ProtoExtractor::kPath:
    extractor_func_ = [](const Http::HeaderMap&, const std::string& p,
                         const Http::Utility::QueryParams&) { return p; };
    break;
  case ProtoExtractor::kEnvironment:
    extractor_func_ = [env_value](const Http::HeaderMap&, const std::string&,
                                  const Http::Utility::QueryParams&) { return env_value; };
    break;
  default:
    throw EnvoyException("Unknown source type for header rewrite extractor");
  }
}

namespace {
struct RegexArgs {
public:
  RegexArgs(size_t group_number) {
    store_.resize(group_number);
    store_[group_number - 1] = re2::RE2::Arg(&value_);
    subs_.resize(group_number);
    for (size_t i = 0; i < group_number; i++) {
      subs_[i] = &(store_[i]);
    }
  }
  std::string value_;
  std::vector<re2::RE2::Arg*> subs_;

private:
  std::vector<re2::RE2::Arg> store_;
};

} // namespace

std::string ExtractorRewriterConfig::Extractor::extract(const Http::HeaderMap& headers,
                                                        const std::string& path,
                                                        const Http::Utility::QueryParams& params) {
  ASSERT(extractor_func_);
  std::string value = extractor_func_(headers, path, params);

  if (value.empty() || group_ == 0 || !regex_) {
    return value;
  }

  auto regex_args = RegexArgs(group_);

  if (re2::RE2::FullMatchN(re2::StringPiece(value.data(), value.size()), *regex_,
                           regex_args.subs_.data(), group_)) {
    return regex_args.value_;
  }
  return "";
}

bool ExtractorRewriterConfig::Rewriter::enableRewrite(const Http::HeaderMap& headers) {
  Common::Http::HttpCommonMatcherContext context(headers);
  if (matcher_ != nullptr) {
    return matcher_->match(context);
  }
  return true;
}

ExtractorRewriterConfig::Rewriter::Rewriter(const ProtoRewriter& proto_config,
                                            ExtractorRewriterConfig& parent)
    : parent_(parent) {

  if (proto_config.has_matcher()) {
    matcher_ = std::make_unique<Common::Http::CommonMatcher>(proto_config.matcher());
  }

  // Only one of update/append has value.
  std::string template_string = proto_config.update() + proto_config.append();
  template_.emplace(parent_.template_env_.parse(template_string));

  // Only One of decoder_header/encoder_header/parameter has value and others must be empty string.
  std::string name = proto_config.header_name() + proto_config.parameter();
  auto header_name = Http::LowerCaseString(name);

#define CASE_AND_CASE(case1, case2) (size_t(case1) * 5) + size_t(case2)

  if (proto_config.target_case() == ProtoRewriter::kParameter ||
      proto_config.target_case() == ProtoRewriter::kPath) {
    parent_.would_update_path_ = true;
  }

  switch (CASE_AND_CASE(proto_config.operation_case(), proto_config.target_case())) {
  case CASE_AND_CASE(ProtoRewriter::kRemove, ProtoRewriter::kHeaderName):
    rewriter_func_ = [header_name](Http::HeaderMap& h, std::string&, Http::Utility::QueryParams&,
                                   const ContextDict&) { h.remove(header_name); };
    break;
  case CASE_AND_CASE(ProtoRewriter::kRemove, ProtoRewriter::kPath):
    break;
  case CASE_AND_CASE(ProtoRewriter::kRemove, ProtoRewriter::kParameter):
    rewriter_func_ = [name](Http::HeaderMap&, std::string&, Http::Utility::QueryParams& q,
                            const ContextDict&) { q.erase(name); };
    break;
  case CASE_AND_CASE(ProtoRewriter::kAppend, ProtoRewriter::kHeaderName):
    rewriter_func_ = [header_name, this](Http::HeaderMap& h, std::string&,
                                         Http::Utility::QueryParams&, const ContextDict& c) {
      std::string value = parent_.template_env_.render(template_.value(), c);
      h.addCopy(header_name, value);
    };
    break;
  case CASE_AND_CASE(ProtoRewriter::kAppend, ProtoRewriter::kPath):
    rewriter_func_ = [this](Http::HeaderMap&, std::string& p, Http::Utility::QueryParams&,
                            const ContextDict& c) {
      std::string value = parent_.template_env_.render(template_.value(), c);
      p += value;
    };
    break;
  case CASE_AND_CASE(ProtoRewriter::kAppend, ProtoRewriter::kParameter):
    rewriter_func_ = [name, this](Http::HeaderMap&, std::string&, Http::Utility::QueryParams& q,
                                  const ContextDict& c) {
      std::string value = parent_.template_env_.render(template_.value(), c);
      q[name] += value;
    };
    break;
  case CASE_AND_CASE(ProtoRewriter::kUpdate, ProtoRewriter::kHeaderName):
    rewriter_func_ = [header_name, this](Http::HeaderMap& h, std::string&,
                                         Http::Utility::QueryParams&, const ContextDict& c) {
      h.remove(header_name);
      std::string value = parent_.template_env_.render(template_.value(), c);
      h.addCopy(header_name, value);
    };
    break;
  case CASE_AND_CASE(ProtoRewriter::kUpdate, ProtoRewriter::kPath):
    rewriter_func_ = [this](Http::HeaderMap&, std::string& p, Http::Utility::QueryParams&,
                            const ContextDict& c) {
      std::string value = parent_.template_env_.render(template_.value(), c);
      p = value.empty() ? p : value;
    };
    break;
  case CASE_AND_CASE(ProtoRewriter::kUpdate, ProtoRewriter::kParameter):
    rewriter_func_ = [name, this](Http::HeaderMap&, std::string&, Http::Utility::QueryParams& q,
                                  const ContextDict& c) {
      std::string value = parent_.template_env_.render(template_.value(), c);
      q[name] = value;
    };
    break;
  default:
    throw EnvoyException(fmt::format("Un-supported target: {} or operation: {} for header rewrite",
                                     size_t(proto_config.target_case()),
                                     size_t(proto_config.operation_case())));
  }
#undef CASE_AND_CASE
}

void ExtractorRewriterConfig::Rewriter::rewrite(Http::HeaderMap& headers, std::string& path,
                                                Http::Utility::QueryParams& params,
                                                const ContextDict& context) {
  if (enableRewrite(headers) && rewriter_func_) {
    try {
      rewriter_func_(headers, path, params, context);
    } catch (std::exception& e) {
      ENVOY_LOG(warn, "{}", e.what());
    }
  }
}

ExtractorRewriterConfig::ExtractorRewriterConfig(const ProtoExtractorRewriter& proto_config) {

  for (const auto& pair : proto_config.extractors()) {
    auto extractor = std::make_unique<Extractor>(pair.second);
    extractors_.push_back({pair.first, std::move(extractor)});
  }

  for (const auto& item : proto_config.rewriters()) {
    auto rewriter = std::make_unique<Rewriter>(item, *this);
    rewriters_.push_back(std::move(rewriter));
  }
}

void ExtractorRewriterConfig::extractContext(Http::HeaderMap& h, std::string& p,
                                             Http::Utility::QueryParams& q, ContextDict& c) const {
  for (const auto& pair : extractors_) {
    c[pair.first] = pair.second->extract(h, p, q);
  }
}

void ExtractorRewriterConfig::rewriteHeaders(Http::HeaderMap& h, std::string& p,
                                             Http::Utility::QueryParams& q, ContextDict& c) const {
  for (const auto& item : rewriters_) {
    item->rewrite(h, p, q, c);
  }
}

// namespace {
// const RouteConfig* getClusterFilterConfig(const StreamInfo::StreamInfo& stream_info) {
//   if (!stream_info.upstreamClusterInfo().has_value()) {
//     return nullptr;
//   }

//   if (stream_info.upstreamClusterInfo().value() == nullptr) {
//     return nullptr;
//   }
//   return stream_info.upstreamClusterInfo().value()->typedMetadata().get<RouteConfig>(
//       Filter::name());
// }
// } // namespace

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  auto route = decoder_callbacks_->route();
  route_config_ = Http::Utility::resolveMostSpecificPerFilterConfig<RouteConfig>(name_, route);
  // cluster_config_ = getClusterFilterConfig(decoder_callbacks_->streamInfo());

  auto path_with_query = headers.Path() ? headers.Path()->value().getStringView() : "";

  path_ = std::string(Http::PathUtil::removeQueryAndFragment(path_with_query));
  parameter_ = Http::Utility::parseQueryString(path_with_query);

  if (route_config_ && route_config_->disabled()) {
    return Http::FilterHeadersStatus::Continue;
  }

  // if (cluster_config_ && cluster_config_->disabled()) {
  //   return Http::FilterHeadersStatus::Continue;
  // }

  const auto& common_decoder_rewrites = config_->config_.decodeRewritersConfig();

  common_decoder_rewrites.extractContext(headers, path_, parameter_, render_context_);
  common_decoder_rewrites.rewriteHeaders(headers, path_, parameter_, render_context_);

  bool would_update_path = common_decoder_rewrites.wouldUpdatePath();
  bool clear_route_cache = config_->config_.clearRouteCache();

  // if (cluster_config_) {
  //   const auto& cluster_decoder_rewrites = cluster_config_->config_.decodeRewritersConfig();

  //   cluster_decoder_rewrites.extractContext(headers, path_, parameter_, render_context_);
  //   cluster_decoder_rewrites.rewriteHeaders(headers, path_, parameter_, render_context_);

  //   would_update_path |= cluster_decoder_rewrites.wouldUpdatePath();
  //   clear_route_cache |= cluster_config_->config_.clearRouteCache();
  // }

  if (route_config_) {
    const auto& route_decoder_rewrites = route_config_->config_.decodeRewritersConfig();

    route_decoder_rewrites.extractContext(headers, path_, parameter_, render_context_);
    route_decoder_rewrites.rewriteHeaders(headers, path_, parameter_, render_context_);

    would_update_path |= route_decoder_rewrites.wouldUpdatePath();
    clear_route_cache |= route_config_->config_.clearRouteCache();
  }

  if (would_update_path) {
    std::string query = Envoy::Http::Utility::queryParamsToString(parameter_);
    const_cast<Http::HeaderEntry*>(headers.Path())->value(path_ + query);
  }

  if (clear_route_cache) {
    decoder_callbacks_->clearRouteCache();
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool) {

  if (route_config_ && route_config_->disabled()) {
    return Http::FilterHeadersStatus::Continue;
  }

  // if (cluster_config_ && cluster_config_->disabled()) {
  //   return Http::FilterHeadersStatus::Continue;
  // }

  const auto& common_encoder_rewrites = config_->config_.encodeRewritersConfig();

  common_encoder_rewrites.extractContext(headers, path_, parameter_, render_context_);
  common_encoder_rewrites.rewriteHeaders(headers, path_, parameter_, render_context_);

  if (route_config_) {
    const auto& route_encoder_rewrites = route_config_->config_.encodeRewritersConfig();

    route_encoder_rewrites.extractContext(headers, path_, parameter_, render_context_);
    route_encoder_rewrites.rewriteHeaders(headers, path_, parameter_, render_context_);
  }

  // if (cluster_config_) {
  //   const auto& cluster_encoder_rewrites = cluster_config_->config_.encodeRewritersConfig();

  //   cluster_encoder_rewrites.extractContext(headers, path_, parameter_, render_context_);
  //   cluster_encoder_rewrites.rewriteHeaders(headers, path_, parameter_, render_context_);
  // }

  return Http::FilterHeadersStatus::Continue;
}

} // namespace HeaderRewrite
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
