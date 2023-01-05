#pragma once

#include <cstdint>
#include <memory>

#include "envoy/http/codec.h"
#include "envoy/tcp/conn_pool.h"
#include "envoy/upstream/thread_local_cluster.h"

#include "source/common/buffer/watermark_buffer.h"
#include "source/common/common/cleanup.h"
#include "source/common/common/logger.h"
#include "source/common/dubbo/utility.h"
#include "source/common/protobuf/message_validator_impl.h"
#include "source/common/router/upstream_request.h"
#include "source/common/stream_info/stream_info_impl.h"
#include "source/extensions/filters/network/dubbo_proxy/dubbo_protocol_impl.h"
#include "source/extensions/filters/network/dubbo_proxy/message.h"
#include "source/extensions/filters/network/dubbo_proxy/metadata.h"

#include "absl/status/statusor.h"
#include "hessian2/codec.hpp"
#include "hessian2/object.hpp"
#include "hessian2/reader.hpp"
#include "hessian2/writer.hpp"
#include "rapidjson/document.h"

namespace Envoy {
namespace Proxy {
namespace Upstreams {
namespace Http {
namespace Dubbo {

using namespace Envoy::Proxy::Common::DubboBridge;
using ProtoDubboBridge = proxy::bridge::dubbo::v3::DubboBridge;

inline const std::string& defaultArgumentGetterName() {
  static const std::string Name = std::getenv("DUBBO_BRIDGE_DEFAULT_ARGUMENT_GETTER") != nullptr
                                      ? std::getenv("DUBBO_BRIDGE_DEFAULT_ARGUMENT_GETTER")
                                      : "proxy.dubbo_bridge.argument_getter.simple";
  return Name;
}

inline const std::string& defaultResponseGetterName() {
  static const std::string Name = std::getenv("DUBBO_BRIDGE_DEFAULT_RESPONSE_GETTER") != nullptr
                                      ? std::getenv("DUBBO_BRIDGE_DEFAULT_RESPONSE_GETTER")
                                      : "proxy.dubbo_bridge.response_getter.simple";
  return Name;
}

class DubboBridge : public Envoy::Config::TypedMetadata::Object {
public:
  DubboBridge(const ProtoDubboBridge& config) {
    if (config.has_context()) {
      static_shared_context_ = std::make_shared<DubboBridgeContext>(config.context());
    } else {
      auto& factory = Envoy::Config::Utility::getAndCheckFactory<DynamicContextCreatorFactory>(
          config.dynamic().context_getter());
      auto message = Envoy::Config::Utility::translateToFactoryConfig(
          config.dynamic().context_getter(), Envoy::ProtobufMessage::getStrictValidationVisitor(),
          factory);
      dynamic_context_creator_ = factory.createDynamicContextCreator(*message);
    }

    if (config.has_argument_getter()) {
      auto& factory = Envoy::Config::Utility::getAndCheckFactory<ArgumentGetterCreatorFactory>(
          config.argument_getter());

      auto message = Envoy::Config::Utility::translateToFactoryConfig(
          config.argument_getter(), Envoy::ProtobufMessage::getStrictValidationVisitor(), factory);
      argument_getter_creator_ = factory.createArgumentGetterCreator(*message);
    } else {
      auto& factory =
          Envoy::Config::Utility::getAndCheckFactoryByName<ArgumentGetterCreatorFactory>(
              defaultArgumentGetterName());
      auto message = factory.createEmptyConfigProto();
      argument_getter_creator_ = factory.createArgumentGetterCreator(*message);
    }

    if (config.has_response_getter()) {
      auto& factory = Envoy::Config::Utility::getAndCheckFactory<ResponseGetterCreatorFactory>(
          config.response_getter());

      auto message = Envoy::Config::Utility::translateToFactoryConfig(
          config.response_getter(), Envoy::ProtobufMessage::getStrictValidationVisitor(), factory);
      response_getter_creator_ = factory.createResponseGetterCreator(*message);
    } else {
      auto& factory =
          Envoy::Config::Utility::getAndCheckFactoryByName<ResponseGetterCreatorFactory>(
              defaultResponseGetterName());
      auto message = factory.createEmptyConfigProto();
      response_getter_creator_ = factory.createResponseGetterCreator(*message);
    }

    if (config.has_protocol_error_code()) {
      protocol_error_code_ = config.protocol_error_code().value();
    }
    if (config.has_resp_exception_code()) {
      resp_exception_code_ = config.resp_exception_code().value();
    }
  }

  DubboBridgeContextSharedPtr getContext(const SrcsRequestContext& source) const {
    if (static_shared_context_ == nullptr) {
      return dynamic_context_creator_(source);
    }
    return static_shared_context_;
  }

  const ArgumentGetterCreator& getArgumentGetterCreator() const { return argument_getter_creator_; }
  const ResponseGetterCreator& getResponseGetterCreator() const { return response_getter_creator_; }

  uint32_t protocolErrorCode() const { return protocol_error_code_; }
  uint32_t respExceptionCode() const { return resp_exception_code_; }

private:
  uint32_t protocol_error_code_{501};
  uint32_t resp_exception_code_{503};

