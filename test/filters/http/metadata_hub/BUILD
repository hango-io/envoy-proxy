load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "metadata_hub_test",
    srcs = ["metadata_hub_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/filters/http/metadata_hub:metadata_hub_filter_lib",
        "@envoy//test/mocks/http:http_mocks",
    ],
)
