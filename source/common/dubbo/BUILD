load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "utility_lib",
    srcs = [
        "utility.cc",
    ],
    hdrs = [
        "utility.h",
    ],
    copts = [
        "-Wno-old-style-cast",
        "-Wno-unused-parameter",
        "-D USE_VECTOR_MOCK_MAP",
    ],
    external_deps = [
        "rapidjson",
    ],
    repository = "@envoy",
    deps = [
        "//api/proxy/upstreams/http/dubbo/v3:pkg_cc_proto",
        "@com_github_alibaba_hessian2//hessian2:codec_impl_lib",
        "@com_github_alibaba_hessian2//hessian2/basic_codec:object_codec_lib",
        "@envoy//envoy/http:header_map_interface",
        "@envoy//envoy/network:connection_interface",
        "@envoy//envoy/network:filter_interface",
        "@envoy//envoy/registry",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/common:logger_lib",
        "@envoy//source/common/http:codes_lib",
        "@envoy//source/common/http:headers_lib",
        "@envoy//source/common/http:utility_lib",
        "@envoy//source/common/network:connection_lib",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:dubbo_hessian2_serializer_impl_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:dubbo_protocol_impl_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:hessian_utils_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:message_lib",
        "@envoy//source/extensions/filters/network/dubbo_proxy:metadata_lib",
    ],
)
