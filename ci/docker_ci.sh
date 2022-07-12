#! /bin/bash

set -e

# Image hub and image tag.
DEFAULT_IMAGE_HUB="cicd/envoy-proxy"
DEFAULT_IMAGE_TAG=$(git describe --tags --always --dirty)-$(git rev-parse --short HEAD)

IMAGE_HUB=${ENVOY_PROXY_IMAGE_HUB:-${DEFAULT_IMAGE_HUB}}
# Tag typically should not be overridden.
IMAGE_TAG=${ENVOY_PROXY_IMAGE_TAG:-${DEFAULT_IMAGE_TAG}}

CI_PIPELINE=${CI_PIPELINE:-"false"}

if [[ $(arch) == "aarch64" ]]; then
    ENVOY_BUILD_BINARY_ARCH="arm64"
else
    ENVOY_BUILD_BINARY_ARCH="amd64"
fi

ENVOY_IMAGE_BUILD_PATH=$(pwd)/generated/image/${ENVOY_BUILD_BINARY_ARCH}

ENVOY_BUILD_BASE_IMAGE=$(ci/base_image)
echo "Envoy build base image: ${ENVOY_BUILD_BASE_IMAGE}"

if [[ ${ENVOY_BUILD_BASE_IMAGE} == "" ]]; then
    echo "Please set base image for Envoy image building"
    exit 1
fi

read -ra DOCKER_BUILD_EXTRA_ARGS <<<"${DOCKER_BUILD_EXTRA_ARGS:-}"

########################################################################################

function pre_clear_building() {
    mkdir -p "${ENVOY_IMAGE_BUILD_PATH}"
    rm -rf "${ENVOY_IMAGE_BUILD_PATH:?}"/*
}

# Build Envoy binary and copy binary to the image build path.
function envoy_binary_build() {
    read -r ENVOY_BUILD_MODE ENVOY_BINARY_STRIPPED <<<"$1 $2"

    ci/do_ci.sh "${ENVOY_BUILD_MODE}"

    if [[ ${ENVOY_BUILD_MODE} == "build_dbg" ]]; then
        IMAGE_TAG=${IMAGE_TAG}-dbg
    fi

    local ENVOY_BINARY_PATH
    ENVOY_BINARY_PATH="$(pwd)/linux/${ENVOY_BUILD_BINARY_ARCH}"

    local ENVOY_BINARY
    if [[ ${ENVOY_BINARY_STRIPPED} == "true" ]]; then
        ENVOY_BINARY=${ENVOY_BINARY_PATH}/envoy.stripped
    else
        IMAGE_TAG=${IMAGE_TAG}-unstripped
        ENVOY_BINARY=${ENVOY_BINARY_PATH}/envoy
    fi

    cp "${ENVOY_BINARY}" "${ENVOY_IMAGE_BUILD_PATH}"/envoy
}

function copy_others_config() {
    cp ./ci/Dockerfile "${ENVOY_IMAGE_BUILD_PATH}"
    cp ./ci/entrypoint.sh "${ENVOY_IMAGE_BUILD_PATH}"
    cp ./ci/packaging/* "${ENVOY_IMAGE_BUILD_PATH}"/
}

function build_envoy_image() {
    local PARAMS_FOR_BUILD
    PARAMS_FOR_BUILD=("$@")

    pre_clear_building
    envoy_binary_build "${PARAMS_FOR_BUILD[@]}"
    copy_others_config

    pushd "${ENVOY_IMAGE_BUILD_PATH}"

    local FINAL_IMAGE_NAME
    FINAL_IMAGE_NAME="${IMAGE_HUB}":"${IMAGE_TAG}"-"${ENVOY_BUILD_BINARY_ARCH}"

    docker build --build-arg envoy_build_base_image="${ENVOY_BUILD_BASE_IMAGE}" \
        -t "${FINAL_IMAGE_NAME}" .
    docker push "${FINAL_IMAGE_NAME}"

    # Clean up image in ci pipeline.
    if [[ ${CI_PIPELINE} == "true" ]]; then
        docker image rm "${FINAL_IMAGE_NAME}"
    fi

    popd
}

########################################################################################

if [[ $# -lt 1 ]]; then
    echo "No ci target specified."
    exit 1
fi

CI_TARGET=$1
shift

case $CI_TARGET in
"stripped_opt")
    build_envoy_image build_opt "true"
    ;;
"no_stripped_opt")
    build_envoy_image build_opt "false"
    ;;
"stripped_dbg")
    build_envoy_image build_dbg "true"
    ;;
"no_stripped_dbg")
    build_envoy_image build_dbg "false"
    ;;
*)
    echo "Invalid ci target."
    exit 1
    ;;
esac
