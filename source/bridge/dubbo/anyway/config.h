#pragma once

#include "source/common/dubbo/utility.h"

#include "api/proxy/bridge/dubbo/anyway/v3/anyway.pb.h"

namespace Envoy {
namespace Proxy {
namespace Bridge {
namespace Dubbo {
namespace Anyway {

class AnywayArgumentGetterCreatorFactory
    : public Common::DubboBridge::ArgumentGetterCreatorFactory {
public:
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<proxy::bridge::dubbo::anyway::v3::ArgumentGetterConfig>();
  }

  std::string name() const override { return "proxy.dubbo_bridge.argument_getter.anyway"; }

  Common::DubboBridge::ArgumentGetterCreator
  createArgumentGetterCreator(const Protobuf::Message& config) override;
};

class AnywayResponseGetterCreatorFactory
    : public Common::DubboBridge::ResponseGetterCreatorFactory {
public:
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<proxy::bridge::dubbo::anyway::v3::ResponseGetterConfig>();
  }

  std::string name() const override { return "proxy.dubbo_bridge.response_getter.anyway"; }

  Common::DubboBridge::ResponseGetterCreator
  createResponseGetterCreator(const Protobuf::Message& config) override;
};

class BffAnywayArgumentGetterCreatorFactory
    : public Common::DubboBridge::ArgumentGetterCreatorFactory {
public:
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<proxy::bridge::dubbo::anyway::v3::BffArgumentGetterConfig>();
  }

  std::string name() const override { return "proxy.dubbo_bridge.argument_getter.anyway_bff"; }

  Common::DubboBridge::ArgumentGetterCreator
  createArgumentGetterCreator(const Protobuf::Message& config) override;
};

class BffAnywayResponseGetterCreatorFactory
    : public Common::DubboBridge::ResponseGetterCreatorFactory {
public:
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<proxy::bridge::dubbo::anyway::v3::BffResponseGetterConfig>();
  }

  std::string name() const override { return "proxy.dubbo_bridge.response_getter.anyway_bff"; }

  Common::DubboBridge::ResponseGetterCreator
  createResponseGetterCreator(const Protobuf::Message& config) override;
};

class BffDynamicContextCreatorFactory : public Common::DubboBridge::DynamicContextCreatorFactory {
public:
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<proxy::bridge::dubbo::anyway::v3::BffDynamicContextConfig>();
  }
  std::string name() const override { return "proxy.dubbo_bridge.dynamic_context.anyway_bff"; }

  // Create dynamic context creator by configuration.
  Common::DubboBridge::DynamicContextCreator
  createDynamicContextCreator(const Protobuf::Message& config) override;
};

} // namespace Anyway
} // namespace Dubbo
} // namespace Bridge
} // namespace Proxy
} // namespace Envoy
