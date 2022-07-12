load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "redis_cache_impl_lib",
    srcs = ["redis_cache_impl.cc"],
    hdrs = ["redis_cache_impl.h"],
    copts = [
        "-Wno-error=old-style-cast",
        "-Wno-error=defaulted-function-deleted",
        "-Wno-error=unused-function",
    ],
    repository = "@envoy",
    deps = [
        ":cache_interface_lib",
        ":http_cache_entry_lib",
        "//source/common/common:proxy_utility_lib",
        "//source/common/redis:async_redis_client_lib",
        "@com_github_redis_hiredis//:libhiredis",
        "@com_github_sewenew_redisplusplus//:libredisplusplus",
        "@envoy//envoy/registry",
        "@envoy//source/common/common:hex_lib",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/event:dispatcher_lib",
        "@envoy//source/common/http:message_lib",
    ],
)

envoy_cc_library(
    name = "local_cache_impl_lib",
    srcs = ["local_cache_impl.cc"],
    hdrs = ["local_cache_impl.h"],
    repository = "@envoy",
    deps = [
        ":cache_interface_lib",
        "//source/common/common:proxy_utility_lib",
        "@envoy//envoy/registry",
        "@envoy//source/common/common:lock_guard_lib",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/common:thread_lib",
    ],
)

envoy_cc_library(
    name = "cache_interface_lib",
    hdrs = ["cache_base.h"],
    repository = "@envoy",
    deps = [
        "//api/proxy/common/cache_api/v3:pkg_cc_proto",
        "@envoy//envoy/buffer:buffer_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/thread_local:thread_local_interface",
        "@envoy//source/common/common:assert_lib",
    ],
)

envoy_cc_library(
    name = "http_cache_entry_lib",
    hdrs = ["http_cache_entry.h"],
    external_deps = [
        "rapidjson",
    ],
    repository = "@envoy",
    deps = [
        ":cache_interface_lib",
        "//source/common/http:proxy_base_lib",
        "@envoy//source/common/http:message_lib",
    ],
)
