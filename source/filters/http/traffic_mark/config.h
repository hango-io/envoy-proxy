#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/traffic_mark/traffic_mark_filter.h"

#include "api/proxy/filters/http/traffic_mark/v2/traffic_mark.pb.h"
#include "api/proxy/filters/http/traffic_mark/v2/traffic_mark.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace TrafficMark {

class HttpTrafficMarkFilterFactory
    : public Envoy::Extensions::HttpFilters::Common::FactoryBase<ProtoCommonConfig> {
public:
  HttpTrafficMarkFilterFactory() : FactoryBase(HttpTrafficMarkFilter::name()) {}

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfig(const Protobuf::Message& config,
                                  Server::Configuration::ServerFactoryContext&,
                                  ProtobufMessage::ValidationVisitor&) override;

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;
};

} // namespace TrafficMark
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
