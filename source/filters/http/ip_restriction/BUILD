licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "ip_restriction_filter_lib",
    srcs = ["ip_restriction.cc"],
    hdrs = ["ip_restriction.h"],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/ip_restriction/v2:pkg_cc_proto",
        "//source/filters/http:well_known_names",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "ip_restriction_filter_config_lib",
    srcs = ["ip_restriction_config.cc"],
    hdrs = ["ip_restriction_config.h"],
    repository = "@envoy",
    deps = [
        ":ip_restriction_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface",
    ],
)
