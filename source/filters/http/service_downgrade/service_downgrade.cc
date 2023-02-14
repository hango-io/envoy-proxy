#include "source/filters/http/service_downgrade/service_downgrade.h"

#include "source/common/http/header_utility.h"
#include "source/common/http/proxy_header.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace ServiceDowngrade {

RouteConfig::RouteConfig(const ProtoRouteConfig& config) {
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

  downgrade_uri_ = Proxy::Common::Http::HttpUri::makeHttpUri(config.downgrade_uri());

  if (config.has_override_remote()) {
    const uint32_t timeout_ms =
        PROTOBUF_GET_MS_OR_DEFAULT(config.override_remote(), timeout, DefaultTimeout);
    override_remote_ = std::make_shared<Proxy::Common::Sender::HttpxRequestSenderConfig>(
        config.override_remote().cluster(), timeout_ms);
  }
}

bool RouteConfig::shouldDowngrade(Http::HeaderMap& headers,
                                  Proxy::Common::Http::Direction direc) const {
  if (has_no_donwgrade_rpx_) {
    return false;
  }

  return direc == Proxy::Common::Http::Direction::DECODE
             ? Http::HeaderUtility::matchHeaders(headers, downgrade_rqx_)
             : Http::HeaderUtility::matchHeaders(headers, downgrade_rpx_);
}

CommonConfig::CommonConfig(const ProtoCommonConfig& config,
                           Server::Configuration::FactoryContext& context)
    : context_(context) {
  if (config.has_used_remote()) {
    const uint32_t timeout_ms =
        PROTOBUF_GET_MS_OR_DEFAULT(config.used_remote(), timeout, DefaultTimeout);
    used_remote_ = std::make_shared<Proxy::Common::Sender::HttpxRequestSenderConfig>(
        config.used_remote().cluster(), timeout_ms);
  }
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  rqx_headers_ = &headers;
  router_config_ = Http::Utility::resolveMostSpecificPerFilterConfig<RouteConfig>(
      name(), decoder_callbacks_->route());
  if (!router_config_) {
    return Http::FilterHeadersStatus::Continue;
  }

  // 根据请求判断是否发生降级并以此为根据决定是否缓存请求体。此处初次设置 should_downgrade_
  // 值（原始默认为 false）。
  should_downgrade_ =
      router_config_->shouldDowngrade(*rqx_headers_, Proxy::Common::Http::Direction::DECODE);
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool) {
  if (should_downgrade_) {
    buffered_rqx_body_.add(data);
  }
  return Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  // 假设请求阶段未执行则 should_downgrade_ 必然为 false；如果请求不应当降级，则 should_downgrade_
  // 同样为 false
  if (!should_downgrade_) {
    ENVOY_LOG(debug, "No request info or no service downgrade for current request");
    return Http::FilterHeadersStatus::Continue;
  }
  ASSERT(rqx_headers_);
  rpx_headers_ = &headers;

  // 既然能够执行到此处，说明 should_downgrade_ 为 true。说明 route_config_ 不可能为空。
  // 根据响应再次检查 should_downgrade_ 值
  should_downgrade_ =
      should_downgrade_ &&
      router_config_->shouldDowngrade(*rpx_headers_, Proxy::Common::Http::Direction::ENCODE);

  if (!should_downgrade_) {
    ENVOY_LOG(debug, "No service downgrade for current response");
    return Http::FilterHeadersStatus::Continue;
  }

  // Try downgrade request and get new response.

  headers_only_resp_ = end_stream;
  request_sender_ = std::make_unique<Proxy::Common::Sender::HttpxRequestSender>(
      router_config_->overrideRemote() ? router_config_->overrideRemote() : config_->usedRemote(),
      config_->context().clusterManager());

  auto downgrade_headers_ptr = Http::RequestHeaderMapImpl::create();
  Http::HeaderMapImpl::copyFrom(*downgrade_headers_ptr, *rqx_headers_);

  auto& request_uri = router_config_->downgradeUri();
  if (request_uri) {
    downgrade_headers_ptr->setScheme(request_uri->scheme);
    downgrade_headers_ptr->setHost(request_uri->host);
    downgrade_headers_ptr->setPath(request_uri->path);
  }

  auto downgrade_request =
      std::make_unique<Http::RequestMessageImpl>(std::move(downgrade_headers_ptr));

  // 由于路由刷新可能存在应该缓存body而没有缓存的情况，但是该情况无法避免且出现概率极低，因此忽略
  downgrade_request->body().move(buffered_rqx_body_);

  // 在 sendRequest 过程中，可能存在会由于无 upstream host 等原因直接调用 onSuccess/onFailure
  // 回调函数，所以需要额外判断 local_request_over_ 标志位。
  if (request_sender_->sendRequest(std::move(downgrade_request), this) ==
          Proxy::Common::Sender::SendRequestStatus::ONCALL &&
      !local_request_over_) {

    // 只考虑 HTTP 1.1 协议。
    // 如果响应只有 Headers 而没有 Body，则可以安全的直接无视 encodeData 阶段。如果响应之中具有
    // Body，则必须在 Headers 阶段完全暂停插件链，保证当前插件的 encodeData
    // 在获得了降级响应之后再恢复执行，且在 encodeData 阶段中完成 Body 替换。
    is_sending_request_ = true;
    return headers_only_resp_ ? Http::FilterHeadersStatus::StopIteration
                              : Http::FilterHeadersStatus::StopAllIterationAndBuffer;
  } else {
    ENVOY_LOG(debug, "Failed to send downgrade request to remote");
    downgrade_suspend_ = true;
    return Http::FilterHeadersStatus::Continue;
  }
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  if (!should_downgrade_ || downgrade_complete_ || downgrade_suspend_) {
    return Http::FilterDataStatus::Continue;
  }

  // 降级成功且使用新响应体替换原始响应体
  data.drain(data.length());
  if (end_stream) {
    encoder_callbacks_->addEncodedData(buffered_rpx_body_, false);
    downgrade_complete_ = true;
    return Http::FilterDataStatus::Continue;
  }
  return Http::FilterDataStatus::StopIterationNoBuffer;
}

