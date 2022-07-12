#! /usr/bin/dumb-init /bin/bash
# shellcheck shell=bash

# Some pre-defined path. If some files need to be created during the
# execution of Envoy, the path where the files are located must exist.
# `/var/log/envoy` is default log path for Envoy.
read -ra PRE_CREATE_PATH_ARRAY <<<"${PRE_CREATE_PATHS:-} /var/log/envoy"

for PRE_CREATE_PATH in "${PRE_CREATE_PATH_ARRAY[@]}"; do
    if [ ! -d "${PRE_CREATE_PATH}" ]; then
        echo "Pre create directory ${PRE_CREATE_PATH}"
        sudo mkdir -p "${PRE_CREATE_PATH}"
        sudo chown "$(id -u)":"$(id -g)" "${PRE_CREATE_PATH}"
    fi
done

LOGROTATE_CONFIG_PATH="/etc/logrotate.d/docker-logrotate.conf"

LOGROTATE="sudo /usr/sbin/logrotate ${LOGROTATE_CONFIG_PATH} &>/dev/null"

CRON_SCHEDULE=${CRON_SCHEDULE:-"* * * * *"}
echo "${CRON_SCHEDULE} ${LOGROTATE}" | crontab

# This env config is deprecated.
export LOG_ROTATE_NUM=${LOG_ROTATE_NUM:-"4"}

export LOG_ROTATE_ROLL=${LOG_ROTATE_ROLL:-${LOG_ROTATE_NUM}}
export LOG_ROTATE_SIZE=${LOG_ROTATE_SIZE:-"256M"}
export LOG_ROTATE_PATH=${LOG_ROTATE_PATH:-"/var/log/envoy/*.log"}

LOGROTATE_CONFIG=$(envsubst </etc/envoy/logrotate.conf)

sudo su root -c "echo '${LOGROTATE_CONFIG}' > ${LOGROTATE_CONFIG_PATH}"

# Start cron service
sudo service cron start &>/dev/null

BOOTSTRAP_BINARY=${BOOTSTRAP_BINARY:-"/usr/local/bin/envoy-link"}
${BOOTSTRAP_BINARY} "$@"
