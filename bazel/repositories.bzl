load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "new_git_repository",
)
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load(":repository_locations.bzl", "REPOSITORY_LOCATIONS")

# function copied from envoy: @envoy/bazel/repositories.bzl
def _repository_impl(name, **kwargs):
    # `existing_rule_keys` contains the names of repositories that have already
    # been defined in the Bazel workspace. By skipping repos with existing keys,
    # users can override dependency versions by using standard Bazel repository
    # rules in their WORKSPACE files.
    existing_rule_keys = native.existing_rules().keys()
    if name in existing_rule_keys:
        return

    location = REPOSITORY_LOCATIONS[name]

    # Git tags are mutable. We want to depend on commit IDs instead. Give the
    # user a useful error if they accidentally specify a tag.
    if "tag" in location:
        fail(
            "Refusing to depend on Git tag %r for external dependency %r: use 'commit' instead." %
            (location["tag"], name),
        )

    if "commit" in location:
        # Git repository at given commit ID.
        new_git_repository(
            name = name,
            remote = location["remote"],
            commit = location["commit"],
            **kwargs
        )
    elif "single_include" in location:
        http_file(
            name = name,
            urls = location["urls"],
            sha256 = location["sha256"],
            **kwargs
        )
    else:
        http_archive(
            name = name,
            urls = location["urls"],
            sha256 = location["sha256"],
            strip_prefix = location.get("strip_prefix", ""),
            **kwargs
        )

def envoy_netease_dependencies():
    _repository_impl(
        "com_github_edenhill_librdkafka",
        build_file = "@envoy_netease//bazel/external:librdkafka.BUILD",
    )
    _repository_impl(
        "com_github_redis_hiredis",
        build_file = "@envoy_netease//bazel/external:hiredis.BUILD",
    )
    _repository_impl(
        "com_github_sewenew_redisplusplus",
        build_file = "@envoy_netease//bazel/external:redisplus.BUILD",
    )
    _repository_impl(
        "com_github_pantor_inja",
        build_file = "@envoy_netease//bazel/external:pantor_inja.BUILD",
    )
    _repository_impl(
        "com_github_spiderlabs_libmodsecurity",
        build_file = "@envoy_netease//bazel/external:libmodsecurity.BUILD",
    )
    _repository_impl(
        "com_github_alibaba_hessian2",
    )
