load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "proxy_base_test",
    srcs = ["proxy_base_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/http:proxy_base_lib",
    ],
)

envoy_cc_test(
    name = "proxy_header_test",
    srcs = ["proxy_header_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/http:proxy_header_lib",
    ],
)

envoy_cc_test(
    name = "proxy_matcher_test",
    srcs = ["proxy_matcher_test.cc"],
    repository = "@envoy",
    deps = [
        "//source/common/http:proxy_matcher_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
