#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "source/common/dubbo/utility.h"
#include "source/common/http/utility.h"

#include "test/mocks/tcp/mocks.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Proxy {
namespace Common {
namespace DubboBridge {

namespace {

inline void addInt32(Envoy::Buffer::Instance& buffer, uint32_t value) {
  value = htobe32(value);
  buffer.add(&value, 4);
}

inline void addInt64(Envoy::Buffer::Instance& buffer, uint64_t value) {
  value = htobe64(value);
  buffer.add(&value, 8);
}

class GenericTypeArgumentGetterTest : public ArgumentGetter {
public:
  GenericTypeArgumentGetterTest(SrcsRequestContext& source) {

    doc_.Parse(source.getBody().data(), source.getBody().size());
  }
  HessianValueSharedPtr getArgumentByParameter(const Parameter&) const override {
    return Utility::jsonValueToHessianValue(doc_, "not_support", false);
  }

  Json::Document doc_;
};

class ArgumentGetterTest : public ArgumentGetter {
public:
  ArgumentGetterTest(const SrcsRequestContext& source, absl::optional<absl::string_view> prefix) {
    if (prefix.has_value()) {
      source.iterateHeaders([this, prefix](absl::string_view key, absl::string_view value) {
        if (absl::StartsWith(key, prefix.value())) {
          attatchments_.emplace(std::string(key), std::string(value));
        }
        return true;
      });
    }
  }

  HessianValueSharedPtr getArgumentByParameter(const Parameter&) const override { return nullptr; }
  OptRef<const FlatStringMap> attachmentsFromGetter() const override { return attatchments_; }
  FlatStringMap attatchments_;
};

// void writeHessianHeartbeatRequestMessage(Envoy::Buffer::Instance& buffer, int64_t request_id) {
//   uint8_t msg_type = 0xc2; // request message, two_way, not event
//   msg_type = msg_type | 0x20;

//   buffer.add(std::string{'\xda', '\xbb'});
//   buffer.writeByte(static_cast<uint8_t>(msg_type));
//   buffer.writeByte(0x14);;
//   addInt64(buffer, request_id);                    // Request Id
//   buffer.add(std::string{0x00, 0x00, 0x00, 0x01}); // Body Length
//   buffer.add(std::string{0x01});                   // Body
// }

TEST(ConvertUtilityTest, JsonValueToHessionValueTest) {

  // java.lang.Boolean
  {
    const std::string type = "java.lang.Boolean";

    JsonDocument doc;
    doc.Parse("false");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toBoolean().value(), false);

    doc.Parse("true");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toBoolean().value(), true);

