#include "source/common/metadata/typed_metadata.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Metadata {

namespace {
std::string doubleToString(double number) {
  return int64_t(number) == number ? std::to_string(int64_t(number)) : std::to_string(number);
}
} // namespace

MetadataToSimpleMap::MetadataToSimpleMap(const ProtobufWkt::Struct& data) {
  for (const auto& pair : data.fields()) {
    std::string field_string;
    switch (pair.second.kind_case()) {
    case ProtobufWkt::Value::KindCase::kBoolValue:
      field_string = pair.second.bool_value() ? "true" : "false";
      break;
    case ProtobufWkt::Value::KindCase::kStringValue:
      field_string = pair.second.string_value();
      break;
    case ProtobufWkt::Value::KindCase::kNumberValue:
      field_string = doubleToString(pair.second.number_value());
      break;
    default:
      break;
    }
    metadata_values_[pair.first] = field_string;
  }
}

} // namespace Metadata
} // namespace Common
} // namespace Proxy
} // namespace Envoy
