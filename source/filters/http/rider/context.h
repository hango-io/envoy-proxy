#pragma once

#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"
#include "envoy/thread_local/thread_local.h"

#include "common/common/c_smart_ptr.h"
#include "common/common/lock_guard.h"
#include "common/common/thread.h"
#include "common/http/utility.h"
#include "common/singleton/const_singleton.h"

#include "filters/http/rider/common.h"
#include "filters/http/rider/context_interfaces.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

class LuaVirtualMachine;
class Filter;

/**
 * Plugin is first constructed when LDS init to load data and validate things.
 * Plugin is assigned to a Lua VM when filter config construct.
 */
class Plugin {
public:
  // std::string json;
  Plugin(const std::string& name, const std::string& vm_id, const std::string& code,
         const std::string& configuration)
      : name_(name), vm_id_(vm_id), code_(code), configuration_(configuration),
        key_(name + "||" + configuration) {}

  const std::string& name() const { return name_; }
  const std::string& key() const { return key_; }
  const std::string& code() const { return code_; }
  const std::string& configuration() const { return configuration_; }

private:
  const std::string name_;
  const std::string vm_id_;
  const std::string code_;
  const std::string configuration_;
  std::string log_prefix_;
  std::string key_;
};

using PluginSharedPtr = std::shared_ptr<Plugin>;

// Function names exported by plugin.
class PluginExportFunctionNameValues {
public:
  const std::string OnConfigure = "on_configure";
  const std::string OnRequest = "on_request";
  const std::string OnResponse = "on_response";
};
typedef ConstSingleton<PluginExportFunctionNameValues> PluginExportFunctionNames;

class PluginHandle : public ThreadLocal::ThreadLocalObject {
public:
  PluginHandle(std::shared_ptr<LuaVirtualMachine> vm, std::shared_ptr<Plugin> plugin);

  ~PluginHandle();

  LuaVirtualMachine* vm() { return vm_.get(); }

  const Plugin* plugin() const { return plugin_.get(); }

  int& onConfigureRef() { return on_configure_ref_; }
  int& onRequestRef() { return on_request_ref_; }
  int& onResponseRef() { return on_response_ref_; }

  int& onRequestHeaderRef() { return on_request_header_ref_; }
  int& onRequestBodyRef() { return on_request_body_ref_; }
  int& onResponseHeaderRef() { return on_response_header_ref_; }
  int& onResponseBodyRef() { return on_response_body_ref_; }

  std::string& version() { return version_; }

private:
  std::shared_ptr<LuaVirtualMachine> vm_;
  std::shared_ptr<Plugin> plugin_;

  int on_configure_ref_{LUA_NOREF};
  int on_request_ref_{LUA_NOREF};
  int on_response_ref_{LUA_NOREF};

  int on_request_header_ref_{LUA_NOREF};
  int on_request_body_ref_{LUA_NOREF};
  int on_response_header_ref_{LUA_NOREF};
  int on_response_body_ref_{LUA_NOREF};
  std::string version_;
};

using PluginHandleSharedPtr = std::shared_ptr<PluginHandle>;

/***
 * ContextBase contains a collection of interfaces for CallIn(host->lua) and CallOut(lua->host).
 * When a plugin is loaded, a new context will be assigned to it. Filter implements context
 * interfaces to handle requests.
 */
