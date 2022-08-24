load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "filter_lib",
    srcs = ["header_rewrite.cc"],
    hdrs = ["header_rewrite.h"],
    copts = [
        "-Wno-error=unused-private-field",
        "-Wno-error=tautological-overlap-compare",
        "-Wno-error=non-virtual-dtor",
        "-Wno-error=unused-parameter",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/common/matcher/v3:pkg_cc_proto",
        "//api/proxy/filters/http/header_rewrite/v2:pkg_cc_proto",
        "//source/common/http:proxy_matcher_lib",
        "@com_github_nlohmann_json//:json",
        "@com_github_pantor_inja//:inja-lib",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/http:query_params_interface",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/upstream:upstream_interface",
        "@envoy//source/common/common:empty_string",
        "@envoy//source/common/common:regex_lib",
        "@envoy//source/common/config:metadata_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:header_utility_lib",
        "@envoy//source/common/http:path_utility_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/protobuf:message_validator_lib",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=tautological-overlap-compare",
        "-Wno-error=non-virtual-dtor",
        "-Wno-error=unused-parameter",
    ],
    repository = "@envoy",
    deps = [
        ":filter_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
    ],
)
