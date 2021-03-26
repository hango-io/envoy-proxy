load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "proxy_utility_test",
    srcs = ["proxy_utility_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/common:proxy_utility_lib",
    ],
)
