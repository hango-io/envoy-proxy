#include "source/filters/http/static_downgrade/static_downgrade_filter.h"

#include "envoy/common/exception.h"
#include "envoy/router/router.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/assert.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/headers.h"
#include "source/common/http/utility.h"
#include "source/common/stream_info/utility.h"

#include "absl/strings/str_split.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace StaticDowngrade {

StaticDowngradeConfig::StaticDowngradeConfig(const ProtoDowngradeConfig& config,
                                             Server::Configuration::ServerFactoryContext& context) {
  for (auto& header_data : config.downgrade_rqx().headers()) {
    downgrade_rqx_.push_back(std::make_unique<Http::HeaderUtility::HeaderData>(header_data));
  }

  for (auto& header_data : config.downgrade_rpx().headers()) {
    downgrade_rpx_.push_back(std::make_unique<Http::HeaderUtility::HeaderData>(header_data));
  }

  if (config.has_response_flags()) {
    uint64_t flags = 0;
    for (const auto& view :
         absl::StrSplit(config.response_flags().value(), ',', absl::SkipEmpty())) {
      auto flag = StreamInfo::ResponseFlagUtils::toResponseFlag(std::string(view));
      if (!flag.has_value()) {
        throw EnvoyException(fmt::format("Unsupported response flag: {}", view));
      }
      flags |= flag.value();
    }
    if (flags != 0) {
      response_flags_ = flags;
    }
  }
  if (config.has_response_code_details()) {
    response_code_details_matcher_ =
        std::make_unique<Matchers::StringMatcherImpl<envoy::type::matcher::v3::StringMatcher>>(
            config.response_code_details());
  }

  if (downgrade_rpx_.size() == 0) {
    has_no_donwgrade_rpx_ = true; // no rpx no downgrade
    return;
  }

  if (!config.has_static_response()) {
    return; // use default
  }

  if (config.static_response().http_status() != 0) {
    static_http_status_ = config.static_response().http_status();
  }
  for (auto& header : config.static_response().headers()) {
    static_http_headers_.insert({Http::LowerCaseString(header.key()), header.value()});
  }
  if (!config.static_response().has_body()) {
    return;
  }

  static const ssize_t max_body_size = 4096;
  auto& static_http_body = config.static_response().body();
  auto& api = context.api();

  if (!static_http_body.filename().empty()) {
    std::string filename = static_http_body.filename();
    if (!api.fileSystem().fileExists(filename)) {
      ENVOY_LOG(error, "Response body file: {} not exist", filename);
      return;
    }
    ssize_t size = api.fileSystem().fileSize(filename);
    if (size <= 0 && size > max_body_size) {
      ENVOY_LOG(error, "Size({}) of response body file {} error", size, filename);
      return;
    }
    static_http_body_ = api.fileSystem().fileReadToEnd(filename);
  } else {
    std::string temp;
    if (!static_http_body.inline_bytes().empty())
      temp = static_http_body.inline_bytes();
    else
      temp = static_http_body.inline_string();

    if (temp.length() <= max_body_size)
      static_http_body_ = std::move(temp);
    else
      ENVOY_LOG(error, "Response body string/bytes size: {} error", temp.size());
  }
}

bool StaticDowngradeConfig::shouldDowngrade(Http::HeaderMap& rqx, Http::HeaderMap& rpx,
                                            const StreamInfo::StreamInfo& info) const {
  if (has_no_donwgrade_rpx_) {
    return false;
  }
  bool result = Http::HeaderUtility::matchHeaders(rqx, downgrade_rqx_) &&
                Http::HeaderUtility::matchHeaders(rpx, downgrade_rpx_);

  if (response_flags_.has_value()) {
    result = result && (info.responseFlags() & response_flags_.value());
  }
  if (nullptr != response_code_details_matcher_) {
    result =
        result && response_code_details_matcher_->match(info.responseCodeDetails().value_or(""));
  }
  return result;
}

