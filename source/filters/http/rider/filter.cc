#include "source/filters/http/rider/filter.h"

#include <cstdint>
#include <memory>

#include "envoy/http/codes.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/buffer/buffer_impl.h"
#include "source/common/common/assert.h"
#include "source/common/common/enum_to_int.h"
#include "source/common/common/logger.h"
#include "source/common/crypto/utility.h"
#include "source/common/http/header_utility.h"
#include "source/common/http/message_impl.h"
#include "source/common/http/utility.h"
#include "source/extensions/filters/common/lua/wrappers.h"
#include "source/filters/http/rider/context.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

thread_local ContextBase* global_ctx = nullptr;

ConfigImpl::ConfigImpl(const FilterConfigProto& proto_config,
                       Upstream::ClusterManager& cluster_manager,
                       AccessLog::AccessLogManager& log_manager, TimeSource& time_source,
                       ThreadLocal::SlotAllocator& tls, Api::Api& api)
    : cluster_manager_(cluster_manager), time_source_(time_source),
      plugin_handle_(tls.allocateSlot()), allow_no_route_(proto_config.plugin().allow_no_route()) {
  // TODO(Tong Cai): async load.
  std::string code, source;
  if (proto_config.plugin().code().has_remote()) {
    throw EnvoyException(fmt::format("remote code not supported"));
  }

  code = Envoy::Config::DataSource::read(proto_config.plugin().code().local(), true, api);

  plugin_ = std::make_shared<Plugin>(
      proto_config.plugin().name(), "", code,
      MessageUtil::getJsonStringFromMessageOrError(proto_config.plugin().config()));
  PluginSharedPtr plugin = plugin_;

  auto vm_id = proto_config.plugin().vm_config().vm_id();
  // TODO(Tong Cai): validate the package path.
  auto package_path = proto_config.plugin().vm_config().package_path();
  if (!package_path.empty() && package_path.back() != ';') {
    package_path.push_back(';');
  }

  {
    // Temporary Lua VM for config validation.
    auto vm = std::make_shared<LuaVirtualMachine>(vm_id, package_path);
    vm->startPlugin(plugin);
  }

  plugin_handle_->set(
      [plugin, vm_id, package_path](Event::Dispatcher&) -> ThreadLocal::ThreadLocalObjectSharedPtr {
        auto vm = getOrCreateThreadLocalLuaVM(vm_id, package_path);
        auto handle = vm->startPlugin(plugin);
        return handle;
      });

  if (proto_config.plugin().has_log_file() && !proto_config.plugin().log_file().path().empty()) {
    log_file_ = log_manager.createAccessLog(
        {Envoy::Filesystem::DestinationType::File, proto_config.plugin().log_file().path()});
  }
}

RoutePluginConfig::RoutePluginConfig(
    const proxy::filters::http::rider::v3alpha1::RoutePluginConfig& proto_config)
    : name_(proto_config.name()),
      configuration_(MessageUtil::getJsonStringFromMessageOrError(proto_config)),
      hash_(MessageUtil::hash(proto_config)) {}

RouteConfig::RouteConfig(
    const proxy::filters::http::rider::v3alpha1::RouteFilterConfig& proto_config) {
  for (int i = 0; i < proto_config.plugins_size(); i++) {
    plugins_.push_back(RoutePluginConfig(proto_config.plugins(i)));
  }
}

void Filter::onDestroy() {
  ENVOY_LOG(debug, "onDestroy");
  destroyed_ = true;
  resetInternalState();
}

void Filter::resetInternalState() {
  destroyed_ = true;

  request_body_.drain(request_body_.length());
  response_body_.drain(response_body_.length());

  if (request_stream_wrapper_.get()) {
    request_stream_wrapper_->onReset();
  }
  if (response_stream_wrapper_.get()) {
    response_stream_wrapper_->onReset();
  }

  if (shared_table_reference_ > 0) {
    lua_State* state = config_.pluginHandle().vm()->luaState();
    luaL_unref(state, LUA_REGISTRYINDEX, shared_table_reference_);
  }
}

// lookupPerFilterConfig find a proper per_filter_config given plugin name.
static void lookupPerFilterConfig(absl::string_view name, absl::optional<RoutePluginConfig>& config,
                                  const RouteConfig* route_config) {

  if (route_config != nullptr) {
    for (const auto& it : route_config->plugins()) {
      if (name == it.name()) {
        config.emplace(it.name(), it.configuration(), it.hash());
        return;
      }
    }
  }
  return;
}

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) {
  auto route = decoder_callbacks_.callbacks_->route();
  if (!route || !route->routeEntry()) {
    // TODO(Tong Cai): handle cases in which the route being modified by
    // subsequent filters.
    if (!config_.allowNoRoute()) {
      ENVOY_LOG(debug, "no route");
      return Http::FilterHeadersStatus::Continue;
    }
  } else {
    lookupPerFilterConfig(
        config_.pluginName(), route_config_,
        dynamic_cast<const RouteConfig*>(route->mostSpecificPerFilterConfig(name())));
  }

  request_headers_ = &headers;
  if (config_.pluginHandle().version() == Version::v1) {
    return doHeaders(request_stream_wrapper_, StreamDirection::DownstreamToUpstream,
                     *(config_.pluginHandle().vm()), request_coroutine_, decoder_callbacks_,
                     config_.pluginHandle().onRequestRef(), headers, end_stream);
  } else {
    return doHeaders(request_stream_wrapper_, StreamDirection::DownstreamToUpstream,
                     *(config_.pluginHandle().vm()), request_coroutine_, decoder_callbacks_,
                     config_.pluginHandle().onRequestHeaderRef(),
                     config_.pluginHandle().onRequestBodyRef(), headers, end_stream);
  }
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
  if (!config_.allowNoRoute() && !route_config_.has_value()) {
    return Http::FilterHeadersStatus::Continue;
  }

  response_headers_ = &headers;
  if (config_.pluginHandle().version() == Version::v1) {
    return doHeaders(response_stream_wrapper_, StreamDirection::UpstreamToDownstream,
                     *(config_.pluginHandle().vm()), response_coroutine_, encoder_callbacks_,
                     config_.pluginHandle().onResponseRef(), headers, end_stream);
  } else {
    return doHeaders(response_stream_wrapper_, StreamDirection::UpstreamToDownstream,
                     *(config_.pluginHandle().vm()), response_coroutine_, encoder_callbacks_,
                     config_.pluginHandle().onResponseHeaderRef(),
                     config_.pluginHandle().onResponseBodyRef(), headers, end_stream);
  }
}