void Filter::onSuccess(const Http::AsyncClient::Request&, Http::ResponseMessagePtr&& response) {
  if (!is_sending_request_) {
    // 在创建异步请求的过程中直接完成请求并调用 onSuccess，即异步请求未发出。此时不做任何操作
    // 直接返回。
    local_request_over_ = true;
    return;
  }

  is_sending_request_ = false;
  auto& header = response->headers();
  ENVOY_LOG(trace, "Remote response headers:\n{}", header);

  Proxy::Common::Http::HeaderUtility::replaceHeaders(*rpx_headers_, header);
  // 如果响应只有 headers，不包含 Body，则当前插件 encodeData 等方法不会被调用，降级完成。
  // 如果响应包含 body，则当前插件需要进一步等到 encodeData 方法并替换其中 body 数据。
  downgrade_complete_ = headers_only_resp_;

  static Http::LowerCaseString downgrade_status("x-downgrade-status");
  rpx_headers_->addCopy(downgrade_status, "SERVICE");

  downgrade_complete_ ? encoder_callbacks_->addEncodedData(response->body(), false)
                      : buffered_rpx_body_.move(response->body());

  encoder_callbacks_->continueEncoding();
}

void Filter::onFailure(const Http::AsyncClient::Request&, Http::AsyncClient::FailureReason) {
  if (!is_sending_request_) {
    // 在创建异步请求的过程中直接完成请求并调用 onFailure，即异步请求未发出。此时不做任何操作
    // 直接返回。
    local_request_over_ = true;
    return;
  }

  downgrade_suspend_ = true;
  is_sending_request_ = false;
  encoder_callbacks_->continueEncoding();
}

// TODO(wangbaiping): Support Tracing.
void Filter::onBeforeFinalizeUpstreamSpan(Tracing::Span&, const Http::ResponseHeaderMap*) {}

void Filter::onDestroy() {
  if (request_sender_ && is_sending_request_) {
    is_sending_request_ = false;
    downgrade_suspend_ = true;
    request_sender_->cancel();
    request_sender_.reset();
  }
}

} // namespace ServiceDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
