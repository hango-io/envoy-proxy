#pragma once

#include "api/proxy/filters/http/rider/v3alpha1/rider.pb.h"
#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"

#include "common/common/c_smart_ptr.h"
#include "common/common/lock_guard.h"
#include "common/common/thread.h"
#include "common/common/logger.h"
#include "common/http/utility.h"

#include "lua.hpp"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

const static std::string lua_registry_key_ctx_table = "_envoy_ctx_table";
const static std::string lua_registry_key_config_table = "_envoy_config_table";
const static std::string lua_global_context_key = "_envoy_context";
const static std::string lua_global_stream_direction = "_envoy_stream_direction";

class ContextBase;

enum class FFIReturnCode {
  Success = 0,
  BadContext = -1,
  NotFound = -2,
  BadArgument = -3,
  Unsupported = -4,
};

enum class StreamDirection {
  DownstreamToUpstream,
  UpstreamToDownstream,
};

enum class LuaStreamOpSourceType {
  Request,
  Response,
};

const static std::string lua_coroutine_phase_key = "__envoy_coroutine_phase";

extern "C" {
typedef struct {
  int len;
  const char* data;
} envoy_lua_ffi_str_t;

typedef struct {
  envoy_lua_ffi_str_t key;
  envoy_lua_ffi_str_t value;
} envoy_lua_ffi_table_elt_t;

typedef struct {
  // data is buffer start address.
  envoy_lua_ffi_table_elt_t* data;
  // size is number of entries in the buffer.
  int size;
  // capacity is the number of entries the buffer can hold.
  int capacity;
} envoy_lua_ffi_string_pairs;
}

/*
 * We use indicator mentioned https://www.lua.org/manual/5.1/manual.html#3.7
 * to annotate C API like functions.
 */

class LuaUtils {
public:
  static void stackDump(lua_State* state) { stackDump(state, 0); }

  static void stackDump(lua_State* state, int rc) {
    // Do not touch the stack if error code is LUA_ERRMEM or LUA_ERRERR.
    if (rc > LUA_ERRSYNTAX) {
      return;
    }

    stackDumpToFile(stderr, state);
  }

  // TODO(Tong Cai): rewrite this function.
  static void stackDumpToFile(FILE* out, lua_State* L) {
    int i;
    int top = lua_gettop(L);
    fprintf(out, "stack depth: %d\n", top);
    for (i = 1; i <= top; i++) { /* repeat for each level */
      fprintf(out, "*** stack entry %d ***\n", i);
      int t = lua_type(L, i);
      switch (t) {
      case LUA_TSTRING: /* strings */
        fprintf(out, "`%s'", lua_tostring(L, i));
        break;

      case LUA_TBOOLEAN: /* booleans */
        fprintf(out, lua_toboolean(L, i) ? "true" : "false");
        break;

      case LUA_TNUMBER: /* numbers */
        fprintf(out, "%g", lua_tonumber(L, i));
        break;

      default: /* other values */
        fprintf(out, "%s", lua_typename(L, t));
        break;
      }
      fprintf(out, "  "); /* put a separator */
    }
    fprintf(out, "\n"); /* end the listing */
  }

  static void setContext(lua_State* state, ContextBase* r) {
    lua_pushlightuserdata(state, static_cast<void*>(r));
    lua_setglobal(state, lua_global_context_key.c_str());
  }

  static inline ContextBase* getContext(lua_State* state) {
    lua_getglobal(state, lua_global_context_key.c_str());
    ContextBase* context = static_cast<ContextBase*>(lua_touserdata(state, -1));

    return context;
  }

  template <typename T> static T* getUserData(lua_State* state, const std::string& key) {
    lua_getglobal(state, key.c_str());
    T* data = static_cast<T*>(lua_touserdata(state, -1));
    lua_pop(state, 1);

    return data;
  }

  template <typename T>
  static void setLightuserdata(lua_State* state, const std::string& key, T* data) {
    lua_pushlightuserdata(state, static_cast<void*>(data));
    lua_setglobal(state, key.c_str());
  }

  /**
   * Modified from
   * https://github.com/openresty/lua-nginx-module/blob/v0.10.15/src/ngx_http_lua_util.c#L209
   *
   * Create new table and set _G field to itself.
   *
   * indicator [0, 1, m+e]
   * After return:
   *         | new table | <- top
   *         |    ...    |
   */
  static void createNewGlobalsTable(lua_State* state, int narr, int nrec) {
    lua_createtable(state, narr, nrec + 1);
    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "_G");
  }

  /*
   * Set globals table to the value at the top of the stack.
   *
   * @indicator [-1, 0, -]
   */
  static void setGlobalsTable(lua_State* state) { lua_replace(state, LUA_GLOBALSINDEX); }

  /*
   * @indicator [0, 1, -]
   */
  static void getGlobalsTable(lua_State* state) { lua_pushvalue(state, LUA_GLOBALSINDEX); }

  /*
   * See:
   *  - http://lua-users.org/wiki/EnvironmentsTutorial
   *  - https://www.lua.org/pil/14.3.html
   *
   * Lua stack will not be changed.
   *
   * @indicator [0, 0, m+e]
   */
  static void setupSandbox(lua_State* state) {
    // Create new global table.
    createNewGlobalsTable(state, 0, 0);

    // Set metatable for the new globals table, ensure access to original globals table.
    lua_createtable(state, 0, 1);
    getGlobalsTable(state);
    lua_setfield(state, -2, "__index");
    lua_setmetatable(state, -2);

    // Replace original globals table with the new one.
    setGlobalsTable(state);
  }

  /**
   * Add path to lua _G["package.path"]
   *
   * Stack is unchanged.
   *
   * @param L state for lua VM.
   * @param path additional path to search lua modules.
   * @return return true if successful, false otherwise.
   */
  static bool addLuaPackagePath(lua_State* state, const std::string& path) {
    // Push the "package" table
    lua_getglobal(state, "package");

    if (!lua_istable(state, -1)) {
      return false;
    }

    lua_pushlstring(state, path.c_str(), path.length());
    lua_getfield(state, -2, "path");
    lua_concat(state, 2);
    lua_setfield(state, -2, "path");

    // Pop the "package" table
    lua_pop(state, 1);
    return true;
  }
};

/**
 * An exception specific to Lua errors.
 */
class LuaException : public EnvoyException {
public:
  using EnvoyException::EnvoyException;
};

class LuaStackUnchange {
public:
  LuaStackUnchange(lua_State* state) : original_top_(lua_gettop(state)), state_(state) {}
  LuaStackUnchange(int original_top, lua_State* state)
      : original_top_(original_top), state_(state) {}

  ~LuaStackUnchange() {
    int top = lua_gettop(state_);
    if (original_top_ != top) {
      LuaUtils::stackDumpToFile(stderr, state_);
      std::cout << fmt::format("expect stack top to be {}, got {}", original_top_, top)
                << std::endl;
    }
    ASSERT(original_top_ == top);
  }

private:
  int original_top_;
  lua_State* state_;
};

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
