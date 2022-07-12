#include "source/common/sender/httpx_request_sender.h"

#include "source/common/http/header_map_impl.h"
#include "source/common/http/message_impl.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Sender {

HttpxRequestSenderConfig::HttpxRequestSenderConfig(const std::string& cluster, uint32_t timeout)
    : cluster_name_(cluster), timeout_(timeout) {}

HttpxRequestSender::HttpxRequestSender(HttpxRequestSenderConfigSharedPtr config,
                                       Envoy::Upstream::ClusterManager& cm)
    : config_(std::move(config)), cm_(cm) {}

// RemoteCallerHandler
void HttpxRequestSender::cancel() {
  ASSERT(request_ != nullptr);
  if (request_) {
    request_->cancel();
    request_ = nullptr;
  }
}

SendRequestStatus HttpxRequestSender::sendRequest(Http::RequestMessagePtr&& message,
                                                  Http::AsyncClient::Callbacks* callback) {
  if (!config_ || !message || !callback) {
    // Failture directly.
    ENVOY_LOG(debug, "Empty headers or invalid callback for remote caller.");
    return SendRequestStatus::FAILED;
  }
  auto& cluster = config_->cluster();

  const auto thread_local_cluster = cm_.getThreadLocalCluster(cluster);

  if (thread_local_cluster != nullptr) {
    ENVOY_LOG(debug, "Send http request to cluster {}.", cluster);
    ENVOY_LOG(trace, "Request headers:\n{}", message->headers());
    request_ = thread_local_cluster->httpAsyncClient().send(
        std::move(message), *callback,
        Envoy::Http::AsyncClient::RequestOptions().setTimeout(config_->timeout()));

    return request_ != nullptr ? SendRequestStatus::ONCALL : SendRequestStatus::FAILED;
  } else {
    ENVOY_LOG(debug, "Remote caller cluster '{}' does not exist.", cluster);
    return SendRequestStatus::FAILED;
  }
}

} // namespace Sender
} // namespace Common
} // namespace Proxy
} // namespace Envoy
