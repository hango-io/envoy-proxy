#include <memory>

#include "envoy/registry/registry.h"

#include "filters/http/rider/config.h"

#include "common/protobuf/utility.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

Http::FilterFactoryCb RiderFilterConfigFactory::createFilterFactoryFromProtoTyped(
    const FilterConfigProto& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  std::shared_ptr<ConfigImpl> filter_config(
      new ConfigImpl{proto_config, context.clusterManager(), context.accessLogManager(),
                     context.timeSource(), context.threadLocal(), context.api()});
  return [filter_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Filter>(*filter_config));
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
RiderFilterConfigFactory::createRouteSpecificFilterConfigTyped(
    const RouteConfigProto& proto_config, Server::Configuration::ServerFactoryContext&,
    ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<RouteConfig>(proto_config);
}

/**
 * Static registration for the Rider filter. @see RegisterFactory.
 */
REGISTER_FACTORY(RiderFilterConfigFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
