#include "source/common/filter_state/plain_state.h"
#include "source/common/common/proxy_utility.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace FilterState {

absl::optional<std::string> PlainStateImpl::serializeAsString() const { return value_; }

} // namespace FilterState
} // namespace Common
} // namespace Proxy
} // namespace Envoy
