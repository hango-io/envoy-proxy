#include "source/filters/http/dynamic_downgrade/dynamic_downgrade_filter.h"

#include <algorithm>

#include "envoy/common/exception.h"

#include "source/common/common/assert.h"
#include "source/common/common/proxy_utility.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/headers.h"
#include "source/common/http/message_impl.h"
#include "source/common/http/proxy_header.h"
#include "source/common/json/json_loader.h"
#include "source/common/protobuf/utility.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace DynamicDowngrade {

namespace {
std::set<std::string> readCacheKeys(std::string& json_body) {
  std::vector<std::string> cache_keys{30};
  try {
    auto object = Json::Factory::loadFromString(json_body);
    cache_keys = object->getStringArray("cache_keys", true);
  } catch (std::exception& e) {
    cache_keys = {};
  }
  std::set<std::string> unique_cache_keys;
  for (std::string& key : cache_keys) {
    if (!Common::Common::StringUtil::md5StringOrNot(key)) {
      continue;
    }
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    unique_cache_keys.insert(key);
  }
  return unique_cache_keys;
}
} // namespace

std::map<std::string, std::string> DynamicDowngradeCommonConfig::ADMIN_HANDLER_UUID_MAP = {};

DynamicDowngradeCommonConfig::DynamicDowngradeCommonConfig(
    const ProtoCommonConfig& config, Server::Configuration::FactoryContext& context)
    : context_(context) {

  if (!config.used_caches().empty()) {
    used_caches_ = std::make_shared<Proxy::Common::Sender::CacheGetterSetterConfig>(
        config.used_caches(), context_);
  }
  if (config.has_used_remote()) {
    const uint32_t timeout_ms =
        PROTOBUF_GET_MS_OR_DEFAULT(config.used_remote(), timeout, DefaultTimeout);
    used_remote_ = std::make_shared<Proxy::Common::Sender::HttpxRequestSenderConfig>(
        config.used_remote().cluster(), timeout_ms);
  }

  // 无api prefix提供或者无缓存情况下不扩展Admin api
  if (config.apis_prefix().empty() || !used_caches_) {
    return;
  }

  final_apis_prefix_ = "/httpfilter/dynamic_downgrade/" + config.apis_prefix();
  admin_handler_uuid_ = context_.api().randomGenerator().uuid();
  ADMIN_HANDLER_UUID_MAP[final_apis_prefix_] = admin_handler_uuid_;

  Server::Admin::HandlerCb func = [this](absl::string_view path_and_query, Http::HeaderMap&,
                                         Buffer::Instance& response,
                                         Server::AdminStream& stream) -> Http::Code {
    auto params = Http::Utility::parseQueryString(path_and_query);
    std::string action, unique_id, string_cache_key;
    action = params.find("action") != params.end() ? params["action"] : "";

    if (action.empty() || action != "remove") {
      response.add("{\"error\":\"no action or unsupported action.\"}");
      return Http::Code::BadRequest;
    }
    std::string string_body = stream.getRequestBody() ? stream.getRequestBody()->toString() : "";
    if (string_body.empty()) {
      response.add("{\"error\":\"empty body is unsupported.\"}");
      return Http::Code::BadRequest;
    }
    std::set<std::string> cache_keys = readCacheKeys(string_body);

    for (auto& cache_key : cache_keys) {
      auto cache_handler =
          std::make_shared<Proxy::Common::Sender::CacheRequestSender>(used_caches_.get(), nullptr);
      cache_handler->removeResponse(cache_key);
    }
    response.add("{\"info\":\"ok\"}");
    return Http::Code::OK;
  };
  // Setup new handler. If old handler exist then rewrite it.
  context_.admin().removeHandler(final_apis_prefix_);
  context_.admin().addHandler(final_apis_prefix_, "remove item in all cache.", func, true, true);
}

DynamicDowngradeCommonConfig::~DynamicDowngradeCommonConfig() {
  if (final_apis_prefix_.empty()) {
    return;
  }
  auto iter = ADMIN_HANDLER_UUID_MAP.find(final_apis_prefix_);
  if (iter != ADMIN_HANDLER_UUID_MAP.end() && iter->second == admin_handler_uuid_) {
    // Handler in admin not overrided by new configs.
    context_.admin().removeHandler(final_apis_prefix_);
    ADMIN_HANDLER_UUID_MAP.erase(final_apis_prefix_);
  }
}

Envoy::Proxy::Common::Sender::CacheGetterSetterConfigSharedPtr&
DynamicDowngradeCommonConfig::usedCaches() {
  return used_caches_;
}

