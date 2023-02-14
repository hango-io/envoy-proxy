#include "source/filters/http/traffic_mark/config.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace TrafficMark {

Http::FilterFactoryCb HttpTrafficMarkFilterFactory::createFilterFactoryFromProtoTyped(
    const ProtoCommonConfig& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  auto shared_config = std::make_shared<ConfigImpl>(proto_config);
  return [shared_config, &context](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamDecoderFilter(Http::StreamDecoderFilterSharedPtr{
        new HttpTrafficMarkFilter(*shared_config, context.clusterManager())});
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
HttpTrafficMarkFilterFactory::createRouteSpecificFilterConfig(
    const Protobuf::Message& config, Server::Configuration::ServerFactoryContext&,
    ProtobufMessage::ValidationVisitor&) {
  const auto& proto_config = dynamic_cast<const ProtoRouteConfig&>(config);
  return std::make_shared<TrafficMarkRouteConfig>(proto_config);
}

REGISTER_FACTORY(HttpTrafficMarkFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace TrafficMark
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
