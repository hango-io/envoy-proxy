#! /bin/bash

# Build container image based on 'ci/build.Dockerfile'
ENVOY_DOCKER_IMAGE="hangoio/envoy-build-ubuntu:v0.0.1-amd64"

# If the host OS does not provide dpkg command then please edit this script and set ARCH by youself.
# ENVOY_ARCH="arm64"
# ENVOY_ARCH="amd64"
ENVOY_ARCH=$(dpkg --print-architecture)

ENVOY_DOCKER_BUILD_DIR=${ENVOY_DOCKER_BUILD_DIR:-"/build"}

DOCKER_RUNNING_COMMAND=$*
DOCKER_RUNNING_MODE="-it"

if [[ ${DOCKER_RUNNING_COMMAND} == "" ]]; then
    DOCKER_RUNNING_COMMAND="while true;do echo hello docker;sleep 5;done" # Run command forever and never stop.
    DOCKER_RUNNING_MODE="-d"
fi

mkdir -p "${ENVOY_DOCKER_BUILD_DIR}"

if [[ ! -S $SSH_AUTH_SOCK ]]; then
    eval "$(ssh-agent -s)"
fi

docker run --rm ${DOCKER_RUNNING_MODE} --privileged ${DOCKER_TTY_OPTION} -e HTTP_PROXY=${http_proxy} \
    -e HTTPS_PROXY=${https_proxy} -v $(dirname ${SSH_AUTH_SOCK}):$(dirname ${SSH_AUTH_SOCK}) -v ${HOME}:${HOME} \
    -v "$PWD":/source -v ${ENVOY_DOCKER_BUILD_DIR}:/build -v /var/run/docker.sock:/var/run/docker.sock \
    -v /opt/envoy-cache:/opt/envoy-cache ${ADDITIONAL_VOLUME} -e SSH_AUTH_SOCK=${SSH_AUTH_SOCK} \
    -v /etc/passwd:/etc/passwd -u root:root \
    -e BAZEL_BUILD_EXTRA_OPTIONS -e BAZEL_EXTRA_TEST_OPTIONS -e BAZEL_REMOTE_CACHE -e BAZEL_REMOTE_INSTANCE \
    -e NUM_CPUS -e ENVOY_RBE --cap-add SYS_PTRACE --cap-add NET_RAW --cap-add NET_ADMIN ${ENVOY_DOCKER_IMAGE} \
    /bin/bash -c "${DOCKER_RUNNING_COMMAND}"
