licenses(["notice"])  # Apache 2

# This library is for internal hiredis use, because hiredis assumes a
# different include prefix for itself than external libraries do.
cc_library(
    name = "hiredis_deps",
    hdrs = [
        "dict.c",
        "dict.h",
        "fmacros.h",
    ],
)

cc_library(
    name = "libhiredis",
    srcs = glob(
        [
            "*.h",
            "*.c",
            "adapters/*.c",
            "adapters/*.h",
        ],
        exclude = ["test.c"],
    ),
    hdrs = glob([
        "*.h",
        "adapters/*.h",
    ]),
    include_prefix = "hiredis",
    visibility = ["//visibility:public"],
    deps = [
        ":hiredis_deps",
        "//external:ssl",
    ],
)
