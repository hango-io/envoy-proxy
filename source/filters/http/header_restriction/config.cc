#include "source/filters/http/header_restriction/config.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace HeaderRestriction {

static constexpr char HeaderRestriction[] = "proxy.filters.http.header_restriction";
static constexpr char UARestriction[] = "proxy.filters.http.ua_restriction";
static constexpr char RefererRestriction[] = "proxy.filters.http.referer_restriction";

using HeaderRestrictionFactory = Factory<HeaderRestriction>;
using UARestrictionFactory = Factory<UARestriction>;
using RefererRestrictionFactory = Factory<RefererRestriction>;

REGISTER_FACTORY(HeaderRestrictionFactory, Server::Configuration::NamedHttpFilterConfigFactory);
REGISTER_FACTORY(UARestrictionFactory, Server::Configuration::NamedHttpFilterConfigFactory);
REGISTER_FACTORY(RefererRestrictionFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace HeaderRestriction
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
