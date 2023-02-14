#pragma once

#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "envoy/config/typed_metadata.h"
#include "envoy/http/filter.h"
#include "envoy/router/router.h"
#include "envoy/server/filter_config.h"
#include "envoy/stats/scope.h"
#include "envoy/stats/stats_macros.h"

#include "source/common/filter_state/plain_state.h"
#include "source/common/metadata/typed_metadata.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/metadata_hub/v2/metadata_hub.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace MetadataHub {

using ProtoCommonConfig = proxy::filters::http::metadata_hub::v2::ProtoCommonConfig;

class MetadataHubCommonConfig : Logger::Loggable<Logger::Id::filter> {
public:
  using DecodeHeadersHandler = std::function<absl::string_view(Http::RequestHeaderMap&)>;
  using EncodeHeadersHandler = std::function<absl::string_view(Http::ResponseHeaderMap&)>;

  MetadataHubCommonConfig(const ProtoCommonConfig& config);

  const auto& decodeHeadersToState() { return decode_headers_to_state_; }
  const auto& encodeHeadersToState() { return encode_headers_to_state_; }

  const auto& routeMatadataToState() { return route_metadata_to_state_; }

private:
  void handleDecodeHeaders(Http::LowerCaseString&& name, Http::LowerCaseString&& rename);
  void handleEncodeHeaders(Http::LowerCaseString&& name, Http::LowerCaseString&& rename);

  std::vector<std::pair<Http::LowerCaseString, DecodeHeadersHandler>> decode_headers_to_state_;
  std::vector<std::pair<Http::LowerCaseString, EncodeHeadersHandler>> encode_headers_to_state_;
  std::vector<std::string> route_metadata_to_state_;
};

class HttpMetadataHubFilter : public Http::PassThroughFilter, Logger::Loggable<Logger::Id::filter> {
public:
  HttpMetadataHubFilter(MetadataHubCommonConfig* config) : config_(config){};
  ~HttpMetadataHubFilter() = default;

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.metadatahub");
  }

private:
  MetadataHubCommonConfig* config_{nullptr};
};

class TypedMetadataFactory : public Envoy::Router::HttpRouteTypedMetadataFactory {
public:
  std::string name() const override { return HttpMetadataHubFilter::name(); }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Struct& data) const override {
    return std::make_unique<Common::Metadata::MetadataToSimpleMap>(data);
  }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Any&) const override {
    return nullptr;
  }
};

} // namespace MetadataHub

} // namespace HttpFilters

} // namespace Proxy
} // namespace Envoy
