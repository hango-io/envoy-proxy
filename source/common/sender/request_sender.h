#pragma once

#include "envoy/buffer/buffer.h"
#include "envoy/http/async_client.h"
#include "envoy/http/header_map.h"

#include "source/common/http/headers.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace Sender {

enum class SendRequestStatus {
  FAILED,
  ONCALL,
};

class RequestSender : public Envoy::Http::AsyncClient::Request {
public:
  virtual SendRequestStatus sendRequest(Envoy::Http::RequestMessagePtr&&,
                                        Envoy::Http::AsyncClient::Callbacks*) PURE;
};

using RequestSenderPtr = std::unique_ptr<RequestSender>;
using RequestSenderSharedPtr = std::shared_ptr<RequestSender>;

} // namespace Sender
} // namespace Common
} // namespace Proxy
} // namespace Envoy
