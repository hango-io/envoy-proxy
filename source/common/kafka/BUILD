load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)
load("@envoy//bazel:envoy_internal.bzl", "envoy_external_dep_path")

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "kafka_producer_lib",
    srcs = ["producer.cc"],
    hdrs = ["producer.h"],
    copts = [
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        envoy_external_dep_path("librdkafka"),
        "@envoy//source/common/common:logger_lib",
    ],
)