Http::FilterHeadersStatus
Filter::doHeaders(StreamWrapperPtr& stream_wrapper, StreamDirection direction,
                  LuaVirtualMachine& vm, CoroutinePtr& coroutine, FilterCallbacks& callbacks,
                  int function_ref, Http::RequestOrResponseHeaderMap& headers, bool end_stream) {
  if (error_ || function_ref == LUA_NOREF) {
    ENVOY_LOG(debug, "skip do headers");
    return Http::FilterHeadersStatus::Continue;
  }

  // TODO(Tong Cai): skipping based on options/matchers. For example, if plugin
  // depends on route config, we can skip if no route.

  coroutine = vm.createCoroutine();
  coroutine->initialize(this, direction);
  stream_wrapper =
      std::make_unique<StreamWrapper>(*coroutine, *this, callbacks, &headers, end_stream);

  Http::FilterHeadersStatus status = Http::FilterHeadersStatus::Continue;
  try {
    status = stream_wrapper->start(function_ref);
  } catch (LuaException& e) {
    scriptError(e);
  }

  return status;
}

Http::FilterHeadersStatus Filter::doHeaders(StreamWrapperPtr& stream_wrapper,
                                            StreamDirection direction, LuaVirtualMachine& vm,
                                            CoroutinePtr& coroutine, FilterCallbacks& callbacks,
                                            int function_ref, int function_ref_body,
                                            Http::RequestOrResponseHeaderMap& headers,
                                            bool end_stream) {
  if (responded && function_ref == LUA_NOREF) {
    responded = false;
    return Http::FilterHeadersStatus::StopIteration;
  }
  if (error_ || function_ref == LUA_NOREF) {
    ENVOY_LOG(debug, "skip do headers");
    return Http::FilterHeadersStatus::Continue;
  }

  // TODO(Tong Cai): skipping based on options/matchers. For example, if plugin
  // depends on route config, we can skip if no route.

  coroutine = vm.createCoroutine();
  coroutine->initialize(this, direction);
  stream_wrapper =
      std::make_unique<StreamWrapper>(*coroutine, *this, callbacks, &headers, end_stream);

  Http::FilterHeadersStatus status = Http::FilterHeadersStatus::Continue;
  try {
    status = stream_wrapper->start(function_ref, function_ref_body);
  } catch (LuaException& e) {
    scriptError(e);
  }

  return status;
}

void Filter::scriptError(const LuaException& e) {
  error_ = true;
  log(spdlog::level::err, e.what());
}

Http::FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  if (config_.pluginHandle().version() == Version::v1) {
    if (error_ || !request_stream_wrapper_.get()) {
      return Http::FilterDataStatus::Continue;
    }
    return doData(*request_stream_wrapper_, data, end_stream);
  } else {
    if (error_ || config_.pluginHandle().onRequestBodyRef() == LUA_NOREF) {
      return Http::FilterDataStatus::Continue;
    }
    if (!request_stream_wrapper_.get()) {
      request_coroutine_ = config_.pluginHandle().vm()->createCoroutine();
      request_coroutine_->initialize(this, StreamDirection::DownstreamToUpstream);
      request_stream_wrapper_ = std::make_unique<StreamWrapper>(
          *request_coroutine_, *this, decoder_callbacks_, request_headers_, end_stream);
    }
    return doData(*request_stream_wrapper_, config_.pluginHandle().onRequestBodyRef(), data,
                  end_stream);
  }
}

Http::FilterDataStatus Filter::encodeData(Buffer::Instance& data, bool end_stream) {
  if (config_.pluginHandle().version() == Version::v1) {
    if (error_ || !response_stream_wrapper_.get()) {
      return Http::FilterDataStatus::Continue;
    }
    return doData(*response_stream_wrapper_, data, end_stream);
  } else {
    if (error_ || config_.pluginHandle().onResponseBodyRef() == LUA_NOREF) {
      return Http::FilterDataStatus::Continue;
    }
    if (!response_stream_wrapper_.get()) {
      response_coroutine_ = config_.pluginHandle().vm()->createCoroutine();
      response_coroutine_->initialize(this, StreamDirection::UpstreamToDownstream);
      response_stream_wrapper_ = std::make_unique<StreamWrapper>(
          *response_coroutine_, *this, encoder_callbacks_, response_headers_, end_stream);
    }
    return doData(*response_stream_wrapper_, config_.pluginHandle().onResponseBodyRef(), data,
                  end_stream);
  }
}

