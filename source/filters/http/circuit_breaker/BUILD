load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "circuit_breaker_filter_lib",
    srcs = ["filter.cc"],
    hdrs = ["filter.h"],
    repository = "@envoy",
    deps = [
        ":circuit_breaker_filter_common_lib",
        "//api/proxy/filters/http/circuit_breaker/v2:pkg_cc_proto",
        "//source/common/stats:stats_rolling_number_lib",
        "@envoy//envoy/api:api_interface",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/runtime:runtime_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/stats:stats_interface",
        "@envoy//envoy/stats:stats_macros",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/http:codes_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "circuit_breaker_filter_common_lib",
    srcs = [
        "common.cc",
        "impl.cc",
    ],
    hdrs = [
        "common.h",
        "impl.h",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/circuit_breaker/v2:pkg_cc_proto",
        "//source/common/stats:stats_rolling_number_lib",
        "@envoy//envoy/api:api_interface",
        "@envoy//envoy/runtime:runtime_interface",
        "@envoy//source/common/protobuf:utility_lib",
    ],
)

envoy_cc_library(
    name = "circuit_breaker_filter_config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":circuit_breaker_filter_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
