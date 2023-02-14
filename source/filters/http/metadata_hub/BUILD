load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "metadata_hub_filter_lib",
    srcs = ["metadata_hub.cc"],
    hdrs = ["metadata_hub.h"],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/metadata_hub/v2:pkg_cc_proto",
        "//source/common/filter_state:plain_state_impl",
        "//source/common/metadata:typed_metadata_lib",
        "@envoy//envoy/config:typed_metadata_interface",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/router:router_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/stats:stats_interface",
        "@envoy//envoy/stats:stats_macros",
        "@envoy//envoy/stream_info:filter_state_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:path_utility_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "metadata_hub_filter_config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":metadata_hub_filter_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
