#pragma once

#include "envoy/registry/registry.h"
#include "envoy/router/router.h"

#include "api/proxy/upstreams/http/dubbo/v3/dubbo.pb.h"

namespace Envoy {

namespace Proxy {
namespace Upstreams {
namespace Http {
namespace Dubbo {

/**
 * Config registration for the TcpConnPool. @see Router::GenericConnPoolFactory
 */
class DubboGenericConnPoolFactory : public Envoy::Router::GenericConnPoolFactory {
public:
  std::string name() const override { return "proxy.upstreams.http.dubbo"; }
  std::string category() const override { return "envoy.upstreams"; }
  Envoy::Router::GenericConnPoolPtr
  createGenericConnPool(Envoy::Upstream::ThreadLocalCluster& thread_local_cluster, bool is_connect,
                        const Envoy::Router::RouteEntry& route_entry,
                        absl::optional<Envoy::Http::Protocol> downstream_protocol,
                        Envoy::Upstream::LoadBalancerContext* ctx) const override;
  Envoy::ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<proxy::upstreams::http::dubbo::v3::DubboUpstreamPool>();
  }
};

DECLARE_FACTORY(DubboGenericConnPoolFactory);

} // namespace Dubbo
} // namespace Http
} // namespace Upstreams
} // namespace Proxy
} // namespace Envoy
