#! /bin/bash

# Copied from envoy/ci/run_envoy_docker.sh

set -e

# Run docker build image and can use this docker as work space to develop, test, and build new Envoy.
UBUNTU_DOCKER_IMAGE="envoyproxy/envoy-build-ubuntu:81a93046060dbe5620d5b3aa92632090a9ee4da6"
CENTOS_DOCKER_IMAGE="envoyproxy/envoy-build-centos:81a93046060dbe5620d5b3aa92632090a9ee4da6"

ENVOY_DOCKER_IMAGE=${UBUNTU_DOCKER_IMAGE}

if [[ ${ENVOY_BUILD_OS} == "centos" ]]; then
    ENVOY_DOCKER_IMAGE=${CENTOS_DOCKER_IMAGE}
fi

export ENVOY_BUILD_IMAGE=${ENVOY_DOCKER_IMAGE}

DOCKER_RUNNING_COMMAND=$*
DOCKER_RUNNING_MODE="-it"

if [[ ${DOCKER_RUNNING_COMMAND} == "" ]]; then
    DOCKER_RUNNING_COMMAND="while true;do echo hello docker;sleep 5;done" # Run command forever.
    DOCKER_RUNNING_MODE="-d"
fi

read -ra ENVOY_DOCKER_OPTIONS <<<"${ENVOY_DOCKER_OPTIONS:-}"

# We run as root and later drop permissions. This is required to setup the USER
# in useradd below, which is need for correct Python execution in the Docker
# environment.
ENVOY_DOCKER_OPTIONS+=(-u root:root)
ENVOY_DOCKER_OPTIONS+=(-v /var/run/docker.sock:/var/run/docker.sock)
ENVOY_DOCKER_OPTIONS+=(--cap-add SYS_PTRACE --cap-add NET_RAW --cap-add NET_ADMIN)
BUILD_DIR_MOUNT_DEST=/build
SOURCE_DIR="${PWD}"
SOURCE_DIR_MOUNT_DEST=/source
START_COMMAND=("/bin/bash" "-lc" "groupadd --gid $(id -g) -f envoygroup \
    && useradd -o --uid $(id -u) --gid $(id -g) --no-create-home --home-dir /build envoybuild \
    && usermod -a -G pcap envoybuild \
    && chown envoybuild:envoygroup /build \
    && sudo -EHs -u envoybuild bash -c 'cd /source && ${DOCKER_RUNNING_COMMAND}'")

# Replace backslash with forward slash for Windows style paths
ENVOY_DOCKER_BUILD_DIR=${ENVOY_DOCKER_BUILD_DIR:-"${HOME}/envoy_build"}
mkdir -p "${ENVOY_DOCKER_BUILD_DIR}"

CONTAINER_NAME=${USER}-$(date +'%Y%m%d%H%M')

[[ -n "${SSH_AUTH_SOCK}" ]] && ENVOY_DOCKER_OPTIONS+=(-v "${SSH_AUTH_SOCK}:${SSH_AUTH_SOCK}" -e SSH_AUTH_SOCK)

# Since we specify an explicit hash, docker-run will pull from the remote repo if missing.
docker run --rm "${DOCKER_RUNNING_MODE}" --name "${CONTAINER_NAME}" \
    "${ENVOY_DOCKER_OPTIONS[@]}" \
    -v "${ENVOY_DOCKER_BUILD_DIR}":"${BUILD_DIR_MOUNT_DEST}" \
    -v "${SOURCE_DIR}":"${SOURCE_DIR_MOUNT_DEST}" \
    -e AZP_BRANCH \
    -e HTTP_PROXY \
    -e HTTPS_PROXY \
    -e NO_PROXY \
    -e BAZEL_STARTUP_OPTIONS \
    -e BAZEL_BUILD_EXTRA_OPTIONS \
    -e BAZEL_EXTRA_TEST_OPTIONS \
    -e BAZEL_REMOTE_CACHE \
    -e ENVOY_STDLIB \
    -e BUILD_REASON \
    -e BAZEL_REMOTE_INSTANCE \
    -e GCP_SERVICE_ACCOUNT_KEY \
    -e NUM_CPUS \
    -e ENVOY_RBE \
    -e FUZZIT_API_KEY \
    -e ENVOY_BUILD_IMAGE \
    -e ENVOY_SRCDIR \
    -e ENVOY_BUILD_TARGET \
    -e SYSTEM_PULLREQUEST_PULLREQUESTNUMBER \
    -e GCS_ARTIFACT_BUCKET \
    -e GITHUB_TOKEN \
    -e BUILD_SOURCEBRANCHNAME \
    -e BAZELISK_BASE_URL \
    -e ENVOY_BUILD_ARCH \
    -e SLACK_TOKEN \
    -e BUILD_URI -e REPO_URI \
    "${ENVOY_BUILD_IMAGE}" \
    "${START_COMMAND[@]}"
