#include "common/common/assert.h"

#include "filters/http/rider/common.h"
#include "filters/http/rider/context.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

extern "C" {

// TODO(Tong Cai): we should add api version to function signature.
#define FFI_FUNC(Name) envoy_http_lua_ffi_##Name

/* General Interface */
int FFI_FUNC(log)(ContextBase *context, int level, const char *message) {
  ASSERT(context);
  context->log(static_cast<spdlog::level::level_enum>(level), message);
  return 0;
}

uint64_t FFI_FUNC(get_current_time_milliseconds)(ContextBase *context) {
  ASSERT(context);
  return context->getCurrentTimeMilliseconds();
}

/* Root Interface */
int FFI_FUNC(get_configuration)(ContextBase *context,
                                envoy_lua_ffi_str_t *buffer) {
  ASSERT(context);
  return context->getConfiguration(buffer);
}

/* Stream Interface */
void FFI_FUNC(file_log)(ContextBase *context, const char *buf, int len) {
  ASSERT(context);
  return context->fileLog(buf, len);
}

int FFI_FUNC(get_shared_table)(ContextBase *context) {
  ASSERT(context);
  return context->getOrCreateSharedTable();
}

int FFI_FUNC(get_route_configuration)(ContextBase *context,
                                      envoy_lua_ffi_str_t *buffer) {
  ASSERT(context);
  return context->getRouteConfiguration(buffer);
}

uint64_t FFI_FUNC(get_route_config_hash)(ContextBase *context) {
  ASSERT(context);
  return context->getRouteConfigHash();
}

/* Header Interface */

int FFI_FUNC(get_header_map_size)(ContextBase *context,
                                  LuaStreamOpSourceType source) {
  ASSERT(context);
  return context->getHeaderMapSize(source);
}

int FFI_FUNC(get_header_map)(ContextBase *context, LuaStreamOpSourceType source,
                             envoy_lua_ffi_string_pairs *buffer) {
  ASSERT(context);
  return context->getHeaderMap(source, buffer);
}

int FFI_FUNC(get_header_map_value)(ContextBase *context,
                                   LuaStreamOpSourceType source,
                                   const char *key, int key_len,
                                   envoy_lua_ffi_str_t *value) {
  ASSERT(context);
  int rc = context->getHeaderMapValue(source, absl::string_view(key, key_len),
                                      value);
  return rc;
}

int FFI_FUNC(set_header_map_value)(ContextBase *context,
                                   LuaStreamOpSourceType source,
                                   const char *key, int key_len,
                                   const char *value, int value_len) {
  ASSERT(context);
  return context->setHeaderMapValue(source, absl::string_view(key, key_len),
                                    absl::string_view(value, value_len));
}

int FFI_FUNC(remove_header_map_value)(ContextBase *context,
                                      LuaStreamOpSourceType source,
                                      const char *key, int key_len) {
  ASSERT(context);
  return context->removeHeaderMapValue(source, absl::string_view(key, key_len));
}

int FFI_FUNC(get_query_parameters)(ContextBase *context,
                                   envoy_lua_ffi_string_pairs *buf) {
  ASSERT(context);
  return context->getQueryParameters(buf);
}

/* StreamInfo Interface */
const char *FFI_FUNC(upstream_host)(ContextBase *context) {
  ASSERT(context);
  return context->upstreamHost();
}

const char *FFI_FUNC(upstream_cluster)(ContextBase *context) {
  ASSERT(context);
  return context->upstreamCluster();
}

const char *FFI_FUNC(downstream_local_address)(ContextBase *context) {
  ASSERT(context);
  return context->downstreamLocalAddress();
}

const char *FFI_FUNC(downstream_remote_address)(ContextBase *context) {
  ASSERT(context);
  return context->downstreamRemoteAddress();
}

int64_t FFI_FUNC(start_time)(ContextBase *context) {
  ASSERT(context);
  return context->startTime();
}

#undef FFI_FUNC
}

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
