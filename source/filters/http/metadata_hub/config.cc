#include "filters/http/metadata_hub/config.h"
#include "filters/http/metadata_hub/metadata_hub.h"
#include "filters/http/well_known_names.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace MetadataHub {

Http::FilterFactoryCb HttpMetadataHubConfigFactory::createFilterFactoryFromProto(
    const Protobuf::Message& config, const std::string&, Server::Configuration::FactoryContext&) {
  const auto& proto_config = dynamic_cast<const ProtoCommonConfig&>(config);
  auto shared_config = std::make_shared<MetadataHubCommonConfig>(proto_config);
  return [shared_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(
        Http::StreamFilterSharedPtr{new HttpMetadataHubFilter(shared_config.get())});
  };
}

ProtobufTypes::MessagePtr HttpMetadataHubConfigFactory::createEmptyConfigProto() {
  return std::make_unique<ProtoCommonConfig>();
}

std::string HttpMetadataHubConfigFactory::name() const { return HttpFilterNames::get().MetadataHub; }

REGISTER_FACTORY(HttpMetadataHubConfigFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace MetadataHub
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
