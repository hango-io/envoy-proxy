#! /bin/bash

set -e

if [[ $(arch) == "aarch64" ]]; then
    ENVOY_BUILD_BINARY_ARCH="arm64"
else
    ENVOY_BUILD_BINARY_ARCH="amd64"
fi

ENVOY_SRCDIR=$(pwd)
export ENVOY_SRCDIR

BAZEL_BUILD_EXTRA_OPTIONS=("${BAZEL_BUILD_EXTRA_OPTIONS} --jobs=${BAZEL_JOBS:-"12"}")

# shellcheck source=/dev/null
source envoy/ci/build_setup.sh "-nofetch"
trap - EXIT

# After compiling Envoy, store the corresponding binaries in this directory. Can be used
# for later testing or image builds.
ENVOY_BUILD_BINARY_PATH="$(pwd)/linux/${ENVOY_BUILD_BINARY_ARCH}"
mkdir -p "${ENVOY_BUILD_BINARY_PATH}"
rm -rf "${ENVOY_BUILD_BINARY_PATH:?}"/*

ENVOY_BINARY_TARGET="envoy-proxy-static"

########################################################################################

function envoy_binary_build() {
    setup_clang_toolchain

    BUILD_MODE=$1

    echo "bazel build" "${BAZEL_BUILD_OPTIONS[@]}" -c "${BUILD_MODE}" :"${ENVOY_BINARY_TARGET}"

    # Generate extensions.
    python3 source/generator.py ||
        (echo "Faild to generator Envoy deps" && echo "Exit" && exit 1)

    bazel build "${BAZEL_BUILD_OPTIONS[@]}" -c "${BUILD_MODE}" :"${ENVOY_BINARY_TARGET}" ||
        (echo "Faild to build Envoy binary" && echo "Exit" && exit 1)
    strip bazel-bin/${ENVOY_BINARY_TARGET} -o bazel-bin/${ENVOY_BINARY_TARGET}.stripped

    cp bazel-bin/${ENVOY_BINARY_TARGET} "${ENVOY_BUILD_BINARY_PATH}"/envoy
    cp bazel-bin/${ENVOY_BINARY_TARGET}.stripped "${ENVOY_BUILD_BINARY_PATH}"/envoy.stripped
}

function envoy_unit_test() {
    setup_clang_toolchain

    TEST_MODE="$1"
    shift

    TEST_TARGET=("$@")

    echo bazel test "${BAZEL_BUILD_OPTIONS[@]}" -c "${TEST_MODE}" "${TEST_TARGET[@]}"
    bazel test "${BAZEL_BUILD_OPTIONS[@]}" -c "${TEST_MODE}" "${TEST_TARGET[@]}"
}

function envoy_unit_coverage() {
    setup_clang_toolchain

    TEST_TARGET=("$@")

    BAZEL_BUILD_OPTIONS="${BAZEL_BUILD_OPTIONS[*]} --define tcmalloc=gperftools" \
        test/gen_coverage.sh "${TEST_TARGET[@]}"

}

function envoy_test_asan() {
    setup_clang_toolchain

    TEST_TARGET=("$@")

    BAZEL_BUILD_OPTIONS+=(-c dbg "--config=clang-asan" "--build_tests_only")
    echo "bazel ASAN/UBSAN debug build with tests"
    bazel test "${BAZEL_BUILD_OPTIONS[@]}" "${TEST_TARGET[@]}"
}

function envoy_test_tsan() {
    setup_clang_toolchain

    TEST_TARGET=("$@")

    BAZEL_BUILD_OPTIONS=("--config=rbe-toolchain-tsan" "${BAZEL_BUILD_OPTIONS[@]}" "-c" "dbg" "--build_tests_only")
    echo "bazel TSAN debug build with tests"
    bazel test "${BAZEL_BUILD_OPTIONS[@]}" "${TEST_TARGET[@]}"
}

function envoy_test_msan() {
    ENVOY_STDLIB=libc++
    setup_clang_toolchain

    TEST_TARGET=("$@")

    # rbe-toolchain-msan must comes as first to win library link order.
    BAZEL_BUILD_OPTIONS=("--config=rbe-toolchain-msan" "${BAZEL_BUILD_OPTIONS[@]}" "-c" "dbg" "--build_tests_only")
    echo "bazel MSAN debug build with tests"
    echo "Building and testing envoy tests ${TEST_TARGET[*]}"
    bazel test "${BAZEL_BUILD_OPTIONS[@]}" "${TEST_TARGET[@]}"
}

function check_clang_tidy() {
    setup_clang_toolchain

    exec_root=$(bazel info execution_root "${BAZEL_BUILD_OPTIONS[@]}") || exit 1
    echo "exec_root: ${exec_root}"
    # If exec_root is exists try to clean old compile_commands.json.
    if [[ -d "${exec_root}" ]]; then
        find "${exec_root}" -type f -name '*.compile_commands.json' -exec rm -f {} +
    fi

    # Don't block on clang-tidy errors.
    BAZEL_BUILD_OPTIONS="${BAZEL_BUILD_OPTIONS[*]}" \
        ENVOY_SRCDIR=$(pwd)/envoy SRCDIR=$(pwd) COMP_DB_TARGETS="//source/..." \
        RUN_FULL_CLANG_TIDY=1 \
        "$(pwd)"/envoy/ci/run_clang_tidy.sh || exit 0
}

function refresh_clangdb() {
    setup_clang_toolchain

    exec_root=$(bazel info execution_root "${BAZEL_BUILD_OPTIONS[@]}") || exit 1
    echo "exec_root: ${exec_root}"
    # If exec_root is exists try to clean old compile_commands.json.
    if [[ -d "${exec_root}" ]]; then
        find "${exec_root}" -type f -name '*.compile_commands.json' -exec rm -f {} +
    fi

    BAZEL_BUILD_OPTIONS="${BAZEL_BUILD_OPTIONS[*]}" \
        "$(pwd)/envoy/tools/gen_compilation_database.py" \
        --include_headers --include_genfiles --include_external "//source/..." "//test/..."
    # Kill clangd to reload the compilation database
    pkill clangd || :
}

function check_code_format() {
    tools/code_format/check_format.py check
}

########################################################################################

if [[ $# -lt 1 ]]; then
    echo "No ci target specified."
    exit 1
fi

# Set go proxy.
export GOPROXY=https://goproxy.cn,direct

CI_TARGET=$1
shift

UNIT_TARGET="//test/..."
if [[ $# -ge 1 ]]; then
    UNIT_TARGET=("$@")
fi

case $CI_TARGET in
"build_opt")
    envoy_binary_build opt
    ;;
"build_dbg")
    envoy_binary_build dbg
    ;;
"unit_test_opt")
    envoy_unit_test opt "${UNIT_TARGET[@]}"
    ;;
"unit_test_dbg")
    envoy_unit_test dbg "${UNIT_TARGET[@]}"
    ;;
"unit_coverage")
    envoy_unit_coverage "${UNIT_TARGET[@]}"
    ;;
"check_format")
    check_code_format
    ;;
"clang_tidy")
    check_clang_tidy
    ;;
"refresh_clangdb")
    refresh_clangdb
    ;;
"unit_test_asan")
    envoy_test_asan "${UNIT_TARGET[@]}"
    ;;
"unit_test_tsan")
    envoy_test_tsan "${UNIT_TARGET[@]}"
    ;;
"unit_test_msan")
    envoy_test_msan "${UNIT_TARGET[@]}"
    ;;
*)
    echo "Invalid ci target."
    exit 1
    ;;
esac
