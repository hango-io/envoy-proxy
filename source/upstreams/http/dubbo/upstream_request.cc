#include "source/upstreams/http/dubbo/upstream_request.h"

#include <cstdint>
#include <memory>

#include "envoy/config/typed_metadata.h"
#include "envoy/router/router.h"
#include "envoy/upstream/upstream.h"

#include "source/common/common/assert.h"
#include "source/common/common/utility.h"
#include "source/common/http/codes.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/http/headers.h"
#include "source/common/http/message_impl.h"
#include "source/common/network/transport_socket_options_impl.h"
#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/protobuf/utility.h"
#include "source/common/router/router.h"
#include "source/extensions/common/proxy_protocol/proxy_protocol_header.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace Envoy {
namespace Proxy {
namespace Upstreams {
namespace Http {
namespace Dubbo {

using FlatStringMap = absl::flat_hash_map<std::string, std::string>;

class TypedMetadataFactory : public Envoy::Router::HttpRouteTypedMetadataFactory {
public:
  std::string name() const override { return "proxy.upstreams.http.dubbo"; }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const Envoy::ProtobufWkt::Struct& data) const override {
    ProtoDubboBridge typed_config;
    Envoy::MessageUtil::jsonConvert(data, Envoy::ProtobufMessage::getStrictValidationVisitor(),
                                    typed_config);

    return std::make_unique<DubboBridge>(typed_config);
  }
  std::unique_ptr<const Envoy::Config::TypedMetadata::Object>
  parse(const Envoy::ProtobufWkt::Any& data) const override {

    ProtoDubboBridge typed_config;
    Envoy::MessageUtil::anyConvertAndValidate(data, typed_config,
                                              Envoy::ProtobufMessage::getStrictValidationVisitor());
    return std::make_unique<DubboBridge>(typed_config);
  }
};

static Envoy::Registry::RegisterFactory<TypedMetadataFactory,
                                        Envoy::Router::HttpRouteTypedMetadataFactory>
    register_;

void DubboConnPool::onPoolReady(Envoy::Tcp::ConnectionPool::ConnectionDataPtr&& conn_data,
                                Envoy::Upstream::HostDescriptionConstSharedPtr host) {
  upstream_handle_ = nullptr;
  Envoy::Network::Connection& latched_conn = conn_data->connection();
  auto upstream =
      std::make_unique<DubboUpstream>(&callbacks_->upstreamToDownstream(), std::move(conn_data));
  callbacks_->onPoolReady(std::move(upstream), host,
                          latched_conn.connectionInfoProvider().localAddress(),
                          latched_conn.streamInfo(), {});
}

DubboUpstream::DubboUpstream(Envoy::Router::UpstreamToDownstream* upstream_request,
                             Envoy::Tcp::ConnectionPool::ConnectionDataPtr&& upstream)
    : upstream_request_(upstream_request), upstream_conn_data_(std::move(upstream)) {
  upstream_conn_data_->connection().enableHalfClose(true);
  upstream_conn_data_->addUpstreamCallbacks(*this);
}

DubboUpstream::~DubboUpstream() {
  // Saw close flag or no response than try close upstream connection.
  cleanup(to_close_upstream_connection_ || !responded_);
}

Envoy::Http::Status DubboUpstream::encodeHeaders(const Envoy::Http::RequestHeaderMap& headers,
                                                 bool end_stream) {

  request_headers_ = &headers;
  if (end_stream) {
    writeRequestToUpstream();
  }

  return Envoy::Http::okStatus();
}

void DubboUpstream::encodeData(Envoy::Buffer::Instance& data, bool end_stream) {
  bufferd_body_.move(data);
  if (end_stream) {
    writeRequestToUpstream();
  }
}

void DubboUpstream::encodeTrailers(const Envoy::Http::RequestTrailerMap&) {
  writeRequestToUpstream();
}

absl::optional<std::string> DubboUpstream::convertHttpToDubbo(const DubboBridge& dubbo_bridge,
                                                              Envoy::Buffer::OwnedImpl& buffer) {

  try {
    SrcsRequestContext source_context(*request_headers_, bufferd_body_);
    auto context = dubbo_bridge.getContext(source_context);
    ASSERT(context != nullptr);
    auto argment_getter = dubbo_bridge.getArgumentGetterCreator()(*context, source_context);
    context->writeDubboRequest(buffer, source_context, *argment_getter);
  } catch (const std::exception& e) {
    // Cache all here to avoid possible crash.
    return std::string(e.what());
  }
  return absl::nullopt;
}

void DubboUpstream::writeRequestToUpstream() {
  ASSERT(upstream_conn_data_ != nullptr);
  ASSERT(upstream_request_ != nullptr);

  if (request_complete_) {
    return;
  }
  request_complete_ = true;

  auto* dubbo_bridge =
      upstream_request_->route().typedMetadata().get<DubboBridge>("proxy.upstreams.http.dubbo");

  if (dubbo_bridge == nullptr) {
    // Mock async response here. writeRequestToUpstream will be called in the
    // Router::UpstreamRequest::encode* before Router::Filter::onRequestComplete. If we response
    // here directly, the local response may result in that the upstream request to be completed
    // and cleared. It means the upstream request will be destroyed when Envoy calling
    // Router::UpstreamRequest::encode*. It may break the Envoy.
    upstream_conn_data_->connection().dispatcher().post([guard = upstream_reset_guard_, this] {
      // Make sure the upstream request is still active.
      ASSERT(guard != nullptr);
      if (*guard) {
        handleResponse(400, ResponseGetter::Type::Local,
                       "No valid dubbo conversion context for current route or request", false);
      }
    });
    return;
  }

  protocol_error_code_ = dubbo_bridge->protocolErrorCode();
  resp_exception_code_ = dubbo_bridge->respExceptionCode();
  response_getter_ = dubbo_bridge->getResponseGetterCreator()();

  Envoy::Buffer::OwnedImpl buffer;
  auto error_message = convertHttpToDubbo(*dubbo_bridge, buffer);

  if (error_message.has_value()) {
    // Mock async response here. writeRequestToUpstream will be called in the
    // Router::UpstreamRequest::encode* before Router::Filter::onRequestComplete. If we response
    // here directly, the local response may result in that the upstream request to be completed
    // and cleared. It means the upstream request will be destroyed when Envoy calling
    // Router::UpstreamRequest::encode*. It may break the Envoy.
    upstream_conn_data_->connection().dispatcher().post(
        [guard = upstream_reset_guard_, this, error_message] {
          // Make sure the upstream request is still active.
          ASSERT(guard != nullptr);
          if (*guard) {
            handleResponse(400, ResponseGetter::Type::Local, error_message.value(), false);
          }
        });
    return;
  }
  upstream_conn_data_->connection().write(buffer, false);
}

using Envoy::Extensions::NetworkFilters::DubboProxy::MessageType;
using Envoy::Extensions::NetworkFilters::DubboProxy::RpcResponseType;

DubboUpstream::ResponseStatus DubboUpstream::readRespnoseFromUpstream() {
  if (!response_context_) {
    response_context_ =
        protocol_impl_.decodeHeader(dubbo_response_buffer_, response_metadata_).first;
  }

  // Waiting for the whole request or response.
  if (response_context_ == nullptr ||
      dubbo_response_buffer_.length() < response_context_->bodySize() + 16) {
    return ResponseStatus::WAITING;
  }

  // Handle heartbeat directly.
  while (response_metadata_->messageType() == MessageType::HeartbeatRequest ||
         response_metadata_->messageType() == MessageType::HeartbeatResponse) {

    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), info,
                        "Heartbeat package from dubbo server");

