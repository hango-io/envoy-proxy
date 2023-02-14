#include "source/filters/http/detailed_stats/config.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DetailedStats {

Http::FilterFactoryCb
Factory::createFilterFactoryFromProtoTyped(const ProtoCommonConfig& config, const std::string&,
                                           Server::Configuration::FactoryContext& context) {
  for (const auto& ns : config.custom_stat_namespaces()) {
    context.api().customStatNamespaces().registerStatNamespace(ns);
  }

  auto shared_config = std::make_shared<CommonConfig>(config, context);
  return [shared_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamEncoderFilter(
        Http::StreamEncoderFilterSharedPtr{new Filter(shared_config.get())});
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
Factory::createRouteSpecificFilterConfigTyped(const ProtoRouterConfig& config,
                                              Server::Configuration::ServerFactoryContext&,
                                              ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<RouterConfig>(config);
}

REGISTER_FACTORY(Factory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace DetailedStats
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
