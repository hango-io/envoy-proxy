load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "dynamic_downgrade_filter_lib",
    srcs = ["dynamic_downgrade_filter.cc"],
    hdrs = ["dynamic_downgrade_filter.h"],
    copts = [
        "-Wno-error=old-style-cast",
        #"-Wno-error=pessimizing-move",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/dynamic_downgrade/v2:pkg_cc_proto",
        "//source/common/cache:redis_cache_impl_lib",
        "//source/common/common:proxy_utility_lib",
        "//source/common/http:proxy_base_lib",
        "//source/common/http:proxy_header_lib",
        "//source/common/sender:cache_request_sender_lib",
        "//source/common/sender:httpx_request_sender_lib",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/router:router_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:message_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/json:json_loader_lib",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "dynamic_downgrade_factory_lib",
    srcs = ["dynamic_downgrade_factory.cc"],
    hdrs = ["dynamic_downgrade_factory.h"],
    repository = "@envoy",
    deps = [
        ":dynamic_downgrade_filter_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
