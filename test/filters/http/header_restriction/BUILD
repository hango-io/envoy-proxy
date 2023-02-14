load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "header_restriction_test",
    srcs = ["header_restriction.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/header_restriction:filter_lib",
        "@envoy//test/mocks/http:http_mocks",
    ],
)
