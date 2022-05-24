#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/rider/filter.h"

#include "api/proxy/filters/http/rider/v3alpha1/rider.pb.h"
#include "api/proxy/filters/http/rider/v3alpha1/rider.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

/**
 * Config registration for the Rider filter. @see NamedHttpFilterConfigFactory.
 */
class RiderFilterConfigFactory
    : public Envoy::Extensions::HttpFilters::Common::FactoryBase<FilterConfigProto,
                                                                 RouteConfigProto> {

public:
  RiderFilterConfigFactory() : FactoryBase(Filter::name()) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const FilterConfigProto& proto_config, const std::string&,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const RouteConfigProto& proto_config,
                                       Server::Configuration::ServerFactoryContext& context,
                                       ProtobufMessage::ValidationVisitor&) override;
};

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
