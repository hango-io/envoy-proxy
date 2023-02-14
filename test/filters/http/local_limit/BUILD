load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "local_limit_test",
    srcs = ["local_limit_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/local_limit:local_limit_filter_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:factory_context_mocks",
    ],
)
