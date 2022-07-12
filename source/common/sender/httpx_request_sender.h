#pragma once

#include "envoy/upstream/cluster_manager.h"

#include "source/common/common/logger.h"
#include "source/common/sender/request_sender.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Sender {

class HttpxRequestSenderConfig {
public:
  HttpxRequestSenderConfig(const std::string& cluster, uint32_t timeout);
  const std::string& cluster() const { return cluster_name_; }
  const std::chrono::milliseconds& timeout() const { return timeout_; }

private:
  const std::string cluster_name_;
  const std::chrono::milliseconds timeout_;
};

using HttpxRequestSenderConfigPtr = std::unique_ptr<HttpxRequestSenderConfig>;
using HttpxRequestSenderConfigSharedPtr = std::shared_ptr<HttpxRequestSenderConfig>;

class HttpxRequestSender : public RequestSender, public Logger::Loggable<Logger::Id::client> {
public:
  HttpxRequestSender(HttpxRequestSenderConfigSharedPtr config, Upstream::ClusterManager&);

  void cancel() override;
  SendRequestStatus sendRequest(Envoy::Http::RequestMessagePtr&&,
                                Envoy::Http::AsyncClient::Callbacks*) override;

private:
  HttpxRequestSenderConfigSharedPtr config_{nullptr};
  Envoy::Upstream::ClusterManager& cm_;
  Envoy::Http::AsyncClient::Request* request_{nullptr};
};

using HttpxRequestSenderPtr = std::unique_ptr<HttpxRequestSender>;

} // namespace Sender
} // namespace Common
} // namespace Proxy
} // namespace Envoy
