#include "source/filters/http/detailed_stats/detailed_stats.h"

#include "source/common/common/utility.h"
#include "source/common/config/utility.h"
#include "source/common/protobuf/message_validator_impl.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DetailedStats {

namespace {

std::string metadataToString(const ProtobufWkt::Struct& data) {
  auto iter = data.fields().find("stats");
  if (iter == data.fields().end()) {
    return "";
  }

  std::vector<std::string> name;
  if (iter->second.kind_case() != ProtobufWkt::Value::kListValue) {
    return "";
  }

  for (const auto& value : iter->second.list_value().values()) {
    if (value.kind_case() == ProtobufWkt::Value::kStringValue) {
      name.push_back(StringUtil::escapeDotToPlus(value.string_value()));
      continue;
    }
    return "";
  }

  return absl::StrJoin(name, ".");
}

} // namespace

MetadataStats::MetadataStats(const std::string& prefix, const ProtobufWkt::Struct& data) {
  const std::string name = metadataToString(data);
  if (name.empty()) {
    disabled_ = true;
    return;
  }

  auto* stats = Runtime::LoaderSingleton::getExisting();
  ASSERT(stats != nullptr);

  request_total_scope_ = stats->getRootScope().createScope(prefix + "." + name + ".request_total");
  request_bytes_scope_ = stats->getRootScope().createScope(prefix + "." + name + ".request_bytes");
  response_bytes_scope_ =
      stats->getRootScope().createScope(prefix + "." + name + ".response_bytes");
  request_time_scope_ = stats->getRootScope().createScope(prefix + "." + name + ".request_time");
}

class RouteTypedMetadataFactory : public Envoy::Router::HttpRouteTypedMetadataFactory {
public:
  std::string name() const override { return Filter::name(); } // namespace DetailedStats
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Struct& data) const override {
    return std::make_unique<MetadataStats>("detailed_route", data);
  }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Any&) const override {
    return nullptr;
  }
};

static Registry::RegisterFactory<RouteTypedMetadataFactory,
                                 Envoy::Router::HttpRouteTypedMetadataFactory>
    route_register_;

class HostTypedMetadataFactory : public Envoy::Upstream::HostTypedMetadataFactory {
public:
  std::string name() const override { return Filter::name(); }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Struct& data) const override {
    return std::make_unique<MetadataStats>("detailed_host", data);
  }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Any&) const override {
    return nullptr;
  }
};

static Registry::RegisterFactory<HostTypedMetadataFactory,
                                 Envoy::Upstream::HostTypedMetadataFactory>
    host_register_;

class TagsRouteTypedMetadataFactory : public Envoy::Router::HttpRouteTypedMetadataFactory {
public:
  std::string name() const override { return "proxy.metadata_stats.detailed_stats"; }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Struct& data) const override {
    proxy::filters::http::detailed_stats::v2::PrefixedStatTags prefixed_stat_tags;

    MessageUtil::jsonConvert(data, Envoy::ProtobufMessage::getStrictValidationVisitor(),
                             prefixed_stat_tags);
    return std::make_unique<TagsMetadataStats>(prefixed_stat_tags);
  }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Any& data) const override {
    proxy::filters::http::detailed_stats::v2::PrefixedStatTags prefixed_stat_tags;

    Envoy::Config::Utility::translateOpaqueConfig(
        data, Envoy::ProtobufMessage::getStrictValidationVisitor(), prefixed_stat_tags);

    return std::make_unique<TagsMetadataStats>(prefixed_stat_tags);
  }
};

static Registry::RegisterFactory<TagsRouteTypedMetadataFactory,
                                 Envoy::Router::HttpRouteTypedMetadataFactory>
    tags_route_register_;

class TagsHostTypedMetadataFactory : public Envoy::Upstream::HostTypedMetadataFactory {
public:
  std::string name() const override { return "proxy.metadata_stats.detailed_stats"; }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Struct& data) const override {
    proxy::filters::http::detailed_stats::v2::PrefixedStatTags prefixed_stat_tags;

    MessageUtil::jsonConvert(data, Envoy::ProtobufMessage::getStrictValidationVisitor(),
                             prefixed_stat_tags);
    return std::make_unique<TagsMetadataStats>(prefixed_stat_tags);
  }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Any& data) const override {
    proxy::filters::http::detailed_stats::v2::PrefixedStatTags prefixed_stat_tags;

    Envoy::Config::Utility::translateOpaqueConfig(
        data, Envoy::ProtobufMessage::getStrictValidationVisitor(), prefixed_stat_tags);

    return std::make_unique<TagsMetadataStats>(prefixed_stat_tags);
  }
};

static Registry::RegisterFactory<TagsHostTypedMetadataFactory,
                                 Envoy::Upstream::HostTypedMetadataFactory>
    tags_host_register_;

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool) {
  response_headers_ = &headers;
  return Http::FilterHeadersStatus::Continue;
}

namespace {

bool useOldContext() {
  static const bool use_old_context = std::getenv("USE_OLD_DETAILED_STATS") != nullptr;
  return use_old_context;
}

} // namespace

void Filter::onDestroy() {
  if (response_headers_ == nullptr) {
    return;
  }
  const auto& stream_info = encoder_callbacks_->streamInfo();
  const auto route = encoder_callbacks_->route();
  if (route == nullptr) {
    return;
  }

  if (useOldContext()) {
    const MetadataStats* route_typed_metadata =
        route->typedMetadata().get<MetadataStats>(Filter::name());
    const MetadataStats* host_typed_metadata = nullptr;
    if (auto upstream_info = stream_info.upstreamInfo(); upstream_info.has_value()) {
      auto upstream_host = upstream_info->upstreamHost();
      host_typed_metadata = upstream_host == nullptr
                                ? nullptr
                                : upstream_host->typedMetadata().get<MetadataStats>(Filter::name());
    }

    if (route_typed_metadata == nullptr && host_typed_metadata == nullptr) {
      return;
    }

    MetadataStats::StreamStatsContext context;
    auto storage = config_->statNameStorage(*response_headers_, stream_info);
    context.stat_name_ = Stats::StatName(storage.get());
    context.request_bytes_ = stream_info.bytesReceived();
    context.response_bytes_ = stream_info.bytesSent();

    auto duration = stream_info.requestComplete();
    if (duration.has_value()) {
      context.request_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  stream_info.requestComplete().value())
                                  .count();
    }

    if (route_typed_metadata != nullptr) {
      route_typed_metadata->onStreamComplete(context);
    }
    if (host_typed_metadata != nullptr) {
      host_typed_metadata->onStreamComplete(context);
    }
  } else {
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

    auto context = config_->streamStatsContext(*request_header, *response_headers_, stream_info);

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