Http::FilterDataStatus Filter::doData(StreamWrapper& stream_wrapper, Buffer::Instance& data,
                                      bool end_stream) {
  Http::FilterDataStatus status = Http::FilterDataStatus::Continue;

  try {
    status = stream_wrapper.onData(data, end_stream);
  } catch (const LuaException& e) {
    absl::string_view plugin_name = config_.pluginName();
    ENVOY_LOG(error, "run plugin {} error: {}", plugin_name, e.what());
    error_ = true;
    status = Http::FilterDataStatus::Continue;
  }

  return status;
}

Http::FilterDataStatus Filter::doData(StreamWrapper& stream_wrapper, int function_ref,
                                      Buffer::Instance& data, bool end_stream) {
  Http::FilterDataStatus status = Http::FilterDataStatus::Continue;

  try {
    status = stream_wrapper.onData(function_ref, data, end_stream);
  } catch (const LuaException& e) {
    absl::string_view plugin_name = config_.pluginName();
    ENVOY_LOG(error, "run plugin {} error: {}", plugin_name, e.what());
    error_ = true;
    status = Http::FilterDataStatus::Continue;
  }

  return status;
}

void Filter::log(spdlog::level::level_enum level, const char* message) {
  switch (level) {
  case spdlog::level::trace:
    ENVOY_LOG(trace, "script log: {}", message);
    return;
  case spdlog::level::debug:
    ENVOY_LOG(debug, "script log: {}", message);
    return;
  case spdlog::level::info:
    ENVOY_LOG(info, "script log: {}", message);
    return;
  case spdlog::level::warn:
    ENVOY_LOG(warn, "script log: {}", message);
    return;
  case spdlog::level::err:
    ENVOY_LOG(error, "script log: {}", message);
    return;
  case spdlog::level::critical:
    ENVOY_LOG(critical, "script log: {}", message);
    return;
  case spdlog::level::off:
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  case spdlog::level::n_levels:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

int Filter::luaHttpCall(lua_State* state, StreamDirection direction) {
  if (StreamDirection::DownstreamToUpstream == direction) {
    return request_stream_wrapper_->luaHttpCall(state);
  } else {
    return response_stream_wrapper_->luaHttpCall(state);
  }
}

int Filter::luaRespond(lua_State* state, StreamDirection direction) {
  ASSERT(StreamDirection::DownstreamToUpstream == direction ||
         StreamDirection::UpstreamToDownstream == direction);

  if (StreamDirection::DownstreamToUpstream == direction) {
    return request_stream_wrapper_->luaRespond(state);
  } else {
    return response_stream_wrapper_->luaRespond(state);
  }
}

void Filter::fileLog(const char* buf, int len) {
  if (config_.logFile()) {
    config_.logFile()->write(absl::string_view(buf, len));
  }
}

int Filter::getOrCreateSharedTable() {
  if (shared_table_reference_ > 0) {
    return shared_table_reference_;
  }

  lua_State* state = config_.pluginHandle().vm()->luaState();
  lua_createtable(state, 0, 16);
  shared_table_reference_ = luaL_ref(state, LUA_REGISTRYINDEX);
  return shared_table_reference_;
}

uint64_t Filter::getRouteConfigHash() {
  if (!route_config_.has_value()) {
    return 0;
  }

  return route_config_.value().hash();
}

int Filter::getRouteConfiguration(envoy_lua_ffi_str_t* buffer) {
  if (!route_config_.has_value()) {
    return static_cast<int>(FFIReturnCode::NotFound);
  }

  buffer->data = route_config_.value().configuration().data();
  buffer->len = route_config_.value().configuration().length();
  return static_cast<int>(FFIReturnCode::Success);
}

int Filter::luaBody(lua_State* state, StreamDirection direction) {
  ASSERT(StreamDirection::DownstreamToUpstream == direction ||
         StreamDirection::UpstreamToDownstream == direction);

  if (StreamDirection::DownstreamToUpstream == direction) {
    return request_stream_wrapper_->luaBody(state);
  } else {
    return response_stream_wrapper_->luaBody(state);
  }
}

void Filter::DecoderCallbacks::respond(Http::ResponseHeaderMapPtr&& headers, Buffer::Instance* body,
                                       lua_State*) {
  callbacks_->encodeHeaders(std::move(headers), body == nullptr, "");
  if (body && !parent_.destroyed_) {
    callbacks_->encodeData(*body, true);
  }
}

void Filter::EncoderCallbacks::respond(Http::ResponseHeaderMapPtr&&, Buffer::Instance*,
                                       lua_State* state) {
  // TODO(mattklein123): Support response in response path if nothing has been
  // continued yet.
  luaL_error(state, "respond not currently supported in the response path");
}

// StreamInfoInterface
const char* Filter::upstreamHost() {
  ASSERT(decoder_callbacks_.callbacks_);

  auto upstream_info = decoder_callbacks_.callbacks_->streamInfo().upstreamInfo();
  if (upstream_info == nullptr) {
    return nullptr;
  }
  auto host = upstream_info->upstreamHost();
  if (host == nullptr) {
    return nullptr;
  }

  return host->address()->asString().c_str();
}

const char* Filter::upstreamCluster() {
  ASSERT(decoder_callbacks_.callbacks_);

  auto upstream_info = decoder_callbacks_.callbacks_->streamInfo().upstreamInfo();
  if (upstream_info == nullptr) {
    return nullptr;
  }
  auto host = upstream_info->upstreamHost();
  if (host == nullptr) {
    return nullptr;
  }

  return host->cluster().name().c_str();
}

const char* Filter::downstreamLocalAddress() {
  ASSERT(decoder_callbacks_.callbacks_);

  auto addr =
      decoder_callbacks_.callbacks_->streamInfo().downstreamAddressProvider().localAddress();
  if (addr.get()) {
    return addr->asString().c_str();
  }
  return nullptr;
}

const char* Filter::downstreamRemoteAddress() {
  ASSERT(decoder_callbacks_.callbacks_);

  auto addr =
      decoder_callbacks_.callbacks_->streamInfo().downstreamAddressProvider().remoteAddress();
  if (addr.get()) {
    return addr->asString().c_str();
  }

  return nullptr;
}

namespace inner {

static int getHeaderMapSize(Http::HeaderMap* headers) { return headers ? headers->size() : 0; }

static int getHeaderMap(Http::HeaderMap* headers, envoy_lua_ffi_string_pairs* buf) {
  if (!(buf && buf->data && buf->capacity)) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }

  buf->size = 0;
  Envoy::Http::HeaderMap::ConstIterateCb cb =
      [buf](const Envoy::Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    if (buf->size == buf->capacity) {
      return Envoy::Http::HeaderMap::Iterate::Break;
    }

    envoy_lua_ffi_table_elt_t* entry = buf->data + buf->size;
    auto key = header.key().getStringView();
    auto value = header.value().getStringView();
    entry->key.data = key.data();
    entry->key.len = key.length();
    entry->value.data = value.data();
    entry->value.len = value.length();
    buf->size++;

    return Envoy::Http::HeaderMap::Iterate::Continue;
  };

  headers->iterate(cb);
  return static_cast<int>(FFIReturnCode::Success);
}

static int getHeaderMapValue(Http::HeaderMap* headers, absl::string_view key,
                             envoy_lua_ffi_str_t* value) {
  if (!(headers && value) || key.empty()) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }

  const auto get_result = Http::HeaderUtility::getAllOfHeaderAsString(
      *headers, Http::LowerCaseString(std::string(key.data(), key.length())));
  if (!get_result.result().has_value()) {
    return static_cast<int>(FFIReturnCode::NotFound);
  }

  value->data = get_result.result().value().data();
  value->len = get_result.result().value().length();
  return static_cast<int>(FFIReturnCode::Success);
}

static int getHeaderMapValueSize(Http::HeaderMap* headers, absl::string_view key) {
  if (key.empty()) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }
  const auto get_result =
      headers->get(Http::LowerCaseString(std::string(key.data(), key.length())));
  return get_result.size();
}

