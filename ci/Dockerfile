ARG envoy_build_base_image=ubuntu:20.04
FROM ${envoy_build_base_image}

# Add some binaries to image.
COPY envoy entrypoint.sh /usr/local/bin/
RUN ln -s /usr/local/bin/envoy /usr/local/bin/envoy-link

# Copy log rotate config templates and other files.
COPY packaging/ /etc/envoy/
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
