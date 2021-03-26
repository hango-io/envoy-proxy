#! /bin/bash

COMMIT=`git rev-parse --short HEAD`

bazel build @additional_distfiles//:archives.tar
./tools/upload.py envoy build/$COMMIT-archives.tar bazel-bin/external/additional_distfiles/archives.tar
