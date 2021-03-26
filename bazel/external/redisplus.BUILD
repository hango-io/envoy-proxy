licenses(["notice"])  # Apache 2

cc_library(
    name = "libredisplusplus",
    srcs = glob(
        [
            "src/sw/redis++/*.cc",
            "src/sw/redis++/*.cpp",
        ],
    ),
    hdrs = glob([
        "src/sw/redis++/*.h",
        "src/sw/redis++/*.hpp",
    ]),
    copts = ["-Isrc/sw/redis++"],
    linkstatic = 1,
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_redis_hiredis//:libhiredis",
    ]
)
