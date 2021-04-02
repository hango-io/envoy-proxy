#pragma once

#include <unordered_map>

#include "api/proxy/filters/http/rider/v3alpha1/rider.pb.h"
#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"

#include "extensions/filters/common/lua/lua.h"

#include "common/common/c_smart_ptr.h"
#include "common/common/lock_guard.h"
#include "common/common/thread.h"
#include "common/common/assert.h"
#include "common/http/utility.h"

#include "filters/http/rider/common.h"
#include "filters/http/rider/context.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace Rider {

int static_lua_get_request_body(lua_State* state) {
  ContextBase* context = LuaUtils::getContext(state);

  return context->luaBody(state, StreamDirection::DownstreamToUpstream);
}

int static_lua_get_response_body(lua_State* state) {
  ContextBase* context = LuaUtils::getContext(state);

  return context->luaBody(state, StreamDirection::UpstreamToDownstream);
}

int static_lua_respond(lua_State* state) {
  ContextBase* context = LuaUtils::getContext(state);

  lua_getglobal(state, lua_global_stream_direction.c_str());
  return context->luaRespond(state, StreamDirection(luaL_checkinteger(state, -1)));
}

int static_lua_http_call(lua_State* state) {
  ContextBase* context = LuaUtils::getContext(state);

  lua_getglobal(state, lua_global_stream_direction.c_str());
  return context->luaHttpCall(state, StreamDirection(luaL_checkinteger(state, -1)));
}

} // namespace Rider
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy
