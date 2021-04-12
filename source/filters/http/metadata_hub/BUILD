licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

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
        "@envoy//include/envoy/stream_info:filter_state_interface",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "metadata_hub_filter_config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":metadata_hub_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface",
    ],
)
