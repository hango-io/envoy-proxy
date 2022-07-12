load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

## Pluggable HTTP cache filter

envoy_package()

envoy_cc_library(
    name = "cache_filter_lib",
    srcs = ["cache_filter.cc"],
    hdrs = ["cache_filter.h"],
    copts = [
        "-Wno-error=old-style-cast",
        #"-Wno-error=pessimizing-move",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/super_cache/v2:pkg_cc_proto",
        "//source/common/common:proxy_utility_lib",
        "//source/common/sender:cache_request_sender_lib",
        "@envoy//envoy/registry",
        "@envoy//source/common/common:macros",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/json:json_loader_lib",
        "@envoy//source/common/protobuf",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "super_cache_filter_config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":cache_filter_lib",
        "@envoy//envoy/common:time_interface",
        "@envoy//envoy/registry",
        "@envoy//envoy/stats:stats_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
