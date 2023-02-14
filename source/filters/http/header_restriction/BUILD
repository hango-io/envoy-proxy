load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "filter_lib",
    srcs = ["header_restriction.cc"],
    hdrs = ["header_restriction.h"],
    copts = [
        "-Wno-error=unused-private-field",
        "-Wno-error=unused-parameter",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/header_restriction/v2:pkg_cc_proto",
        "//source/common/http:proxy_matcher_lib",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/network:address_lib",
        "@envoy//source/common/network:utility_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
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
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