    // Drain header and body of heartbeat request/response.
    dubbo_response_buffer_.drain(response_context_->bodySize() + 16);

    // Reset heartbeat request/response.
    response_metadata_ =
        std::make_shared<Envoy::Extensions::NetworkFilters::DubboProxy::MessageMetadata>();
    response_context_ = nullptr;

    // Try re-decode response from buffer.
    response_context_ =
        protocol_impl_.decodeHeader(dubbo_response_buffer_, response_metadata_).first;
    // Waiting for the whole request or response.
    if (response_context_ == nullptr ||
        dubbo_response_buffer_.length() < response_context_->bodySize() + 16) {
      return ResponseStatus::WAITING;
    }
  }

  ASSERT(response_metadata_->messageType() != MessageType::HeartbeatRequest &&
         response_metadata_->messageType() != MessageType::HeartbeatResponse);
  ASSERT(dubbo_response_buffer_.length() >= response_context_->bodySize() + 16);

  switch (response_metadata_->messageType()) {
  case MessageType::Response:
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), trace,
                        "Response package from dubbo server");
    break;
  case MessageType::Request:
  case MessageType::Oneway:
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), error,
                        "Request package from dubbo server");
    FALLTHRU;
  case MessageType::Exception:
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), error,
                        "Exception package from dubbo server");
    FALLTHRU;
  default:
    handleResponse(protocol_error_code_, ResponseGetter::Type::Exception,
                   fmt::format("Expected response message but get {}",
                               size_t(response_metadata_->messageType())),
                   true);
    return ResponseStatus::OK;
  }

  // Drain header of response.
  dubbo_response_buffer_.drain(16);

  Hessian2::Decoder decoder(std::make_unique<BufferReader>(dubbo_response_buffer_));

  if (response_metadata_->responseStatus() !=
      Envoy::Extensions::NetworkFilters::DubboProxy::ResponseStatus::Ok) {
    auto response_value = decoder.decode<Hessian2::Object>();

    if (response_context_->bodySize() < decoder.offset()) {
      handleResponse(protocol_error_code_, ResponseGetter::Type::Exception,
                     fmt::format("RpcResult size({}) large than body size({})", decoder.offset(),
                                 response_context_->bodySize()),
                     true);
      return ResponseStatus::OK;
    }

    handleResponse(protocol_error_code_, ResponseGetter::Type::Exception, std::move(response_value),
                   true);
    return ResponseStatus::OK;
  }

  auto type_data = decoder.decode<int32_t>();

  if (type_data == nullptr) {
    handleResponse(protocol_error_code_, ResponseGetter::Type::Exception,
                   "No response type get from dubbo response and do nothing", true);
    return ResponseStatus::OK;
  }

  RpcResponseType type = static_cast<RpcResponseType>(*type_data);

  bool has_value = false;
  bool has_exception = false;

  switch (type) {
  case RpcResponseType::ResponseWithException:
  case RpcResponseType::ResponseWithExceptionWithAttachments:
    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), error,
                        "Exception package from dubbo server");
    has_value = true;
    has_exception = true;
    break;
  case RpcResponseType::ResponseWithValue:
  case RpcResponseType::ResponseValueWithAttachments:
    has_value = true;
    break;
  case RpcResponseType::ResponseWithNullValue:
  case RpcResponseType::ResponseNullValueWithAttachments:
    has_value = false;
    break;
  default:
    throw Envoy::EnvoyException(
        fmt::format("not supported return type {}", static_cast<uint8_t>(type)));
  }

  if (has_value) {

    ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), debug,
                        "Decode response binary to hessian object");
    auto response_value = decoder.decode<Hessian2::Object>();

    if (response_context_->bodySize() < decoder.offset()) {
      handleResponse(protocol_error_code_, ResponseGetter::Type::Exception,
                     fmt::format("RpcResult size({}) large than body size({})", decoder.offset(),
                                 response_context_->bodySize()),
                     true);
      return ResponseStatus::OK;
    }

    handleResponse(has_exception ? resp_exception_code_ : 200,
                   has_exception ? ResponseGetter::Type::Exception : ResponseGetter::Type::Value,
                   std::move(response_value), false);
  } else {
    handleResponse(has_exception ? resp_exception_code_ : 200,
                   has_exception ? ResponseGetter::Type::Exception : ResponseGetter::Type::Value,
                   HessianValuePtr{nullptr}, false);
  }

  return ResponseStatus::OK;
}

