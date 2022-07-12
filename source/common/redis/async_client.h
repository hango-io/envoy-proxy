#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "source/common/common/logger.h"

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "hiredis/adapters/libevent.h"
#include "hiredis/async.h"
#include "hiredis/hiredis.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Redis {

namespace {

void setTimeval(timeval& time, uint64_t value) {
  time.tv_sec = value / 1000;
  time.tv_usec = (value % 1000) * 1000;
}

} // namespace
class AsyncClient : public Logger::Loggable<Logger::Id::redis> {
public:
  enum Status {
    // 未初始化或者连接断开后资源被清理
    NOT_INIT,
    // 初始化失败
    INIT_ERROR,
    // 正在尝试连接当中
    CONNECTING,
    // 已经连接但是未必能够提供服务
    CONNECTED,
    // 可以提供服务
    WORKING,
    // 整个客户端结束
    COMPLETED,
  };

  struct Endpoint {
    std::string host_;
    int port_{0};
  };

  class Exception : public std::runtime_error {
  public:
    Exception(const std::string& message) : std::runtime_error(message) {}
  };

  using Reply = std::string;
  using Error = std::string;

  // 对外暴露的命令回调函数，目前为了简单起见，所有响应都为字符串
  using CommandCallback =
      std::function<void(absl::optional<Reply> reply, absl::optional<Error> error)>;

  AsyncClient(const Endpoint& endpoint, const std::string& password, event_base* base,
              uint64_t timeout_ms = 20, bool reconnect = true, uint32_t max_reconnect = UINT32_MAX);

  AsyncClient(const std::vector<Endpoint>&, const std::string&, const std::string&, event_base*,
              uint64_t, bool, uint32_t) {
    throw Exception("NOT IMPLEMENTED SENTINEL NOW.");
  }

  virtual ~AsyncClient() {
    resetAsyncContext(COMPLETED);

    if (reconnect_event_) {
      evtimer_del(reconnect_event_);
    }
    try_reconnecting_ = false;
    reconnect_event_ = nullptr;
  }

  AsyncClient() = delete;

  void setConnectTimeout(uint64_t timeout_ms) { setTimeval(connect_timeout_, timeout_ms); }

  void setCommandTimeout(uint64_t timeout_ms) { setTimeval(command_timeout_, timeout_ms); }

  // 默认为 30s
  void setReconnectInterval(uint64_t interval_ms) { setTimeval(reconnect_interval_, interval_ms); }

  Status clientStatus() const { return client_status_; }

  // 最简单的 Redis 命令封装
  void set(const std::string& key, const std::string& value, uint64_t expire_ms);
  void get(const std::string& key, CommandCallback callback);
  void del(const std::string& key);

private:
  void connect();

