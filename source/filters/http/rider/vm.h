#pragma once

#include <unordered_map>

#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"

#include "source/common/common/assert.h"
#include "source/common/common/c_smart_ptr.h"
#include "source/common/common/lock_guard.h"
#include "source/common/common/thread.h"
#include "source/common/http/utility.h"
#include "source/extensions/filters/common/lua/lua.h"
#include "source/filters/http/rider/common.h"
#include "source/filters/http/rider/context.h"

#include "api/proxy/filters/http/rider/v3alpha1/rider.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

class Coroutine;

// LuaThreadRef is referrence to a lua thread.
using LuaThreadRef = Envoy::Extensions::Filters::Common::Lua::LuaRef<lua_State>;

class LuaVirtualMachine : public std::enable_shared_from_this<LuaVirtualMachine>,
                          public Logger::Loggable<Logger::Id::lua> {
public:
  LuaVirtualMachine() = default;
  LuaVirtualMachine(const std::string& vm_id, const std::string& package_path);
  virtual ~LuaVirtualMachine(){};

  virtual std::string vm_id() const { return vm_id_; }

  virtual std::unique_ptr<Coroutine> createCoroutine() { NOT_IMPLEMENTED_GCOVR_EXCL_LINE; };

  virtual lua_State* luaState() { return state_.get(); }

  virtual PluginHandleSharedPtr startPlugin(PluginSharedPtr plugin);

private:
  template <class T> void registerType();

  const std::string vm_id_;
  CSmartPtr<lua_State, lua_close> state_;
};

using LuaVirtualMachineSharedPtr = std::shared_ptr<LuaVirtualMachine>;

class LuaThread : public LuaVirtualMachine {
public:
  LuaThread(LuaVirtualMachineSharedPtr parent);

  PluginHandleSharedPtr startPluginHandle(PluginSharedPtr plugin,
                                          std::shared_ptr<ContextBase> context_base);

  std::unique_ptr<Coroutine> createCoroutine() override;

  lua_State* luaState() override { return state_ref_.get(); }

private:
  LuaVirtualMachineSharedPtr parent_;
  LuaThreadRef state_ref_;
};

class Coroutine : public Logger::Loggable<Logger::Id::lua> {
public:
  enum class Status { NotStarted, Yielded, Finished };

  Coroutine(lua_State* state, lua_State* parent_state);
  ~Coroutine() { ENVOY_LOG(trace, "coroutine destruction"); }

  void initialize(ContextBase* context, StreamDirection direction);

  void start(int function_ref, const std::function<void()>& yield_callback);

  void resume(int num_args, const std::function<void()>& yield_callback);

  lua_State* luaState() { return coroutine_state_.get(); }

private:
  Status status_{Status::NotStarted};
  LuaThreadRef coroutine_state_;
};
using CoroutinePtr = std::unique_ptr<Coroutine>;

LuaVirtualMachineSharedPtr getOrCreateThreadLocalLuaVM(const std::string& vm_id,
                                                       const std::string& package_path);

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
