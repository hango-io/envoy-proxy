load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_test(
    name = "filter_test",
    srcs = ["filter_test.cc"],
    data = [
        "testlib.lua",
    ],
    repository = "@envoy",
    deps = [
        "//source/filters/http/rider:filter_lib",
        "@envoy//source/common/stream_info:stream_info_lib",
        "@envoy//test/mocks/access_log:access_log_mocks",
        "@envoy//test/mocks/api:api_mocks",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:environment_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "filter_test_v2",
    srcs = ["filter_test_v2.cc"],
    data = [
        "testlib_v2.lua",
    ],
    repository = "@envoy",
    deps = [
        "//source/filters/http/rider:filter_lib",
        "@envoy//source/common/stream_info:stream_info_lib",
        "@envoy//test/mocks/access_log:access_log_mocks",
        "@envoy//test/mocks/api:api_mocks",
        "@envoy//test/mocks/http:http_mocks",
        "@envoy//test/mocks/thread_local:thread_local_mocks",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:environment_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    data = [
        "testlib.lua",
    ],
    repository = "@envoy",
    deps = [
        "//source/filters/http/rider:config_lib",
        "@envoy//test/mocks/server:server_mocks",
        "@envoy//test/test_common:environment_lib",
    ],
)
