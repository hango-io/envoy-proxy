#include "source/filters/http/rider/vm.h"

#include <unordered_map>

#include "source/extensions/filters/common/lua/wrappers.h"
#include "source/filters/http/rider/capi.h"
#include "source/filters/http/rider/context.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

thread_local std::unordered_map<std::string, std::weak_ptr<LuaVirtualMachine>>
    local_lua_virtual_machines;

static void initializeCallInAPIs(lua_State* state) {
  LuaStackUnchange stack_unchange(state);

  // envoy.* APIs will accessed through this table.
  lua_createtable(state, 0, 20);

  // APIs for envoy.req.*
  lua_createtable(state, 0, 8);
  lua_pushcfunction(state, static_lua_get_request_body);
  lua_setfield(state, -2, "get_body");
  lua_setfield(state, -2, "req");

  // APIs for envoy.resp.*
  lua_createtable(state, 0, 4);
  lua_pushcfunction(state, static_lua_get_response_body);
  lua_setfield(state, -2, "get_body");
  lua_setfield(state, -2, "resp");

  // APIs for envoy.runtime.*
  lua_createtable(state, 0, 4);
  lua_setfield(state, -2, "runtime");

  // APIs for envoy.streaminfo.*
  lua_createtable(state, 0, 8);
  lua_setfield(state, -2, "streaminfo");

  // APIs for envoy.stream.*
  lua_createtable(state, 0, 8);
  lua_setfield(state, -2, "stream");

  // envoy.respond
  lua_pushcfunction(state, static_lua_respond);
  lua_setfield(state, -2, "respond");

  // envoy.httpCall
  lua_pushcfunction(state, static_lua_http_call);
  lua_setfield(state, -2, "httpCall");

  lua_getglobal(state, "package");
  lua_getfield(state, -1, "loaded");
  lua_pushvalue(state, -3);
  lua_setfield(state, -2, "envoy");
  lua_pop(state, 2);

  lua_setglobal(state, "envoy");
}

LuaVirtualMachine::LuaVirtualMachine(const std::string& vm_id, const std::string& package_path)
    : vm_id_(vm_id), state_(lua_open()) {
  lua_State* state = state_.get();
  luaL_openlibs(state);

  if (!package_path.empty() && !LuaUtils::addLuaPackagePath(state, package_path)) {
    // TODO: output more details
    throw LuaException(fmt::format("failed to add package path {}", package_path));
  }

  registerType<Envoy::Extensions::Filters::Common::Lua::BufferWrapper>();

  initializeCallInAPIs(state);
}

PluginHandleSharedPtr LuaVirtualMachine::startPlugin(PluginSharedPtr plugin) {
  auto lua_thread = std::make_shared<LuaThread>(this->shared_from_this());
  // TODO(Tong Cai): check emplace result.
  auto it = root_contexts_.emplace(plugin->key(),
                                   std::make_unique<ContextBase>(lua_thread.get(), plugin));

  auto plugin_handle = lua_thread->startPlugin(plugin);

  it.first->second->onConfigure(*plugin_handle);
  return plugin_handle;
}

template <class T> void LuaVirtualMachine::registerType() { T::registerType(luaState()); }

LuaThread::LuaThread(LuaVirtualMachineSharedPtr parent) : parent_(parent) {
  lua_State* new_state = lua_newthread(parent->luaState());
  if (!new_state) {
    throw LuaException("lua_newthread error");
  }
  state_ref_.reset(std::make_pair(new_state, parent->luaState()), false);
  LuaUtils::setupSandbox(new_state);
}

