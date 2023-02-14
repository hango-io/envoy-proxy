#pragma once

#include <atomic>

#include "source/common/buffer/buffer_impl.h"
#include "source/common/http/utility.h"
#include "source/extensions/filters/network/dubbo_proxy/dubbo_protocol_impl.h"
#include "source/extensions/filters/network/dubbo_proxy/message.h"
#include "source/extensions/filters/network/dubbo_proxy/metadata.h"

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/proxy/bridge/dubbo/v3/dubbo.pb.h"
#include "api/proxy/bridge/dubbo/v3/dubbo.pb.validate.h"
#include "hessian2/basic_codec/object_codec.hpp"
#include "hessian2/codec.hpp"
#include "hessian2/object.hpp"
#include "hessian2/reader.hpp"
#include "hessian2/writer.hpp"
#include "rapidjson/document.h"

namespace Envoy {
namespace Proxy {
namespace Common {
namespace DubboBridge {

namespace Json = rapidjson;

using JsonValue = Json::Value;
using JsonDocument = Json::Document;
using JsonAllocator = Json::Value::AllocatorType;
using JsonDocumentPtr = std::unique_ptr<Json::Document>;

using HessianValue = Hessian2::Object;
using HessianValueSharedPtr = std::shared_ptr<HessianValue>;
using HessianValuePtr = std::unique_ptr<HessianValue>;

using Envoy::Extensions::NetworkFilters::DubboProxy::ContextSharedPtr;
using Envoy::Extensions::NetworkFilters::DubboProxy::DubboProtocolImpl;
using Envoy::Extensions::NetworkFilters::DubboProxy::MessageMetadata;
using Envoy::Extensions::NetworkFilters::DubboProxy::MessageMetadataSharedPtr;

using ProtoDubboBridgeContext = proxy::bridge::dubbo::v3::DubboBridgeContext;

using FlatStringMap = absl::flat_hash_map<std::string, std::string>;

class BufferWriter : public Hessian2::Writer {
public:
  BufferWriter(Envoy::Buffer::Instance& buffer) : buffer_(buffer) {}

  void rawWrite(const void* data, uint64_t size) override;
  void rawWrite(absl::string_view data) override;

private:
  Envoy::Buffer::Instance& buffer_;
};

class BufferReader : public Hessian2::Reader {
public:
  BufferReader(Envoy::Buffer::Instance& buffer) : buffer_(buffer) {}

  uint64_t length() const override { return buffer_.length(); }
  void rawReadNBytes(void* data, size_t len, size_t offset) override;

private:
  Envoy::Buffer::Instance& buffer_;
};

class Utility {
public:
  static HessianValuePtr jsonValueToHessianValue(const JsonValue& value, absl::string_view type,
                                                 bool ignore_map_null_value);

  static std::string hessianValueToJsonString(const HessianValue* value);

  static HessianValueSharedPtr getNullHessianValue() {
    static auto Null = std::make_shared<Hessian2::NullObject>();
    return Null;
  }
};

struct PathAndType {
  absl::string_view path;
  absl::string_view type;
};

struct GenericType {
  std::string path;
  std::vector<std::string> splited_path;
  std::string type;
};

using GenericTypes = std::vector<GenericType>;
using GenericTypesSharedPtr = std::shared_ptr<GenericTypes>;

class Parameter {
public:
  Parameter(size_t index, const ProtoDubboBridgeContext::Parameter& config,
            bool ignore_null_map_pair);
  Parameter() = default;

  size_t index_{};

  std::string type_;
  std::string name_;

  bool required_{};

  HessianValueSharedPtr default_;
  GenericTypesSharedPtr generic_;
};

using ParameterPtr = std::unique_ptr<Parameter>;

class SrcsRequestContext {
public:
  using IterateFn = std::function<bool(absl::string_view, absl::string_view)>;

  SrcsRequestContext(const Http::RequestHeaderMap& headers, Buffer::Instance& buffer);

  absl::optional<absl::string_view> getHeader(absl::string_view key) const {
    return headers_.getByKey(key);
  }
  absl::optional<absl::string_view> getCookie(absl::string_view key) const {
    if (!cookies_.has_value()) {
      cookies_.emplace(Http::Utility::parseCookies(headers_));
    }
    auto iter = cookies_->find(key);
    return iter != cookies_->end() ? absl::make_optional<absl::string_view>(iter->second)
                                   : absl::nullopt;
  }
  absl::string_view getBody() const { return linearized_body_; }

  void iterateHeaders(IterateFn cb) const {
    headers_.iterate([cb](const Http::HeaderEntry& entry) -> Http::HeaderMap::Iterate {
      return cb(entry.key().getStringView(), entry.value().getStringView())
                 ? Http::HeaderMap::Iterate::Continue
                 : Http::HeaderMap::Iterate::Break;
    });
  }

private:
  const Http::RequestHeaderMap& headers_;
  mutable absl::optional<FlatStringMap> cookies_;
  const absl::string_view linearized_body_;
};

class ArgumentGetter;
class ResponseGetter;

class DubboBridgeContext {
public:
  using UpdateAttachmentFn = std::function<void(FlatStringMap&)>;

  enum Source { None, Body, Query };

  DubboBridgeContext(const ProtoDubboBridgeContext& context);

  void writeDubboRequest(Envoy::Buffer::OwnedImpl& buffer, const SrcsRequestContext& context,
                         const ArgumentGetter& getter) const;

  void writeSrcsResponse(Envoy::Buffer::OwnedImpl& buffer, const SrcsRequestContext& context,
                         const ResponseGetter& getter) const;

  size_t getParametersSize() const { return params_.size(); }

  bool lostParameterName() const { return lost_parameter_name_; }

  Source getArgumentSource() const { return argument_source_; }

