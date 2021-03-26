#pragma once

#include "envoy/registry/registry.h"

#include "extensions/filters/http/common/factory_base.h"
#include "filters/http/local_limit/local_limit_filter.h"
#include "filters/http/well_known_names.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace LocalLimit {

class HttpLocalLimitFilterFactory : public Server::Configuration::NamedHttpFilterConfigFactory {
public:
  Http::FilterFactoryCb
  createFilterFactoryFromProto(const Protobuf::Message& config, const std::string&,
                               Server::Configuration::FactoryContext&) override;

  ProtobufTypes::MessagePtr createEmptyConfigProto() override;

  ProtobufTypes::MessagePtr createEmptyRouteConfigProto() override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfig(const Protobuf::Message& config,
                                  Server::Configuration::ServerFactoryContext&,
                                  ProtobufMessage::ValidationVisitor&) override;

  std::string name() const override { return HttpFilterNames::get().LocalLimit; }
};

} // namespace LocalLimit
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
