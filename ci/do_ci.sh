#!/bin/bash

# Run a CI build/test target, e.g. docs, asan.

set -e

ISTIO_REPOSITORY_URL=${ISTIO_REPOSITORY_URL:-"git@github.com:istio/istio.git"}
ISTIO_REPOSITORY_COM=${ISTIO_REPOSITORY_COM:-"2dd7b6207f02cec8b42f4263ac197434f4ec9b4a"}

VERSION=$(git describe --tags --always --dirty)
COMMIT=$(git rev-parse --short HEAD)

ENVOY_BINARY_TARGET="envoy-proxy-static"

RELEASE_IMAGE_FOR=${RELEASE_IMAGE_FOR:-"internal"}

ENVOY_PROXY_IMAGE_REPO=${ENVOY_PROXY_HUB:-"cicd"}
IMAGE_TAG="${IMAGE_TAG:-${VERSION}-${COMMIT}}"

CI_PIPELINE=${CI_PIPELINE:-}
ENVOY_IMAGE_BUILD_PATH=${ENVOY_IMAGE_BUILD_PATH:-"/tmp/_build_envoy"}
BAZEL_JOBS=${BAZEL_JOBS:-"12"}

ENVOY_BUILD_DISTDIR_URL=${ENVOY_BUILD_DISTDIR_URL:-}

rm -rf ${ENVOY_IMAGE_BUILD_PATH} && mkdir -p ${ENVOY_IMAGE_BUILD_PATH}

export ENVOY_SRCDIR=$(pwd)
source envoy/ci/build_setup.sh "-nofetch"

BAZEL_BUILD_OPTIONS+=("--jobs=$BAZEL_JOBS")

TOOLS_PATH="/build/envoy_build_deps"
mkdir -p "${TOOLS_PATH}"

function clear_tools_path() {
    if [[ ${CLEAR_TOOLS_PATH_ANYWAY} == "true" ]]; then
        rm -rf ${TOOLS_PATH}
        mkdir -p "${TOOLS_PATH}"
    fi
}

function go_path_if_not_present() {
    if [[ ! -d ${TOOLS_PATH}/go || ! -f ${TOOLS_PATH}/go/bin/go ]]; then
        rm -rf ${TOOLS_PATH}/go # Clear go path.
        ENVOY_ARCH="amd64"
        if [[ $(arch) == "aarch64" ]]; then
            ENVOY_ARCH="arm64"
        fi
        GOLANG_PKG=go1.15.5.linux-${ENVOY_ARCH}.tar.gz
        echo "downloading ${GOLANG_PKG}"
        wget https://dl.google.com/go/$GOLANG_PKG -P ${TOOLS_PATH}
        tar -zxvf ${TOOLS_PATH}/$GOLANG_PKG -C ${TOOLS_PATH}
        rm ${TOOLS_PATH}/$GOLANG_PKG
    fi

    export GOROOT=${TOOLS_PATH}/go
    export PATH=${PATH}:${GOROOT}/bin
}

function distdir_if_not_present() {
    if [[ -d ${TOOLS_PATH}/distdir && -f ${TOOLS_PATH}/distdir/distdir_for_envoy ]]; then
        BAZEL_BUILD_OPTIONS+=("--distdir=${TOOLS_PATH}/distdir")
        return
    fi

    rm -rf ${TOOLS_PATH}/distdir

    if [[ ${ENVOY_BUILD_DISTDIR_URL} != "" ]]; then
        mkdir -p ${TOOLS_PATH}/distdir
        DATE=$(date +%Y%m%d)
        wget ${ENVOY_BUILD_DISTDIR_URL} -O ${TOOLS_PATH}/deps-${DATE}.tar.gz
        tar -zxvf ${TOOLS_PATH}/deps-${DATE}.tar.gz -C ${TOOLS_PATH}/distdir --strip-components=1
        rm ${TOOLS_PATH}/deps-${DATE}.tar.gz
        touch ${TOOLS_PATH}/distdir/distdir_for_envoy
        BAZEL_BUILD_OPTIONS+=("--distdir=${TOOLS_PATH}/distdir")
    fi
}

function build_envoy_agent() {
    pushd ${ENVOY_IMAGE_BUILD_PATH}
    git clone ${ISTIO_REPOSITORY_URL}

    pushd istio

    git checkout ${ISTIO_REPOSITORY_COM} && git submodule update --init
    common/scripts/gobuild.sh ${ENVOY_IMAGE_BUILD_PATH}/agent ./pilot/cmd/pilot-agent

    popd
    popd
}

