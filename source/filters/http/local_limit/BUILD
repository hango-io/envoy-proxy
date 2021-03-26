licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "local_limit_filter_lib",
    srcs = ["local_limit_filter.cc"],
    hdrs = ["local_limit_filter.h"],
    copts = [
        "-Wno-error=unused-private-field",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/filters/http/local_limit/v2:pkg_cc_proto",
        "//source/common/common:proxy_token_bucket_lib",
        "//source/filters/http:well_known_names",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "local_limit_factory_lib",
    srcs = ["local_limit_factory.cc"],
    hdrs = ["local_limit_factory.h"],
    repository = "@envoy",
    deps = [
        ":local_limit_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface",
    ],
)
