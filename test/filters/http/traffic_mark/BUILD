load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "traffic_mark_filter_test",
    srcs = ["traffic_mark_filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/traffic_mark:traffic_mark_filter_lib",
        "@envoy//source/common/stream_info:stream_info_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/traffic_mark:config",
        "@envoy//test/mocks/server:server_mocks",
    ],
)
