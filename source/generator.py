#! /usr/bin/python3

import os


def get_filters_extensions(exts):
    result = os.popen(
        "bazel query 'kind(\"cc_library rule\", \"//source/filters/...\")' --show_progress=false")
    return exts + [x.strip() for x in result.readlines() if "external" not in x]


def update_extensions_conf(exts, file_path):
    tmpl = '''
def ALL_EXTENSIONS():
    return [
{}
    ]
    '''

    content = ""

    for x in exts:
        content = content + "\"" + x + "\"" + "," + "\n"

    with open(file_path, "w+") as f:
        f.write(tmpl.format(content))


if __name__ == "__main__":
    exts = []
    exts = get_filters_extensions(exts)
    work_path = os.getcwd()

    if(not work_path.endswith("envoy-proxy")):
        print("Error: error work home. Please run script under envoy-proxy")
        exit()

    file_path = os.path.join(work_path, "source/extensions.bzl")

    update_extensions_conf(exts, file_path)
