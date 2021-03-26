#pragma once

#include "envoy/registry/registry.h"

#include "extensions/filters/http/common/factory_base.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace MetadataHub {

class HttpMetadataHubConfigFactory : public Server::Configuration::NamedHttpFilterConfigFactory {
public:
  Http::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message&, const std::string&,
                               Server::Configuration::FactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  std::string name() const override;
};

} // namespace MetadataHub
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
