licenses(["notice"])  # Apache 2

# librdkafka编译出来的C++库不包含所依赖的C库
# 所以两个库都必须添加并链接且链接顺序固定

genrule(
    name = "librdkafka",
    srcs = glob(["**"]),
    outs = [
        "librdkafka++.a",
        "librdkafka.a",
    ],
    cmd = "cd external/com_github_edenhill_librdkafka \
        && ./configure --disable-ssl --disable-zstd && make \
        && mv src-cpp/librdkafka++.a ../../$(@D) \
        && mv src/librdkafka.a ../../$(@D)",
)

cc_import(
    name = "librdkafka-c-lib",
    static_library = ":librdkafka.a",
)

cc_import(
    name = "librdkafka-c++-lib",
    static_library = ":librdkafka++.a",
)

cc_library(
    name = "librdkafka-lib",
    hdrs = ["src-cpp/rdkafkacpp.h"],
    strip_include_prefix = "src-cpp/",
    visibility = ["//visibility:public"],
    deps = [
        ":librdkafka-c++-lib",
        ":librdkafka-c-lib",
    ],
)
