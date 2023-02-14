#include "source/common/redis/async_client.h"

#include <iostream>

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Redis {

AsyncClient::AsyncClient(const AsyncClient::Endpoint& endpoint, const std::string& password,
                         event_base* base, uint64_t timeout_ms, bool reconnect,
                         uint32_t max_reconnect)
    : working_endpoint_(endpoint), password_(password), event_base_(base),
      should_reconnect_(reconnect), max_reconnect_count_(max_reconnect) {
  memset(&options_, 0, sizeof(redisOptions));

  if (!event_base_) {
    throw Exception("EVENT BASE CAN NOT BE NULL AND PLEASE CHECK YOUR CODE");
  }

  setCommandTimeout(timeout_ms);
  setConnectTimeout(timeout_ms);

  REDIS_OPTIONS_SET_TCP(&options_, working_endpoint_.host_.c_str(), working_endpoint_.port_);

  options_.connect_timeout = &connect_timeout_;
  options_.command_timeout = &command_timeout_;

  connect();
}

void AsyncClient::connect() {
  ENVOY_LOG(debug, "Try create context and connect to {}:{}", options_.endpoint.tcp.ip,
            options_.endpoint.tcp.port);
  redis_async_context_ = redisAsyncConnectWithOptions(&options_);
  if (redis_async_context_ == nullptr || redis_async_context_->err) {
    ENVOY_LOG(error, "Cannot create redis async context and check your config/network");
    if (redis_async_context_ != nullptr && redis_async_context_->errstr != nullptr) {
      ENVOY_LOG(error, "    Error: {}", absl::string_view(redis_async_context_->errstr));
    }
    resetAsyncContext(INIT_ERROR);
    return;
  }

  redis_async_context_->data = this;

  redisLibeventAttach(redis_async_context_, event_base_);
  if (redisAsyncSetConnectCallback(redis_async_context_, connectCb) != REDIS_OK) {
    ENVOY_LOG(error, "Cannot set connect callback for async context");
    resetAsyncContext(INIT_ERROR);
    return;
  }
  if (redisAsyncSetDisconnectCallback(redis_async_context_, disconnectCb) != REDIS_OK) {
    ENVOY_LOG(error, "Cannot set disconnect callback for async context");
    resetAsyncContext(INIT_ERROR);
    return;
  }
}

void AsyncClient::set(const std::string& key, const std::string& value, uint64_t expire_ms) {
  if (client_status_ != WORKING || !redis_async_context_ || expire_ms == 0) {
    return;
  }
  redisAsyncCommand(redis_async_context_, nullptr, nullptr, "SET %s %b PX %lld", key.c_str(),
                    value.c_str(), value.size(), expire_ms);
}

void AsyncClient::get(const std::string& key, CommandCallback callback) {
  if (client_status_ != WORKING || !redis_async_context_) {
    return;
  }

  unsigned long command_id = next_command_id_++;
  commands_map_[command_id] = callback;

  // NOLINTNEXTLINE
  redisAsyncCommand(redis_async_context_, commandCb, (void*)command_id, "GET %s", key.c_str());
}

void AsyncClient::del(const std::string& key) {
  if (client_status_ != WORKING || !redis_async_context_) {
    return;
  }

  redisAsyncCommand(redis_async_context_, nullptr, nullptr, "DEL %s", key.c_str());
}

} // namespace Redis
} // namespace Common
} // namespace Proxy
} // namespace Envoy