  DubboBridgeContextSharedPtr static_shared_context_;
  DynamicContextCreator dynamic_context_creator_;
  ArgumentGetterCreator argument_getter_creator_;
  ResponseGetterCreator response_getter_creator_;
};

class DubboConnPool : public Envoy::Router::GenericConnPool,
                      public Envoy::Tcp::ConnectionPool::Callbacks {
public:
  DubboConnPool(Envoy::Upstream::ThreadLocalCluster& thread_local_cluster, bool,
                const Envoy::Router::RouteEntry& route_entry, absl::optional<Envoy::Http::Protocol>,
                Envoy::Upstream::LoadBalancerContext* ctx) {
    conn_pool_data_ = thread_local_cluster.tcpConnPool(route_entry.priority(), ctx);
  }
  void newStream(Envoy::Router::GenericConnectionPoolCallbacks* callbacks) override {
    callbacks_ = callbacks;
    upstream_handle_ = conn_pool_data_.value().newConnection(*this);
  }

  bool cancelAnyPendingStream() override {
    if (upstream_handle_) {
      upstream_handle_->cancel(Envoy::Tcp::ConnectionPool::CancelPolicy::Default);
      upstream_handle_ = nullptr;
      return true;
    }
    return false;
  }
  Envoy::Upstream::HostDescriptionConstSharedPtr host() const override {
    return conn_pool_data_.value().host();
  }

  bool valid() { return conn_pool_data_.has_value(); }

  // Tcp::ConnectionPool::Callbacks
  void onPoolFailure(Envoy::ConnectionPool::PoolFailureReason reason,
                     absl::string_view transport_failure_reason,
                     Envoy::Upstream::HostDescriptionConstSharedPtr host) override {
    upstream_handle_ = nullptr;
    callbacks_->onPoolFailure(reason, transport_failure_reason, host);
  }

  void onPoolReady(Envoy::Tcp::ConnectionPool::ConnectionDataPtr&& conn_data,
                   Envoy::Upstream::HostDescriptionConstSharedPtr host) override;

private:
  absl::optional<Envoy::Upstream::TcpPoolData> conn_pool_data_;
  Envoy::Tcp::ConnectionPool::Cancellable* upstream_handle_{};
  Envoy::Router::GenericConnectionPoolCallbacks* callbacks_{};
};

class DubboUpstream : public Envoy::Router::GenericUpstream,
                      public Envoy::Tcp::ConnectionPool::UpstreamCallbacks {
public:
  DubboUpstream(Envoy::Router::UpstreamToDownstream* upstream_request,
                Envoy::Tcp::ConnectionPool::ConnectionDataPtr&& upstream);

  // GenericUpstream
  void encodeData(Envoy::Buffer::Instance& data, bool end_stream) override;
  void encodeMetadata(const Envoy::Http::MetadataMapVector&) override {}
  Envoy::Http::Status encodeHeaders(const Envoy::Http::RequestHeaderMap&, bool end_stream) override;
  void encodeTrailers(const Envoy::Http::RequestTrailerMap&) override;
  void readDisable(bool disable) override;
  void resetStream() override;
  void setAccount(Envoy::Buffer::BufferMemoryAccountSharedPtr) override {}

  // Tcp::ConnectionPool::UpstreamCallbacks
  void onUpstreamData(Envoy::Buffer::Instance& data, bool end_stream) override;
  void onEvent(Envoy::Network::ConnectionEvent event) override;
  void onAboveWriteBufferHighWatermark() override;
  void onBelowWriteBufferLowWatermark() override;
  const Envoy::StreamInfo::BytesMeterSharedPtr& bytesMeter() override { return bytes_meter_; }

  static std::atomic<int64_t> GLOBAL_REQUEST_ID;

private:
  enum ResponseStatus { OK, WAITING };

  absl::optional<std::string> convertHttpToDubbo(const DubboBridge& dubbo_bridge,
                                                 Envoy::Buffer::OwnedImpl& buffer);

  void writeRequestToUpstream();
  ResponseStatus readRespnoseFromUpstream();

  void handleRawResponse(uint32_t code, absl::string_view body, bool close);

  void handleResponse(uint32_t code, ResponseGetter::Type type, HessianValuePtr response,
                      bool close);
  void handleResponse(uint32_t code, ResponseGetter::Type type, absl::string_view raw_str,
                      bool close);

  const Envoy::Http::RequestHeaderMap* request_headers_{};
  Envoy::Buffer::OwnedImpl bufferd_body_;
  Envoy::Buffer::OwnedImpl dubbo_response_buffer_;
  bool request_complete_{};

  Envoy::Extensions::NetworkFilters::DubboProxy::MessageMetadataSharedPtr response_metadata_{
      new Envoy::Extensions::NetworkFilters::DubboProxy::MessageMetadata()};
  Envoy::Extensions::NetworkFilters::DubboProxy::ContextSharedPtr response_context_;
  Envoy::Extensions::NetworkFilters::DubboProxy::DubboProtocolImpl protocol_impl_;

  uint32_t protocol_error_code_{501};
  uint32_t resp_exception_code_{503};

  ResponseGetterPtr response_getter_;

  Envoy::Router::UpstreamToDownstream* upstream_request_;
  Envoy::Tcp::ConnectionPool::ConnectionDataPtr upstream_conn_data_;
  Envoy::StreamInfo::BytesMeterSharedPtr bytes_meter_{
      std::make_shared<Envoy::StreamInfo::BytesMeter>()};

  // Guard to ensure that the total request still active.
  std::shared_ptr<bool> upstream_reset_guard_{std::make_shared<bool>(true)};
};

} // namespace Dubbo
} // namespace Http
} // namespace Upstreams
} // namespace Proxy
} // namespace Envoy