static int getHeaderMapValueIndex(Http::HeaderMap* headers, absl::string_view key,
                                  envoy_lua_ffi_str_t* value, int index) {
  if (!(headers && value) || key.empty()) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }

  const auto get_result =
      headers->get(Http::LowerCaseString(std::string(key.data(), key.length())));
  if (get_result.empty() || index < 0 || static_cast<uint64_t>(index) >= get_result.size()) {
    return static_cast<int>(FFIReturnCode::NotFound);
  }

  value->data = get_result[index]->value().getStringView().data();
  value->len = get_result[index]->value().size();
  return static_cast<int>(FFIReturnCode::Success);
}

static int setHeaderMap(Http::HeaderMap* headers, envoy_lua_ffi_string_pairs* buf) {
  if (!(buf && buf->data && buf->capacity)) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }

  std::vector<std::string> keys;
  headers->iterate([&keys](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    keys.push_back(std::string(header.key().getStringView()));
    return Http::HeaderMap::Iterate::Continue;
  });
  for (auto& k : keys) {
    const Http::LowerCaseString lower_key{k};
    headers->remove(lower_key);
  }
  for (int i = 0; i < buf->size; i++) {
    absl::string_view key(buf->data[i].key.data, buf->data[i].key.len);
    absl::string_view value(buf->data[i].value.data, buf->data[i].value.len);
    headers->addCopy(Http::LowerCaseString(std::string(key)), value);
  }
  return static_cast<int>(FFIReturnCode::Success);
}

static int setHeaderMapValue(Http::HeaderMap* headers, absl::string_view key,
                             absl::string_view value) {
  if (!headers || key.empty() || value.empty()) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }

  const Http::LowerCaseString& key_s = Http::LowerCaseString(std::string(key.data(), key.length()));
  const std::string val_s(value.data(), value.length());

  headers->setCopy(key_s, value);
  return static_cast<int>(FFIReturnCode::Success);
}

static int removeHeaderMapValue(Http::HeaderMap* headers, absl::string_view key) {
  if (!headers || key.empty()) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }

  headers->remove(Http::LowerCaseString(std::string(key.data(), key.length())));
  return static_cast<int>(FFIReturnCode::Success);
}

} // namespace inner