    doc.Parse("\"true\"");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toBoolean().value(), true);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("\"123\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"123\"' is invalid for current type");

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("123");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '123' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, false)->toBoolean().value(), false);

    doc.Parse("true");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, false)->toBoolean().value(), true);
  }

  // boolean
  {
    const std::string type = "boolean";

    JsonDocument doc;
    doc.Parse("false");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toBoolean().value(), false);

    doc.Parse("true");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toBoolean().value(), true);

    doc.Parse("null");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'null' is invalid for current type");

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("123");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '123' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, false)->toBoolean().value(), false);

    doc.Parse("true");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, false)->toBoolean().value(), true);
  }

  // java.lang.Float
  {
    const std::string type = "java.lang.Float";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234.5678");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toDouble().value(), 1234.5678);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // float
  {
    const std::string type = "float";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234.5678");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toDouble().value(), 1234.5678);

    doc.Parse("null");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'null' is invalid for current type");

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // java.lang.Double
  {
    const std::string type = "java.lang.Double";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234.5678");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toDouble().value(), 1234.5678);

    doc.Parse("1234.567891234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toDouble().value(),
              1234.567891234567);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // double
  {
    const std::string type = "double";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234.5678");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toDouble().value(), 1234.5678);

    doc.Parse("1234.567891234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toDouble().value(),
              1234.567891234567);

    doc.Parse("null");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'null' is invalid for current type");

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // java.lang.Byte
  {
    const std::string type = "java.lang.Byte";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234);

    doc.Parse("123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 123);

    doc.Parse("-123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), -123);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // byte
  {
    const std::string type = "byte";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234);

    doc.Parse("123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 123);

    doc.Parse("-123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), -123);

    doc.Parse("null");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'null' is invalid for current type");

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // java.lang.Short
  {
    const std::string type = "java.lang.Short";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234);

    doc.Parse("123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 123);

    doc.Parse("-123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), -123);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // short
  {
    const std::string type = "short";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234);

    doc.Parse("123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 123);

    doc.Parse("-123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), -123);

    doc.Parse("null");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'null' is invalid for current type");

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // java.lang.Integer
  {
    const std::string type = "java.lang.Integer";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234);

    doc.Parse("123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 123);

    doc.Parse("-123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), -123);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // int
  {
    const std::string type = "int";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234);

    doc.Parse("123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 123);

    doc.Parse("-123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), -123);

    doc.Parse("null");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'null' is invalid for current type");

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // java.lang.Long
  {
    const std::string type = "java.lang.Long";

    JsonDocument doc;
    doc.Parse("9223372036854775806");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toLong().value(),
              9223372036854775806);

    doc.Parse("1234");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234);

    doc.Parse("123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 123);

    doc.Parse("-123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), -123);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // long
  {
    const std::string type = "long";

    JsonDocument doc;
    doc.Parse("1234567");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234567);

    doc.Parse("1234");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 1234);

    doc.Parse("123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), 123);

    doc.Parse("-123");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->toInteger().value(), -123);

    doc.Parse("null");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'null' is invalid for current type");

    doc.Parse("\"\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"\"' is invalid for current type");

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("\"abc\"");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '\"abc\"' is invalid for current type");
  }

  // java.lang.Character====>json
  {
    const std::string type = "java.lang.Character";

    JsonDocument doc;
    doc.Parse("\"a\"");

    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()), "a");

    doc.Parse("\"abc\"");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()), "abc");

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("false");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'false' is invalid for current type");

    doc.Parse("123");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value '123' is invalid for current type");
  }

  // char====>json
  {
    const std::string type = "char";
    JsonDocument doc;
    doc.Parse("\"a\"");

    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()), "a");

    doc.Parse("\"abc\"");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()), "abc");

    doc.Parse("null");
    EXPECT_THROW_WITH_MESSAGE(Utility::jsonValueToHessianValue(doc, type, true),
                              Envoy::EnvoyException,
                              "Dubbo bridge: value 'null' is invalid for current type");
  }

  // java.lang.String====>json
  {
    const std::string type = "java.lang.String";
    JsonDocument doc;
    doc.Parse("\"test_value\"");

    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()),
              "test_value");

    doc.Parse("\"\"");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()), "");

    doc.Parse("\"a\"");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()), "a");

    doc.Parse("123");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true))->toString().value(), "123");

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse("false");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true))->toString().value(), "false");

    doc.Parse("[\"test_value\",false,1234]");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true))->toString().value(),
              "[\"test_value\",false,1234]");

    doc.Parse("{\"key1\":123,\"key2\":false}");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true))->toString().value(),
              "{\"key1\":123,\"key2\":false}");

    const auto data = std::string(DubboProtocolImpl::MaxBodySize - 2048, 'a');
    JsonValue value;
    value.SetString(data.c_str(), data.size(), doc.GetAllocator());
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(value, type, true))->toString().value(), data);
  }

  // java.lang.Enum====>json
  {
    const std::string type = "java.lang.Enum";
    JsonDocument doc;

    doc.Parse("\"AAAAA\"");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()), "AAAAA");

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }

  // java.time.DayOfWeek====>json
  {
    const std::string type = "java.time.DayOfWeek";
    JsonDocument doc;

    doc.Parse("\"AAAAA\"");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()), "AAAAA");

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }

  // java.math.BigInteger
  {
    const std::string type = "java.math.BigInteger";
    JsonDocument doc;

    doc.Parse("\"123456789123456789123\"");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()),
              "123456789123456789123");

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }

  // java.math.BigDecimal.
  {
    const std::string type = "java.math.BigDecimal";
    JsonDocument doc;

    doc.Parse("\"123456789123456789123.123456789123456789123\"");
    EXPECT_EQ(*(Utility::jsonValueToHessianValue(doc, type, true)->toString().value()),
              "123456789123456789123.123456789123456789123");

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }

  // java.lang.Void.
  {
    const std::string type = "java.lang.Void";
    JsonDocument doc;

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }

  // Type not supported object  Hessian2::Object::Type::UntypedMap
  {
    const std::string type = "other_type";
    const std::string value = "{\
                               \"praenomen\":\"Gaius\",\
                               \"nomen\":null,\
                               \"cognomen\":\"Caezar\",\
                               \"born\":-100,\
                               \"died\":-44\
                               }";
    JsonDocument doc;

    doc.Parse(value);
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::UntypedMap);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }

  // Type not supported object >50
  {
    rapidjson::Document doc;
    JsonValue objects(rapidjson::kObjectType);
    objects.AddMember("key", 132, doc.GetAllocator());

    for (int i = 0; i < 51; i++) {
      JsonValue tem(rapidjson::kObjectType);
      tem.AddMember("key", objects, doc.GetAllocator());
      objects = tem;
    }

    EXPECT_EQ(Utility::jsonValueToHessianValue(objects, "other_type", true)->type(),
              Hessian2::Object::Type::UntypedMap);
  }

  // Type not supported list
  {
    const std::string type = "other_type";
    const std::string value = "{[\"test_value\",false,1234]}";
    JsonDocument doc;

    doc.Parse(value);
    Utility::jsonValueToHessianValue(doc, type, true);

    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }

  // java.util.List
  {
    std::string type = "java.util.List";
    std::string value = "[11,22,1234]";
    JsonDocument doc;

    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::UntypedList);
    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    type = "java.util.ArrayList";
    value = "[11,22,1234]";

    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::UntypedList);
    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    type = "java.util.LinkedList";
    value = "[11,22,1234]";

    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, false)->type(),
              Hessian2::Object::Type::UntypedList);
    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    type = "com/alibaba.fastjson.JSONArray";
    value = "[11,22,1234]";

    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, false)->type(),
              Hessian2::Object::Type::UntypedList);
    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }

  // java.util.Map
  {
    std::string type = "java.util.Map";
    std::string value = "{\"praenomen\":\"Gaius\",\"nomen\":\"Julius\",\"cognomen\":\"Caezar\","
                        "\"born\":-100,\"died\":-4.4213123}";
    JsonDocument doc;
    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kObjectType);
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::UntypedMap);
    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse(value);
    type = "java.util.HashMap";
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::UntypedMap);
    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse(value);
    type = "java.util.TreeMap";
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, false)->type(),
              Hessian2::Object::Type::UntypedMap);
    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);

    doc.Parse(value);
    type = "java.util.LinkedHashMap";
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, false)->type(),
              Hessian2::Object::Type::UntypedMap);
    doc.Parse("null");
    EXPECT_EQ(Utility::jsonValueToHessianValue(doc, type, true)->type(),
              Hessian2::Object::Type::Null);
  }
}

