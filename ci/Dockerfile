FROM ubuntu:18.04

# Keep this for aggra.
ARG aggra_version
ARG agent_version
# Need this to work on qingzhou CICD.
ARG build_dir

RUN echo $agent_version > /etc/aggra_version
RUN echo $agent_version > /etc/agent_version

RUN apt-get update && apt-get install -y                                                \
    ca-certificates curl                                                                \
    dumb-init                                                                           \
    gdb gettext-base git                                                                \
    iproute2 iptables iputils-ping                                                      \
    knot-dnsutils                                                                       \
    linux-tools-generic logrotate lsof luarocks                                         \
    netcat net-tools                                                                    \
    perl python python3-pip                                                             \
    tcpdump telnet                                                                      \
    sudo

RUN apt-get autoremove -y && apt-get clean && rm -rf /tmp/* /var/tmp/* /var/lib/apt/lists/*

# Add some binary
ADD $build_dir/agent /usr/local/bin/agent
ADD $build_dir/istio-iptables.sh /usr/local/.pri/istio-iptables.sh
ADD $build_dir/envoy /usr/local/bin/envoy

# Copy Envoy bootstrap templates used by pilot-agent
COPY $build_dir/envoy_bootstrap.json /var/lib/istio/envoy/envoy_bootstrap_tmpl.json
COPY $build_dir/gcp_envoy_bootstrap.json /var/lib/istio/envoy/gcp_envoy_bootstrap_tmpl.json

# Log rotate config tmp
COPY $build_dir/logrotate.conf /etc/envoy/docker-logrotate.conf.tmpl

# Entry point
ADD $build_dir/entrypoint.sh /entrypoint.sh

RUN chmod 755 /usr/local/bin/envoy /usr/local/bin/agent entrypoint.sh \
    /usr/local/.pri/istio-iptables.sh

RUN ln -s /usr/local/bin/envoy /usr/local/bin/envoy-link

# Sudoers used to allow tcpdump and other debug utilities.
RUN useradd -m --uid 1337 istio-proxy && echo "istio-proxy ALL=NOPASSWD: ALL" >>/etc/sudoers && \
    chown -R istio-proxy /var/lib/istio && chown -R istio-proxy /usr/local/bin

ENTRYPOINT ["/entrypoint.sh"]
