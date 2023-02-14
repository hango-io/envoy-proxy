#include "source/filters/http/dynamic_downgrade/dynamic_downgrade_factory.h"

#include "api/proxy/filters/http/dynamic_downgrade/v2/dynamic_downgrade.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DynamicDowngrade {

Http::FilterFactoryCb HttpDynamicDowngradeFilterFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  const auto& proto_config = dynamic_cast<const ProtoCommonConfig&>(config);
  auto shared_config = std::make_shared<DynamicDowngradeCommonConfig>(proto_config, context);
  return [shared_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(
        Http::StreamFilterSharedPtr{new HttpDynamicDowngradeFilter(shared_config.get())});
  };
}

ProtobufTypes::MessagePtr HttpDynamicDowngradeFilterFactory::createEmptyConfigProto() {
  return std::make_unique<ProtoCommonConfig>();
}

// Route config type is same as init config type in default

ProtobufTypes::MessagePtr HttpDynamicDowngradeFilterFactory::createEmptyRouteConfigProto() {
  return std::make_unique<ProtoRouteConfig>();
}

Router::RouteSpecificFilterConfigConstSharedPtr
HttpDynamicDowngradeFilterFactory::createRouteSpecificFilterConfig(
    const Protobuf::Message& config, Server::Configuration::ServerFactoryContext&,
    ProtobufMessage::ValidationVisitor&) {
  const auto& proto_config = dynamic_cast<const ProtoRouteConfig&>(config);
  return std::make_shared<DynamicDowngradeRouteConfig>(proto_config);
}

REGISTER_FACTORY(HttpDynamicDowngradeFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace DynamicDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