TEST(ConvertUtilityTest, HessionValueToStringTest) {
  // nullptr
  {
    HessianValue* hessian_value = nullptr;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "null");
  }

  // Hessian2::StringObject
  {
    auto value = Hessian2::StringObject("test_value");

    HessianValue* hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "\"test_value\"");
  }

  // Hessian2::BooleanObject
  {
    auto value = Hessian2::BooleanObject(false);

    HessianValue* hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "false");

    value = Hessian2::BooleanObject(true);
    hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "true");
  }

  // Hessian2::IntegerObject
  {
    auto value = Hessian2::IntegerObject(1234);

    HessianValue* hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "1234");

    value = Hessian2::IntegerObject(-1234);
    hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "-1234");
  }

  // Hessian2::LongObject
  {
    auto value = Hessian2::LongObject(21314);

    HessianValue* hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "21314");

    value = Hessian2::LongObject(12345678);
    hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "12345678");

    value = Hessian2::LongObject(123456787654321);
    hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "123456787654321");
  }

  // Hessian2::DoubleObject
  {
    auto value = Hessian2::DoubleObject(21314.1234);

    HessianValue* hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "21314.1234");

    value = Hessian2::DoubleObject(1.2345678);
    hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "1.2345678");
  }

  // Hessian2::DateObject
  {
    std::chrono::milliseconds date1(1000), date2(20000); // 1s、20s
    auto value = Hessian2::DateObject(date1);

    HessianValue* hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "1000");

    value = Hessian2::DateObject(date2);
    hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "20000");
  }

  // Hessian2::BinaryObject
  {
    std::vector<uint8_t> data{1, 2, 3, 4};
    auto value = Hessian2::BinaryObject(data);
    HessianValue* hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "\"\\u0001\\u0002\\u0003\\u0004\"");

    std::vector<uint8_t> data1;
    auto value1 = Hessian2::BinaryObject(data1);
    HessianValue* hessian_value1 = &value1;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value1), "\"\"");
  }

  // Hessian2::RefObject
  {
    // circular
    std::unique_ptr<Hessian2::RefObject> val1, val2;
    val1 = std::make_unique<Hessian2::RefObject>(val2.get());
    val2 = std::make_unique<Hessian2::RefObject>(val1.get());
    EXPECT_EQ(Utility::hessianValueToJsonString(val1.get()), "null");

    auto value = std::make_unique<Hessian2::StringObject>("test_value");
    auto ref_value = std::make_unique<Hessian2::RefObject>(value.get());
    EXPECT_EQ(Utility::hessianValueToJsonString(ref_value.get()), "\"test_value\"");
  }

  // Hessian2::UntypedListObject
  {
    // std::string type = "java.util.List";
    auto list_value = std::make_unique<Hessian2::UntypedListObject>();
    auto list_data = list_value->toMutableUntypedList();
    list_data->emplace_back(Hessian2::ObjectPtr(new Hessian2::StringObject("dasd")));
    list_data->emplace_back(Hessian2::ObjectPtr(new Hessian2::LongObject(1444)));
    EXPECT_EQ(Utility::hessianValueToJsonString(list_value.get()), "[\"dasd\",1444]");
  }

  // Hessian2::TypedListObject
  {
    Hessian2::Object::UntypedList list_val;
    list_val.emplace_back(Hessian2::ObjectPtr(new Hessian2::StringObject("dasd")));
    list_val.emplace_back(Hessian2::ObjectPtr(new Hessian2::LongObject(1345)));
    Hessian2::Object::TypedList val("key1", std::move(list_val));
    auto list_value = std::make_unique<Hessian2::TypedListObject>(std::move(val));
    EXPECT_EQ(Utility::hessianValueToJsonString(list_value.get()), "[\"dasd\",1345]");
  }

  // Hessian2::TypedMapObject
  {
    Hessian2::Object::UntypedMap map_val;
    map_val.emplace(Hessian2::ObjectPtr(new Hessian2::StringObject("dasdssd")),
                    Hessian2::ObjectPtr(new Hessian2::LongObject(135)));
    map_val.emplace(Hessian2::ObjectPtr(new Hessian2::StringObject("world")),
                    Hessian2::ObjectPtr(new Hessian2::LongObject(415)));
    Hessian2::Object::TypedMap val("key2", std::move(map_val));
    auto map_value = std::make_unique<Hessian2::TypedMapObject>(std::move(val));

    EXPECT_EQ(Utility::hessianValueToJsonString(map_value.get()),
              "{\"world\":415,\"dasdssd\":135}");
  }

  // Hessian2::UntypedMapObject
  {
    auto map_val = std::make_unique<Hessian2::UntypedMapObject>();
    auto map_data = map_val->toMutableUntypedMap();
    map_data->emplace(Hessian2::ObjectPtr(new Hessian2::StringObject("dasd")),
                      Hessian2::ObjectPtr(new Hessian2::LongObject(1345)));
    map_data->emplace(Hessian2::ObjectPtr(new Hessian2::BooleanObject(false)),
                      Hessian2::ObjectPtr(new Hessian2::LongObject(45)));

    EXPECT_EQ(Utility::hessianValueToJsonString(map_val.get()), "{\"false\":45,\"dasd\":1345}");
  }

  // Hessian2::ClassInstanceObject ref=nullptr
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "null");
  }

  // Unknow type definition
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value", "value2"};
    auto raw_define =
        std::make_shared<Hessian2::Object::RawDefinition>("java.sql.Date", std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(
        Hessian2::ObjectPtr(new Hessian2::DateObject(std::chrono::milliseconds(1000))));
    EXPECT_THROW_WITH_MESSAGE(Utility::hessianValueToJsonString(class_val.get()),
                              Envoy::EnvoyException, "Unknown type definition for: java.sql.Date");
  }

  // Expected fields size
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define =
        std::make_shared<Hessian2::Object::RawDefinition>("java.sql.Date", std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(
        Hessian2::ObjectPtr(new Hessian2::DateObject(std::chrono::milliseconds(1000))));
    class_data->data_.emplace_back(
        Hessian2::ObjectPtr(new Hessian2::DateObject(std::chrono::milliseconds(2000))));
    EXPECT_THROW_WITH_MESSAGE(
        Utility::hessianValueToJsonString(class_val.get()), Envoy::EnvoyException,
        "Expected fields size: 1 but actual fields size: 2 for java.sql.Date");
  }

  // Hessian2::ClassInstanceObject "not support type"
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define =
        std::make_shared<Hessian2::Object::RawDefinition>("not support type", std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::StringObject(
        "2312412512512651261261261271271371717137171716561263273652362632")));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()),
              "{\"value\":\"2312412512512651261261261271271371717137171716561263273652362632\"}");
  }

  // Hessian2::ClassInstanceObject "java.math.BigDecimal"
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigDecimal",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::StringObject(
        "2312412512512651261261261271271371717137171716561263273652362632")));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()),
              "2312412512512651261261261271271371717137171716561263273652362632");
  }

  // Hessian2::ClassInstanceObject "java.math.BigDecimal" type is not string
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigDecimal",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::LongObject(1234)));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "null");
  }

  // Hessian2::ClassInstanceObject "java.math.BigInteger" not has mag
  {
    std::string type = "java.util.List";
    std::string value = "[11214251,22215215,1234521515]";
    JsonDocument doc;
    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"mag"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigInteger",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Utility::jsonValueToHessianValue(doc, type, true));
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::IntegerObject(21314)));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "null");
  }

  // Hessian2::ClassInstanceObject "java.math.BigInteger" the indexs are valid.
  {
    std::string type = "java.util.List";
    std::string value = "[11214251,22215215,1234521515]";
    JsonDocument doc;
    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"test", "test2", "mag", "signum"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigInteger",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Utility::jsonValueToHessianValue(doc, type, true));
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::IntegerObject(21314)));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "null");
  }

  // Hessian2::ClassInstanceObject "java.math.BigInteger" signum` are not valid.
  {
    std::string type = "java.util.List";
    std::string value = "[11214251,22215215,1234521515]";
    JsonDocument doc;
    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"mag", "signum"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigInteger",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Utility::jsonValueToHessianValue(doc, type, true));
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::StringObject("test_value")));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "null");
  }

  // Hessian2::ClassInstanceObject "java.math.BigInteger" signum=0
  {
    std::string type = "java.util.List";
    std::string value = "[11214251,22215215,1234521515]";
    JsonDocument doc;
    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"mag", "signum"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigInteger",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Utility::jsonValueToHessianValue(doc, type, true));
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::IntegerObject(0)));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "0");
  }

  // Hessian2::ClassInstanceObject "java.math.BigInteger" signum<0
  {
    std::string type = "java.util.List";
    std::string value = "[11214251,22215215,1234521515]";
    JsonDocument doc;
    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"mag", "signum"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigInteger",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Utility::jsonValueToHessianValue(doc, type, true));
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::IntegerObject(-134)));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "null");
  }

  // Hessian2::ClassInstanceObject "java.math.BigInteger" untyped list
  {
    std::string type = "java.util.List";
    std::string value = "[11214251,22215215,1234521515]";
    JsonDocument doc;
    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"mag", "signum"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigInteger",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Utility::jsonValueToHessianValue(doc, type, true));
    class_data->data_.emplace_back(
        Hessian2::ObjectPtr(Hessian2::ObjectPtr(new Hessian2::IntegerObject(21314))));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "null");
  }

  // Hessian2::ClassInstanceObject "java.math.BigInteger" typed list
  {
    std::string type = "java.util.List";
    std::string value = "[11214251,22215215,1234521515]";
    JsonDocument doc;
    doc.Parse(value);
    EXPECT_EQ(doc.GetType(), Json::Type::kArrayType);
    auto untyped_list_value = Utility::jsonValueToHessianValue(doc, type, true);
    auto list_data =
        Hessian2::Object::TypedList("list", std::move(*untyped_list_value->toMutableUntypedList()));
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"mag", "signum"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>("java.math.BigInteger",
                                                                        std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(
        Hessian2::ObjectPtr(new Hessian2::TypedListObject(std::move(list_data))));
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::IntegerObject(21314)));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()),
              "\"206866418270755036052409771\"");
  }

  // Hessian2::ClassInstanceObject "java.sql.Date"
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define =
        std::make_shared<Hessian2::Object::RawDefinition>("java.sql.Date", std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(
        Hessian2::ObjectPtr(new Hessian2::DateObject(std::chrono::milliseconds(1000))));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "1000");
  }

  // Hessian2::ClassInstanceObject "java.sql.Time"
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define =
        std::make_shared<Hessian2::Object::RawDefinition>("java.sql.Time", std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(
        Hessian2::ObjectPtr(new Hessian2::DateObject(std::chrono::milliseconds(2000))));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "2000");
  }

  // Hessian2::ClassInstanceObject "java.sql.Timestamp"
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define =
        std::make_shared<Hessian2::Object::RawDefinition>("java.sql.Timestamp", std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(
        Hessian2::ObjectPtr(new Hessian2::DateObject(std::chrono::milliseconds(3000))));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "3000");
  }

  // Hessian2::ClassInstanceObject "java.util.concurrent.atomic.AtomicInteger"
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>(
        "java.util.concurrent.atomic.AtomicInteger", std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::IntegerObject(1234)));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "1234");
  }

  // Hessian2::ClassInstanceObject "java.util.concurrent.atomic.AtomicLong"
  {
    auto class_val = std::make_unique<Hessian2::ClassInstanceObject>();
    auto class_data = class_val->toMutableClassInstance();
    std::vector<std::string> defines = {"value"};
    auto raw_define = std::make_shared<Hessian2::Object::RawDefinition>(
        "java.util.concurrent.atomic.AtomicLong", std::move(defines));
    class_data->def_ = raw_define;
    class_data->data_.emplace_back(Hessian2::ObjectPtr(new Hessian2::LongObject(12341234)));
    EXPECT_EQ(Utility::hessianValueToJsonString(class_val.get()), "12341234");
  }

  // Hessian2::NullObject
  {
    auto value = Hessian2::NullObject();

    HessianValue* hessian_value = &value;
    EXPECT_EQ(Utility::hessianValueToJsonString(hessian_value), "null");
  }
}

