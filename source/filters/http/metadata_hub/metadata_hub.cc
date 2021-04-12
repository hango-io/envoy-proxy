#include "filters/http/metadata_hub/metadata_hub.h"

#include "envoy/common/exception.h"

#include "common/common/assert.h"
#include "common/http/headers.h"
#include "common/http/path_utility.h"
#include "common/http/utility.h"

#include "common/filter_state/plain_state.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace MetadataHub {

class TypedMetadataFactory : public Envoy::Router::HttpRouteTypedMetadataFactory {
public:
  std::string name() const override { return HttpMetadataHubFilter::name(); }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const ProtobufWkt::Struct& data) const override {
    return std::make_unique<Common::Metadata::MetadataToSimpleMap>(data);
  }
};

static Registry::RegisterFactory<TypedMetadataFactory, Envoy::Router::HttpRouteTypedMetadataFactory>
    register_;

void MetadataHubCommonConfig::handleDecodeHeaders(Http::LowerCaseString&& name,
                                                  Http::LowerCaseString&& rename) {
  ENVOY_LOG(debug, "Normal Request header: {} for metadata hub", name.get());
  decode_headers_to_state_.push_back(
      {std::move(rename), [n = std::move(name)](Http::RequestHeaderMap& h) -> absl::string_view {
         auto result = h.get(n);
         return result.empty() ? result[0]->value().getStringView() : "";
       }});
}

void MetadataHubCommonConfig::handleEncodeHeaders(Http::LowerCaseString&& name,
                                                  Http::LowerCaseString&& rename) {
  ENVOY_LOG(debug, "Normal Response header: {} for metadata hub", name.get());
  encode_headers_to_state_.push_back(
      {std::move(rename), [n = std::move(name)](Http::ResponseHeaderMap& h) -> absl::string_view {
         auto result = h.get(n);
         return result.empty() ? result[0]->value().getStringView() : "";
       }});
}

#undef CHECK_INLINE_HEADER

MetadataHubCommonConfig::MetadataHubCommonConfig(const ProtoCommonConfig& config) {
  // 向前兼容 set_to_metadata 字段一个版本
  ProtoCommonConfig mutable_config(config);
  mutable_config.mutable_decode_headers_to_state()->MergeFrom(config.set_to_metadata());

#define HANDLE_HEADERS_TO_STATE(records, func)                                                     \
  {                                                                                                \
    for (const auto& record : records) {                                                           \
      std::string name = record.name();                                                            \
      if (name.empty()) {                                                                          \
        continue;                                                                                  \
      }                                                                                            \
      std::string rename = record.rename().empty() ? name : record.rename();                       \
      func(Http::LowerCaseString(name), Http::LowerCaseString(rename));                            \
    }                                                                                              \
  }

  HANDLE_HEADERS_TO_STATE(mutable_config.decode_headers_to_state(), handleDecodeHeaders);
  HANDLE_HEADERS_TO_STATE(mutable_config.encode_headers_to_state(), handleEncodeHeaders);

#undef HANDLE_HEADERS_TO_STATE

  for (const auto& metadata_namespace : config.route_metadata_to_state()) {
    route_metadata_to_state_.push_back(metadata_namespace);
  }
}

namespace {

void metadataToState(const Envoy::Config::TypedMetadata& metadata,
                     const Envoy::StreamInfo::FilterStateSharedPtr& filter_state,
                     const std::string& ns, Envoy::Tracing::Span& span) {
  auto metadata_object = metadata.get<Common::Metadata::MetadataToSimpleMap>(ns);
  if (!metadata_object) {
    return;
  }

  for (const auto& pair : metadata_object->metadata_values_) {
    filter_state->setData(pair.first,
                          std::make_unique<Common::FilterState::PlainStateImpl>(pair.second),
                          StreamInfo::FilterState::StateType::Mutable);
    span.setTag(pair.first, pair.second);
  }
}

} // namespace

Http::FilterHeadersStatus HttpMetadataHubFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                               bool) {
  // insert value to state
  auto& state = decoder_callbacks_->streamInfo().filterState();

  // Use path as tracing operation.
  ASSERT(headers.Path());
  decoder_callbacks_->activeSpan().setOperation(
      Http::PathUtil::removeQueryAndFragment(headers.Path()->value().getStringView()));

  for (const auto& record : config_->decodeHeadersToState()) {
    auto value_view = record.second(headers);
    if (value_view.empty()) {
      continue;
    }
    ENVOY_LOG(trace, "Set Request header value: {} with name: {} to state", value_view,
              record.first.get());
    state->setData(record.first.get(),
                   std::make_unique<Common::FilterState::PlainStateImpl>(std::string(value_view)),
                   StreamInfo::FilterState::StateType::ReadOnly);
  }

  // 未指定 route metadata 名称空间，故直接返回
  if (config_->routeMatadataToState().empty()) {
    return Http::FilterHeadersStatus::Continue;
  }
  // 如果路由不存在，则直接返回
  auto route = decoder_callbacks_->route();
  if (!route.get()) {
    return Http::FilterHeadersStatus::Continue;
  }
  auto route_entry = route->routeEntry();
  if (!route_entry) {
    return Http::FilterHeadersStatus::Continue;
  }

  // 已指定 route metadata 名称空间，且路由存在，则将对应 metadata 插入 filter state
  // 必须在 decode 阶段完成该工作，避免 DC 等无 encode 阶段的请求无法获取相关 metadata
  for (const auto& metadata_namespace : config_->routeMatadataToState()) {
    ENVOY_LOG(debug, "Set route metadata in {} to filter state.", metadata_namespace);
    metadataToState(route_entry->typedMetadata(), state, metadata_namespace,
                    decoder_callbacks_->activeSpan());
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus HttpMetadataHubFilter::encodeHeaders(Http::ResponseHeaderMap& headers,
                                                               bool) {
  auto& state = decoder_callbacks_->streamInfo().filterState();

  for (const auto& record : config_->encodeHeadersToState()) {
    auto value_view = record.second(headers);
    if (value_view.empty()) {
      continue;
    }
    ENVOY_LOG(trace, "Set Response header value: {} with name: {} to state", value_view,
              record.first.get());
    state->setData(record.first.get(),
                   std::make_unique<Common::FilterState::PlainStateImpl>(std::string(value_view)),
                   StreamInfo::FilterState::StateType::ReadOnly);
  }

  return Http::FilterHeadersStatus::Continue;
}

} // namespace MetadataHub
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
