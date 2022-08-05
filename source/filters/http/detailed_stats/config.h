
#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"
#include "source/filters/http/detailed_stats/detailed_stats.h"

#include "api/proxy/filters/http/detailed_stats/v2/detailed_stats.pb.h"
#include "api/proxy/filters/http/detailed_stats/v2/detailed_stats.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DetailedStats {

class Factory : public Extensions::HttpFilters::Common::FactoryBase<ProtoCommonConfig> {
public:
  Factory() : FactoryBase(Filter::name()) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;
};

} // namespace DetailedStats
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
