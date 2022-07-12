ARG envoy_build_base_image=ubuntu:18.04
FROM ${envoy_build_base_image}

WORKDIR /usr/local/bin

# Add some binaries to image.
ADD envoy entrypoint.sh ./
RUN ln -s ./envoy ./envoy-link

# Log rotate config template.
ADD logrotate.conf /etc/envoy/logrotate.conf

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