Envoy::Proxy::Common::Sender::HttpxRequestSenderConfigSharedPtr&
DynamicDowngradeCommonConfig::usedRemote() {
  return used_remote_;
}

DynamicDowngradeRouteConfig::DynamicDowngradeRouteConfig(const ProtoRouteConfig& config) {

  for (auto& header_data : config.downgrade_rqx().headers()) {
    downgrade_rqx_.push_back(std::make_unique<Http::HeaderUtility::HeaderData>(header_data));
  }
  for (auto& header_data : config.downgrade_rpx().headers()) {
    downgrade_rpx_.push_back(std::make_unique<Http::HeaderUtility::HeaderData>(header_data));
  }
  if (downgrade_rpx_.size() == 0) {
    has_no_donwgrade_rpx_ = true; // no rpx/rqx no downgrade
    return;
  }

  if (config.downgrade_src() == ProtoRouteConfig::HTTPX) {
    // 使用接口降级无需初始化缓存相关配置
    downgrade_with_cache_ = false;
    downgrade_uri_ = Proxy::Common::Http::HttpUri::makeHttpUri(config.downgrade_uri());
    if (config.has_override_remote()) {
      const uint32_t timeout_ms =
          PROTOBUF_GET_MS_OR_DEFAULT(config.override_remote(), timeout, DefaultTimeout);
      override_remote_ = std::make_shared<Proxy::Common::Sender::HttpxRequestSenderConfig>(
          config.override_remote().cluster(), timeout_ms);
    }
    return;
  } else {
    downgrade_with_cache_ = true;
  }

  // 初始化缓存相关配置
  for (auto& header_data : config.cache_rpx_rpx().headers()) {
    cache_rpx_rpx_.push_back(std::make_unique<Http::HeaderUtility::HeaderData>(header_data));
  }

  std::map<std::string, Proxy::Common::Sender::ProtoTTL> ttl_config(config.cache_ttls().begin(),
                                                                    config.cache_ttls().end());
  cache_config_ = std::make_shared<Proxy::Common::Sender::SpecificCacheConfig>(
      config.key_maker(), ttl_config, "downgrade");
}

bool DynamicDowngradeRouteConfig::shouldDowngrade(Http::HeaderMap& headers,
                                                  Proxy::Common::Http::Direction direc) const {
  if (has_no_donwgrade_rpx_) {
    return false;
  }

  return direc == Proxy::Common::Http::Direction::DECODE
             ? Http::HeaderUtility::matchHeaders(headers, downgrade_rqx_)
             : Http::HeaderUtility::matchHeaders(headers, downgrade_rpx_);
}

bool DynamicDowngradeRouteConfig::downgradeWithCache() const { return downgrade_with_cache_; }

bool DynamicDowngradeRouteConfig::checkEnableCache(const Http::HeaderMap& response_headers) const {
  if (cache_rpx_rpx_.size() == 0 || !downgrade_with_cache_) {
    return false;
  }
  return Http::HeaderUtility::matchHeaders(response_headers, cache_rpx_rpx_);
}

