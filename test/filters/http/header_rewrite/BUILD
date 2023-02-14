load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "header_rewrite_test",
    srcs = ["header_rewrite_test.cc"],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=tautological-overlap-compare",
        "-Wno-error=non-virtual-dtor",
    ],
    repository = "@envoy",
    deps = [
        "//source/filters/http/header_rewrite:filter_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
    ],
)

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    # These options are necessary for inja build.
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=tautological-overlap-compare",
        "-Wno-error=non-virtual-dtor",
    ],
    repository = "@envoy",
    deps = [
        "//source/filters/http/header_rewrite:config_lib",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/server:server_mocks",
    ],
)
