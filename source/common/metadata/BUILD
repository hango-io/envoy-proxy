licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "typed_metadata_lib",
    srcs = ["typed_metadata.cc"],
    hdrs = ["typed_metadata.h"],
    repository = "@envoy",
    deps = [
        "//source/common/common:proxy_utility_lib",
        "@envoy//envoy/config:typed_metadata_interface",
        "@envoy//source/common/common:utility_lib",
        "@envoy//source/common/protobuf",
    ],
)
