#!/usr/bin/python
# -*- coding: UTF-8 -*-

HTTP_FILTER_CONFIG_CC_T = """
#include "source/filters/http/${SNAKE_CASE_NAME}/config.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace ${CAMEL_CASE_NAME} {

Http::FilterFactoryCb
Factory::createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config,
                                           const std::string& stats_prefix,
                                           Server::Configuration::FactoryContext& context) {
  auto shared_config = std::make_shared<CommonConfig>(proto_config);
  return [shared_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(Http::StreamFilterSharedPtr{new Filter(shared_config.get())});
  };
}

Router::RouteSpecificFilterConfigConstSharedPtr
Factory::createRouteSpecificFilterConfigTyped(const ProtoRouteConfig& proto_config,
                                              Server::Configuration::ServerFactoryContext&,
                                              ProtobufMessage::ValidationVisitor&) {
  return std::make_shared<RouteConfig>(proto_config);
}

REGISTER_FACTORY(Factory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace ${CAMEL_CASE_NAME}
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy

"""

HTTP_FILTER_CONFIG_H_T = """
#pragma once

#include "envoy/registry/registry.h"

#include "source/extensions/filters/http/common/factory_base.h"

#include "source/filters/http/${SNAKE_CASE_NAME}/${SNAKE_CASE_NAME}.h"

#include "api/proxy/filters/http/${SNAKE_CASE_NAME}/v2/${SNAKE_CASE_NAME}.pb.h"
#include "api/proxy/filters/http/${SNAKE_CASE_NAME}/v2/${SNAKE_CASE_NAME}.pb.validate.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace ${CAMEL_CASE_NAME} {

class Factory
    : public Extensions::HttpFilters::Common::FactoryBase<ProtoCommonConfig, ProtoRouteConfig> {
public:
  Factory() : FactoryBase(Filter::name()) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const ProtoCommonConfig& proto_config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;

  Router::RouteSpecificFilterConfigConstSharedPtr
  createRouteSpecificFilterConfigTyped(const ProtoRouteConfig&,
                                       Server::Configuration::ServerFactoryContext&,
                                       ProtobufMessage::ValidationVisitor&) override;
};

} // namespace ${CAMEL_CASE_NAME}
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy

"""

HTTP_FILTER_FILTER_CC_T = """
#include "source/filters/http/${SNAKE_CASE_NAME}/${SNAKE_CASE_NAME}.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace ${CAMEL_CASE_NAME} {

Http::FilterHeadersStatus Filter::decodeHeaders(Http::RequestHeaderMap&, bool) {
  ENVOY_LOG(debug, "Hello, my first envoy C++ decode filter.");
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus Filter::encodeHeaders(Http::ResponseHeaderMap&, bool) {
  ENVOY_LOG(debug, "Hello, my first envoy C++ encode filter.");
  return Http::FilterHeadersStatus::Continue;
}

} // namespace ${CAMEL_CASE_NAME}
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy

"""

HTTP_FILTER_FILTER_H_T = """
#pragma once

#include <string>

#include "envoy/http/filter.h"
#include "envoy/server/filter_config.h"

#include "source/extensions/filters/http/common/pass_through_filter.h"

#include "api/proxy/filters/http/${SNAKE_CASE_NAME}/v2/${SNAKE_CASE_NAME}.pb.h"

namespace Envoy {
namespace Proxy {
namespace HttpFilters {
namespace ${CAMEL_CASE_NAME} {

using ProtoCommonConfig = proxy::filters::http::${SNAKE_CASE_NAME}::v2::ProtoCommonConfig;
using ProtoRouteConfig = proxy::filters::http::${SNAKE_CASE_NAME}::v2::ProtoRouteConfig;

class RouteConfig : public Router::RouteSpecificFilterConfig, Logger::Loggable<Logger::Id::filter> {
public:
  RouteConfig(const ProtoRouteConfig& config) : config_(config) {}

private:
  ProtoRouteConfig config_;
};

class CommonConfig : Logger::Loggable<Logger::Id::filter> {
public:
  CommonConfig(const ProtoCommonConfig& config) : config_(config) {}

private:
  ProtoCommonConfig config_;
};

/*
 * Filter inherits from PassthroughFilter instead of StreamFilter because PassthroughFilter
 * implements all the StreamFilter's virtual mehotds by default. This reduces a lot of unnecessary
 * code.
 */
class Filter : public Envoy::Http::PassThroughFilter, Logger::Loggable<Logger::Id::filter> {
public:
  Filter(CommonConfig* config) : config_(config){};

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap&, bool) override;

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap&, bool) override;

  static const std::string& name() {
    CONSTRUCT_ON_FIRST_USE(std::string, "proxy.filters.http.${SNAKE_CASE_NAME}");
  }

private:
  CommonConfig* config_{nullptr};
};

} // namespace ${CAMEL_CASE_NAME}
} // namespace HttpFilters
} // namespace Proxy
} // namespace Envoy

"""

HTTP_FILTER_FILTER_BUILD_T = """
licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "filter_lib",
    srcs = ["${SNAKE_CASE_NAME}.cc"],
    hdrs = ["${SNAKE_CASE_NAME}.h"],
    copts = [
        "-Wno-error=unused-private-field",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/${SNAKE_CASE_NAME}/v2:pkg_cc_proto",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    copts = [
        "-Wno-error=unused-parameter",
    ],
    repository = "@envoy",
    deps = [
        ":filter_lib",
        "@envoy//envoy/server:filter_config_interface",
    ],
)

"""

HTTP_FILTER_PROTO_BUILD_T = """
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_package")

licenses(["notice"])  # Apache 2

api_proto_package()

"""

HTTP_FILTER_PROTO_T = """
syntax = "proto3";

package proxy.filters.http.${SNAKE_CASE_NAME}.v2;

option java_package = "proxy.filters.http.${SNAKE_CASE_NAME}.v2";
option java_outer_classname = "${CAMEL_CASE_NAME}Proto";
option java_multiple_files = true;

import "validate/validate.proto";

message ProtoCommonConfig {
}

message ProtoRouteConfig {
}

"""
