#include "source/upstreams/http/dubbo/config.h"

#include "source/upstreams/http/dubbo/upstream_request.h"

namespace Envoy {
namespace Proxy {
namespace Upstreams {
namespace Http {
namespace Dubbo {

Envoy::Router::GenericConnPoolPtr DubboGenericConnPoolFactory::createGenericConnPool(
    Envoy::Upstream::ThreadLocalCluster& thread_local_cluster, bool is_connect,
    const Envoy::Router::RouteEntry& route_entry,
    absl::optional<Envoy::Http::Protocol> downstream_protocol,
    Envoy::Upstream::LoadBalancerContext* ctx) const {
  auto ret = std::make_unique<DubboConnPool>(thread_local_cluster, is_connect, route_entry,
                                             downstream_protocol, ctx);
  return (ret->valid() ? std::move(ret) : nullptr);
}

REGISTER_FACTORY(DubboGenericConnPoolFactory, Envoy::Router::GenericConnPoolFactory);

} // namespace Dubbo
} // namespace Http
} // namespace Upstreams
} // namespace Proxy
} // namespace Envoy
