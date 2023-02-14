load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "circuit_breaker_filter_test",
    srcs = ["circuit_breaker_filter_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/circuit_breaker:circuit_breaker_filter_common_lib",
        "//source/filters/http/circuit_breaker:circuit_breaker_filter_lib",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:registry_lib",
        "@envoy//test/test_common:simulated_time_system_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/circuit_breaker:circuit_breaker_filter_config_lib",
        "//source/filters/http/circuit_breaker:circuit_breaker_filter_lib",
        "@envoy//source/common/protobuf:utility_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)