Http::FilterHeadersStatus HttpDynamicDowngradeFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                                    bool) {
  rqx_headers_ = &headers;
  router_config_ = Http::Utility::resolveMostSpecificPerFilterConfig<DynamicDowngradeRouteConfig>(
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

Http::FilterDataStatus HttpDynamicDowngradeFilter::decodeData(Buffer::Instance& data, bool) {
  // 如果 should_downgrade_ 为 true，则 route_config_ 必然存在不可能为空。
  if (should_downgrade_ && !router_config_->downgradeWithCache()) {
    buffered_rqx_body_.add(data);
  }
  return Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus
HttpDynamicDowngradeFilter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  // 假设请求阶段未执行则 should_downgrade_ 必然为 false；如果请求不应当降级，则 should_downgrade_
  // 同样为 false
  if (!should_downgrade_) {
    ENVOY_LOG(debug, "No request info or no dynamic downgrade for current request");
    return Http::FilterHeadersStatus::Continue;
  }
  ASSERT(rqx_headers_);
  rpx_headers_ = &headers;

  // 不再考虑路由刷新的可能性，与请求阶段保持一致，使用相同的route_config_。
  // 既然能够执行到此处，说明 should_downgrade_ 为 true。说明 route_config_ 不可能为空。
  // 根据响应重新设置 should_downgrade_ 值
  should_downgrade_ =
      should_downgrade_ &&
      router_config_->shouldDowngrade(*rpx_headers_, Proxy::Common::Http::Direction::ENCODE);

  if (!should_downgrade_) {
    should_cache_rpx_ = router_config_->checkEnableCache(headers);
  }

  // 无降级无缓存
  if (!should_downgrade_ && !should_cache_rpx_) {
    ENVOY_LOG(debug, "No downgrade and no cache reponse: Do Nothing");
    return Http::FilterHeadersStatus::Continue;
  }

  headers_only_resp_ = end_stream;

  if (router_config_->downgradeWithCache()) {
    request_sender_ = std::make_shared<Proxy::Common::Sender::CacheRequestSender>(
        config_->usedCaches().get(), router_config_->cacheConfig().get());
  } else {
    request_sender_ = std::make_shared<Proxy::Common::Sender::HttpxRequestSender>(
        router_config_->overrideRemote() ? router_config_->overrideRemote() : config_->usedRemote(),
        config_->context().clusterManager());
  }

  // 无降级有缓存
  if (!should_downgrade_ && should_cache_rpx_) {
    ENVOY_LOG(debug, "No downgrade but need cache response for future.");
    auto headers_copy = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(*rpx_headers_);
    response_to_cache_ = std::make_unique<Http::ResponseMessageImpl>(std::move(headers_copy));
    cache_key_ = router_config_->cacheConfig()->cacheKey(*rqx_headers_);

    auto cache_request_sender =
        dynamic_cast<Proxy::Common::Sender::CacheRequestSender*>(request_sender_.get());
    if (headers_only_resp_ && cache_request_sender) {
      cache_request_sender->insertResponse(cache_key_, std::move(response_to_cache_));
    }
    return Http::FilterHeadersStatus::Continue;
  }
  // 接口缓存降级
  Envoy::Http::RequestMessagePtr downgrade_request = nullptr;

  if (router_config_->downgradeWithCache()) {
    downgrade_request = std::make_unique<Proxy::Common::Http::WeakHeaderOnlyMessage>(*rqx_headers_);
  } else {
    downgrade_request = std::make_unique<Http::RequestMessageImpl>(
        Http::createHeaderMap<Http::RequestHeaderMapImpl>(*rqx_headers_));
    auto& request_uri = router_config_->downgradeUri();
    if (request_uri) {
      downgrade_request->headers().setScheme(request_uri->scheme);
      downgrade_request->headers().setHost(request_uri->host);
      downgrade_request->headers().setPath(request_uri->path);
    }
    // 由于路由刷新可能存在应该缓存body而没有缓存的情况，但是该情况无法避免且出现概率极低，因此忽略
    downgrade_request->body().move(buffered_rqx_body_);
  }

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

Http::FilterDataStatus HttpDynamicDowngradeFilter::encodeData(Buffer::Instance& data,
                                                              bool end_stream) {
  // 降级完成或者降级被中止或者无任何操作
  if (downgrade_complete_ || downgrade_suspend_ || (!should_downgrade_ && !should_cache_rpx_)) {
    return Http::FilterDataStatus::Continue;
  }

  // no downgrade but cache
  if (!should_downgrade_ && should_cache_rpx_) {
    ASSERT(headers_only_resp_ == false);
    buffered_rpx_body_.add(data);
    if (end_stream) {
      auto cache_request_sender =
          dynamic_cast<Proxy::Common::Sender::CacheRequestSender*>(request_sender_.get());
      if (cache_request_sender) {
        response_to_cache_->body().move(buffered_rpx_body_);
        cache_request_sender->insertResponse(cache_key_, std::move(response_to_cache_));
      }
    }
    return Http::FilterDataStatus::Continue;
  }

  // 降级成功且使用外部响应体替换原始响应体
  data.drain(data.length());
  if (end_stream) {
    encoder_callbacks_->addEncodedData(buffered_rpx_body_, false);
    downgrade_complete_ = true;
    return Http::FilterDataStatus::Continue;
  }
  return Http::FilterDataStatus::StopIterationNoBuffer;
}

void HttpDynamicDowngradeFilter::onSuccess(const Http::AsyncClient::Request&,
                                           Http::ResponseMessagePtr&& response) {
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
  rpx_headers_->addCopy(downgrade_status, "DYNAMIC");

  downgrade_complete_ ? encoder_callbacks_->addEncodedData(response->body(), false)
                      : buffered_rpx_body_.move(response->body());

  encoder_callbacks_->continueEncoding();
}

void HttpDynamicDowngradeFilter::onFailure(const Http::AsyncClient::Request&,
                                           Http::AsyncClient::FailureReason) {
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

void HttpDynamicDowngradeFilter::onBeforeFinalizeUpstreamSpan(Tracing::Span&,
                                                              const Http::ResponseHeaderMap*) {}

void HttpDynamicDowngradeFilter::onDestroy() {
  if (request_sender_ && is_sending_request_) {
    request_sender_->cancel();
  }
}

} // namespace DynamicDowngrade
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
