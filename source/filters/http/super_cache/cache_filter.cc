#include "source/filters/http/super_cache/cache_filter.h"

#include <algorithm>
#include <functional>
#include <set>

#include "envoy/registry/registry.h"
#include "envoy/server/admin.h"

#include "source/common/common/hex.h"
#include "source/common/common/proxy_utility.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/headers.h"
#include "source/common/http/proxy_header.h"
#include "source/common/http/utility.h"
#include "source/common/json/json_loader.h"

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "openssl/md5.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace SuperCache {

namespace {

// Http Protocol && (get || head)
bool isCacheableRequest(Http::RequestHeaderMap& headers) {
  const Http::HeaderEntry* method = headers.Method();
  auto& method_value = Http::Headers::get().MethodValues;
  return method && headers.Path() && headers.Host() &&
         (method->value() == method_value.Get || method->value() == method_value.Head);
}

bool isCacheableResponse(Http::ResponseHeaderMap& headers) {
  const auto cache_control = headers.get(Http::CustomHeaders::get().CacheControl);
  if (!cache_control.empty()) {
    return !StringUtil::caseFindToken(cache_control[0]->value().getStringView(), ",", "private");
  }
  return true;
}

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

HttpCacheFilter::HttpCacheFilter(CommonCacheConfig* config, TimeSource&, const std::string& name)
    : config_(config), filter_name_(name) {}

// no cache or no config: no additional headers be added
// config disable cache: x-cache-status: BYPASS
// cache miss : x-cache-key: <>, x-cache-status: MISS
// cache hit: x-cache-key: <>, x-cache-status: HIT, x-cache-hit: <>

Http::FilterHeadersStatus HttpCacheFilter::decodeHeaders(Http::RequestHeaderMap& headers,
                                                         bool end_stream) {
  // only get or head method can be cached
  if (!isCacheableRequest(headers)) {
    ENVOY_LOG(debug, "Cache filter is diabled for request is not cacheable");
    return Http::FilterHeadersStatus::Continue;
  }
  // route level switch
  Router::RouteConstSharedPtr route = decoder_callbacks_->route();
  if (!route.get()) {
    ENVOY_LOG(debug, "Cache filter is diabled for no uesful route.");
    return Http::FilterHeadersStatus::Continue;
  }
  // route level cache switch
  route_config_ =
      Http::Utility::resolveMostSpecificPerFilterConfig<RouteCacheConfig>(filter_name_, route);
  if (!route_config_ || !route_config_->checkEnable(headers, RouteCacheConfig::Type::RQ)) {
    ENVOY_LOG(debug, "Cache filter is disabled for no route config/not cacheable");
    return Http::FilterHeadersStatus::Continue;
  }

  enable_caches_ = true;

  request_sender_ = std::make_shared<Proxy::Common::Sender::CacheRequestSender>(
      config_->usedCaches().get(), route_config_->cacheConfig().get());

  request_sender_->setStreamId(decoder_callbacks_->streamId());

  auto cache_request = std::make_unique<Proxy::Common::Http::WeakHeaderOnlyMessage>(headers);

  if (request_sender_->sendRequest(std::move(cache_request), this) ==
      Proxy::Common::Sender::SendRequestStatus::ONCALL) {
    return end_stream ? Http::FilterHeadersStatus::StopIteration
                      : Http::FilterHeadersStatus::StopAllIterationAndWatermark;
  }

  cache_suspend_ = true;
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus HttpCacheFilter::encodeHeaders(Http::ResponseHeaderMap& headers,
                                                         bool end_stream) {
  if (!enable_caches_ || !isCacheableResponse(headers) ||
      !route_config_->checkEnable(headers, RouteCacheConfig::Type::RP)) {
    // 当前由于路由配置对于当前请求无需缓存或者响应不符合基本缓存需求
    ENVOY_LOG(debug, "Cache filter cannot cache current response");
    enable_caches_ = false;
    return Http::FilterHeadersStatus::Continue;
  }

  ASSERT(request_sender_.get());

  // Try to cache response when request is miss in cache or low level fill is enable.
  if (route_config_->lowlevelFill() || !hit_in_caches_) {
    auto headers_copy = Http::ResponseHeaderMapImpl::create();
    Http::HeaderMapImpl::copyFrom(*headers_copy, headers);
    headers_copy->removeEnvoyUpstreamServiceTime();

    response_to_cache_ = std::make_unique<Http::ResponseMessageImpl>(std::move(headers_copy));
    if (end_stream) {
      request_sender_->insertResponse("", std::move(response_to_cache_));
    }
  }

  headers.addCopy(Http::LowerCaseString("x-cache-key"), request_sender_->cacheKey());

  if (!hit_in_caches_) {
    config_->stats_.miss_.inc();
    headers.addCopy(Http::LowerCaseString("x-cache-status"), "MISS");
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus HttpCacheFilter::encodeData(Buffer::Instance& data, bool end_stream) {
  if (!enable_caches_ || !response_to_cache_) {
    return Http::FilterDataStatus::Continue;
  }

  response_to_cache_->body().add(data);
  if (end_stream) {
    ASSERT(request_sender_.get());
    request_sender_->insertResponse("", std::move(response_to_cache_));
  }
  return Http::FilterDataStatus::Continue;
}

void HttpCacheFilter::onSuccess(const Http::AsyncClient::Request&,
                                Http::ResponseMessagePtr&& response) {
  hit_in_caches_ = true;
  config_->stats_.hit_.inc();

  auto response_headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(response->headers());

  response_headers->addCopy(Http::LowerCaseString("x-cache-status"), "HIT");
  response_headers->addCopy(Http::LowerCaseString("x-cache-hit"),
                            request_sender_->reqeustHitInCache());

  const bool end_stream = response->body().length() == 0;
  ENVOY_LOG(debug, "Encode headers from cache:\n {}", response->headers());
  decoder_callbacks_->streamInfo().setResponseCodeDetails("request_hit_in_cache");

  decoder_callbacks_->encodeHeaders(std::move(response_headers), end_stream, "CACHE_HIT");
  if (end_stream) {
    return;
  }

  ENVOY_LOG(debug, "Encode data from cache, length: {}", response->body().length());
  decoder_callbacks_->encodeData(response->body(), true);
}

void HttpCacheFilter::onFailure(const Http::AsyncClient::Request&,
                                Http::AsyncClient::FailureReason) {
  decoder_callbacks_->continueDecoding();
}

void HttpCacheFilter::onDestroy() {
  if (request_sender_) {
    request_sender_->cancel();
  }
}

std::map<std::string, std::string> CommonCacheConfig::ADMIN_HANDLER_UUID_MAP = {};

CommonCacheConfig::CommonCacheConfig(const ProtoConfig& config,
                                     Server::Configuration::FactoryContext& context,
                                     const std::string& stats_prefix)
    : context_(context), stats_(generateStats(stats_prefix, context_.scope())) {
  // create or get cache
  if (!config.used_caches().empty()) {
    used_caches_ = std::make_shared<Proxy::Common::Sender::CacheGetterSetterConfig>(
        config.used_caches(), context_);
  }

  // 无api prefix提供或者无缓存情况下不扩展Admin api
  if (config.apis_prefix().empty() || !used_caches_) {
    return;
  }

  // extend admin port
  final_apis_prefix_ = "/httpfilter/super_cache/" + config.apis_prefix();
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

CommonCacheConfig::~CommonCacheConfig() {
  if (final_apis_prefix_.empty()) {
    return;
  }
  auto iter = ADMIN_HANDLER_UUID_MAP.find(final_apis_prefix_);
  if (iter != ADMIN_HANDLER_UUID_MAP.end() && iter->second == admin_handler_uuid_) {
    // Handler in admin is not rewritten by new configs.
    context_.admin().removeHandler(final_apis_prefix_);
    ADMIN_HANDLER_UUID_MAP.erase(final_apis_prefix_);
  }
}

Proxy::Common::Sender::CacheGetterSetterConfigSharedPtr& CommonCacheConfig::usedCaches() {
  return used_caches_;
}

RouteCacheConfig::RouteCacheConfig(const RouteProtoConfig& config) {
  for (auto& header_data : config.enable_rqx().headers()) {
    enable_rqx_.push_back(std::make_unique<Http::HeaderUtility::HeaderData>(header_data));
  }
  for (auto& header_data : config.enable_rpx().headers()) {
    enable_rpx_.push_back(std::make_unique<Http::HeaderUtility::HeaderData>(header_data));
  }

  low_level_fill_ = config.low_level_fill();

  std::map<std::string, Proxy::Common::Sender::ProtoTTL> ttl_config(config.cache_ttls().begin(),
                                                                    config.cache_ttls().end());
  cache_config_ =
      std::make_shared<Proxy::Common::Sender::SpecificCacheConfig>(config.key_maker(), ttl_config);
}

bool RouteCacheConfig::checkEnable(const Http::HeaderMap& headers, Type type) const {
  const auto& enable = type == Type::RQ ? enable_rqx_ : enable_rpx_;
  if (enable.size() == 0) {
    return true;
  }
  return Http::HeaderUtility::matchHeaders(headers, enable);
}

} // namespace SuperCache
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
