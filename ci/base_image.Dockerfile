# Dockerfile used to create basic runtime image.

# This phase will build the istio agent binary.
FROM golang:1.18.3

ARG build_binary_arch

ADD istio /istio

ENV GOARCH=${build_binary_arch}
RUN cd /istio && common/scripts/gobuild.sh /agent ./pilot/cmd/pilot-agent

FROM ubuntu:18.04

# Install tools which are necessary for the sidecar or gateway.
RUN apt update && DEBIAN_FRONTEND="noninteractive" apt install -y \
    ca-certificates curl \
    dumb-init \
    git gettext-base \
    iproute2 iptables iputils-ping \
    linux-tools-common linux-tools-generic \
    logrotate lsof luarocks \
    net-tools \
    tcpdump telnet tzdata \
    sudo \
    && apt clean && rm -rf /tmp/* /var/tmp/* /var/lib/apt/lists/*

# Set runtime timezone.
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Copy Envoy bootstrap templates used by agent.
ADD envoy_bootstrap_tmpl.json  /var/lib/istio/envoy/

# Copy agent binary for agent builder.
COPY --from=0 /agent /usr/local/bin/pilot-agent
# Set BOOTSTRAP_BINARY env value to agent.
ENV BOOTSTRAP_BINARY=/usr/local/bin/pilot-agent

# Add rider to image and download related deps.
ADD rider /usr/local/lib/rider
RUN cd /usr/local/lib/rider && luarocks make

# Sudoers used to allow tcpdump and other debug utilities.
RUN useradd -m --uid 1337 istio-proxy
RUN echo "istio-proxy ALL=NOPASSWD: ALL" >>/etc/sudoers
RUN chown -R istio-proxy /var/lib/istio && chown -R istio-proxy /usr/local/bin