  bool ignoreMapNullPair() const { return ignore_null_map_pair_; }

  const FlatStringMap& staticAttachmentsForTest() const { return static_attachs_; }
  const FlatStringMap& headerAttachmentsForTest() const { return header_attachs_; }
  const FlatStringMap& cookieAttachmentsForTest() const { return cookie_attachs_; }

private:
  void writeStaticAttachments(Hessian2::Encoder& encoder) const {
    for (const auto& attachement : static_attachs_) {
      ENVOY_LOG_TO_LOGGER(Envoy::Logger::Registry::getLog(Envoy::Logger::Id::dubbo), trace,
                          "Try write static: {}/{} to dubbo attachment", attachement.first,
                          attachement.second);
      encoder.encode(attachement.first);
      encoder.encode(attachement.second);
    }
  }
  void writeHeaderAttachments(Hessian2::Encoder& encoder, const SrcsRequestContext& context) const {
    for (const auto& attachment : header_attachs_) {
      auto header_entry = context.getHeader(attachment.second);
      if (header_entry.has_value()) {
        encoder.encode(attachment.first);
        encoder.encode(header_entry.value());
      }
    }
  }

  void writeCookieAttachments(Hessian2::Encoder& encoder, const SrcsRequestContext& context) const {
    if (cookie_attachs_.empty()) {
      return;
    }
    for (const auto& attachment : cookie_attachs_) {
      auto cookie_entry = context.getCookie(attachment.second);
      if (cookie_entry.has_value()) {
        encoder.encode(attachment.first);
        encoder.encode(cookie_entry.value());
      }
    }
  }

  void writeListBegin(Hessian2::Encoder& encoder, Envoy::Buffer::OwnedImpl& buffer,
                      int32_t length) const {
    if (length <= 7) {
      buffer.writeByte(static_cast<uint8_t>(0x78 + length));
    } else {
      buffer.writeByte(static_cast<uint8_t>(0x58));
      encoder.encode(length);
    }
  }

  void writeRequestBody(Envoy::Buffer::OwnedImpl& buffer, const SrcsRequestContext& context,
                        const ArgumentGetter& getter) const;

  std::string service_;
  std::string version_;
  std::string method_;

  FlatStringMap static_attachs_;
  FlatStringMap header_attachs_;
  FlatStringMap cookie_attachs_;

  std::vector<ParameterPtr> params_;

  bool lost_parameter_name_{};
  Source argument_source_{};
  bool ignore_null_map_pair_{};
};
using DubboBridgeContextSharedPtr = std::shared_ptr<DubboBridgeContext>;

struct DubboResponse : public Envoy::Event::DeferredDeletable {
  enum Status { OK, WAITING };

  DubboResponse() : metadata_(new MessageMetadata()) {}

  void reset() {
    metadata_ = std::make_shared<MessageMetadata>();
    context_ = nullptr;
    is_exception_ = false;
    close_connection_ = false;
    value_ = nullptr;
  }

  static Status decodeDubboFromBuffer(Envoy::Buffer::Instance& buffer, DubboResponse& response);

  MessageMetadataSharedPtr metadata_;
  ContextSharedPtr context_;

  bool is_exception_{false};
  bool close_connection_{false};

  Hessian2::ObjectPtr value_{};

private:
  DubboProtocolImpl impl_;
};
using DubboResponsePtr = std::unique_ptr<DubboResponse>;

class ArgumentGetter {
public:
  virtual ~ArgumentGetter() = default;

  virtual HessianValueSharedPtr getArgumentByParameter(const Parameter& parameter) const PURE;
  // Optional attachments from getter implementation. This interface could be implemented by the
  // derived class to provide additional attachments optionally.
  virtual OptRef<const FlatStringMap> attachmentsFromGetter() const { return {}; }
};
using ArgumentGetterPtr = std::unique_ptr<ArgumentGetter>;

using ArgumentGetterCreator =
    std::function<ArgumentGetterPtr(const DubboBridgeContext&, const SrcsRequestContext&)>;

class ArgumentGetterCreatorFactory : public Envoy::Config::TypedFactory {
public:
  std::string category() const override { return "proxy.dubbo_bridge.argument_getter"; }

  // Create argument getter creator by configuration.
  virtual ArgumentGetterCreator createArgumentGetterCreator(const Protobuf::Message& config) PURE;
};

class ResponseGetter {
public:
  virtual ~ResponseGetter() = default;

  enum Type { Value, Exception, Local };

  struct Response {
    std::string value;
    // Optional response code that will override all configured response codes and be used as the
    // final HTTP response code.
    absl::optional<uint32_t> code;
  };

  virtual Response getStringFromResponse(const HessianValuePtr& response, Type type) const PURE;
};
using ResponseGetterPtr = std::unique_ptr<ResponseGetter>;

using ResponseGetterCreator = std::function<ResponseGetterPtr()>;

class ResponseGetterCreatorFactory : public Envoy::Config::TypedFactory {
public:
  std::string category() const override { return "proxy.dubbo_bridge.response_getter"; }

  // Create response getter creator by configuration.
  virtual ResponseGetterCreator createResponseGetterCreator(const Protobuf::Message& config) PURE;
};

using DynamicContextCreator = std::function<DubboBridgeContextSharedPtr(const SrcsRequestContext&)>;

class DynamicContextCreatorFactory : public Envoy::Config::TypedFactory {
public:
  std::string category() const override { return "proxy.dubbo_bridge.dynamic_context"; }

  // Create dynamic context creator by configuration.
  virtual DynamicContextCreator createDynamicContextCreator(const Protobuf::Message& config) PURE;
};

} // namespace DubboBridge
} // namespace Common
} // namespace Proxy
} // namespace Envoy
