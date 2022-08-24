#include "source/filters/http/header_rewrite/config.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRewrite {

static constexpr char HeaderRewrite[] = "proxy.filters.http.header_rewrite";
using HeaderRewriteFactory = Factory<HeaderRewrite>;
REGISTER_FACTORY(HeaderRewriteFactory, Server::Configuration::NamedHttpFilterConfigFactory);

/**
 * Some alias name filter.
 */
static constexpr char HeaderRewriteForRequestHeadersToAdd[] =
    "proxy.filters.http.header_rewrite.request_headers_to_add";
static constexpr char HeaderRewriteForTrafficMark[] =
    "proxy.filters.http.header_rewrite.traffic_mark";

using HeaderRewriteForRequestHeadersToAddFactory = Factory<HeaderRewriteForRequestHeadersToAdd>;
using HeaderRewriteForTrafficMarkFactory = Factory<HeaderRewriteForTrafficMark>;

REGISTER_FACTORY(HeaderRewriteForRequestHeadersToAddFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);
REGISTER_FACTORY(HeaderRewriteForTrafficMarkFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace HeaderRewrite
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
