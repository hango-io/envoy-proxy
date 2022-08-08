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

    int envoy_http_lua_ffi_v2_log(int level, const char* message);
    int envoy_http_lua_ffi_v2_get_configuration(envoy_lua_ffi_str_t* buffer);
    int envoy_http_lua_ffi_v2_get_route_configuration(envoy_lua_ffi_str_t *buffer);
    

    uint64_t envoy_http_lua_ffi_v2_get_route_config_hash();

    int envoy_http_lua_ffi_v2_get_header_map(int source, envoy_lua_ffi_string_pairs* buffer);
    int envoy_http_lua_ffi_v2_get_header_map_size(int source);
    int envoy_http_lua_ffi_v2_get_header_map_value(int source, const char* key, int key_len, envoy_lua_ffi_str_t* value);
    int envoy_http_lua_ffi_v2_get_header_map_value_size(int source, const char* key, int key_len);
    int envoy_http_lua_ffi_v2_get_header_map_value_index(int source, const char* key, int key_len, envoy_lua_ffi_str_t* value, int index);
    int envoy_http_lua_ffi_v2_set_header_map(int source, envoy_lua_ffi_string_pairs* buffer);
    int envoy_http_lua_ffi_v2_set_header_map_value(int source, const char* key, int key_len, const char* value, int value_len);
    int envoy_http_lua_ffi_v2_remove_header_map_value(int source, const char* key, int key_len);
    int envoy_http_lua_ffi_v2_get_query_parameters(int source, envoy_lua_ffi_string_pairs* buf);
    int envoy_http_lua_ffi_v2_get_body(int source, envoy_lua_ffi_str_t* body);
]]

-- Log levels
local INFO = 2
local ERR = 4

local SOURCE_REQUEST = 0
local SOURCE_RESPONSE = 1

-- FFI return codes
local FFI_OK = 0
local FFI_NotFound = -2

function envoy.logInfo(message)
    C.envoy_http_lua_ffi_v2_log(INFO, tostring(message))
end

function envoy.logErr(message)
    C.envoy_http_lua_ffi_v2_log(ERR, tostring(message))
end

function envoy.get_configuration()
    local buffer = ffi_new("envoy_lua_ffi_str_t[1]")
    local rc = C.envoy_http_lua_ffi_v2_get_configuration(buffer)
    if rc ~= 0 then
        error("error get configuration in script")
    end
    return ffi_str(buffer[0].data, buffer[0].len)
end

function envoy.get_route_config_hash()

    return C.envoy_http_lua_ffi_v2_get_route_config_hash()
end

function envoy.get_route_configuration()
    local buffer = ffi_new("envoy_lua_ffi_str_t[1]")

    local rc = C.envoy_http_lua_ffi_v2_get_route_configuration(buffer)
    --if rc == FFI_NotFound then
    --    return nil
    --end
    if rc ~= 0 then
        error("error get route configuration: "..tostring(rc))
    end
    return ffi_str(buffer[0].data, buffer[0].len)
end

local function get_headers(source)

    local header_map_size = C.envoy_http_lua_ffi_v2_get_header_map_size(source)
    if header_map_size == 0  then return nil end
    local raw_buf = ffi_new("envoy_lua_ffi_table_elt_t[?]", header_map_size)
    local pairs_buf = ffi_new("envoy_lua_ffi_string_pairs[1]", { [0] = {raw_buf, 0, header_map_size} })

    local rc = C.envoy_http_lua_ffi_v2_get_header_map(source, pairs_buf)
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
    return C.envoy_http_lua_ffi_v2_get_header_map_size(source)
end

local function get_header_map_value(source, key)
    local buffer = ffi_new("envoy_lua_ffi_str_t[1]")
    local rc = C.envoy_http_lua_ffi_v2_get_header_map_value(source, key, #key, buffer)
    if rc ~= 0 then
        return nil
    end
    return ffi_str(buffer[0].data, buffer[0].len)
end

local function get_header_map_value_size(source, key)
    if type(key) ~= "string" then
        error("header name must be a string", 2)
    end

    return C.envoy_http_lua_ffi_v2_get_header_map_value_size(source, key, #key)
end

local function get_header_map_value_index(source, key, index)
    if type(key) ~= "string" then
        error("header name must be a string", 2)
    end

    local buffer = ffi_new("envoy_lua_ffi_str_t[1]")
    local rc = C.envoy_http_lua_ffi_v2_get_header_map_value_index(source, key, #key, buffer, index)
    if rc ~= FFI_OK then
        return nil
    end
    return ffi_str(buffer[0].data, buffer[0].len)
end

local function set_headers(source, headers)
    if type(headers) ~= "table" then
        error("headers must be a table", 2)
    end

    local length = 0
    for key, value in pairs(headers) do 
        length = length + 1
    end
    local raw_buf = ffi_new("envoy_lua_ffi_table_elt_t[?]", length)
    local pairs_buf = ffi_new("envoy_lua_ffi_string_pairs[1]", { [0] = {raw_buf, length, length} })

    local index = 0
    for key, value in pairs(headers) do 
        pairs_buf[0].data[index].key.data = key
        pairs_buf[0].data[index].key.len = #key
        pairs_buf[0].data[index].value.data = value
        pairs_buf[0].data[index].value.len = #value
        index = index + 1
    end

    local rc = C.envoy_http_lua_ffi_v2_set_header_map(source, pairs_buf)
    if rc ~= FFI_OK then
        error("error set headers: "..tonumber(rc))
    end

end

local function set_header_map_value(source, key, value)
    local rc = C.envoy_http_lua_ffi_v2_set_header_map_value(source, key, #key, value, #value)
    if rc ~= 0 then
        error("error set header")
    end
end

local function remove_header_map_value(source, key)
    local rc = C.envoy_http_lua_ffi_v2_remove_header_map_value(source, key, #key)
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

function envoy.req.get_header_size(key)
    return get_header_map_value_size(SOURCE_REQUEST, key)
end

function envoy.req.get_header_index(key, index)
    return get_header_map_value_index(SOURCE_REQUEST, key, index)
end

function envoy.resp.get_header_size(key)
    return get_header_map_value_size(SOURCE_RESPONSE, key)
end

function envoy.resp.get_header_index(key, index)
    return get_header_map_value_index(SOURCE_RESPONSE, key, index)
end

function envoy.req.set_headers(headers)
    return set_headers(SOURCE_REQUEST, headers)
end

function envoy.resp.set_headers(headers)
    return set_headers(SOURCE_RESPONSE, headers)
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

local function get_body_value(source)
    local buffer = ffi_new("envoy_lua_ffi_str_t[1]")
    local rc = C.envoy_http_lua_ffi_v2_get_body(source, buffer)
    if rc ~= FFI_OK then
        return nil
    end
    return ffi_str(buffer[0].data, buffer[0].len)
end

function envoy.req.get_body()
    return get_body_value(SOURCE_REQUEST)
end

function envoy.resp.get_body()
    return get_body_value(SOURCE_RESPONSE)
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