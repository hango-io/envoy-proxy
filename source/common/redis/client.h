#pragma once

#include <memory>
#include <vector>

#include "source/common/common/logger.h"

#include "src/sw/redis++/redis++.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Redis {

class ClientOptions {
public:
  enum class RedisType { Cluster, Sentinel, General };

  std::string hosts_;
  std::string password_;
  RedisType redis_type_{RedisType::General};

  // Connection
  std::chrono::milliseconds connect_timeout_{0};
  std::chrono::milliseconds socket_timeout_{0};
  size_t pool_size_;
  bool keep_alive_{true};

  // TODO: Sentinel

  // TODO: Cluster
};

class RedisClient : public Logger::Loggable<Logger::Id::client> {

  using RedisClientPtr = std::unique_ptr<sw::redis::Redis>;
  using RedisClusterClientPtr = std::unique_ptr<sw::redis::RedisCluster>;
  using NodeList = std::vector<std::pair<std::string, int>>;

public:
  RedisClient(const ClientOptions& options);

  void command(const std::vector<std::string>& args);

private:
  void parseRedisNodes(const std::string& src, std::vector<std::pair<std::string, int>>& dst);
  void connectToGeneral();

private:
  ClientOptions options_;
  RedisClientPtr client_{nullptr};
};

} // namespace Redis
} // namespace Common
} // namespace Proxy
} // namespace Envoy
