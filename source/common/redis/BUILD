load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "redis_client_lib",
    srcs = ["client.cc"],
    hdrs = ["client.h"],
    copts = [
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        "@com_github_redis_hiredis//:libhiredis",
        "@com_github_sewenew_redisplusplus//:libredisplusplus",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/common:utility_lib",
    ],
)

envoy_cc_library(
    name = "async_redis_client_lib",
    srcs = ["async_client.cc"],
    hdrs = ["async_client.h"],
    copts = [
        "-Wno-error=old-style-cast",
        "-Wno-error=unused-function",
    ],
    repository = "@envoy",
    deps = [
        "@com_github_redis_hiredis//:libhiredis",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/event:dispatcher_lib",
    ],
)
