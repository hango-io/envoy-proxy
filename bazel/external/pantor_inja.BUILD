load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

cc_library(
    name = "inja-lib",
    hdrs = ["single_include/inja/inja.hpp"],
    strip_include_prefix = "single_include/",
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_nlohmann_json//:json",
    ],
)