  static void authenticateCb(redisAsyncContext* c, void* void_reply, void*) {
    AsyncClient* client = (AsyncClient*)c->data;

    if (!client) {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::redis), debug,
                          "Async redis context is freeing and do nothing");
      return;
    }

    redisReply* reply = (redisReply*)(void_reply);
    if (reply && strncmp(reply->str, "OK", 2) == 0) {
      client->client_status_ = WORKING;
    } else {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::redis), error,
                          "Client failed to authenticated and check your password");

      client->should_reconnect_ = false;
      client->resetAsyncContext(COMPLETED);
    }
  }

  void authenticate() {
    ENVOY_LOG(debug, "Async authenticate command to remote redis server");
    redisAsyncCommand(redis_async_context_, authenticateCb, nullptr, "AUTH %s", password_.c_str());
  }

  static void connectCb(const redisAsyncContext* c, int status) {
    AsyncClient* client = (AsyncClient*)c->data;

    if (!client) {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::redis), debug,
                          "Async redis client is freeing and do nothing");
      return;
    }

    if (client->try_reconnecting_) {
      // 重连完成之后重设相关状态
      client->reconnect_count_ = REDIS_OK == status ? 0 : client->reconnect_count_;
      client->try_reconnecting_ = false;
      client->reconnect_event_ = nullptr;
    }

    if (REDIS_OK == status) {
      // 连接成功
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::redis), debug,
                          "Redis client connect redis server succeeded");
      client->client_status_ = CONNECTED;
      if (!client->password_.empty()) {
        client->authenticate();
      } else {
        client->client_status_ = WORKING;
      }
    } else {
      // 连接失败
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::redis), error,
                          "Redis client connect redis server failed");
      // 连接失败，hiredis 会自动完成资源清理；将 redis_async_context 设置为空，避免重复释放内存
      client->client_status_ = NOT_INIT;
      client->redis_async_context_->data = nullptr;
      client->redis_async_context_ = nullptr;
      client->waitingToReconnect();
    }
  }

  static void disconnectCb(const redisAsyncContext* c, int status) {
    AsyncClient* client = (AsyncClient*)c->data;
    // 重置 redis async context 将可能触发 disconnectCb。此时不做任何操作
    if (REDIS_OK == status || !client) {
      // 主动断开连接无需任何其他操作
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::redis), debug,
                          "Redis client disconnect with redis server and no retry");
      return;
    }
    // 连接被动断开并尝试重新建立连接
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::redis), debug,
                        "Redis client disconnect with redis server and try to reconnect");

    // 连接外部断开，hiredis 会自动完成资源清理；将 redis_async_context 设置为空，避免重复释放内存
    client->client_status_ = NOT_INIT;
    client->redis_async_context_->data = nullptr;
    client->redis_async_context_ = nullptr;
    client->waitingToReconnect();
  }

  static void reconnectCb(evutil_socket_t, short, void* arg) { ((AsyncClient*)arg)->connect(); }

  static void commandCb(redisAsyncContext* c, void* void_reply, void* void_id) {
    AsyncClient* client = (AsyncClient*)c->data;

    if (!client) {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::redis), debug,
                          "Async redis client is freeing and do nothing");
      return;
    }

    auto& commands_map = client->commands_map_;
    unsigned long command_id = (unsigned long)void_id;

    auto command_callback = commands_map.find(command_id);

    if (command_callback == commands_map.end()) {
      return;
    }

    redisReply* reply = (redisReply*)(void_reply);

    if (reply != nullptr && reply->str != nullptr) {
      command_callback->second(std::string(reply->str, reply->len), absl::nullopt);
    } else if (c->errstr != nullptr) {
      command_callback->second(absl::nullopt, std::string(c->errstr));
    } else {
      command_callback->second(absl::nullopt, absl::nullopt);
    }

    commands_map.erase(command_id);
  }

  void resetAsyncContext(Status new_status) {
    ENVOY_LOG(debug, "Try reset and free redis async context");
    client_status_ = new_status;

    redisAsyncContext* to_free = nullptr;
    if (redis_async_context_) {
      redis_async_context_->data = nullptr;
      to_free = redis_async_context_;
      redis_async_context_ = nullptr;
    }

    // 客户端不会主动断开连接，如果在 free context 之前 context 处于
    // 连接断开中的状态，说明客户端被异常断开，由 hiredis 自动回收内存
    if (to_free && !(to_free->c.flags & REDIS_DISCONNECTING))
      redisAsyncFree(to_free);

    // 清理所有回调函数
    for (const auto& cb : commands_map_) {
      cb.second(absl::nullopt, absl::nullopt);
    }
    commands_map_.clear();
  }

  void waitingToReconnect() {
    ENVOY_LOG(debug, "Try set timer to waiting for create new connection");
    // 正在重新连接当中
    if (try_reconnecting_) {
      return;
    }

    // 不应当重新连接或者已经超出最大重连次数
    if (!should_reconnect_ || reconnect_count_++ > max_reconnect_count_) {
      should_reconnect_ = false;
      client_status_ = COMPLETED;
      return;
    }

    ENVOY_LOG(debug, "Set reconnect callback timer and waiting");
    try_reconnecting_ = true;
    reconnect_event_ = evtimer_new(event_base_, reconnectCb, this);
    evtimer_add(reconnect_event_, &reconnect_interval_);

    client_status_ = CONNECTING;
    return;
  }

  Status client_status_{NOT_INIT};

  unsigned long next_command_id_{0};

  std::unordered_map<unsigned long, CommandCallback> commands_map_;

  Endpoint working_endpoint_;
  std::string password_;

  // 对象所有权不属于当前客户端，故而析构时忽略它
  event_base* event_base_{nullptr};

  bool should_reconnect_{false};
  bool try_reconnecting_{false};

  const uint32_t max_reconnect_count_{0};
  uint32_t reconnect_count_{0};
  event* reconnect_event_{nullptr};
  timeval reconnect_interval_{30, 0};

  timeval connect_timeout_{0, 0};
  timeval command_timeout_{0, 0};

  redisOptions options_;

  // 对象所有权属于当前客户端，析构时需要回收内存
  redisAsyncContext* redis_async_context_{nullptr};
};

} // namespace Redis
} // namespace Common
} // namespace Proxy
} // namespace Envoy
