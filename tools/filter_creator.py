#!/usr/bin/env python

import argparse
import os
import string

import filter_template.http_filter

parser = argparse.ArgumentParser(
    description="A simple empty Envoy filter creator for netease."
)

parser.add_argument(
    "--type",
    "-t",
    type=str,
    help="Envoy filter type: http, network, listener.",
    default="http",
)

parser.add_argument(
    "--name",
    "-n",
    type=str,
    help="Envoy filter name. 'com.netease.filters.' will be added automatically as prefix. Please use upper camel case to input filter name.",
)

args = parser.parse_args()


def help():
    parser.print_help()


if not args.name or len(args.name) == 0:
    print("Error: no filter name.")
    help()
    exit(1)

if args.type != "http":
    print("Error: not supported filter type. Only http is supported now.")
    help()
    exit(1)

current_path = os.getcwd()

# check path
if not current_path.endswith("envoy-netease"):
    print("Error: please run this script in project: envoy-netease")
    exit()


def get_SNAKE_CASE_NAME(oname):
    lst = []
    for index, char in enumerate(oname):
        if char.isupper() and index != 0:
            lst.append("_")
        lst.append(char)
    return "".join(lst).lower()


filter = {
    "SNAKE_CASE_NAME": get_SNAKE_CASE_NAME(args.name),
    "CAMEL_CASE_NAME": args.name,
    "TYPE": args.type,
}


def create_http_filter(filter):
    http_template = {}

    http_template[
        "./source/filters/http/{}/{}.h".format(
            filter["SNAKE_CASE_NAME"], filter["SNAKE_CASE_NAME"]
        )
    ] = filter_template.http_filter.HTTP_FILTER_FILTER_H_T

    http_template[
        "./source/filters/http/{}/{}.cc".format(
            filter["SNAKE_CASE_NAME"], filter["SNAKE_CASE_NAME"]
        )
    ] = filter_template.http_filter.HTTP_FILTER_FILTER_CC_T

    http_template[
        "./source/filters/http/{}/config.h".format(filter["SNAKE_CASE_NAME"])
    ] = filter_template.http_filter.HTTP_FILTER_CONFIG_H_T

    http_template[
        "./source/filters/http/{}/config.cc".format(filter["SNAKE_CASE_NAME"])
    ] = filter_template.http_filter.HTTP_FILTER_CONFIG_CC_T

    http_template[
        "./source/filters/http/{}/BUILD".format(filter["SNAKE_CASE_NAME"])
    ] = filter_template.http_filter.HTTP_FILTER_FILTER_BUILD_T

    http_template[
        "./api/proxy/filters/http/{}/v2/{}.proto".format(
            filter["SNAKE_CASE_NAME"],
            filter["SNAKE_CASE_NAME"],
        )
    ] = filter_template.http_filter.HTTP_FILTER_PROTO_T

    http_template[
        "./api/proxy/filters/http/{}/v2/BUILD".format(
            filter["SNAKE_CASE_NAME"])
    ] = filter_template.http_filter.HTTP_FILTER_PROTO_BUILD_T

    for k, v in http_template.items():
        if os.path.exists(k):
            continue
        if not os.path.exists(os.path.split(k)[0]):
            os.makedirs(os.path.split(k)[0])
        with open(k, mode="a") as f:
            f.write(string.Template(v).substitute(**filter))

    print(
        'An empty http filter is created. Please add "//source/filters/http/{}:config_lib" to BUILD file.'.format(
            filter["SNAKE_CASE_NAME"]
        )
    )


create_http_filter(filter)
