load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "proxy_util_lib",
    hdrs = ["proxy_util.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/common:utility_lib",
    ],
)

envoy_cc_library(
    name = "proxy_utility_lib",
    srcs = ["proxy_utility.cc"],
    hdrs = ["proxy_utility.h"],
    copts = [
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/common:utility_lib",
    ],
)

envoy_cc_library(
    name = "thread_safe_token_bucket_impl_lib",
    hdrs = [
        "proxy_likely.h",
        "thread_safe_token_bucket_impl.h",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/common:token_bucket_impl_lib",
    ],
)

envoy_cc_library(
    name = "proxy_token_bucket_lib",
    srcs = ["token_bucket.cc"],
    hdrs = ["token_bucket.h"],
    copts = [
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//source/exe:envoy_common_lib",
    ],
)
