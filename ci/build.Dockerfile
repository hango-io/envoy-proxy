# Dockerfile of build image for CI and developing. This will edit the Envoy
# community build image and add some required dependencies.
# **NOTE**: the output image will use same tag with the community version.

ARG community_image_tag
FROM envoyproxy/envoy-build-ubuntu:${community_image_tag}

RUN sed -i.bak '/kitware/d' /etc/apt/sources.list
RUN apt update && DEBIAN_FRONTEND="noninteractive" apt install -y libpcre++-dev
