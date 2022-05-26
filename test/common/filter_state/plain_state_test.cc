#include "source/common/filter_state/plain_state.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace FilterState {

TEST(PlainStateImplTest, PlainStateImplTest) {
  // string
  PlainStateImpl empty_string_state(std::string(""));
  EXPECT_EQ("", empty_string_state.serializeAsString().value());

  PlainStateImpl normal_string_state(std::string("xxxabcdefg0123456"));
  EXPECT_EQ("xxxabcdefg0123456", normal_string_state.serializeAsString().value());
}

} // namespace FilterState
} // namespace Common
} // namespace Proxy
} // namespace Envoy
