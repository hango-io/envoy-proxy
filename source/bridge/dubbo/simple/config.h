#pragma once

#include "source/common/dubbo/utility.h"

#include "api/proxy/bridge/dubbo/simple/v3/simple.pb.h"

namespace Envoy {
namespace Proxy {
namespace Bridge {
namespace Dubbo {
namespace Simple {

class QueryGetter : public Common::DubboBridge::ArgumentGetter {
public:
  QueryGetter(const Common::DubboBridge::DubboBridgeContext& context,
              const Common::DubboBridge::SrcsRequestContext& source);

  Common::DubboBridge::HessianValueSharedPtr
  getArgumentByParameter(const Common::DubboBridge::Parameter& parameter) const override;

  const Envoy::Http::Utility::QueryParams params_;
  const bool ignore_null_map_pair_{};
};

class BodyGetter : public Common::DubboBridge::ArgumentGetter {
public:
  BodyGetter(const Common::DubboBridge::DubboBridgeContext& context,
             const Common::DubboBridge::SrcsRequestContext& source);

  Common::DubboBridge::HessianValueSharedPtr
  getArgumentByParameter(const Common::DubboBridge::Parameter& parameter) const override;

  Common::DubboBridge::JsonDocument doc_;
  const bool ignore_null_map_pair_{};
  const bool lost_parameter_name_{};
};

class EmptyGetter : public Common::DubboBridge::ArgumentGetter {
public:
  Common::DubboBridge::HessianValueSharedPtr
  getArgumentByParameter(const Common::DubboBridge::Parameter&) const override {
    return nullptr;
  }
};

class SimpleArgumentGetter : public Common::DubboBridge::ArgumentGetter {
public:
  SimpleArgumentGetter(const Common::DubboBridge::DubboBridgeContext&,
                       const Common::DubboBridge::SrcsRequestContext&);

  Common::DubboBridge::HessianValueSharedPtr
  getArgumentByParameter(const Common::DubboBridge::Parameter& parameter) const override {
    ASSERT(inner_getter_ != nullptr);
    return inner_getter_->getArgumentByParameter(parameter);
  }

private:
  Common::DubboBridge::ArgumentGetterPtr inner_getter_;
};

class SimpleArgumentGetterCreatorFactory
    : public Common::DubboBridge::ArgumentGetterCreatorFactory {
public:
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<proxy::bridge::dubbo::simple::v3::ArgumentGetterConfig>();
  }

  std::string name() const override { return "proxy.dubbo_bridge.argument_getter.simple"; }

  Common::DubboBridge::ArgumentGetterCreator
  createArgumentGetterCreator(const Protobuf::Message& config) override;
};

class SimpleResponseGetter : public Common::DubboBridge::ResponseGetter {
public:
  SimpleResponseGetter() = default;

  Response getStringFromResponse(const Common::DubboBridge::HessianValuePtr& response,
                                 Type) const override;
};

class SimpleResponseGetterCreatorFactory
    : public Common::DubboBridge::ResponseGetterCreatorFactory {
public:
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<proxy::bridge::dubbo::simple::v3::ResponseGetterConfig>();
  }

  std::string name() const override { return "proxy.dubbo_bridge.response_getter.simple"; }

  Common::DubboBridge::ResponseGetterCreator
  createResponseGetterCreator(const Protobuf::Message& config) override;
};

} // namespace Simple
} // namespace Dubbo
} // namespace Bridge
} // namespace Proxy
} // namespace Envoy
