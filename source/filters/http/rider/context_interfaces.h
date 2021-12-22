#pragma once

#include <vector>

#include "api/proxy/filters/http/rider/v3alpha1/rider.pb.h"
#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"

#include "common/common/c_smart_ptr.h"
#include "common/common/lock_guard.h"
#include "common/common/thread.h"
#include "common/http/utility.h"

#include "filters/http/rider/common.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

class Plugin;

/**
 * When TLS plugin config is created, we getOrCreate a root context(and create a root
 * Lua thread)
 *
 *
 *
 *
 *
 *
 */

/**
 * Cache configuration in Lua:
 *
 * Lua thread that
 *
 *
 */

class PluginHandle;

/**
 * RootInterface is the interface specific to RootContexts.
 * A RootContext is associated with one more more plugins and is the parent of all stream Context(s)
 * created for that plugin.
 */
class RootInterface {
public:
  virtual ~RootInterface() = default;

  virtual void onConfigure(PluginHandle& plugin_handle) PURE;

  virtual int getConfiguration(envoy_lua_ffi_str_t* buffer) PURE;
};

class GeneralInterface {
public:
  virtual ~GeneralInterface() = default;
  /**
   * error is for Lua plugin to notify host that there is a error, rest of the code
   * should be skipped.
   */
  virtual void error(const char* message) PURE;
  virtual void log(spdlog::level::level_enum level, const char* message) PURE;
  virtual uint32_t getLogLevel() PURE;
  virtual uint64_t getCurrentTimeMilliseconds() PURE;
};

class StreamInterface {
public:
  virtual ~StreamInterface() = default;

  virtual void fileLog(const char* buf, int len) PURE;

  virtual int getRouteConfiguration(envoy_lua_ffi_str_t* buffer) PURE;

  virtual uint64_t getRouteConfigHash() PURE;

  /**
   * @return reference to the context table
   *
   * Lua reference system: https://www.lua.org/pil/27.3.2.html
   */
  virtual int getOrCreateSharedTable() PURE;

  virtual int luaBody(lua_State* state, StreamDirection direction) PURE;
  virtual int luaHttpCall(lua_State* state, StreamDirection direction) PURE;
  virtual int luaRespond(lua_State* state, StreamDirection direction) PURE;
};

class StreamInfoInterface {
public:
  virtual ~StreamInfoInterface() = default;

  virtual const char* upstreamHost() PURE;
  virtual const char* upstreamCluster() PURE;
  virtual const char* downstreamLocalAddress() PURE;
  virtual const char* downstreamRemoteAddress() PURE;
  virtual int64_t startTime() PURE;
  // TODO: getBodyBytes(LuaStreamOpSourceType type)
};

class HeaderInterface {
public:
  virtual ~HeaderInterface() = default;

  virtual int getHeaderMapSize(LuaStreamOpSourceType type) PURE;
  virtual int getHeaderMap(LuaStreamOpSourceType type, envoy_lua_ffi_string_pairs* buf) PURE;
  virtual int getHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key,
                                envoy_lua_ffi_str_t* value) PURE;
  virtual int getHeaderMapValueSize(LuaStreamOpSourceType type, absl::string_view key) PURE;
  virtual int getHeaderMapValueIndex(LuaStreamOpSourceType type, absl::string_view key,
                                     envoy_lua_ffi_str_t* value, int index) PURE;
  virtual int setHeaderMap(LuaStreamOpSourceType type, envoy_lua_ffi_string_pairs* buf) PURE;
  virtual int setHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key,
                                absl::string_view value) PURE;
  virtual int removeHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key) PURE;

  virtual int getQueryParameters(envoy_lua_ffi_string_pairs* buf) PURE;
};

class FilterMetadataInterface {
public:
  virtual ~FilterMetadataInterface() = default;
  virtual int getMetadataValue(envoy_lua_ffi_str_t* filter_name, envoy_lua_ffi_str_t* key,
                               envoy_lua_ffi_str_t* value) PURE;
  virtual int getDynamicMetadataValue(envoy_lua_ffi_str_t* filter_name, envoy_lua_ffi_str_t* key,
                                      envoy_lua_ffi_str_t* value) PURE;
};

class BodyInterface {
public:
  virtual ~BodyInterface() = default;
  virtual int getBody(LuaStreamOpSourceType type, envoy_lua_ffi_str_t* body) PURE;
};

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
