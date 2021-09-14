#!/usr/bin/env python3

# Simple script to download all deps and compress them into *.tar.gz file.

import time
import importlib
import os.path
import requests
import tarfile

API_DEPS = importlib.machinery.SourceFileLoader(
    "api", "envoy/api/bazel/repository_locations.bzl").load_module()
DEPS = importlib.machinery.SourceFileLoader(
    "deps", "envoy/bazel/repository_locations.bzl").load_module()


def url_download(url, filename):
    down_res = requests.get(url)
    with open(filename, "wb") as file:
        file.write(down_res.content)


def make_targz(output_filename, source_dir):
    with tarfile.open(output_filename, "w:gz") as tar:
        tar.add(source_dir, arcname=os.path.basename(source_dir))


if __name__ == "__main__":
    DEPS.REPOSITORY_LOCATIONS_SPEC.update(API_DEPS.REPOSITORY_LOCATIONS_SPEC)

    temp_dir = "/tmp/distdir" + str(int(round(time.time() * 1000)))
    os.makedirs(temp_dir)

    for _, loc in DEPS.REPOSITORY_LOCATIONS_SPEC.items():
        version = loc.get("version")
        url = loc.get("urls")[0].format(
            version=version,
            dash_version=version.replace(".", "-"),
            underscore_version=version.replace(".", "_"),
        )
        file_path = os.path.join(temp_dir, url.split("/")[-1])
        print("downloading dep: {} .....".format(file_path))
        url_download(url, os.path.join(temp_dir, file_path))

    make_targz("archives.tar.gz", temp_dir)