function envoy_binary_build() {
    setup_clang_toolchain
    BINARY_TYPE="$1"

    if [[ "${BINARY_TYPE}" == "release" ]]; then
        COMPILE_TYPE="opt"
    elif [[ "${BINARY_TYPE}" == "debug" ]]; then
        COMPILE_TYPE="dbg"
    elif [[ "${BINARY_TYPE}" == "sizeopt" ]]; then
        # The COMPILE_TYPE variable is redundant in this case and is only here for
        # readability. It is already set in the .bazelrc config for sizeopt.
        COMPILE_TYPE="opt"
        CONFIG_ARGS="--config=sizeopt"
    elif [[ "${BINARY_TYPE}" == "fastbuild" ]]; then
        COMPILE_TYPE="fastbuild"
    fi

    echo "bazel build ${BAZEL_BUILD_OPTIONS[@]} -c $COMPILE_TYPE :$ENVOY_BINARY_TARGET"

    # Generate extensions.
    ./source/generator.py

    bazel build ${BAZEL_BUILD_OPTIONS[@]} -c "$COMPILE_TYPE" :"$ENVOY_BINARY_TARGET"
    strip -g bazel-bin/"$ENVOY_BINARY_TARGET" -o bazel-bin/"${ENVOY_BINARY_TARGET}.stripped"

    trap - EXIT
}

function envoy_test() {
    setup_clang_toolchain
    TEST_TARGET="$1"

    echo "bazel test ${BAZEL_BUILD_OPTIONS[@]} $TEST_TARGET "

    trap - EXIT
}

function copy_files_for_image_build() {
    read RELEASE INTERNAL <<<"$1 $2"

    if [[ ${INTERNAL} == "internal" ]]; then
        ENVOY_BIN=${ENVOY_BINARY_TARGET} # No stripped for internal image.
    elif [[ ${RELEASE} == "release" ]]; then
        ENVOY_BIN=${ENVOY_BINARY_TARGET}.stripped
    else
        ENVOY_BIN=${ENVOY_BINARY_TARGET}
    fi

    cp ./bazel-bin/$ENVOY_BIN ${ENVOY_IMAGE_BUILD_PATH}/envoy

    cp ./ci/Dockerfile ${ENVOY_IMAGE_BUILD_PATH}
    cp ./ci/entrypoint.sh ${ENVOY_IMAGE_BUILD_PATH}
    cp ./ci/packaging/* ${ENVOY_IMAGE_BUILD_PATH}/
}

function envoy_image_build() {
    if [[ -n ${CI_PIPELINE} ]]; then
        return
    fi

    if [[ -z ${ENVOY_PROXY_IMAGE_REPO} ]]; then
        return
    fi

    BINARY_TYPE=$1
    IMAGE=${ENVOY_PROXY_IMAGE_REPO}:${IMAGE_TAG}
    if [[ ${BINARY_TYPE} == "debug" ]]; then
        IMAGE=${IMAGE}-${BINARY_TYPE}
    fi

    cd ${ENVOY_IMAGE_BUILD_PATH}
    docker build --build-arg build_dir=. -t "$IMAGE" .
    docker push "$IMAGE"
}

if [[ $# -lt 1 ]]; then
    set compile
fi

CI_TARGET=$1
shift

# Clear tools if needed.
clear_tools_path

# Init go tools and deps.
go_path_if_not_present
echo ${GOROOT}
distdir_if_not_present

# Set go proxy.
export GOPROXY=https://goproxy.cn,direct

if [[ "$CI_TARGET" == "envoy-release" ]]; then
    envoy_binary_build release
    build_envoy_agent
    copy_files_for_image_build release ${RELEASE_IMAGE_FOR}
    envoy_image_build release
elif [[ "$CI_TARGET" == "envoy-debug" ]]; then
    envoy_binary_build debug
    build_envoy_agent
    copy_files_for_image_build
    envoy_image_build debug
elif [[ "$CI_TARGET" == "compile" ]]; then
    envoy_binary_build fastbuild
elif [[ "$CI_TARGET" == "compile.debug" ]]; then
    envoy_binary_build debug
elif [[ $CI_TARGET == "compile.release" ]]; then
    envoy_binary_build release
elif [[ $CI_TARGET == "test" ]]; then
    envoy_test $1
else
    echo "Invalid target."
    exit 1
fi
