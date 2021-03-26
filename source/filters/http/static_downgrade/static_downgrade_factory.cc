#include "filters/http/static_downgrade/static_downgrade_factory.h"

#include "api/proxy/filters/http/static_downgrade/v2/static_downgrade.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace StaticDowngrade {

Http::FilterFactoryCb HttpStaticDowngradeFilterFactory::createFilterFactoryFromProto(
    const Protobuf::Message&, const std::string&, Server::Configuration::FactoryContext&) {
  return [](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(Http::StreamFilterSharedPtr{new HttpStaticDowngradeFilter()});
  };
}

ProtobufTypes::MessagePtr HttpStaticDowngradeFilterFactory::createEmptyConfigProto() {
  return std::make_unique<ProtoCommonConfig>();
}

// Route config type is same as init config type in default

ProtobufTypes::MessagePtr HttpStaticDowngradeFilterFactory::createEmptyRouteConfigProto() {
  return std::make_unique<ProtoRouteConfig>();
}

Router::RouteSpecificFilterConfigConstSharedPtr
HttpStaticDowngradeFilterFactory::createRouteSpecificFilterConfig(
    const Protobuf::Message& config, Server::Configuration::ServerFactoryContext& context,
    ProtobufMessage::ValidationVisitor&) {
  const auto& proto_config = dynamic_cast<const ProtoRouteConfig&>(config);
  return std::make_shared<StaticDowngradeRouteConfig>(proto_config, context);
}

REGISTER_FACTORY(HttpStaticDowngradeFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace StaticDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
