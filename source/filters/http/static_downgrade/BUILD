load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "static_downgrade_filter_lib",
    srcs = ["static_downgrade_filter.cc"],
    hdrs = ["static_downgrade_filter.h"],
    copts = [
        #"-Wno-error=unused-private-field"
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/static_downgrade/v2:pkg_cc_proto",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/router:router_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/stream_info:stream_info_interface",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:matchers_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/stream_info:utility_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "static_downgrade_factory_lib",
    srcs = ["static_downgrade_factory.cc"],
    hdrs = ["static_downgrade_factory.h"],
    repository = "@envoy",
    deps = [
        ":static_downgrade_filter_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
