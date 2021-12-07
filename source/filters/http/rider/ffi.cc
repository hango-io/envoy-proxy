#include "common/common/assert.h"

#include "filters/http/rider/common.h"
#include "filters/http/rider/context.h"
#include "filters/http/rider/filter.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

extern "C" {

// TODO(Tong Cai): we should add api version to function signature.
#define FFI_FUNC(Name) envoy_http_lua_ffi_##Name
#define FFI_FUNC_V2(Name) envoy_http_lua_ffi_v2_##Name

/* General Interface */
int FFI_FUNC(log)(ContextBase* context, int level, const char* message) {
  ASSERT(context);
  context->log(static_cast<spdlog::level::level_enum>(level), message);
  return 0;
}

uint64_t FFI_FUNC(get_current_time_milliseconds)(ContextBase* context) {
  ASSERT(context);
  return context->getCurrentTimeMilliseconds();
}

/* Root Interface */
int FFI_FUNC(get_configuration)(ContextBase* context, envoy_lua_ffi_str_t* buffer) {
  ASSERT(context);
  return context->getConfiguration(buffer);
}

/* Stream Interface */
void FFI_FUNC(file_log)(ContextBase* context, const char* buf, int len) {
  ASSERT(context);
  return context->fileLog(buf, len);
}

int FFI_FUNC(get_shared_table)(ContextBase* context) {
  ASSERT(context);
  return context->getOrCreateSharedTable();
}

int FFI_FUNC(get_route_configuration)(ContextBase* context, envoy_lua_ffi_str_t* buffer) {
  ASSERT(context);
  return context->getRouteConfiguration(buffer);
}

uint64_t FFI_FUNC(get_route_config_hash)(ContextBase* context) {
  ASSERT(context);
  return context->getRouteConfigHash();
}

/* Header Interface */

int FFI_FUNC(get_header_map_size)(ContextBase* context, LuaStreamOpSourceType source) {
  ASSERT(context);
  return context->getHeaderMapSize(source);
}

int FFI_FUNC(get_header_map)(ContextBase* context, LuaStreamOpSourceType source,
                             envoy_lua_ffi_string_pairs* buffer) {
  ASSERT(context);
  return context->getHeaderMap(source, buffer);
}

int FFI_FUNC(get_header_map_value)(ContextBase* context, LuaStreamOpSourceType source,
                                   const char* key, int key_len, envoy_lua_ffi_str_t* value) {
  ASSERT(context);
  int rc = context->getHeaderMapValue(source, absl::string_view(key, key_len), value);
  return rc;
}

int FFI_FUNC(set_header_map_value)(ContextBase* context, LuaStreamOpSourceType source,
                                   const char* key, int key_len, const char* value, int value_len) {
  ASSERT(context);
  return context->setHeaderMapValue(source, absl::string_view(key, key_len),
                                    absl::string_view(value, value_len));
}

int FFI_FUNC(remove_header_map_value)(ContextBase* context, LuaStreamOpSourceType source,
                                      const char* key, int key_len) {
  ASSERT(context);
  return context->removeHeaderMapValue(source, absl::string_view(key, key_len));
}

int FFI_FUNC(get_query_parameters)(ContextBase* context, envoy_lua_ffi_string_pairs* buf) {
  ASSERT(context);
  return context->getQueryParameters(buf);
}

/* StreamInfo Interface */
const char* FFI_FUNC(upstream_host)(ContextBase* context) {
  ASSERT(context);
  return context->upstreamHost();
}

const char* FFI_FUNC(upstream_cluster)(ContextBase* context) {
  ASSERT(context);
  return context->upstreamCluster();
}

const char* FFI_FUNC(downstream_local_address)(ContextBase* context) {
  ASSERT(context);
  return context->downstreamLocalAddress();
}

const char* FFI_FUNC(downstream_remote_address)(ContextBase* context) {
  ASSERT(context);
  return context->downstreamRemoteAddress();
}

int64_t FFI_FUNC(start_time)(ContextBase* context) {
  ASSERT(context);
  return context->startTime();
}

/* FilterMetadata Interface */
int FFI_FUNC(get_metadata)(ContextBase* context, envoy_lua_ffi_str_t* filter_name,
                           envoy_lua_ffi_str_t* key, envoy_lua_ffi_str_t* value) {
  ASSERT(context);
  return context->getMetadataValue(filter_name, key, value);
}

// FFI v2

/* General Interface */
int FFI_FUNC_V2(log)(int level, const char* message) {
  ASSERT(global_ctx);
  global_ctx->log(static_cast<spdlog::level::level_enum>(level), message);
  return 0;
}

uint64_t FFI_FUNC_V2(get_current_time_milliseconds)() {
  ASSERT(global_ctx);
  return global_ctx->getCurrentTimeMilliseconds();
}

/* Root Interface */
int FFI_FUNC_V2(get_configuration)(envoy_lua_ffi_str_t* buffer) {
  ASSERT(global_ctx);
  return global_ctx->getConfiguration(buffer);
}

/* Stream Interface */
void FFI_FUNC_V2(file_log)(const char* buf, int len) {
  ASSERT(global_ctx);
  return global_ctx->fileLog(buf, len);
}

int FFI_FUNC_V2(get_shared_table)() {
  ASSERT(global_ctx);
  return global_ctx->getOrCreateSharedTable();
}

int FFI_FUNC_V2(get_route_configuration)(envoy_lua_ffi_str_t* buffer) {
  ASSERT(global_ctx);
  return global_ctx->getRouteConfiguration(buffer);
}

uint64_t FFI_FUNC_V2(get_route_config_hash)() {
  ASSERT(global_ctx);
  return global_ctx->getRouteConfigHash();
}

/* Header Interface */

int FFI_FUNC_V2(get_header_map_size)(LuaStreamOpSourceType source) {
  ASSERT(global_ctx);
  return global_ctx->getHeaderMapSize(source);
}

int FFI_FUNC_V2(get_header_map)(LuaStreamOpSourceType source, envoy_lua_ffi_string_pairs* buffer) {
  ASSERT(global_ctx);
  return global_ctx->getHeaderMap(source, buffer);
}

int FFI_FUNC_V2(get_header_map_value)(LuaStreamOpSourceType source, const char* key, int key_len,
                                      envoy_lua_ffi_str_t* value) {
  ASSERT(global_ctx);
  int rc = global_ctx->getHeaderMapValue(source, absl::string_view(key, key_len), value);
  return rc;
}

int FFI_FUNC_V2(set_header_map)(LuaStreamOpSourceType source, envoy_lua_ffi_string_pairs* buffer) {
  ASSERT(global_ctx);
  int rc = global_ctx->setHeaderMap(source, buffer);
  return rc;
}


int FFI_FUNC_V2(set_header_map_value)(LuaStreamOpSourceType source, const char* key, int key_len,
                                      const char* value, int value_len) {
  ASSERT(global_ctx);
  return global_ctx->setHeaderMapValue(source, absl::string_view(key, key_len),
                                       absl::string_view(value, value_len));
}

int FFI_FUNC_V2(remove_header_map_value)(LuaStreamOpSourceType source, const char* key,
                                         int key_len) {
  ASSERT(global_ctx);
  return global_ctx->removeHeaderMapValue(source, absl::string_view(key, key_len));
}

int FFI_FUNC_V2(get_query_parameters)(envoy_lua_ffi_string_pairs* buf) {
  ASSERT(global_ctx);
  return global_ctx->getQueryParameters(buf);
}

/* StreamInfo Interface */
const char* FFI_FUNC_V2(upstream_host)() {
  ASSERT(global_ctx);
  return global_ctx->upstreamHost();
}

const char* FFI_FUNC_V2(upstream_cluster)() {
  ASSERT(global_ctx);
  return global_ctx->upstreamCluster();
}

const char* FFI_FUNC_V2(downstream_local_address)() {
  ASSERT(global_ctx);
  return global_ctx->downstreamLocalAddress();
}

const char* FFI_FUNC_V2(downstream_remote_address)() {
  ASSERT(global_ctx);
  return global_ctx->downstreamRemoteAddress();
}

int64_t FFI_FUNC_V2(start_time)() {
  ASSERT(global_ctx);
  return global_ctx->startTime();
}

/* FilterMetadata Interface */
int FFI_FUNC_V2(get_metadata)(envoy_lua_ffi_str_t* filter_name, envoy_lua_ffi_str_t* key,
                              envoy_lua_ffi_str_t* value) {
  ASSERT(global_ctx);
  return global_ctx->getMetadataValue(filter_name, key, value);
}

/* Body Interface */
int FFI_FUNC_V2(get_body)(LuaStreamOpSourceType source, envoy_lua_ffi_str_t* body) {
  ASSERT(global_ctx);
  return global_ctx->getBody(source, body);
}

#undef FFI_FUNC
}

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