TEST(ConvertUtilityTest, GetNullTest) {
  EXPECT_EQ(Utility::getNullHessianValue()->type(), Hessian2::Object::Type::Null);
}

TEST(SrcsRequestContextTest, ContextTest) {
  Envoy::Http::TestRequestHeaderMapImpl request_headers{{"x-nsf-header1", "val1"},
                                                        {"x-nsf-header2", "val2"},
                                                        {":method", "POST"},
                                                        {"cookie", "key1=val3"}};

  Envoy::Buffer::OwnedImpl buffer("{\"hello\":1}");

  SrcsRequestContext source(request_headers, buffer);
  EXPECT_EQ(source.getHeader("x-nsf-header1"), "val1");
  EXPECT_EQ(source.getHeader("x-nsf-header2"), "val2");
  EXPECT_EQ(source.getCookie("key1"), "val3");
}

TEST(DubboBridgeTest, DubboBasicConfigTest) {
  ProtoDubboBridgeContext context;
  const std::string yaml = R"EOF(
          service: "org.apache.dubbo.UserProvider"
          version: ""
          method: "queryUser"
          group: ""
  )EOF";
  Envoy::TestUtility::loadFromYamlAndValidate(yaml, context);

  EXPECT_NO_THROW(std::make_unique<DubboBridgeContext>(context));
}

