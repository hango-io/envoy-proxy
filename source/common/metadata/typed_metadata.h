#pragma once

#include <map>
#include <string>

#include "common/protobuf/protobuf.h"

#include "envoy/config/typed_metadata.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Metadata {

// Convert simple metadata such as string, number and boolean to plain string map.
class MetadataToSimpleMap : public Envoy::Config::TypedMetadata::Object {
public:
  MetadataToSimpleMap(const ProtobufWkt::Struct& data);
  std::map<std::string, std::string> metadata_values_;
};
using MetadataToSimpleMapPtr = std::unique_ptr<MetadataToSimpleMap>;

} // namespace Metadata
} // namespace Common
} // namespace Proxy
} // namespace Envoy