void DubboUpstream::handleRawResponse(uint32_t code, absl::string_view body, bool close) {
  ASSERT(upstream_request_ != nullptr);
  ASSERT(!responded_);

  responded_ = true;

  auto response_headers = Envoy::Http::ResponseHeaderMapImpl::create();
  response_headers->setStatus(code);
  response_headers->setContentLength(body.size());
  if (close) {
    to_close_upstream_connection_ = true;
    response_headers->setConnection("close");
  }

  bool end_stream = body.empty();

  upstream_request_->decodeHeaders(std::move(response_headers), end_stream);

  // This upstream request may be reset by the router upstream request for retry or any other
  // reason.
  if (end_stream || upstream_request_ == nullptr) {
    return;
  }

  Envoy::Buffer::OwnedImpl buffer;
  buffer.add(body);
  upstream_request_->decodeData(buffer, true);
}

void DubboUpstream::handleResponse(uint32_t code, ResponseGetter::Type type,
                                   HessianValuePtr response, bool close) {
  if (response_getter_ != nullptr) {
    auto final_response = response_getter_->getStringFromResponse(response, type);
    handleRawResponse(final_response.code.value_or(code), final_response.value, close);
    return;
  }
  handleRawResponse(code, Utility::hessianValueToJsonString(response.get()), close);
}