TEST(DubboBridgeTest, DubboBridgeContextTest) {

  {
    ProtoDubboBridgeContext context;
    const std::string yaml = R"EOF(
          service: "org.apache.dubbo.UserProvider"
          version: ""
          method: "queryUser"
          group: ""
          source: HTTP_QUERY
  )EOF";
    Envoy::TestUtility::loadFromYamlAndValidate(yaml, context);

    auto dubbo_bridge = std::make_unique<DubboBridgeContext>(context);
    EXPECT_EQ(dubbo_bridge->getParametersSize(), 0);
    EXPECT_EQ(dubbo_bridge->lostParameterName(), false);
    EXPECT_EQ(dubbo_bridge->ignoreMapNullPair(), false);
    EXPECT_EQ(dubbo_bridge->getArgumentSource(), DubboBridgeContext::Source::Query);
  }

  {
    ProtoDubboBridgeContext context;
    const std::string yaml = R"EOF(
          service: "org.apache.dubbo.UserProvider"
          version: ""
          method: "queryUser"
          group: "a"
          source: HTTP_BODY
          parameters:
          - type: "org.apache.dubbo.User"
            name: "user"
            required: false
            default: "{\"id\": \"A001\"}"
            generic:
            - path: "."
              type: org.apache.dubbo.FakeUser
          attachments:
          - name: \"key\"
            static: "value"
          - name: "header-key"
            header: "original-header-key"
          - name: "cookie-key"
            cookie: "original-cookie-key"
          ignore_null_map_pair: true
  )EOF";
    Envoy::TestUtility::loadFromYamlAndValidate(yaml, context);

    auto dubbo_bridge = std::make_unique<DubboBridgeContext>(context);
    EXPECT_EQ(dubbo_bridge->getParametersSize(), 1);
    EXPECT_EQ(dubbo_bridge->lostParameterName(), false);
    EXPECT_EQ(dubbo_bridge->ignoreMapNullPair(), true);
    EXPECT_EQ(dubbo_bridge->getArgumentSource(), DubboBridgeContext::Source::Body);

    Envoy::Http::TestRequestHeaderMapImpl request_headers{
        {"x-nsf-header1", "val1"},
        {"x-nsf-header2", "val2"},
        {":method", "POST"},
        {"original-header-key", "header-attachment-value"},
        {"cookie", "original-cookie-key=cookie-val"}};

    Envoy::Buffer::OwnedImpl buffer("{\"hello\":1}");

    SrcsRequestContext source(request_headers, buffer);
    Envoy::Buffer::OwnedImpl dubbo_buffer;
    std::unique_ptr<ArgumentGetter> getter = std::make_unique<ArgumentGetterTest>(source, "x-nsf-");
    dubbo_bridge->writeDubboRequest(dubbo_buffer, source, *getter);
  }

  {
    ProtoDubboBridgeContext context;
    const std::string yaml = R"EOF(
          service: "org.apache.dubbo.UserProvider"
          version: ""
          method: "queryUser"
          group: "a"
          source: HTTP_BODY
          parameters:
          - type: "org.apache.dubbo.User"
            name: "user"
            required: true
            generic:
            - path: "."
              type: org.apache.dubbo.FakeUser
          attachments:
          - name: \"key\"
            static: "value"
          - name: "header-key"
            header: "original-header-key"
          - name: "cookie-key"
            cookie: "original-cookie-key"
          ignore_null_map_pair: true
  )EOF";
    Envoy::TestUtility::loadFromYamlAndValidate(yaml, context);

    auto dubbo_bridge = std::make_unique<DubboBridgeContext>(context);
    EXPECT_EQ(dubbo_bridge->getParametersSize(), 1);
    EXPECT_EQ(dubbo_bridge->lostParameterName(), false);
    EXPECT_EQ(dubbo_bridge->ignoreMapNullPair(), true);
    EXPECT_EQ(dubbo_bridge->getArgumentSource(), DubboBridgeContext::Source::Body);

    Envoy::Http::TestRequestHeaderMapImpl request_headers{
        {"x-nsf-header1", "val1"},
        {"x-nsf-header2", "val2"},
        {":method", "POST"},
        {"original-header-key", "header-attachment-value"},
        {"cookie", "original-cookie-key=cookie-val"}};

    Envoy::Buffer::OwnedImpl buffer("{\"hello\":1}");

    SrcsRequestContext source(request_headers, buffer);
    Envoy::Buffer::OwnedImpl dubbo_buffer;
    std::unique_ptr<ArgumentGetter> getter = std::make_unique<ArgumentGetterTest>(source, "x-nsf-");
    EXPECT_THROW_WITH_MESSAGE(
        dubbo_bridge->writeDubboRequest(dubbo_buffer, source, *getter), Envoy::EnvoyException,
        "Dubbo bridge: 0/user/org.apache.dubbo.User is required but not provided");
  }

  // defaule is invalid Json
  {
    ProtoDubboBridgeContext context;
    const std::string yaml = R"EOF(
          service: "org.apache.dubbo.UserProvider"
          version: ""
          method: "queryUser"
          group: "a"
          source: HTTP_BODY
          parameters:
          - type: "org.apache.dubbo.User"
            name: "user"
            required: false
            default: "abc"
            generic:
            - path: "."
              type: org.apache.dubbo.FakeUser
          attachments:
          - name: \"key\"
            static: "value"
          - name: "header-key"
            header: "original-header-key"
          - name: "cookie-key"
            cookie: "original-cookie-key"
          ignore_null_map_pair: true
  )EOF";
    Envoy::TestUtility::loadFromYamlAndValidate(yaml, context);

    EXPECT_THROW_WITH_MESSAGE(
        std::make_unique<DubboBridgeContext>(context), Envoy::EnvoyException,
        "Dubbo bridge: default value: user/org.apache.dubbo.User of abc is invalid Json");
  }

  // default is empty
  {
    ProtoDubboBridgeContext context;
    const std::string yaml = R"EOF(
          service: "org.apache.dubbo.UserProvider"
          version: ""
          method: "queryUser"
          group: "a"
          source: HTTP_BODY
          parameters:
          - type: "org.apache.dubbo.User"
            name: "user"
            required: false
            generic:
            - path: "."
              type: org.apache.dubbo.FakeUser
          attachments:
          - name: \"key\"
            static: "value"
          - name: "header-key"
            header: "original-header-key"
          - name: "cookie-key"
            cookie: "original-cookie-key"
          ignore_null_map_pair: true
  )EOF";
    Envoy::TestUtility::loadFromYamlAndValidate(yaml, context);

    auto dubbo_bridge = std::make_unique<DubboBridgeContext>(context);
    EXPECT_EQ(dubbo_bridge->getParametersSize(), 1);
    EXPECT_EQ(dubbo_bridge->lostParameterName(), false);
    EXPECT_EQ(dubbo_bridge->ignoreMapNullPair(), true);
    EXPECT_EQ(dubbo_bridge->getArgumentSource(), DubboBridgeContext::Source::Body);
  }
}

