#! /usr/bin/dumb-init /bin/bash

pre_create_path_arr=(${PRE_CREATE_PATHS})

for path in ${pre_create_path_arr[@]}; do
    if [ ! -d ${path} ]; then
        echo "Pre create directory ${path}"
        sudo mkdir -p ${path} && sudo chown $(id -u):$(id -g) ${path}
    fi
done

logrotate_config_path="/etc/logrotate.d/docker-logrotate.conf"

logrotate="sudo /usr/sbin/logrotate ${logrotate_config_path} &>/dev/null"

CRON_SCHEDULE=${CRON_SCHEDULE:-"0 * * * *"}
echo "${CRON_SCHEDULE} ${logrotate}" | crontab

export LOG_ROTATE_NUM=${LOG_ROTATE_NUM:-"10"}
export LOG_ROTATE_SIZE=${LOG_ROTATE_SIZE:-"200M"}
export LOG_ROTATE_PATH=${LOG_ROTATE_PATH:-"/var/log/envoy/*.log"}

logrotate_config=$(envsubst </etc/envoy/docker-logrotate.conf.tmpl)

sudo su root -c "echo '${logrotate_config}' > ${logrotate_config_path}"

# Start cron service
service cron start &>/dev/null

/usr/local/bin/agent $@
