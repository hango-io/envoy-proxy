load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "request_sender_interface",
    hdrs = ["request_sender.h"],
    copts = [
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//envoy/http:async_client_interface",
        "@envoy//envoy/http:header_map_interface",
        "@envoy//source/common/http:headers_lib",
    ],
)

envoy_cc_library(
    name = "httpx_request_sender_lib",
    srcs = ["httpx_request_sender.cc"],
    hdrs = ["httpx_request_sender.h"],
    copts = [
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        ":request_sender_interface",
        "@envoy//envoy/upstream:cluster_manager_interface",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:message_lib",
    ],
)

envoy_cc_library(
    name = "cache_request_sender_lib",
    srcs = ["cache_request_sender.cc"],
    hdrs = ["cache_request_sender.h"],
    copts = [
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        ":request_sender_interface",
        "//api/proxy/common/cache_api/v3:pkg_cc_proto",
        "//source/common/cache:local_cache_impl_lib",
        "//source/common/cache:redis_cache_impl_lib",
        "//source/common/common:proxy_utility_lib",
        "//source/common/http:proxy_header_lib",
        "@envoy//envoy/http:header_map_interface",
        "@envoy//source/common/common:empty_string",
        "@envoy//source/common/common:hex_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:message_lib",
        "@envoy//source/common/http:path_utility_lib",
        "@envoy//source/common/http:utility_lib",
    ],
)
