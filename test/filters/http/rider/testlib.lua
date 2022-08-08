local ffi = require("ffi")
local C = ffi.C
local ffi_new = ffi.new
local ffi_str = ffi.string

ffi.cdef[[
    typedef struct ContextBase ContextBase;

    typedef struct {
      int len;
      const char* data;
    } envoy_lua_ffi_str_t;

    typedef struct {
      envoy_lua_ffi_str_t key;
      envoy_lua_ffi_str_t value;
    } envoy_lua_ffi_table_elt_t;

    typedef struct {
      envoy_lua_ffi_table_elt_t* data;
      int size;
      int capacity;
    } envoy_lua_ffi_string_pairs;

    int envoy_http_lua_ffi_log(ContextBase* ctx, int level, const char* message);
    int envoy_http_lua_ffi_get_configuration(ContextBase* context, envoy_lua_ffi_str_t* buffer);
    int envoy_http_lua_ffi_get_route_configuration(ContextBase* context, envoy_lua_ffi_str_t *buffer);
    

    uint64_t envoy_http_lua_ffi_get_route_config_hash(ContextBase* context);

    int envoy_http_lua_ffi_get_header_map(ContextBase* ctx, int source, envoy_lua_ffi_string_pairs* buffer);
    int envoy_http_lua_ffi_get_header_map_size(ContextBase* ctx, int source);
    int envoy_http_lua_ffi_get_header_map_value(ContextBase* ctx, int source, const char* key, int key_len, envoy_lua_ffi_str_t* value);
    int envoy_http_lua_ffi_set_header_map_value(ContextBase* ctx, int source, const char* key, int key_len, const char* value, int value_len);
    int envoy_http_lua_ffi_remove_header_map_value(ContextBase* ctx, int source, const char* key, int key_len);
    int envoy_http_lua_ffi_get_query_parameters(ContextBase* ctx, int source, envoy_lua_ffi_string_pairs* buf);
]]

-- Log levels
local INFO = 2
local ERR = 4

local SOURCE_REQUEST = 0
local SOURCE_RESPONSE = 1

-- FFI return codes
local FFI_OK = 0
local FFI_NotFound = -2

local function get_context()
    return getfenv(0)._envoy_context
end

function envoy.logInfo(message)
    C.envoy_http_lua_ffi_log(get_context(), INFO, tostring(message))
end

function envoy.logErr(message)
    C.envoy_http_lua_ffi_log(get_context(), ERR, tostring(message))
end

function envoy.get_configuration()
    local buffer = ffi_new("envoy_lua_ffi_str_t[1]")
    local rc = C.envoy_http_lua_ffi_get_configuration(get_context(), buffer)
    if rc ~= 0 then
        error("error get configuration in script")
    end
    return ffi_str(buffer[0].data, buffer[0].len)
end

function envoy.get_route_config_hash()
    local ctx = get_context()

    return C.envoy_http_lua_ffi_get_route_config_hash(ctx)
end

function envoy.get_route_configuration()
    local ctx = get_context()
    local buffer = ffi_new("envoy_lua_ffi_str_t[1]")

    local rc = C.envoy_http_lua_ffi_get_route_configuration(ctx, buffer)
    --if rc == FFI_NotFound then
    --    return nil
    --end
    if rc ~= 0 then
        error("error get route configuration: "..tostring(rc))
    end
    return ffi_str(buffer[0].data, buffer[0].len)
end

local function get_headers(source)
    local ctx = get_context()

    local header_map_size = C.envoy_http_lua_ffi_get_header_map_size(ctx, source)
    if header_map_size == 0  then return nil end
    local raw_buf = ffi_new("envoy_lua_ffi_table_elt_t[?]", header_map_size)
    local pairs_buf = ffi_new("envoy_lua_ffi_string_pairs[1]", { [0] = {raw_buf, 0, header_map_size} })

    local rc = C.envoy_http_lua_ffi_get_header_map(ctx, source, pairs_buf)
    if rc ~= 0 then
        error("error get headers")
    end

    local result = {}
    for i = 0, header_map_size - 1  do
      local h = pairs_buf[0].data[i]

      local key = ffi_str(h.key.data, h.key.len)
      local val = ffi_str(h.value.data, h.value.len)

      result[key] = val
    end
    return result
end

local function get_header_map_size(source)
    local ctx = get_context()
    return C.envoy_http_lua_ffi_get_header_map_size(ctx, source)
end

local function get_header_map_value(source, key)
    local ctx = get_context()
    local buffer = ffi_new("envoy_lua_ffi_str_t[1]")
    local rc = C.envoy_http_lua_ffi_get_header_map_value(ctx, source, key, #key, buffer)
    if rc ~= 0 then
        return nil
    end
    return ffi_str(buffer[0].data, buffer[0].len)
end

local function set_header_map_value(source, key, value)
    local ctx = get_context()
    local rc = C.envoy_http_lua_ffi_set_header_map_value(ctx, source, key, #key, value, #value)
    if rc ~= 0 then
        error("error set header")
    end
end

local function remove_header_map_value(source, key)
    local ctx = get_context()
    local rc = C.envoy_http_lua_ffi_remove_header_map_value(ctx, source, key, #key)
    if rc ~= 0 then
        error("error remove header")
    end
end

function envoy.req.get_header_map_size()
    return get_header_map_size(SOURCE_REQUEST)
end

function envoy.resp.get_header_map_size()
    return get_header_map_size(SOURCE_RESPONSE)
end

function envoy.req.get_headers()
    return get_headers(SOURCE_REQUEST)
end

function envoy.resp.get_headers()
    return get_headers(SOURCE_RESPONSE)
end

function envoy.req.get_header(key)
    return get_header_map_value(SOURCE_REQUEST, key)
end

function envoy.resp.get_header(key)
    return get_header_map_value(SOURCE_RESPONSE, key)
end

function envoy.req.set_header(key, value)
    return set_header_map_value(SOURCE_REQUEST, key, value)
end

function envoy.resp.set_header(key, value)
    return set_header_map_value(SOURCE_RESPONSE, key, value)
end

function envoy.req.remove_header(key)
    return remove_header_map_value(SOURCE_REQUEST, key)
end

function envoy.resp.remove_header(key)
    return remove_header_map_value(SOURCE_RESPONSE, key)
end

-- Util functions.

function envoy.concat_headers(headers)
    local result = {}
    local i = 1
    for k, v in pairs(headers) do
        table.insert(result, k.."="..v)
    end
    table.sort(result)
    return table.concat(result, ",")
end