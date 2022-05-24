#include "source/filters/http/ip_restriction/ip_restriction_config.h"

#include "api/proxy/filters/http/ip_restriction/v2/ip_restriction.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace IpRestriction {

Http::FilterFactoryCb HttpIpRestrictionFilterConfig::createFilterFactoryFromProto(
    const Protobuf::Message& config, const std::string& stats_prefix,
    Server::Configuration::FactoryContext& context) {
  const auto& typed_config =
      dynamic_cast<const proxy::filters::http::iprestriction::v2::ListGlobalConfig&>(config);
  auto shared_config = std::make_shared<ListGlobalConfig>(typed_config, context, stats_prefix);
  return [shared_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamDecoderFilter(
        Http::StreamDecoderFilterSharedPtr{new HttpIpRestrictionFilter(shared_config.get())});
  };
}

ProtobufTypes::MessagePtr HttpIpRestrictionFilterConfig::createEmptyConfigProto() {
  return std::make_unique<proxy::filters::http::iprestriction::v2::ListGlobalConfig>();
}

ProtobufTypes::MessagePtr HttpIpRestrictionFilterConfig::createEmptyRouteConfigProto() {
  return std::make_unique<proxy::filters::http::iprestriction::v2::BlackOrWhiteList>();
}

Router::RouteSpecificFilterConfigConstSharedPtr
HttpIpRestrictionFilterConfig::createRouteSpecificFilterConfig(
    const Protobuf::Message& config, Server::Configuration::ServerFactoryContext&,
    ProtobufMessage::ValidationVisitor&) {
  const auto& typed_config =
      dynamic_cast<const proxy::filters::http::iprestriction::v2::BlackOrWhiteList&>(config);
  return std::make_shared<BlackOrWhiteListConfig>(typed_config);
}

std::string HttpIpRestrictionFilterConfig::name() const { return HttpIpRestrictionFilter::name(); }

REGISTER_FACTORY(HttpIpRestrictionFilterConfig,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace IpRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
