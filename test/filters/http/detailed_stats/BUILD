load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "filter_test",
    srcs = ["detailed_stats_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/detailed_stats:filter_lib",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/test_common:test_runtime_lib",
        "@envoy_api//envoy/type/v3:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/detailed_stats:config_lib",
        "@envoy//test/mocks/server:factory_context_mocks",
        "@envoy//test/test_common:test_runtime_lib",
    ],
)
