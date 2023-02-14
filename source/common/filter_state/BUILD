load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "plain_state_impl",
    srcs = ["plain_state.cc"],
    hdrs = ["plain_state.h"],
    repository = "@envoy",
    deps = [
        "//source/common/common:proxy_utility_lib",
        "@envoy//envoy/stream_info:filter_state_interface",
        "@envoy//source/common/common:utility_lib",
    ],
)