PluginHandleSharedPtr LuaThread::startPlugin(PluginSharedPtr plugin) {
  auto plugin_handle = std::make_shared<PluginHandle>(this->shared_from_this(), plugin);
  lua_State* state = luaState();

  int rc = luaL_dostring(state, plugin->code().c_str());
  if (rc) {
    LuaUtils::stackDump(state, rc);
    throw LuaException(fmt::format("load lua script error: {}", rc));
  }

  if (!lua_istable(state, 1)) {
    LuaUtils::stackDump(state);
    throw LuaException(fmt::format("expect lua script to return a table"));
  }

  LuaStackUnchange stackUnchange(0, state);

  // TODO(Tong Cai): refactor.

  lua_getfield(state, -1, "version");
  size_t version_size;
  const char* raw_version = luaL_optlstring(state, -1, nullptr, &version_size);
  if (raw_version != nullptr) {
    std::string version(raw_version, version_size);
    if (version == "v1") {
      plugin_handle->version() = Version::v1;
    } else if (version == "v2") {
      plugin_handle->version() = Version::v2;
    } else {
      plugin_handle->version() = Version::v1;
    }
  } else {
    plugin_handle->version() = Version::v1;
  }
  lua_pop(state, 1);

  lua_getfield(state, -1, "on_configure");
  if (lua_isfunction(state, -1)) {
    plugin_handle->onConfigureRef() = luaL_ref(state, LUA_REGISTRYINDEX);
  } else if (lua_isnil(state, -1)) {
    plugin_handle->onConfigureRef() = LUA_NOREF;
    lua_pop(state, 1);
  } else {
    lua_pop(state, 1);
  }

  if (raw_version != nullptr) {
    lua_getfield(state, -1, "on_request_header");
    if (lua_isfunction(state, -1)) {
      plugin_handle->onRequestHeaderRef() = luaL_ref(state, LUA_REGISTRYINDEX);
    } else if (lua_isnil(state, -1)) {
      plugin_handle->onRequestHeaderRef() = LUA_NOREF;
      lua_pop(state, 1);
    } else {
      lua_pop(state, 1);
    }

    lua_getfield(state, -1, "on_request_body");
    if (lua_isfunction(state, -1)) {
      plugin_handle->onRequestBodyRef() = luaL_ref(state, LUA_REGISTRYINDEX);
    } else if (lua_isnil(state, -1)) {
      plugin_handle->onRequestBodyRef() = LUA_NOREF;
      lua_pop(state, 1);
    } else {
      lua_pop(state, 1);
    }

    lua_getfield(state, -1, "on_response_header");
    if (lua_isfunction(state, -1)) {
      plugin_handle->onResponseHeaderRef() = luaL_ref(state, LUA_REGISTRYINDEX);
    } else if (lua_isnil(state, -1)) {
      plugin_handle->onResponseHeaderRef() = LUA_NOREF;
      lua_pop(state, 1);
    } else {
      lua_pop(state, 1);
    }

    lua_getfield(state, -1, "on_response_body");
    if (lua_isfunction(state, -1)) {
      plugin_handle->onResponseBodyRef() = luaL_ref(state, LUA_REGISTRYINDEX);
    } else if (lua_isnil(state, -1)) {
      plugin_handle->onResponseBodyRef() = LUA_NOREF;
      lua_pop(state, 1);
    } else {
      lua_pop(state, 1);
    }
  } else {
    lua_getfield(state, -1, "on_request");
    if (lua_isfunction(state, -1)) {
      plugin_handle->onRequestRef() = luaL_ref(state, LUA_REGISTRYINDEX);
    } else if (lua_isnil(state, -1)) {
      plugin_handle->onRequestRef() = LUA_NOREF;
      lua_pop(state, 1);
    } else {
      lua_pop(state, 1);
    }

    lua_getfield(state, -1, "on_response");
    if (lua_isfunction(state, -1)) {
      plugin_handle->onResponseRef() = luaL_ref(state, LUA_REGISTRYINDEX);
    } else if (lua_isnil(state, -1)) {
      plugin_handle->onResponseRef() = LUA_NOREF;
      lua_pop(state, 1);
    } else {
      lua_pop(state, 1);
    }
  }

  lua_pop(state, 1);
  return plugin_handle;
}

Coroutine::Coroutine(lua_State* state, lua_State* parent_state)
    : coroutine_state_(std::make_pair(state, parent_state), false) {}

void Coroutine::initialize(ContextBase* context, StreamDirection direction) {
  ASSERT(context);

  lua_State* state = coroutine_state_.get();

  LuaUtils::setContext(state, context);

  lua_pushinteger(state, static_cast<int>(direction));
  lua_setglobal(state, lua_global_stream_direction.c_str());
}

void Coroutine::start(int function_ref, const std::function<void()>& yield_callback) {
  ASSERT(status_ == Status::NotStarted);

  lua_State* state = coroutine_state_.get();

  lua_rawgeti(state, LUA_REGISTRYINDEX, function_ref);
  if (!lua_isfunction(state, -1)) {
    throw LuaException(fmt::format("error start coroutine, expect lua function"));
  }

  status_ = Status::Yielded;
  resume(0, yield_callback);
}

void Coroutine::resume(int num_args, const std::function<void()>& yield_callback) {
  ASSERT(status_ == Status::Yielded);
  ENVOY_LOG(trace, "coroutine resume");

  lua_State* state = coroutine_state_.get();

  int rc = lua_resume(state, num_args);

  if (0 == rc) {
    status_ = Status::Finished;
    ENVOY_LOG(debug, "coroutine finished");
  } else if (LUA_YIELD == rc) {
    status_ = Status::Yielded;
    ENVOY_LOG(debug, "coroutine yielded");
    yield_callback();
  } else {
    status_ = Status::Finished;

    // TODO(Tong Cai): Protective programming. We have saw a core dump caused by
    // LuaException(nullptr). I still don't known in what circumstances lua_resume returns error
    // without leave a error message on stack.
    if (!lua_isstring(state, -1)) {
      throw LuaException(fmt::format("runtime error({})", rc));
    }

    throw LuaException(
        fmt::format("runtime error: {}({})", lua_tostring(coroutine_state_.get(), -1), rc));
  }
}

/**
 * createCoroutine will create a sanboxed lua coroutine. Then get the target function from
 * exports table onto coroutine's stack.
 */
std::unique_ptr<Coroutine> LuaThread::createCoroutine() {
  lua_State* coroutine_state = lua_newthread(state_ref_.get());
  if (!coroutine_state) {
    throw LuaException("lua_newthread failed");
  }

  auto co = std::make_unique<Coroutine>(coroutine_state, state_ref_.get());
  LuaUtils::setupSandbox(coroutine_state);

  return co;
}

LuaVirtualMachineSharedPtr getOrCreateThreadLocalLuaVM(const std::string& vm_id,
                                                       const std::string& package_path) {
  auto it = local_lua_virtual_machines.find(vm_id);
  if (it != local_lua_virtual_machines.end()) {
    auto vm = it->second.lock();
    if (vm) {
      return vm;
    }

    local_lua_virtual_machines.erase(vm_id);
  }

  auto vm = std::make_shared<LuaVirtualMachine>(vm_id, package_path);
  local_lua_virtual_machines[vm_id] = vm;
  return vm;
}

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
