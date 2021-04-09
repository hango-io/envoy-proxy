load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
)

licenses(["notice"])  # Apache 2

envoy_cc_binary(
    name = "envoy-proxy-static",
    repository = "@envoy",
    stamped = True,
    deps = [
        # Http filter extensions
        "//source/filters/http/ip_restriction:ip_restriction_filter_config_lib",
        "//source/filters/http/metadata_hub:metadata_hub_filter_config_lib",
        "//source/filters/http/static_downgrade:static_downgrade_factory_lib",
        "//source/filters/http/local_limit:local_limit_factory_lib",
        "//source/filters/http/rider:config_lib",
        # Access log filter extensions.
        "//source/filters/access_log/escape_filter:escape_filter_factory_lib",
        # Envoy.
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)
