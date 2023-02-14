#!/usr/bin/env python3

# Simple script to download all deps and compress them into *.tar.gz file.

import time
import importlib
import os.path
import requests
import tarfile
import hashlib

RULES_CAN_HANDLE = [
    "@bazel_tools//tools/build_defs/repo:http.bzl%http_archive",
    "@envoy//bazel:genrule_repository.bzl%genrule_repository",
    "@bazel_tools//tools/build_defs/repo:http.bzl%http_file",
]

IGNORE_BY_BUILDS = [
    "@bazel_tools//tools/jdk:jdk.BUILD",
]


def resolved_deps(tmp_path):
    if (os.path.exists(tmp_path)):
        os.remove(tmp_path)
    os.system("bazel sync --experimental_repository_resolved_file={}".format(
        tmp_path))


# Get download url and related sha256 of deps.
def get_url_sha256(dep_attr):
    url = ""
    sha = ""

    if ("url" in dep_attr):
        url = dep_attr["url"]

    if (url == "" and "urls" in dep_attr):
        url = dep_attr["urls"][0]

    if ("sha256" in dep_attr):
        sha = dep_attr["sha256"]

    return url, sha


def make_dep_repos(deps_output_path):
    module = importlib.machinery.SourceFileLoader(
        "deps", deps_output_path).load_module()

    result = {}

    for repo in module.resolved:
        rule_class = repo["original_rule_class"]
        attributes = repo["original_attributes"]

        if (rule_class not in RULES_CAN_HANDLE):
            continue
        if ("build_file" in attributes
                and attributes["build_file"] in IGNORE_BY_BUILDS):
            continue

        url, sha = get_url_sha256(attributes)
        result[url] = sha

    return result


def url_download(url, sha, path):
    if (url == "" or sha == "" or path == ""):
        return

    print("download dep to {}: {}({}) .....".format(path, url, sha))
    download_response = requests.get(url)

    true_sha = hashlib.sha256(download_response.content).hexdigest()
    print("expected: {} actual result: {}".format(sha, true_sha))

    if (sha != true_sha):
        print("downloaded content error and ignore it")
        return

    os.makedirs(os.path.join(path, sha))
    with open(os.path.join(path, sha, "file"), "wb") as output_dep_file:
        output_dep_file.write(download_response.content)


def make_targz(output_filename, source_dir):
    with tarfile.open(output_filename, "w:gz") as tar:
        tar.add(source_dir, arcname="")


if __name__ == "__main__":
    resolved_deps("/tmp/envoy_deps_resolved.bzl")
    result = make_dep_repos("/tmp/envoy_deps_resolved.bzl")

    temp_dir = "/tmp/envoy_deps_cache-" + str(int(round(time.time() * 1000)))
    os.makedirs(temp_dir)

    for url, sha in result.items():
        url_download(url, sha, temp_dir)

    make_targz("archives.tar.gz", temp_dir)
