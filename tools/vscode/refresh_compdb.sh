#!/usr/bin/env bash

# Setting TEST_TMPDIR here so the compdb headers won't be overwritten by another bazel run
TEST_TMPDIR=${BUILD_DIR:-/tmp}/envoy-compdb tools/gen_compile_db.py

# Kill clangd to reload the compilation database
killall -v /opt/llvm/bin/clangd
