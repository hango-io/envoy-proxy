load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "plain_state_test",
    srcs = ["plain_state_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/filter_state:plain_state_impl",
    ],
)
