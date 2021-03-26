licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "kafka_producer_lib",
    srcs = ["producer.cc"],
    hdrs = ["producer.h"],
    repository = "@envoy",
    deps = [
        "@com_github_edenhill_librdkafka//:librdkafka-lib",
        "@envoy//source/common/common:logger_lib",
    ],
     
    copts = [
            "-Wno-error=old-style-cast"
            ],
)
