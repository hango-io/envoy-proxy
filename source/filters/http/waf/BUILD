licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "waf_filter_lib",
    srcs = ["waf.cc"],
    hdrs = ["waf.h"],
    copts = [
        "-Wno-error=unused-private-field",
        "-Wno-unused-function",
        "-Wno-unused-parameter",
        "-Wno-reorder",
        "-Wno-unused-variable",
        "-Wno-sign-compare",
        "-Wno-non-virtual-dtor",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/waf/v2:pkg_cc_proto",
        "@com_github_spiderlabs_libmodsecurity//:libmodsecurity-lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-unused-function",
        "-Wno-unused-parameter",
        "-Wno-reorder",
        "-Wno-unused-variable",
        "-Wno-sign-compare",
        "-Wno-non-virtual-dtor",
    ],
    repository = "@envoy",
    deps = [
        ":waf_filter_lib",
        "@envoy//envoy/server:filter_config_interface",
    ],
)