class ContextBase : public RootInterface,
                    public GeneralInterface,
                    public StreamInterface,
                    public StreamInfoInterface,
                    public HeaderInterface,
                    public FilterMetadataInterface,
                    public BodyInterface,
                    public Logger::Loggable<Logger::Id::lua> {
public:
  ContextBase() {}

  // Root Context
  ContextBase(LuaVirtualMachine* vm, PluginSharedPtr plugin);

  // Stream Context
  ContextBase(Filter* filter, LuaVirtualMachine* vm, PluginSharedPtr plugin);

  // RootInterface
  void onConfigure(PluginHandle& plugin_handle) override;
  int getConfiguration(envoy_lua_ffi_str_t* buffer) override;

  // GeneralInterface
  void error(const char* message) override {
    UNREFERENCED_PARAMETER(message);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  void log(spdlog::level::level_enum level, const char* message) override;
  uint32_t getLogLevel() override { return static_cast<uint32_t>(ENVOY_LOGGER().level()); }
  uint64_t getCurrentTimeMilliseconds() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  // StreamInterface
  void fileLog(const char* buf, int len) override {
    UNREFERENCED_PARAMETER(buf);
    UNREFERENCED_PARAMETER(len);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int getOrCreateSharedTable() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  int getRouteConfiguration(envoy_lua_ffi_str_t* buffer) override {
    UNREFERENCED_PARAMETER(buffer);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  uint64_t getRouteConfigHash() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  int luaBody(lua_State* state, StreamDirection direction) override {
    UNREFERENCED_PARAMETER(state);
    UNREFERENCED_PARAMETER(direction);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int luaHttpCall(lua_State* state, StreamDirection direction) override {
    UNREFERENCED_PARAMETER(state);
    UNREFERENCED_PARAMETER(direction);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int luaRespond(lua_State* state, StreamDirection direction) override {
    UNREFERENCED_PARAMETER(state);
    UNREFERENCED_PARAMETER(direction);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

  // StreamInfoInterface
  const char* upstreamHost() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  const char* upstreamCluster() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  const char* downstreamLocalAddress() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  const char* downstreamRemoteAddress() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }
  int64_t startTime() override { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; }

  // HeaderInterface
  int getHeaderMapSize(LuaStreamOpSourceType type) override {
    UNREFERENCED_PARAMETER(type);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int getHeaderMap(LuaStreamOpSourceType type, envoy_lua_ffi_string_pairs* buf) override {
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(buf);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int getHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key,
                        envoy_lua_ffi_str_t* value) override {
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(key);
    UNREFERENCED_PARAMETER(value);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int getHeaderMapValueSize(LuaStreamOpSourceType type, absl::string_view key) override {
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(key);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int getHeaderMapValueIndex(LuaStreamOpSourceType type, absl::string_view key,
                             envoy_lua_ffi_str_t* value, int index) override {
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(key);
    UNREFERENCED_PARAMETER(value);
    UNREFERENCED_PARAMETER(index);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int setHeaderMap(LuaStreamOpSourceType type, envoy_lua_ffi_string_pairs* buf) override {
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(buf);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int setHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key,
                        absl::string_view value) override {
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(key);
    UNREFERENCED_PARAMETER(value);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }
  int removeHeaderMapValue(LuaStreamOpSourceType type, absl::string_view key) override {
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(key);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

  int getQueryParameters(envoy_lua_ffi_string_pairs* buf) override {
    UNREFERENCED_PARAMETER(buf);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

  // FilterMetadataInterface
  int getMetadataValue(envoy_lua_ffi_str_t* filter_name, envoy_lua_ffi_str_t* key,
                       envoy_lua_ffi_str_t* value) override {
    UNREFERENCED_PARAMETER(filter_name);
    UNREFERENCED_PARAMETER(key);
    UNREFERENCED_PARAMETER(value);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

  int getDynamicMetadataValue(envoy_lua_ffi_str_t* filter_name, envoy_lua_ffi_str_t* key,
                              envoy_lua_ffi_str_t* value) override {
    UNREFERENCED_PARAMETER(filter_name);
    UNREFERENCED_PARAMETER(key);
    UNREFERENCED_PARAMETER(value);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

  // BodyInterface
  int getBody(LuaStreamOpSourceType type, envoy_lua_ffi_str_t* body) override {
    UNREFERENCED_PARAMETER(type);
    UNREFERENCED_PARAMETER(body);
    NOT_IMPLEMENTED_GCOVR_EXCL_LINE;
  }

private:
  LuaVirtualMachine* vm_{};
  std::shared_ptr<Plugin> plugin_;
};

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