// HeaderInterface

int Filter::getQueryParameters(envoy_lua_ffi_string_pairs* buf) {
  if (!(request_headers_ && buf && buf->data)) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }

  buf->size = 0;
  if (request_headers_->Path()) {
    temporary_query_params_ = Http::Utility::parseQueryString(request_headers_->getPathValue());
    envoy_lua_ffi_table_elt_t* cur = buf->data;
    envoy_lua_ffi_table_elt_t* end = buf->data + buf->capacity;
    for (auto& it : temporary_query_params_) {
      if (cur == end) {
        break;
      }

      cur->key.data = it.first.c_str();
      cur->key.len = it.first.length();
      cur->value.data = it.second.c_str();
      cur->value.len = it.second.length();
      buf->size++;
      cur++;
    }
  }

  return static_cast<int>(FFIReturnCode::Success);
}

int Filter::getHeaderMapSize(LuaStreamOpSourceType type) {
  if (LuaStreamOpSourceType::Request == type) {
    return inner::getHeaderMapSize(request_headers_);
  }

  if (LuaStreamOpSourceType::Response == type) {
    return inner::getHeaderMapSize(response_headers_);
  }

  NOT_REACHED_GCOVR_EXCL_LINE;
}

int Filter::getHeaderMap(LuaStreamOpSourceType type, envoy_lua_ffi_string_pairs* buf) {
  if (LuaStreamOpSourceType::Request == type) {
    return inner::getHeaderMap(request_headers_, buf);
  }

  if (LuaStreamOpSourceType::Response == type) {
    return inner::getHeaderMap(response_headers_, buf);
  }

  NOT_REACHED_GCOVR_EXCL_LINE;
}

int Filter::getHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key,
                              envoy_lua_ffi_str_t* value) {
  if (LuaStreamOpSourceType::Request == type) {
    return inner::getHeaderMapValue(request_headers_, key, value);
  }

  if (LuaStreamOpSourceType::Response == type) {
    return inner::getHeaderMapValue(response_headers_, key, value);
  }

  NOT_REACHED_GCOVR_EXCL_LINE;
}

int Filter::getHeaderMapValueSize(LuaStreamOpSourceType type, absl::string_view key) {
  if (LuaStreamOpSourceType::Request == type) {
    return inner::getHeaderMapValueSize(request_headers_, key);
  }

  if (LuaStreamOpSourceType::Response == type) {
    return inner::getHeaderMapValueSize(response_headers_, key);
  }

  NOT_REACHED_GCOVR_EXCL_LINE;
}

int Filter::getHeaderMapValueIndex(LuaStreamOpSourceType type, absl::string_view key,
                                   envoy_lua_ffi_str_t* value, int index) {
  if (LuaStreamOpSourceType::Request == type) {
    return inner::getHeaderMapValueIndex(request_headers_, key, value, index);
  }

  if (LuaStreamOpSourceType::Response == type) {
    return inner::getHeaderMapValueIndex(response_headers_, key, value, index);
  }

  NOT_REACHED_GCOVR_EXCL_LINE;
}

int Filter::setHeaderMap(LuaStreamOpSourceType type, envoy_lua_ffi_string_pairs* buf) {
  if (LuaStreamOpSourceType::Request == type) {
    return inner::setHeaderMap(request_headers_, buf);
  }

  if (LuaStreamOpSourceType::Response == type) {
    return inner::setHeaderMap(response_headers_, buf);
  }

  NOT_REACHED_GCOVR_EXCL_LINE;
}

int Filter::setHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key,
                              absl::string_view value) {
  if (LuaStreamOpSourceType::Request == type) {
    return inner::setHeaderMapValue(request_headers_, key, value);
  }

  if (LuaStreamOpSourceType::Response == type) {
    return inner::setHeaderMapValue(response_headers_, key, value);
  }

  NOT_REACHED_GCOVR_EXCL_LINE;
}

int Filter::removeHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key) {
  if (LuaStreamOpSourceType::Request == type) {
    return inner::removeHeaderMapValue(request_headers_, key);
  }

  if (LuaStreamOpSourceType::Response == type) {
    return inner::removeHeaderMapValue(response_headers_, key);
  }

  NOT_REACHED_GCOVR_EXCL_LINE;
}

int Filter::getMetadataValue(envoy_lua_ffi_str_t* filter_name, envoy_lua_ffi_str_t* key,
                             envoy_lua_ffi_str_t* value) {
  if (decoder_callbacks_.callbacks_->route() == nullptr ||
      decoder_callbacks_.callbacks_->route()->routeEntry() == nullptr) {
    return static_cast<int>(FFIReturnCode::NotFound);
  }

  const auto& metadatas = decoder_callbacks_.callbacks_->route()->metadata();
  const auto& filter_it =
      metadatas.filter_metadata().find(std::string(filter_name->data, filter_name->len));
  if (filter_it == metadatas.filter_metadata().end()) {
    return static_cast<int>(FFIReturnCode::NotFound);
  }

  auto metadata = filter_it->second;

  const auto& it = metadata.fields().find(std::string(key->data, key->len));
  if (it == metadata.fields().end()) {
    return static_cast<int>(FFIReturnCode::NotFound);
  }

  // Only integer and string is supported for now.
  // TODO(Tong Cai): support all types.
  switch (it->second.kind_case()) {
  case ProtobufWkt::Value::kNumberValue:
    temporary_string_ = std::to_string(int64_t(it->second.number_value()));
    value->data = temporary_string_.c_str();
    value->len = temporary_string_.length();
    break;
  case ProtobufWkt::Value::kStringValue: {
    temporary_string_ = it->second.string_value();
    value->data = temporary_string_.c_str();
    value->len = temporary_string_.length();
    break;
  }
  default:
    return static_cast<int>(FFIReturnCode::Unsupported);
  }
  return static_cast<int>(FFIReturnCode::Success);
}

