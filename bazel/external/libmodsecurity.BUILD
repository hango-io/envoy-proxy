load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")

licenses(["notice"])  # Apache 2

genrule(
    name = "libmodsecurity",
    srcs = glob(["**"]),
    outs = [
        "libmodsecurity.a",
    ],
    cmd = "cd external/com_github_spiderlabs_libmodsecurity \
        && ./build.sh && ./configure && make \
        && mv src/.libs/libmodsecurity.a ../../$(@D)",
)

cc_import(
    name = "libmodsecurity-c-lib",
    static_library = ":libmodsecurity.a",
)

cc_library(
    name = "libmodsecurity-lib",
    hdrs = glob(["headers/**/*.h"]),
    linkopts = ["-lpcre"],
    strip_include_prefix = "headers/",
    visibility = ["//visibility:public"],
    deps = [
        ":libmodsecurity-c-lib",
    ],
)
