licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "filter_lib",
    srcs = [
        "filter.cc",
        "vm.cc",
        "context.cc",
        "common.cc",
        "ffi.cc",
    ],
    hdrs = [
        "filter.h",
        "vm.h",
        "context.h",
        "common.h",
        "context_interfaces.h",
        "capi.h",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//envoy/http:codes_interface",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/http:header_map_interface",
        "@envoy//envoy/stream_info:stream_info_interface",
        "@envoy//envoy/thread_local:thread_local_interface",
        "@envoy//envoy/upstream:cluster_manager_interface",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/common:enum_to_int",
        "@envoy//source/common/config:utility_lib",
        "@envoy//source/common/crypto:utility_lib",
        "@envoy//source/common/http:message_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//source/common/protobuf:message_validator_lib",
        "@envoy//source/extensions/filters/common/lua:wrappers_lib",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:c_smart_ptr_lib",
        "@envoy//source/common/config:datasource_lib",
        "@envoy//source/common/singleton:const_singleton",
        "@envoy//source/common/http:header_utility_lib",
        "//api/proxy/filters/http/rider/v3alpha1:pkg_cc_proto",
    ],
    external_deps = [
        "luajit",
    ],
    copts = [
        "-Wno-error=old-style-cast",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/http:utility_lib",
        "//source/filters/http/rider:filter_lib",
    ],
    copts = [
        "-Wno-error=old-style-cast",
    ]
)