int Filter::getDynamicMetadataValue(envoy_lua_ffi_str_t* filter_name, envoy_lua_ffi_str_t* key,
                                    envoy_lua_ffi_str_t* value) {

  const auto& metadatas = decoder_callbacks_.callbacks_->streamInfo().dynamicMetadata();
  const auto& filter_it =
      metadatas.filter_metadata().find(std::string(filter_name->data, filter_name->len));
  if (filter_it == metadatas.filter_metadata().end()) {
    return static_cast<int>(FFIReturnCode::NotFound);
  }

  auto metadata = filter_it->second;

  const auto& it = metadata.fields().find(std::string(key->data, key->len));
  if (it == metadata.fields().end()) {
    return static_cast<int>(FFIReturnCode::NotFound);
  }

  // Only integer and string is supported for now.
  // TODO(Tong Cai): support all types.
  switch (it->second.kind_case()) {
  case ProtobufWkt::Value::kNumberValue:
    temporary_string_ = std::to_string(int64_t(it->second.number_value()));
    value->data = temporary_string_.c_str();
    value->len = temporary_string_.length();
    break;
  case ProtobufWkt::Value::kStringValue: {
    temporary_string_ = it->second.string_value();
    value->data = temporary_string_.c_str();
    value->len = temporary_string_.length();
    break;
  }
  default:
    return static_cast<int>(FFIReturnCode::Unsupported);
  }
  return static_cast<int>(FFIReturnCode::Success);
}

int Filter::getBody(LuaStreamOpSourceType type, envoy_lua_ffi_str_t* body) {
  if (LuaStreamOpSourceType::Request == type) {
    if (decoder_callbacks_.bufferedBody() == nullptr) {
      return static_cast<int>(FFIReturnCode::NotFound);
    } else {
      tmp_body_ = decoder_callbacks_.bufferedBody()->toString();
      body->data = tmp_body_.c_str();
      body->len = tmp_body_.length();
      return static_cast<int>(FFIReturnCode::Success);
    }
  }
  if (LuaStreamOpSourceType::Response == type) {
    if (encoder_callbacks_.bufferedBody() == nullptr) {
      return static_cast<int>(FFIReturnCode::NotFound);
    } else {
      tmp_body_ = encoder_callbacks_.bufferedBody()->toString();
      body->data = tmp_body_.c_str();
      body->len = tmp_body_.length();
      return static_cast<int>(FFIReturnCode::Success);
    }
  }
  NOT_REACHED_GCOVR_EXCL_LINE;
}

StreamWrapper::StreamWrapper(Coroutine& coroutine, Filter& filter, FilterCallbacks& callbacks,
                             Http::RequestOrResponseHeaderMap* headers, bool end_stream)
    : filter_(filter), headers_(headers), filter_callbacks_(callbacks), coroutine_(coroutine),
      yield_callback_([this]() {
        if (this->state_ == State::Running) {
          throw LuaException("Lua code performed an unexpected yield");
        }
      }),
      end_stream_(end_stream) {}

Http::FilterHeadersStatus StreamWrapper::start(int function_ref) {
  // We are on the top of the stack.
  coroutine_.start(function_ref, yield_callback_);
  Http::FilterHeadersStatus status =
      (state_ == State::WaitForBody || state_ == State::HttpCall || state_ == State::Responded)
          ? Http::FilterHeadersStatus::StopIteration
          : Http::FilterHeadersStatus::Continue;

  if (status == Http::FilterHeadersStatus::Continue) {
    headers_continued_ = true;
  }

  return status;
}

Http::FilterHeadersStatus StreamWrapper::start(int function_ref, int function_ref_body) {
  // We are on the top of the stack.
  global_ctx = dynamic_cast<ContextBase*>(&filter_);
  coroutine_.start(function_ref, yield_callback_);
  Http::FilterHeadersStatus status;

  if (state_ == State::HttpCall) {
    status = Http::FilterHeadersStatus::StopAllIterationAndBuffer;
    if (end_stream_ && function_ref_body != LUA_NOREF) {
      start_body_ = true;
      function_ref_body_ = function_ref_body;
    }
  } else if (state_ == State::Responded) {
    status = Http::FilterHeadersStatus::StopIteration;
  } else {
    status = Http::FilterHeadersStatus::StopIteration;
    if (end_stream_) {
      if (function_ref_body == LUA_NOREF) {
        status = Http::FilterHeadersStatus::Continue;
      } else {
        try {
          ENVOY_LOG(debug, "startBody when no data");
          startBody(function_ref_body);
        } catch (LuaException& e) {
          filter_.scriptError(e);
        }
        status = (state_ == State::HttpCall || state_ == State::Responded)
                     ? Http::FilterHeadersStatus::StopIteration
                     : Http::FilterHeadersStatus::Continue;
      }
    }
  }

  return status;
}