StaticDowngradeRouteConfig::StaticDowngradeRouteConfig(
    const ProtoRouteConfig& config, Server::Configuration::ServerFactoryContext& context) {
  ProtoDowngradeConfig primary_proto_config;
  *primary_proto_config.mutable_downgrade_rqx() = config.downgrade_rqx();
  *primary_proto_config.mutable_downgrade_rpx() = config.downgrade_rpx();
  *primary_proto_config.mutable_static_response() = config.static_response();

  if (config.has_response_flags()) {
    *primary_proto_config.mutable_response_flags() = config.response_flags();
  }
  if (config.has_response_code_details()) {
    *primary_proto_config.mutable_response_code_details() = config.response_code_details();
  }

  downgrade_configs_.push_back(
      std::make_unique<StaticDowngradeConfig>(primary_proto_config, context));

  for (auto& proto_config : config.specific_configs()) {
    downgrade_configs_.push_back(std::make_unique<StaticDowngradeConfig>(proto_config, context));
  }
}

const StaticDowngradeConfig*
StaticDowngradeRouteConfig::downgradeConfig(Http::HeaderMap& rqx, Http::HeaderMap& rpx,
                                            const StreamInfo::StreamInfo& info) const {
  // No static downgrade config to get
  if (downgrade_configs_.size() == 0) {
    return nullptr;
  }
  // Serach first matched downgrade config by rqx/rpx
  for (auto& config : downgrade_configs_) {
    if (config->shouldDowngrade(rqx, rpx, info)) {
      return config.get();
    }
  }
  // No downgrade config can used for this rqx/rpx
  return nullptr;
}

Http::FilterHeadersStatus HttpStaticDowngradeFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                                   bool) {
  rqx_headers_ = &headers;
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus HttpStaticDowngradeFilter::encodeHeaders(Http::ResponseHeaderMap& headers,
                                                                   bool end_stream) {
  rpx_headers_ = &headers;
  auto route = encoder_callbacks_->route();

  // no route
  if (!route.get()) {
    ENVOY_LOG(debug, "No route for downgrade and do nothing");
    return Http::FilterHeadersStatus::Continue;
  }

  // encode插件不应该刷新路由，所以可以直接保留route_config，留待encodeData使用
  auto route_config =
      Http::Utility::resolveMostSpecificPerFilterConfig<StaticDowngradeRouteConfig>(name(), route);
  // no config
  if (!route_config) {
    ENVOY_LOG(debug, "No config for downgrade and do nothing");
    return Http::FilterHeadersStatus::Continue;
  }

  if (rqx_headers_ == nullptr) {
    downgrade_config_ =
        route_config->downgradeConfig(*Http::StaticEmptyHeaders::get().request_headers,
                                      *rpx_headers_, encoder_callbacks_->streamInfo());
  } else {
    downgrade_config_ = route_config->downgradeConfig(*rqx_headers_, *rpx_headers_,
                                                      encoder_callbacks_->streamInfo());
  }

  if (downgrade_config_ == nullptr) {
    ENVOY_LOG(debug, "Should not downgrade for this response");
    return Http::FilterHeadersStatus::Continue;
  }

  // update Status
  headers.removeStatus();
  headers.setStatus(downgrade_config_->static_http_status_);

  static Http::LowerCaseString downgrade_status("x-downgrade-status");
  headers.addCopy(downgrade_status, "STATIC");

  // update header
  for (auto& pair : downgrade_config_->static_http_headers_) {
    headers.remove(pair.first);
    headers.addCopy(pair.first, pair.second);
  }

  // update content-length
  headers.removeContentLength();
  headers.setContentLength(downgrade_config_->static_http_body_.length());

  // if response no body
  if (end_stream) {
    Buffer::OwnedImpl body(downgrade_config_->static_http_body_);
    encoder_callbacks_->addEncodedData(body, false);
    downgrade_complete_ = true;
    return Http::FilterHeadersStatus::Continue;
  }

  return Http::FilterHeadersStatus::StopIteration;
}

Http::FilterDataStatus HttpStaticDowngradeFilter::encodeData(Buffer::Instance& data,
                                                             bool end_stream) {
  // no downgrade or downgrade complete
  if (downgrade_config_ == nullptr || downgrade_complete_) {
    return Http::FilterDataStatus::Continue;
  }
  // abandon old body
  data.drain(data.length());
  // add body
  if (end_stream) {
    Buffer::OwnedImpl body(downgrade_config_->static_http_body_);
    encoder_callbacks_->addEncodedData(body, false);
    downgrade_complete_ = true;
    return Http::FilterDataStatus::Continue;
  }
  return Http::FilterDataStatus::StopIterationNoBuffer;
}

} // namespace StaticDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
