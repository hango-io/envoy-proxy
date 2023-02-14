#include "source/filters/http/service_downgrade/config.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace ServiceDowngrade {

Http::FilterFactoryCb
Factory::createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config,
                                           const std::string&,
                                           Server::Configuration::FactoryContext& context) {
  auto shared_config = std::make_shared<CommonConfig>(proto_config, context);
  return [shared_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(Http::StreamFilterSharedPtr{new Filter(shared_config.get())});
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
Factory::createRouteSpecificFilterConfigTyped(const ProtoRouteConfig& proto_config,
                                              Server::Configuration::ServerFactoryContext&,
                                              ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<RouteConfig>(proto_config);
}

REGISTER_FACTORY(Factory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace ServiceDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
