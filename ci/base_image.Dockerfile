# Dockerfile used to create basic runtime image. This basic image is used to
# ensure we can get the whole envoy image more quickly at the runtime.
FROM ubuntu:20.04

# Install some tools which are necessary for the envoy sidecar or gateway.
RUN apt update && DEBIAN_FRONTEND="noninteractive" apt install -y \
    ca-certificates curl \
    dumb-init \
    gdb git gettext-base \
    iproute2 iptables iputils-ping \
    linux-tools-common linux-tools-generic \
    libpcre++-dev logrotate lsof luarocks \
    net-tools \
    tcpdump telnet tzdata \
    sudo \
    && apt clean && rm -rf /tmp/* /var/tmp/* /var/lib/apt/lists/*

# Set runtime timezone.
ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Add rider to image and download related deps.
COPY rider /usr/local/lib/rider
RUN cd /usr/local/lib/rider && luarocks make
