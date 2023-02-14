#include "source/filters/http/traffic_mark/traffic_mark_filter.h"

#include "envoy/common/exception.h"

#include "source/common/common/assert.h"
#include "source/common/common/utility.h"
#include "source/common/config/metadata.h"
#include "source/common/config/well_known_names.h"
#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace TrafficMark {

static const std::string cluster_metadata_group_key = "qingzhou";

absl::string_view tryFindColor(const std::vector<absl::string_view>& colors,
                               absl::string_view to_find) {
  absl::string_view fallback;

  for (const auto& each : colors) {
    if (each == to_find) {
      return to_find;
    }

    if (absl::StartsWith(to_find, each) && to_find[each.length()] == '.' &&
        each.length() > fallback.length()) {
      fallback = each;
    }
  }

  return fallback;
}

Http::FilterHeadersStatus HttpTrafficMarkFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                               bool) {
  auto route = decoder_callbacks_->route();

  if (route == nullptr) {
    return Http::FilterHeadersStatus::Continue;
  }

  const auto* route_entry = route->routeEntry();
  if (route_entry == nullptr) {
    return Http::FilterHeadersStatus::Continue;
  }

  // Get metadata struct of envoy lb from dynamic metadata.
  auto& envoy_lb_struct =
      (*decoder_callbacks_->streamInfo()
            .dynamicMetadata()
            .mutable_filter_metadata())[Envoy::Config::MetadataFilters::get().ENVOY_LB];

  const auto& metadata_as_mark = config_.metadataAsMark();
  if (!metadata_as_mark.empty()) {
    const auto& route_metadata = route->metadata();

    for (const auto& pair : metadata_as_mark) {
      const auto& metadata_value = Envoy::Config::Metadata::metadataValue(
          &route_metadata, pair.second.namespace_, pair.second.path_);
      switch (metadata_value.kind_case()) {
      case ProtobufWkt::Value::kStringValue:
      case ProtobufWkt::Value::kNumberValue:
      case ProtobufWkt::Value::kBoolValue:
        ENVOY_LOG(debug, "Traffic mark: set {}:{} to envoy.lb metadata", pair.first,
                  metadata_value.DebugString());
        (*envoy_lb_struct.mutable_fields())[pair.first] = metadata_value;
        break;
      default:
        break;
      }
    }
  }

  absl::string_view request_color;
  const auto header_result = headers.get(Http::LowerCaseString(config_.headerKey()));
  if (!header_result.empty()) {
    request_color = header_result[0]->value().getStringView();
  }

  ENVOY_LOG(trace, "request color {}", request_color);

  const std::string& cluster_name = route_entry->clusterName();
  auto* cluster = cm_.getThreadLocalCluster(cluster_name);
  if (cluster == nullptr || cluster->info() == nullptr) {
    return Http::FilterHeadersStatus::Continue;
  }

  const auto& cluster_metadata = cluster->info()->metadata();
  const auto& all_colors_value = Envoy::Config::Metadata::metadataValue(
      &cluster_metadata, cluster_metadata_group_key, config_.allColorsKey());
  std::vector<absl::string_view> all_colors;
  if (all_colors_value.kind_case() == ProtobufWkt::Value::kStringValue) {
    all_colors = StringUtil::splitToken(all_colors_value.string_value(), ",");
  } else {
    ENVOY_LOG(debug, "invalid all_colors value");
    return Http::FilterHeadersStatus::Continue;
  }
  if (all_colors.empty()) {
    ENVOY_LOG(debug, "should have at least one color");
    return Http::FilterHeadersStatus::Continue;
  }

  auto result = tryFindColor(all_colors, request_color);
  ENVOY_LOG(trace, "fallback to {}", result);

  auto& metadata = decoder_callbacks_->streamInfo().dynamicMetadata();
  Envoy::Config::Metadata::mutableMetadataValue(
      metadata, Envoy::Config::MetadataFilters::get().ENVOY_LB, config_.matchKey())
      .set_string_value(std::string(result));

  return Http::FilterHeadersStatus::Continue;
}

} // namespace TrafficMark
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