TEST(DubboBridgeTest, GenericTest) {
  {
    ProtoDubboBridgeContext context;
    const std::string yaml = R"EOF(
          service: "org.apache.dubbo.UserProvider"
          version: ""
          method: "queryUser"
          group: ""
          source: HTTP_QUERY
  )EOF";
    Envoy::TestUtility::loadFromYamlAndValidate(yaml, context);

    auto dubbo_bridge = std::make_unique<DubboBridgeContext>(context);
    EXPECT_EQ(dubbo_bridge->getParametersSize(), 0);
    EXPECT_EQ(dubbo_bridge->lostParameterName(), false);
    EXPECT_EQ(dubbo_bridge->ignoreMapNullPair(), false);
    EXPECT_EQ(dubbo_bridge->getArgumentSource(), DubboBridgeContext::Source::Query);
  }

  {
    ProtoDubboBridgeContext context;
    const std::string yaml = R"EOF(
          service: "com.proxy.apigateway.dubbo.api.GenericService"
          version: "0.0.0"
          method: "method"
          group: "group-a"
          source: HTTP_BODY
          parameters:
          - type: "com.proxy.apigateway.dubbo.Entity.B"
            required: false
            generic:
            - path: ".dataB"
              type: com.proxy.apigateway.dubbo.Entity.C
          attachments:
          - name: \"key\"
            static: "value"
          - name: "header-key"
            header: "original-header-key"
          - name: "cookie-key"
            cookie: "original-cookie-key"
          ignore_null_map_pair: false
  )EOF";
    Envoy::TestUtility::loadFromYamlAndValidate(yaml, context);

    auto dubbo_bridge = std::make_unique<DubboBridgeContext>(context);
    EXPECT_EQ(dubbo_bridge->getArgumentSource(), DubboBridgeContext::Source::Body);

    Envoy::Http::TestRequestHeaderMapImpl request_headers{
        {"x-nsf-header1", "val1"},
        {"x-nsf-header2", "val2"},
        {":method", "POST"},
        {"original-header-key", "header-attachment-value"},
        {"cookie", "original-cookie-key=cookie-val"}};

    Envoy::Buffer::OwnedImpl buffer(
        "{\"nameB\":\"Hello B\",\"dataB\":{\"stringC\":\"Hello "
        "C\",\"bigDecimalC\":12356,\"integerC\":-50,\"doubleC\":-90.66,\"listC\":[\"apple\","
        "\"pear\"],\"intc\":-1,\"charC\":\"A\",\"floatC\":0.035,\"booleanC\":true}}");

    SrcsRequestContext source(request_headers, buffer);
    Envoy::Buffer::OwnedImpl dubbo_buffer;
    std::unique_ptr<ArgumentGetter> getter =
        std::make_unique<GenericTypeArgumentGetterTest>(source);
    dubbo_bridge->writeDubboRequest(dubbo_buffer, source, *getter);
  }
}

