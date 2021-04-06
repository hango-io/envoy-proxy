#include "filters/http/rider/context.h"
#include "filters/http/rider/filter.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

PluginHandle::PluginHandle(std::shared_ptr<LuaVirtualMachine> vm,
                           std::shared_ptr<Plugin> plugin)
    : vm_(vm), plugin_(plugin) {}

PluginHandle::~PluginHandle() {
  if (!vm_.get()) {
    return;
  }

  lua_State *state = vm_.get()->luaState();
  if (on_configure_ref_ != LUA_NOREF) {
    luaL_unref(state, LUA_REGISTRYINDEX, on_configure_ref_);
  }
  if (on_request_ref_ != LUA_NOREF) {
    luaL_unref(state, LUA_REGISTRYINDEX, on_request_ref_);
  }
  if (on_response_ref_ != LUA_NOREF) {
    luaL_unref(state, LUA_REGISTRYINDEX, on_response_ref_);
  }
}

ContextBase::ContextBase(LuaVirtualMachine *vm, PluginSharedPtr plugin)
    : vm_(vm), plugin_(plugin) {
  LuaUtils::setContext(vm->luaState(), this);
}

ContextBase::ContextBase(Filter *filter, LuaVirtualMachine *vm,
                         PluginSharedPtr plugin)
    : vm_(vm), plugin_(plugin) {
  LuaUtils::setContext(vm->luaState(), dynamic_cast<ContextBase *>(filter));
}

void ContextBase::onConfigure(PluginHandle &plugin_handle) {
  ASSERT(vm_);

  ENVOY_LOG(debug, "plugin {} on configure", plugin_->name());

  lua_State *state = vm_->luaState();
  int saved_top = lua_gettop(state);
  int ref = plugin_handle.onConfigureRef();
  if (ref == LUA_NOREF) {
    return;
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
  // TODO(Tong Cai): handle lua_pcall results/errors in a common function.
  int rc = lua_pcall(state, 0, 1, 0);
  if (rc) {
    LuaUtils::stackDump(state, rc);
    throw LuaException(fmt::format("lua pcall error: {}", rc));
  }

  lua_settop(state, saved_top);
}

int ContextBase::getConfiguration(envoy_lua_ffi_str_t *buffer) {
  if (!buffer) {
    return static_cast<int>(FFIReturnCode::BadArgument);
  }

  if (plugin_.get()) {
    buffer->data = plugin_->configuration().data();
    buffer->len = plugin_->configuration().length();
    return static_cast<int>(FFIReturnCode::Success);
  }

  return static_cast<int>(FFIReturnCode::NotFound);
}

void ContextBase::log(spdlog::level::level_enum level, const char *message) {
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

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
