#include "common/common/proxy_utility.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Common {

TEST(StringUtilTest, EscapeForJson) {
  // 普通字符串
  std::string test1 = "abcdefghigklmnopqrstuvwxyz1234567890!@#$%^&*()_+-={}[]|';:?/.,<>";
  std::string new_test1 = StringUtil::escapeForJson(test1);
  EXPECT_EQ(test1, new_test1);

  // 带双引号字符串
  std::string test2 = "abedefg\"xxxxxxxx";
  std::string expect_test2 = "abedefg\\\"xxxxxxxx";
  std::string new_test2 = StringUtil::escapeForJson(test2);
  EXPECT_EQ(expect_test2, new_test2);

  // 带\字符串
  std::string test3 = "abcdefg\\xxxxxxxx";
  std::string expect_test3 = "abcdefg\\\\xxxxxxxx";
  std::string new_test3 = StringUtil::escapeForJson(test3);
  EXPECT_EQ(expect_test3, new_test3);

  // 空白字符串
  std::string test4 = "xxx\b\n\r\f\txxx";
  std::string expect_test4 = "xxx\\b\\n\\r\\f\\txxx";
  std::string new_test4 = StringUtil::escapeForJson(test4);
  EXPECT_EQ(expect_test4, new_test4);

  // 混合测试
  std::string test5 = "xxx\"xxx\\xxx\nxxx";
  std::string expect_test5 = "xxx\\\"xxx\\\\xxx\\nxxx";
  std::string new_test5 = StringUtil::escapeForJson(test5);
  EXPECT_EQ(expect_test5, new_test5);

  // 空字符串
  std::string test6 = "";
  std::string expect_test6 = "";
  std::string new_test6 = StringUtil::escapeForJson(test6);
  EXPECT_EQ(expect_test6, new_test6);
}

} // namespace Common
} // namespace Common
} // namespace Proxy
} // namespace Envoy
