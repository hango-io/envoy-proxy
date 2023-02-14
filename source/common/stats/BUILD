load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "stats_rolling_number_lib",
    srcs = ["rolling_number.cc"],
    hdrs = ["rolling_number.h"],
    copts = [
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//envoy/common:time_interface",
        "@envoy//source/common/common:logger_lib",
    ],
)
