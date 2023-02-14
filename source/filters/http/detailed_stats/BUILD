load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "filter_lib",
    srcs = ["detailed_stats.cc"],
    hdrs = ["detailed_stats.h"],
    copts = [
        "-Wno-error=unused-private-field",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/detailed_stats/v2:pkg_cc_proto",
        "//source/common/http:proxy_matcher_lib",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/runtime:runtime_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/stats:stats_interface",
        "@envoy//source/common/common:enum_to_int",
        "@envoy//source/common/common:utility_lib",
        "@envoy//source/common/config:utility_lib",
        "@envoy//source/common/protobuf:message_validator_lib",
        "@envoy//source/common/stream_info:utility_lib",
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
