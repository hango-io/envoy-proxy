#pragma once

#include <string>

#include "envoy/http/filter.h"
#include "envoy/server/filter_config.h"

#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/traffic_mark/v2/traffic_mark.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace TrafficMark {

using ProtoCommonConfig = proxy::filters::http::traffic_mark::v2::ProtoCommonConfig;
using ProtoRouteConfig = proxy::filters::http::traffic_mark::v2::ProtoRouteConfig;
using ProtoMetadataPath = proxy::filters::http::traffic_mark::v2::MetadataPath;

class TrafficMarkRouteConfig : public Router::RouteSpecificFilterConfig,
                               Logger::Loggable<Logger::Id::filter> {
public:
  TrafficMarkRouteConfig(const ProtoRouteConfig& config) : config_(config) {}

private:
  ProtoRouteConfig config_;
};

class ConfigImpl : Logger::Loggable<Logger::Id::filter> {
public:
  struct MetadataPath {
    MetadataPath(const ProtoMetadataPath& path) : namespace_(path.metadata_namespace()) {
      for (const auto& segment : path.path()) {
        path_.push_back(segment);
      }
    }

    std::string namespace_;
    std::vector<std::string> path_;
  };

  ConfigImpl(const ProtoCommonConfig& config)
      : header_key_(config.header_key()), match_key_(config.match_key()),
        all_colors_key_(config.all_colors_key()) {
    for (const auto& mark : config.metadata_as_mark()) {
      metadata_as_mark_.insert({mark.first, MetadataPath(mark.second)});
    }
  }

  const std::string& headerKey() const { return header_key_; }
  const std::string& matchKey() const { return match_key_; }
  const std::string& allColorsKey() const { return all_colors_key_; }
  const std::map<std::string, MetadataPath>& metadataAsMark() const { return metadata_as_mark_; }

private:
  const std::string header_key_;
  const std::string match_key_;
  const std::string all_colors_key_;
  std::map<std::string, MetadataPath> metadata_as_mark_;
};

class HttpTrafficMarkFilter : public Http::PassThroughDecoderFilter,
                              Logger::Loggable<Logger::Id::filter> {
public:
  HttpTrafficMarkFilter(const ConfigImpl& config, Upstream::ClusterManager& cm)
      : config_(config), cm_(cm) {}

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.traffic_mark");
  }

private:
  const ConfigImpl& config_;
  Upstream::ClusterManager& cm_;
};

} // namespace TrafficMark
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
