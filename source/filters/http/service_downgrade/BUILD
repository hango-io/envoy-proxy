load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "filter_lib",
    srcs = ["service_downgrade.cc"],
    hdrs = ["service_downgrade.h"],
    copts = [
        "-Wno-error=unused-private-field",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/service_downgrade/v2:pkg_cc_proto",
        "//source/common/http:proxy_base_lib",
        "//source/common/http:proxy_header_lib",
        "//source/common/sender:httpx_request_sender_lib",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    copts = [
        "-Wno-error=unused-parameter",
    ],
    repository = "@envoy",
    deps = [
        ":filter_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
