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

#include "common/buffer/watermark_buffer.h"
#include "common/common/token_bucket_impl.h"

#include "extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/metadata_hub/v2/metadata_hub.pb.h"

#include "common/filter_state/plain_state.h"
#include "common/metadata/typed_metadata.h"

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

private:
  MetadataHubCommonConfig* config_{nullptr};
};

} // namespace MetadataHub

} // namespace HttpFilters

} // namespace Proxy
} // namespace Envoy
