load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "ip_restriction_filter_lib",
    srcs = ["ip_restriction.cc"],
    hdrs = ["ip_restriction.h"],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/ip_restriction/v2:pkg_cc_proto",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/stats:stats_interface",
        "@envoy//envoy/stats:stats_macros",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/network:address_lib",
        "@envoy//source/common/network:cidr_range_lib",
        "@envoy//source/common/network:utility_lib",
        "@envoy//source/common/singleton:const_singleton",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "ip_restriction_filter_config_lib",
    srcs = ["ip_restriction_config.cc"],
    hdrs = ["ip_restriction_config.h"],
    repository = "@envoy",
    deps = [
        ":ip_restriction_filter_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
