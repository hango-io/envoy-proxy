#include "source/filters/http/detailed_stats/detailed_stats.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DetailedStats {

std::string messageToString(const ProtobufWkt::Message& message) {
  std::string text_format;

  {
    Protobuf::TextFormat::Printer printer;
    printer.SetExpandAny(true);
    printer.SetUseFieldNumber(true);
    printer.SetSingleLineMode(true);
    printer.SetHideUnknownFields(true);
    printer.PrintToString(message, &text_format);
  }

  return text_format;
}

absl::flat_hash_map<std::string, TagsMetadataStats::StatsScopesWeakPtr>
    TagsMetadataStats::StatsScopes::cached_stats_scopes_{};
absl::Mutex TagsMetadataStats::StatsScopes::mutex_{};

TagsMetadataStats::StatsScopesSharedPtr TagsMetadataStats::StatsScopes::getStatsScopesFromProto(
    const proxy::filters::http::detailed_stats::v2::PrefixedStatTags& proto_config) {
  const std::string string_config = messageToString(proto_config);

  absl::MutexLock l(&mutex_);

  auto iter = cached_stats_scopes_.find(string_config);
  if (iter == cached_stats_scopes_.end() || iter->second.expired()) {
    auto new_ptr = StatsScopesSharedPtr{new StatsScopes(proto_config, string_config)};
    cached_stats_scopes_[string_config] = new_ptr;
    return new_ptr;
  }

  return iter->second.lock();
}
void TagsMetadataStats::StatsScopes::eraseStatsScopes(StatsScopesSharedPtr scopes) {
  absl::MutexLock l(&mutex_);
  ASSERT(scopes != nullptr);

  if (scopes.use_count() == 1) {
    cached_stats_scopes_.erase(scopes->string_config_);
  }
  scopes.reset();
}

std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
TagsRouteTypedMetadataFactory::parse(const ProtobufWkt::Struct& data) const {
  proxy::filters::http::detailed_stats::v2::PrefixedStatTags prefixed_stat_tags;

  MessageUtil::jsonConvert(data, Envoy::ProtobufMessage::getStrictValidationVisitor(),
                           prefixed_stat_tags);
  return std::make_unique<TagsMetadataStats>(prefixed_stat_tags);
}

std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
TagsRouteTypedMetadataFactory::parse(const ProtobufWkt::Any& data) const {
  proxy::filters::http::detailed_stats::v2::PrefixedStatTags prefixed_stat_tags;

  Envoy::Config::Utility::translateOpaqueConfig(
      data, Envoy::ProtobufMessage::getStrictValidationVisitor(), prefixed_stat_tags);

  return std::make_unique<TagsMetadataStats>(prefixed_stat_tags);
}

static Registry::RegisterFactory<TagsRouteTypedMetadataFactory,
                                 Envoy::Router::HttpRouteTypedMetadataFactory>
    tags_route_register_;

std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
TagsHostTypedMetadataFactory::parse(const ProtobufWkt::Struct& data) const {
  proxy::filters::http::detailed_stats::v2::PrefixedStatTags prefixed_stat_tags;

  MessageUtil::jsonConvert(data, Envoy::ProtobufMessage::getStrictValidationVisitor(),
                           prefixed_stat_tags);
  return std::make_unique<TagsMetadataStats>(prefixed_stat_tags);
}

std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
TagsHostTypedMetadataFactory::parse(const ProtobufWkt::Any& data) const {
  proxy::filters::http::detailed_stats::v2::PrefixedStatTags prefixed_stat_tags;

  Envoy::Config::Utility::translateOpaqueConfig(
      data, Envoy::ProtobufMessage::getStrictValidationVisitor(), prefixed_stat_tags);

  return std::make_unique<TagsMetadataStats>(prefixed_stat_tags);
}

static Registry::RegisterFactory<TagsHostTypedMetadataFactory,
                                 Envoy::Upstream::HostTypedMetadataFactory>
    tags_host_register_;

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool) {
  response_headers_ = &headers;
  return Http::FilterHeadersStatus::Continue;
}

namespace {

void completeStreamStatsContextImpl(const std::vector<MatchedStatTags>& matched_stats,
                                    const Common::Http::HttpCommonMatcherContext& match_context,
                                    TagsMetadataStats::StreamStatsContext& context) {
  if (matched_stats.size() > 0) {
    // auto params = Http::Utility::parseQueryString(request_headers.getPathValue());
    for (auto& matched_stat : matched_stats) {
      if (matched_stat.first->match(match_context)) {
        context.request_tags_.insert(context.request_tags_.end(), matched_stat.second.cbegin(),
                                     matched_stat.second.cend());
      }
    }
  }
}

} // namespace

void CommonConfig::completeStreamStatsContext(
    const Common::Http::HttpCommonMatcherContext& match_context,
    TagsMetadataStats::StreamStatsContext& context) const {
  completeStreamStatsContextImpl(matched_stats_, match_context, context);
}

void RouterConfig::completeStreamStatsContext(
    const Common::Http::HttpCommonMatcherContext& match_context,
    TagsMetadataStats::StreamStatsContext& context) const {
  completeStreamStatsContextImpl(matched_stats_, match_context, context);
}

void Filter::onDestroy() {
  if (response_headers_ == nullptr) {
    return;
  }
  const auto& stream_info = encoder_callbacks_->streamInfo();
  const auto route = encoder_callbacks_->route();
  if (route == nullptr) {
    return;
  }

  {
    const TagsMetadataStats* route_typed_metadata =
        route->typedMetadata().get<TagsMetadataStats>("proxy.metadata_stats.detailed_stats");
    const TagsMetadataStats* host_typed_metadata = nullptr;
    if (auto upstream_info = stream_info.upstreamInfo(); upstream_info.has_value()) {
      auto upstream_host = upstream_info->upstreamHost();
      host_typed_metadata = upstream_host == nullptr
                                ? nullptr
                                : upstream_host->typedMetadata().get<TagsMetadataStats>(
                                      "proxy.metadata_stats.detailed_stats");
    }

    if (route_typed_metadata == nullptr && host_typed_metadata == nullptr) {
      return;
    }

    const auto request_header = stream_info.getRequestHeaders();
    ASSERT(request_header != nullptr);

    auto context = config_->streamStatsContext(*response_headers_, stream_info);

    Common::Http::HttpCommonMatcherContext match_context(*request_header);
    route->traversePerFilterConfig(
        name(), [&match_context, &context](const Router::RouteSpecificFilterConfig& cfg) {
          if (auto* type_config = dynamic_cast<const RouterConfig*>(&cfg); type_config != nullptr) {
            type_config->completeStreamStatsContext(match_context, context);
          }
        });

    config_->completeStreamStatsContext(match_context, context);

    if (route_typed_metadata != nullptr) {
      route_typed_metadata->onStreamComplete(context);
    }
    if (host_typed_metadata != nullptr) {
      host_typed_metadata->onStreamComplete(context);
    }
  }
}

} // namespace DetailedStats
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