void StreamWrapper::startBody(int function_ref) {
  // We are on the top of the stack.
  global_ctx = dynamic_cast<ContextBase*>(&filter_);
  coroutine_.start(function_ref, yield_callback_);
}

Http::FilterDataStatus StreamWrapper::onData(Buffer::Instance& data, bool end_stream) {
  ASSERT(!end_stream_);
  end_stream_ = end_stream;
  saw_body_ = true;

  if (state_ == State::WaitForBodyChunk) {
    ENVOY_LOG(trace, "resuming for next body chunk");
    Envoy::Extensions::Filters::Common::Lua::LuaDeathRef<
        Envoy::Extensions::Filters::Common::Lua::BufferWrapper>
        wrapper(Envoy::Extensions::Filters::Common::Lua::BufferWrapper::create(
                    coroutine_.luaState(), *headers_, data),
                true);
    state_ = State::Running;
    coroutine_.resume(0, yield_callback_);
  } else if (state_ == State::WaitForBody && end_stream_) {
    ENVOY_LOG(debug, "resuming body due to end stream");
    filter_callbacks_.addData(data);
    state_ = State::Running;
    coroutine_.resume(luaBody(coroutine_.luaState()), yield_callback_);
  } else if (state_ == State::WaitForTrailers && end_stream_) {
    ENVOY_LOG(debug, "resuming nil trailers due to end stream");
    state_ = State::Running;
    coroutine_.resume(0, yield_callback_);
  }

  if (state_ == State::HttpCall || state_ == State::WaitForBody) {
    ENVOY_LOG(trace, "buffering body");
    return Http::FilterDataStatus::StopIterationAndBuffer;
  } else if (state_ == State::Responded) {
    return Http::FilterDataStatus::StopIterationNoBuffer;
  } else {
    headers_continued_ = true;
    return Http::FilterDataStatus::Continue;
  }
}

Http::FilterDataStatus StreamWrapper::onData(int function_ref, Buffer::Instance& data,
                                             bool end_stream) {

  //  ASSERT(!end_stream_);
  end_stream_ = end_stream;
  saw_body_ = true;

  // if httpcall continueIteration, startBody may not be called?
  if (end_stream_ && state_ == State::Running) {
    ENVOY_LOG(debug, "start body coroutine due to end stream");
    filter_callbacks_.addData(data);
    state_ = State::Running;
    try {
      startBody(function_ref);
    } catch (LuaException& e) {
      filter_.scriptError(e);
    }
  }

  if (state_ == State::HttpCall) {
    ENVOY_LOG(trace, "buffering body");
    return Http::FilterDataStatus::StopIterationAndBuffer;
  } else if (state_ == State::Responded) {
    return Http::FilterDataStatus::StopIterationNoBuffer;
  } else if (!end_stream_) {
    return Http::FilterDataStatus::StopIterationAndBuffer;
  } else {
    // headers_continued_ = true;
    return Http::FilterDataStatus::Continue;
  }
}

int StreamWrapper::luaRespond(lua_State* state) {
  ASSERT(state_ == State::Running);

  if (headers_continued_) {
    luaL_error(state, "respond() cannot be called if headers have been continued");
  }

  luaL_checktype(state, 1, LUA_TTABLE);
  size_t body_size;
  const char* raw_body = luaL_optlstring(state, 2, nullptr, &body_size);
  auto headers = Http::ResponseHeaderMapImpl::create();
  buildHeadersFromTable(*headers, state, 1);

  uint64_t status;
  if (!absl::SimpleAtoi(headers->getStatusValue(), &status) || status < 200 || status >= 600) {
    luaL_error(state, ":status must be between 200-599");
  }

  Buffer::InstancePtr body;
  if (raw_body != nullptr) {
    body = std::make_unique<Buffer::OwnedImpl>(raw_body, body_size);
    headers->setContentLength(body_size);
  }

  // Once we respond we treat that as the end of the script even if there is
  // more code. Thus we yield.
  filter_.responded = true;
  filter_callbacks_.respond(std::move(headers), body.get(), state);

  state_ = State::Responded;
  return lua_yield(state, 0);
}

void StreamWrapper::buildHeadersFromTable(Http::HeaderMap& headers, lua_State* state,
                                          int table_index) {
  // Build a header map to make the request. We iterate through the provided
  // table to do this and check that we are getting strings.
  lua_pushnil(state);
  while (lua_next(state, table_index) != 0) {
    // Uses 'key' (at index -2) and 'value' (at index -1).
    const char* key = luaL_checkstring(state, -2);
    const char* value = luaL_checkstring(state, -1);
    headers.addCopy(Http::LowerCaseString(key), value);

    // Removes 'value'; keeps 'key' for next iteration. This is the input for
    // lua_next() so that it can push the next key/value pair onto the stack.
    lua_pop(state, 1);
  }
}

/**
 * Set environment table for the given code closure.
 *
 * Before:
 *         | timeout(number) | <- top
 *         | body(string)    |
 *         | headers(table)  |
 *         | cluster(string) |
 *         |      ... |
 *
 * After:
 *         | body(string)   | <- top
 *         | headers(table) |
 *         |      ... |
 * */