TEST(BufferRewriteTest, BufferRewriteTest) {
  Envoy::Buffer::OwnedImpl buffer;
  buffer.add("test");

  BufferWriter writer_1(buffer);

  std::string test_string("123456789");

  writer_1.rawWrite(test_string);

  writer_1.rawWrite(test_string.data(), 5);

  EXPECT_EQ("test12345678912345", buffer.toString());
}

TEST(BufferReaderTest, BufferReaderTest) {
  Envoy::Buffer::OwnedImpl buffer;
  buffer.add("test");

  BufferReader reader_1(buffer);

  EXPECT_EQ(0, reader_1.offset());
  EXPECT_EQ(4, reader_1.length());

  std::string test_string(4, 0);

  reader_1.readNBytes(test_string.data(), 4);

  EXPECT_EQ("test", test_string);
  EXPECT_EQ(4, reader_1.offset());
}

TEST(DubboResponseTest, DubboResponseTest) {
  // Invalid dubbo magic number.
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    addInt64(buffer, 0);
    addInt64(buffer, 0);

    EXPECT_THROW_WITH_MESSAGE(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
                              Envoy::EnvoyException, "invalid dubbo message magic number 0");
    buffer.drain(buffer.length());
    dubbo_response->reset();
  }

  // Invalid message body size.
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    buffer.add(std::string({'\xda', '\xbb', '\xc2', 0x00}));
    addInt64(buffer, 1);
    addInt32(buffer, DubboProtocolImpl::MaxBodySize + 1);
    std::string exception_string =
        fmt::format("invalid dubbo message size {}", DubboProtocolImpl::MaxBodySize + 1);
    EXPECT_THROW_WITH_MESSAGE(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
                              Envoy::EnvoyException, exception_string);
    buffer.drain(buffer.length());
    dubbo_response->reset();
  }

  // Invalid serialization type.
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    buffer.add(std::string({'\xda', '\xbb', '\xc3', 0x00}));
    addInt64(buffer, 1);
    addInt32(buffer, 0xff);
    EXPECT_THROW_WITH_MESSAGE(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
                              Envoy::EnvoyException, "invalid dubbo message serialization type 3");
    buffer.drain(buffer.length());
    dubbo_response->reset();
  }

  // Invalid response status.
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    buffer.add(std::string({'\xda', '\xbb', 0x42, 0x00}));
    addInt64(buffer, 1);
    addInt32(buffer, 0xff);
    EXPECT_THROW_WITH_MESSAGE(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
                              Envoy::EnvoyException, "invalid dubbo message response status 0");
    buffer.drain(buffer.length());
    dubbo_response->reset();
  }

  // Normal dubbo response message.
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    buffer.add(std::string({'\xda', '\xbb', 0x42, 20}));
    addInt64(buffer, 1);
    addInt32(buffer, 1);
    EXPECT_EQ(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
              DubboResponse::Status::WAITING);
    EXPECT_EQ(1, dubbo_response->metadata_->requestId());
    EXPECT_EQ(1, dubbo_response->context_->bodySize());
    dubbo_response->reset();
  }

  // Normal dubbo heart message
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    buffer.add(std::string{'\xda', '\xbb'});
    buffer.writeByte(0xc2 | 0x20); // Heartbeat response message.
    buffer.writeByte(0x14);        // Response status, 20.
    addInt64(buffer, 0x0F);        // Request Id.
    addInt32(buffer, 1);           // Body length.
    buffer.writeByte('N');         // Body.
    EXPECT_EQ(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
              DubboResponse::Status::WAITING);
  }

  // HessianErrorResponse
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;

    buffer.add(std::string{'\xda', '\xbb'});
    buffer.writeByte(0x42);                            // Response message, two_way, not event.
    buffer.writeByte(0x46);                            // Response status, non 20.
    addInt64(buffer, 1);                               // Request Id.
    addInt32(buffer, 5);                               // Body length.
    buffer.add(std::string{0x05, 't', 'e', 's', 't'}); // No response type for non 20 response.
    EXPECT_EQ(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
              DubboResponse::Status::OK);
  }

  // HessianExceptionResponse
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;

    buffer.add(std::string{'\xda', '\xbb'});
    buffer.writeByte(0x42);                            // Response message, two_way, not event.
    buffer.writeByte(0x14);                            // Response status, 20.
    addInt64(buffer, 1);                               // Request Id.
    addInt32(buffer, 6);                               // Body length.
    buffer.add(std::string{'\x90',                     // Type, exception.
                           0x05, 't', 'e', 's', 't'}); // Body.
    EXPECT_EQ(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
              DubboResponse::Status::OK);
  }

  // HessianResponse
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    buffer.add(std::string{'\xda', '\xbb'});
    buffer.writeByte(0x42);                            // Response message, two_way, not event.
    buffer.writeByte(0x14);                            // Response status, 20.
    addInt64(buffer, 1);                               // Request Id.
    addInt32(buffer, 6);                               // Body length.
    buffer.add(std::string{'\x94',                     // Type, normal.
                           0x05, 't', 'e', 's', 't'}); // Body.
    EXPECT_EQ(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
              DubboResponse::Status::OK);
  }

  // Big HessianResponse
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    buffer.add(std::string{'\xda', '\xbb'});
    buffer.writeByte(0x42); // Response message, two_way, not event.
    buffer.writeByte(0x14); // Response status, 20.
    addInt64(buffer, 1);    // Request Id

    Buffer::OwnedImpl body_buffer;
    Hessian2::WriterPtr writer = std::make_unique<BufferWriter>(body_buffer);
    Hessian2::Encoder encoder(std::move(writer));
    encoder.encode(std::string(DubboProtocolImpl::MaxBodySize - 2048, 'a'));

    addInt32(buffer, body_buffer.length() + 1); // Body length (Body + Type).
    buffer.writeByte(0x94);                     // Type, normal.
    buffer.move(body_buffer);                   // Body.
    EXPECT_EQ(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
              DubboResponse::Status::OK);
  }

  // Not support return type
  {
    auto dubbo_response = std::make_unique<DubboResponse>();
    Envoy::Buffer::OwnedImpl buffer;
    buffer.add(std::string{'\xda', '\xbb'});
    buffer.writeByte(0x42);        // Response message, two_way, not event.
    buffer.writeByte(0x14);        // Response status, 20.
    addInt64(buffer, 1);           // Request Id.
    addInt32(buffer, 6);           // Body length.
    buffer.add(std::string{'\x96', // Type, unsupported.
                           0x05, 't', 'e', 's', 't'});
    EXPECT_THROW_WITH_MESSAGE(DubboResponse::decodeDubboFromBuffer(buffer, *dubbo_response),
                              Envoy::EnvoyException, "not supported return type 6");
  }
}
} // namespace
} // namespace DubboBridge
} // namespace Common
} // namespace Proxy
} // namespace Envoy
