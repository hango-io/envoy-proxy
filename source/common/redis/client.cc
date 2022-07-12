#include "source/common/redis/client.h"

#include "envoy/common/exception.h"

#include "source/common/common/utility.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Redis {

RedisClient::RedisClient(const ClientOptions& options) : options_(options) {
  try {
    connectToGeneral();
  } catch (const sw::redis::Error& e) {
    throw EnvoyException(e.what());
  }
}

void RedisClient::parseRedisNodes(const std::string& src, NodeList& dst) {
  if (!src.length()) {
    return;
  }

  auto nodes = StringUtil::splitToken(src, ",", false);
  for (const auto& i : nodes) {
    auto parts = StringUtil::splitToken(i, ":", false);
    if (parts.size() == 2) {
      int port;
      bool success = absl::SimpleAtoi<int>(parts[1], &port);
      if (success) {
        dst.push_back({std::string(parts[0].data(), parts[0].length()), port});
      }
    }
  }
}

void RedisClient::connectToGeneral() {
  sw::redis::ConnectionOptions opts;
  sw::redis::ConnectionPoolOptions pool_options;

  NodeList nodes;

  parseRedisNodes(options_.hosts_, nodes);

  if (!nodes.size()) {
    throw EnvoyException("no valid redis host");
  }

  auto& node = nodes[0];

  opts.host = node.first;
  opts.port = node.second;
  opts.password = options_.password_;
  if (options_.connect_timeout_.count()) {
    opts.connect_timeout = options_.connect_timeout_;
  }
  if (options_.socket_timeout_.count()) {
    opts.connect_timeout = options_.socket_timeout_;
  }
  opts.keep_alive = options_.keep_alive_;
  if (options_.pool_size_ > 0) {
    pool_options.size = options_.pool_size_;
  }

  client_ = std::make_unique<sw::redis::Redis>(opts, pool_options);
}

void RedisClient::command(const std::vector<std::string>& args) {
  client_->command(args.begin(), args.end());
}

} // namespace Redis
} // namespace Common
} // namespace Proxy
} // namespace Envoy