void DubboUpstream::handleResponse(uint32_t code, ResponseGetter::Type type,
                                   absl::string_view raw_str, bool close) {
  auto local_response = std::make_unique<Hessian2::StringObject>(raw_str);
  handleResponse(code, type, std::move(local_response), close);
}

void DubboUpstream::readDisable(bool disable) {
  if (upstream_conn_data_ == nullptr) {
    return;
  }
  if (upstream_conn_data_->connection().state() != Network::Connection::State::Open) {
    return;
  }
  upstream_conn_data_->connection().readDisable(disable);
}

void DubboUpstream::cleanup(bool close_connection) {
  // Set guard to false. Any pending callbacks will be ignored.
  *upstream_reset_guard_ = false;

  // Then set upstream_request_ to nullptr to ensure possible close event that triggered
  // by this cleanup will be ignored.
  upstream_request_ = nullptr;

  // Close upstream connection if necessary.
  if (close_connection && upstream_conn_data_ != nullptr) {
    upstream_conn_data_->connection().close(Network::ConnectionCloseType::NoFlush);
  }

  // Release the upstream connection anyway.
  upstream_conn_data_ = nullptr;
}

void DubboUpstream::resetStream() { cleanup(true); }

void DubboUpstream::onUpstreamData(Envoy::Buffer::Instance& data, bool end_stream) {
  if (upstream_request_ == nullptr || responded_ || data.length() == 0) {
    return;
  }

  dubbo_response_buffer_.move(data);
  try {
    if (auto s = readRespnoseFromUpstream(); s == ResponseStatus::WAITING && end_stream) {
      handleResponse(protocol_error_code_, ResponseGetter::Type::Exception,
                     "More data is necessary for upstream but end stream", true);
    }
  } catch (const std::exception& e) {
    handleResponse(protocol_error_code_, ResponseGetter::Type::Exception, e.what(), true);
  }
}

void DubboUpstream::onEvent(Envoy::Network::ConnectionEvent event) {
  if (event == Network::ConnectionEvent::Connected) {
    // Ignore connected event.
    return;
  }

  // If stream is reset or destroyed then ignore the event.
  if (upstream_request_ == nullptr) {
    return;
  }

  // If response is already sent then ignore the event.
  if (responded_) {
    // Upstream connection is closed so we don't need to close it again.
    cleanup(false);
    return;
  }

  auto upstream_request = upstream_request_;
  // Upstream connection is closed so we don't need to close it again.
  cleanup(false);
  upstream_request->onResetStream(Envoy::Http::StreamResetReason::ConnectionTermination, "");
}

void DubboUpstream::onAboveWriteBufferHighWatermark() {
  if (upstream_request_) {
    upstream_request_->onAboveWriteBufferHighWatermark();
  }
}

void DubboUpstream::onBelowWriteBufferLowWatermark() {
  if (upstream_request_) {
    upstream_request_->onBelowWriteBufferLowWatermark();
  }
}

} // namespace Dubbo
} // namespace Http
} // namespace Upstreams
} // namespace Proxy
} // namespace Envoy
