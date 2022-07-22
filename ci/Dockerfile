ARG envoy_build_base_image=ubuntu:18.04
FROM ${envoy_build_base_image}
# Add some binaries to image.
ADD envoy entrypoint.sh /usr/local/bin/
RUN ln -s /usr/local/bin/envoy /usr/local/bin/envoy-link

# Log rotate config template.
ADD logrotate.conf /etc/envoy/logrotate.conf

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
