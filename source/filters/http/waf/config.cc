#include "source/filters/http/waf/config.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Waf {

Http::FilterFactoryCb
WafFilterConfig::createFilterFactoryFromProto(const Protobuf::Message& config,
                                              const std::string& stats_prefix,
                                              Server::Configuration::FactoryContext& context) {
  const auto& typed_config = dynamic_cast<const proxy::filters::http::waf::v2::RuleSet&>(config);
  auto shared_config = std::make_shared<RuleSetConfig>(typed_config);
  return [shared_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(Http::StreamFilterSharedPtr{new WafFilter(shared_config.get())});
  };
}

ProtobufTypes::MessagePtr WafFilterConfig::createEmptyConfigProto() {
  return std::make_unique<proxy::filters::http::waf::v2::RuleSet>();
}

ProtobufTypes::MessagePtr WafFilterConfig::createEmptyRouteConfigProto() {
  return std::make_unique<proxy::filters::http::waf::v2::RuleSet>();
}

Router::RouteSpecificFilterConfigConstSharedPtr
WafFilterConfig::createRouteSpecificFilterConfig(const Protobuf::Message& config,
                                                 Server::Configuration::ServerFactoryContext&,
                                                 ProtobufMessage::ValidationVisitor&) {
  const auto& typed_config = dynamic_cast<const proxy::filters::http::waf::v2::RuleSet&>(config);
  return std::make_shared<RuleSetConfig>(typed_config);
}

std::string WafFilterConfig::name() const { return WafFilter::name(); }

REGISTER_FACTORY(WafFilterConfig, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace Waf
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