int StreamWrapper::luaHttpCall(lua_State* state) {
  ASSERT(state_ == State::Running);

  const std::string cluster = luaL_checkstring(state, 1);
  luaL_checktype(state, 2, LUA_TTABLE);
  size_t body_size;
  const char* body = luaL_optlstring(state, 3, nullptr, &body_size);
  int timeout_ms = luaL_checkint(state, 4);
  if (timeout_ms < 0) {
    return luaL_error(state, "http call timeout must be >= 0");
  }

  const auto thread_local_cluster = filter_.clusterManager().getThreadLocalCluster(cluster);
  if (thread_local_cluster == nullptr) {
    return luaL_error(state, "http call cluster invalid. Must be configured");
  }

  auto headers = Http::RequestHeaderMapImpl::create();
  buildHeadersFromTable(*headers, state, 2);
  Http::RequestMessagePtr message(new Http::RequestMessageImpl(std::move(headers)));

  // Check that we were provided certain headers.
  if (message->headers().Path() == nullptr || message->headers().Method() == nullptr ||
      message->headers().Host() == nullptr) {
    return luaL_error(state, "http call headers must include ':path', ':method', and ':authority'");
  }

  if (body != nullptr) {
    message->body().add(body, body_size);
    message->headers().setContentLength(body_size);
  }

  absl::optional<std::chrono::milliseconds> timeout;
  if (timeout_ms > 0) {
    timeout = std::chrono::milliseconds(timeout_ms);
  }

  http_request_ = thread_local_cluster->httpAsyncClient().send(
      std::move(message), *this, Http::AsyncClient::RequestOptions().setTimeout(timeout));
  if (http_request_) {
    state_ = State::HttpCall;
    return lua_yield(state, 0);
  } else {
    // Immediate failure case. The return arguments are already on the stack.
    ASSERT(lua_gettop(state) >= 2);
    return 2;
  }
}

void StreamWrapper::onSuccess(const Http::AsyncClient::Request&,
                              Http::ResponseMessagePtr&& response) {
  ASSERT(state_ == State::HttpCall || state_ == State::Running);
  ENVOY_LOG(debug, "async HTTP response complete");
  http_request_ = nullptr;

  lua_State* state = coroutine_.luaState();

  // We need to build a table with the headers as return param 1. The body will
  // be return param 2.
  lua_newtable(state);
  response->headers().iterate([state](const Http::HeaderEntry& header) -> Http::HeaderMap::Iterate {
    lua_pushlstring(state, header.key().getStringView().data(),
                    header.key().getStringView().length());
    lua_pushlstring(state, header.value().getStringView().data(),
                    header.value().getStringView().length());
    lua_settable(state, -3);
    return Http::HeaderMap::Iterate::Continue;
  });

  // TODO(mattklein123): Avoid double copy here.
  if (response->body().length() > 0) {
    lua_pushlstring(state, response->bodyAsString().c_str(), response->body().length());
  } else {
    lua_pushnil(state);
  }

  // In the immediate failure case, we are just going to immediately return to
  // the script. We have already pushed the return arguments onto the stack.
  if (state_ == State::HttpCall) {
    state_ = State::Running;

    try {
      global_ctx = dynamic_cast<ContextBase*>(&filter_);
      coroutine_.resume(2, yield_callback_);
    } catch (const LuaException& e) {
      filter_.scriptError(e);
      // TODO(Tong Cai): do not block when error occur. This can be
      // configurable.
      state_ = State::Running;
    }

    if (state_ == State::Running) {
      if (start_body_) {
        start_body_ = false;
        try {
          startBody(function_ref_body_);
        } catch (LuaException& e) {
          filter_.scriptError(e);
        }
        if (state_ == State::Running) {
          // headers_continued_ = true;
          filter_callbacks_.continueIteration();
        }
      } else {
        // headers_continued_ = true;
        filter_callbacks_.continueIteration();
      }
    }
  }
}

void StreamWrapper::onFailure(const Http::AsyncClient::Request& request,
                              Http::AsyncClient::FailureReason) {
  ASSERT(state_ == State::HttpCall || state_ == State::Running);
  ENVOY_LOG(debug, "async HTTP failure");

  // Just fake a basic 503 response.
  Http::ResponseMessagePtr response_message(
      new Http::ResponseMessageImpl(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(
          {{Http::Headers::get().Status,
            std::to_string(enumToInt(Http::Code::ServiceUnavailable))}})));
  response_message->body().add("upstream failure");
  onSuccess(request, std::move(response_message));
}

int StreamWrapper::luaBody(lua_State* state) {
  ASSERT(state_ == State::Running);

  if (end_stream_) {
    if (!buffered_body_ && saw_body_) {

      return luaL_error(state, "cannot call body() after body has been streamed");
    } else if (filter_callbacks_.bufferedBody() == nullptr) {
      ENVOY_LOG(debug, "end stream. no body");
      return 0;
    } else {
      Envoy::Extensions::Filters::Common::Lua::BufferWrapper::create(
          state, *headers_, const_cast<Buffer::Instance&>(*filter_callbacks_.bufferedBody()));
      return 1;
    }
  } else if (saw_body_) {
    return luaL_error(state, "cannot call body() after body streaming has started");
  } else {
    ENVOY_LOG(debug, "yielding for full body");
    state_ = State::WaitForBody;
    buffered_body_ = true;
    return lua_yield(state, 0);
  }
}

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
