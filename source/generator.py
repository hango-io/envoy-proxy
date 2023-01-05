#! /usr/bin/python3

import os

env = os.getenv("BAZEL_GENERATOR_QUERY_OPTIONS")
options = (env if env else "") + " --show_progress=false"
print("bazel options used to query extensions: {}".format(options))


def get_extension_labels(exts, scope):
    result = os.popen(
        "bazel query {} 'kind(\"cc_library rule\", \"{}\")'".format(
            options, scope))
    return exts + [
        '"{}"'.format(x.strip())
        for x in result.readlines() if "external" not in x
    ]


def update_extensions_conf(exts, file_path):
    tmpl = '''
def ALL_EXTENSIONS():
    return [
{}
    ]
    '''

    with open(file_path, "w+") as f:
        f.write(tmpl.format(",\n".join(exts)))


if __name__ == "__main__":
    exts = []
    exts = get_extension_labels(exts, "//source/filters/...")
    exts = get_extension_labels(exts, "//source/extensions/...")
    exts = get_extension_labels(exts, "//source/upstreams/...")
    exts = get_extension_labels(exts, "//source/bridge/...")

    if len(exts) == 0:
        exit(1)

    work_path = os.getcwd()

    file_path = os.path.join(work_path, "source/extensions.bzl")

    update_extensions_conf(exts, file_path)
