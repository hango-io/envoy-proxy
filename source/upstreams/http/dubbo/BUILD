load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "config",
    srcs = [
        "config.cc",
    ],
    hdrs = [
        "config.h",
    ],
    repository = "@envoy",
    deps = [
        ":upstream_request_lib",
        "//api/proxy/upstreams/http/dubbo/v3:pkg_cc_proto",
    ],
)

envoy_cc_library(
    name = "upstream_request_lib",
    srcs = [
        "upstream_request.cc",
    ],
    hdrs = [
        "upstream_request.h",
    ],
    external_deps = [
        "rapidjson",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/upstreams/http/dubbo/v3:pkg_cc_proto",
        "//source/common/dubbo:utility_lib",
        "@com_github_alibaba_hessian2//hessian2:codec_impl_lib",
        "@com_github_alibaba_hessian2//hessian2/basic_codec:object_codec_lib",
        "@envoy//envoy/http:codes_interface",
        "@envoy//envoy/http:filter_interface",
        "@envoy//envoy/upstream:upstream_interface",
        "@envoy//source/common/common:assert_lib",
        "@envoy//source/common/common:minimal_logger_lib",
        "@envoy//source/common/common:utility_lib",
        "@envoy//source/common/http:codes_lib",
        "@envoy//source/common/http:header_map_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:message_lib",
        "@envoy//source/common/network:application_protocol_lib",
        "@envoy//source/common/network:transport_socket_options_lib",
        "@envoy//source/common/router:router_lib",
        "@envoy//source/common/upstream:load_balancer_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:dubbo_protocol_impl_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:hessian_utils_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:message_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:metadata_lib",
    ],
)
