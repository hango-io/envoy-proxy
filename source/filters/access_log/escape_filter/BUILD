load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "escape_filter_factory_lib",
    srcs = ["escape_filter.cc"],
    hdrs = ["escape_filter.h"],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/access_log/escape_filter/v2:pkg_cc_proto",
        "//source/common/common:proxy_utility_lib",
        "@envoy//envoy/registry",
        "@envoy//source/common/access_log:access_log_lib",
        "@envoy//source/common/config:utility_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/protobuf",
        "@envoy//source/common/protobuf:message_validator_lib",
    ],
)
