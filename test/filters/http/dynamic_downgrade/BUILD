load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "dynamic_downgrade_test",
    srcs = ["dynamic_downgrade_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/dynamic_downgrade:dynamic_downgrade_filter_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)
