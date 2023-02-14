load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "proxy_base_lib",
    srcs = ["proxy_base.cc"],
    hdrs = ["proxy_base.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/http:message_lib",
    ],
)

envoy_cc_library(
    name = "proxy_header_lib",
    srcs = ["proxy_header.cc"],
    hdrs = ["proxy_header.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:message_lib",
    ],
)

envoy_cc_library(
    name = "proxy_matcher_lib",
    srcs = ["proxy_matcher.cc"],
    hdrs = ["proxy_matcher.h"],
    repository = "@envoy",
    deps = [
        "//api/proxy/common/matcher/v3:pkg_cc_proto",
        "@envoy//source/common/common:matchers_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:path_utility_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/router:config_utility_lib",
    ],
)
