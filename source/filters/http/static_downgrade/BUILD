licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

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
        "//source/filters/http:well_known_names",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "static_downgrade_factory_lib",
    srcs = ["static_downgrade_factory.cc"],
    hdrs = ["static_downgrade_factory.h"],
    repository = "@envoy",
    deps = [
        ":static_downgrade_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface",
    ],
)
