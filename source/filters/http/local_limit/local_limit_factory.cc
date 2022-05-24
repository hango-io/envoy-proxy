#include "source/filters/http/local_limit/local_limit_factory.h"

#include "api/proxy/filters/http/local_limit/v2/local_limit.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace LocalLimit {

Http::FilterFactoryCb HttpLocalLimitFilterFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config, const std::string&, Server::Configuration::FactoryContext&) {
  const auto& proto_config = dynamic_cast<const ProtoCommonConfig&>(config);
  auto shared_config = std::make_shared<LocalLimitCommonConfig>(proto_config);
  return [shared_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamDecoderFilter(
        Http::StreamDecoderFilterSharedPtr{new HttpLocalLimitFilter(shared_config.get())});
  };
}

ProtobufTypes::MessagePtr HttpLocalLimitFilterFactory::createEmptyConfigProto() {
  return std::make_unique<ProtoCommonConfig>();
}

// Route config type is same as init config type in default

ProtobufTypes::MessagePtr HttpLocalLimitFilterFactory::createEmptyRouteConfigProto() {
  return std::make_unique<ProtoRouteConfig>();
}

Router::RouteSpecificFilterConfigConstSharedPtr
HttpLocalLimitFilterFactory::createRouteSpecificFilterConfig(
    const Protobuf::Message& config, Server::Configuration::ServerFactoryContext& context,
    ProtobufMessage::ValidationVisitor&) {
  const auto& proto_config = dynamic_cast<const ProtoRouteConfig&>(config);
  return std::make_shared<LocalLimitRouteConfig>(proto_config, context);
}

REGISTER_FACTORY(HttpLocalLimitFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace LocalLimit
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
