#pragma once

#include "envoy/common/exception.h"
#include "envoy/stream_info/filter_state.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace FilterState {

class PlainStateImpl : public StreamInfo::FilterState::Object {
public:
  PlainStateImpl(const std::string& value) : value_(value) {}

  absl::optional<std::string> serializeAsString() const override;

private:
  std::string value_;
};

} // namespace FilterState
} // namespace Common
} // namespace Proxy
} // namespace Envoy
