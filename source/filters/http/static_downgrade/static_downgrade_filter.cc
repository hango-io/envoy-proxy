#include "filters/http/static_downgrade/static_downgrade_filter.h"
#include "common/buffer/buffer_impl.h"
#include "common/common/assert.h"
#include "common/http/header_map_impl.h"
#include "common/http/header_utility.h"
#include "common/http/headers.h"
#include "envoy/common/exception.h"
#include "envoy/router/router.h"
#include "common/http/utility.h"

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

bool StaticDowngradeConfig::shouldDowngrade(Http::HeaderMap& rqx, Http::HeaderMap& rpx) const {
  if (has_no_donwgrade_rpx_) {
    return false;
  }
  return Http::HeaderUtility::matchHeaders(rqx, downgrade_rqx_) &&
         Http::HeaderUtility::matchHeaders(rpx, downgrade_rpx_);
}

StaticDowngradeRouteConfig::StaticDowngradeRouteConfig(
    const ProtoRouteConfig& config, Server::Configuration::ServerFactoryContext& context) {
  ProtoDowngradeConfig primary_proto_config;
  *primary_proto_config.mutable_downgrade_rqx() = config.downgrade_rqx();
  *primary_proto_config.mutable_downgrade_rpx() = config.downgrade_rpx();
  *primary_proto_config.mutable_static_response() = config.static_response();

  downgrade_configs_.push_back(
      std::make_unique<StaticDowngradeConfig>(primary_proto_config, context));

  for (auto& proto_config : config.specific_configs()) {
    downgrade_configs_.push_back(std::make_unique<StaticDowngradeConfig>(proto_config, context));
  }
}

const StaticDowngradeConfig*
StaticDowngradeRouteConfig::downgradeConfig(Http::HeaderMap& rqx, Http::HeaderMap& rpx) const {
  // No static downgrade config to get
  if (downgrade_configs_.size() == 0) {
    return nullptr;
  }
  // Serach first matched downgrade config by rqx/rpx
  for (auto& config : downgrade_configs_) {
    if (config->shouldDowngrade(rqx, rpx)) {
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

  // encode plugin should not refresh the route,so can keep route_config directly and wait for
  // decodedata to use
  auto route_config =
      Http::Utility::resolveMostSpecificPerFilterConfig<StaticDowngradeRouteConfig>(name(), route);
  // no config
  if (!route_config) {
    ENVOY_LOG(debug, "No config for downgrade and do nothing");
    return Http::FilterHeadersStatus::Continue;
  }

  if (rqx_headers_ == nullptr) {
    downgrade_config_ = route_config->downgradeConfig(
        *Http::StaticEmptyHeaders::get().request_headers, *rpx_headers_);
  } else {
    downgrade_config_ = route_config->downgradeConfig(*rqx_headers_, *rpx_headers_);
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
