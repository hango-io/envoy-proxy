#!/usr/bin/python
# -*- coding: UTF-8 -*-

NETWORK_FILTER_CONFIG_CC_T = """
#include "source/filters/network/${SNAKE_CASE_NAME}/config.h"

namespace Envoy {
namespace Proxy {
namespace NetworkFilters {
namespace ${CAMEL_CASE_NAME} {

Envoy::Network::FilterFactoryCb
Factory::createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config,
                                           Envoy::Server::Configuration::FactoryContext& context) {
  auto config = std::make_shared<CommonConfig>(proto_config, context);
  return [config, &context](Envoy::Network::FilterManager& filter_manager) -> void {
    filter_manager.addReadFilter(std::make_shared<Filter>(config, context));
  };
}

REGISTER_FACTORY(Factory, Envoy::Server::Configuration::NamedNetworkFilterConfigFactory);

} // namespace ${CAMEL_CASE_NAME}
} // namespace NetworkFilters
} // namespace Proxy
} // namespace Envoy

"""

NETWORK_FILTER_CONFIG_H_T = """
#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/network/common/factory_base.h"

#include "source/filters/network/${SNAKE_CASE_NAME}/${SNAKE_CASE_NAME}.h"

#include "api/proxy/filters/network/${SNAKE_CASE_NAME}/v2/${SNAKE_CASE_NAME}.pb.h"
#include "api/proxy/filters/network/${SNAKE_CASE_NAME}/v2/${SNAKE_CASE_NAME}.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace NetworkFilters {
namespace ${CAMEL_CASE_NAME} {

class Factory : public Envoy::Extensions::NetworkFilters::Common::FactoryBase<ProtoCommonConfig> {
public:
  Factory() : FactoryBase(Filter::name()) {}

  Envoy::Network::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config,
                                    Envoy::Server::Configuration::FactoryContext& context) override;
};

} // namespace ${CAMEL_CASE_NAME}
} // namespace NetworkFilters
} // namespace Proxy
} // namespace Envoy

"""

NETWORK_FILTER_FILTER_CC_T = """
#include "source/filters/network/${SNAKE_CASE_NAME}/${SNAKE_CASE_NAME}.h"

namespace Envoy {
namespace Proxy {
namespace NetworkFilters {
namespace ${CAMEL_CASE_NAME} {

Envoy::Network::FilterStatus Filter::onData(Envoy::Buffer::Instance&, bool) {
  return Envoy::Network::FilterStatus::Continue;
}

} // namespace ${CAMEL_CASE_NAME}
} // namespace NetworkFilters
} // namespace Proxy
} // namespace Envoy

"""

NETWORK_FILTER_FILTER_H_T = """
#pragma once

#include <memory>
#include <string>

#include "envoy/network/filter.h"
#include "envoy/server/factory_context.h"

#include "source/common/common/logger.h"

#include "api/proxy/filters/network/${SNAKE_CASE_NAME}/v2/${SNAKE_CASE_NAME}.pb.h"
#include "api/proxy/filters/network/${SNAKE_CASE_NAME}/v2/${SNAKE_CASE_NAME}.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace NetworkFilters {
namespace ${CAMEL_CASE_NAME} {

using ProtoCommonConfig = proxy::filters::network::${SNAKE_CASE_NAME}::v2::ProtoCommonConfig;

class CommonConfig {
public:
  CommonConfig(const ProtoCommonConfig&, Envoy::Server::Configuration::FactoryContext&) {}
};

using CommonConfigSharedPtr = std::shared_ptr<CommonConfig>;

class Filter : public Envoy::Network::ReadFilter,
               public Envoy::Logger::Loggable<Envoy::Logger::Id::filter> {
public:
  Filter(CommonConfigSharedPtr config, Envoy::Server::Configuration::FactoryContext& context)
      : config_(std::move(config)), context_(context) {}

  Envoy::Network::FilterStatus onData(Envoy::Buffer::Instance& data, bool end_stream) override;

  Envoy::Network::FilterStatus onNewConnection() override {
    return Envoy::Network::FilterStatus::Continue;
  }

  void initializeReadFilterCallbacks(Envoy::Network::ReadFilterCallbacks& callbacks) override {
    callbacks_ = &callbacks;
  }

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.network.${SNAKE_CASE_NAME}");
  }

private:
  Envoy::Network::ReadFilterCallbacks* callbacks_{nullptr};

  CommonConfigSharedPtr config_{};

  Envoy::Server::Configuration::FactoryContext& context_;
};

} // namespace ${CAMEL_CASE_NAME}
} // namespace NetworkFilters
} // namespace Proxy
} // namespace Envoy

"""

NETWORK_FILTER_FILTER_BUILD_T = """
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "filter_lib",
    srcs = [
        "${SNAKE_CASE_NAME}.cc",
    ],
    hdrs = [
        "${SNAKE_CASE_NAME}.h",
    ],
    copts = [
        "-Wno-error=unused-private-field",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/network/${SNAKE_CASE_NAME}/v2:pkg_cc_proto",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "factory_lib",
    srcs = [
        "config.cc",
    ],
    hdrs = [
        "config.h",
    ],
    repository = "@envoy",
    deps = [
        ":filter_lib",
        "@envoy//envoy/server:filter_config_interface",
    ],
)

"""

NETWORK_FILTER_PROTO_BUILD_T = """
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

licenses(["notice"])  # Apache 2

api_proto_package()

"""

NETWORK_FILTER_PROTO_T = """
syntax = "proto3";

package proxy.filters.network.${SNAKE_CASE_NAME}.v2;

option java_package = "proxy.filters.network.${SNAKE_CASE_NAME}.v2";
option java_outer_classname = "${CAMEL_CASE_NAME}Proto";
option java_multiple_files = true;

import "validate/validate.proto";

message ProtoCommonConfig {
}

"""
