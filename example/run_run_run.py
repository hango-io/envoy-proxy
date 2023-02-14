#! /usr/bin/python3
# -*- coding: utf-8 -*-

import requests
import urllib
import logging
import re
import subprocess
import json
import os
import time
import argparse

import run_run_cfg


def send_origin_request_and_check_response(origin_request, expected):
    logging.debug("Origin request: {}".format(origin_request))
    logging.debug("Expected: {}".format(expected))

    req_url = (
        origin_request["req_url"]
        if "req_url" in origin_request and isinstance(origin_request["req_url"], str)
        else ""
    )
    method = (
        origin_request["method"]
        if "method" in origin_request and isinstance(origin_request["method"], str)
        else "GET"
    )
    headers = (
        origin_request["headers"]
        if "headers" in origin_request and isinstance(origin_request["headers"], dict)
        else {}
    )
    body = origin_request["body"] if "body" in origin_request else ""
    try:
        if method == "GET":
            response = requests.get(req_url, headers=headers, allow_redirects=False)
        elif method == "POST":
            response = requests.post(
                req_url, headers=headers, data=body, allow_redirects=False
            )
        else:
            logging.error("Unsupported method {0}".format(method))
            return False
    except Exception as error:
        logging.error("Some error: {0}".format(error))
        return False

    logging.debug("Response headers: {}".format(response.headers))
    logging.debug("Response body: {}".format(response.text))

    # check status
    if "status" in expected:
        if not isinstance(expected["status"], str):
            logging.error("Expected status must be regex string.")
            return False
        if not re.fullmatch(expected["status"], str(response.status_code)):
            logging.error(
                "Status code is not match: expected: {0} but return {1}.".format(
                    expected["status"], response.status_code
                )
            )
            return False

    # check headers
    expected_headers = (
        expected["headers"]
        if "headers" in expected and isinstance(expected["headers"], dict)
        else {}
    )
    for key, value in expected_headers.items():
        if not isinstance(value, str):
            logging.error("Expected header value must be regex string.")
            return False
        return_header = response.headers[key] if key in response.headers else ""
        if not re.fullmatch(value, return_header):
            logging.error(
                "Header {0} is not match: expected: {1} but return {2}.".format(
                    key, value, return_header
                )
            )
            return False
    # check body
    if "body" in expected:
        if not isinstance(expected["body"], str):
            logging.error("Expected body must be regex string.")
            return False
        if not re.fullmatch(expected["body"], response.text):
            logging.error(
                "Body is not match: expected: {0} but return {1}.".format(
                    expected["body"], response.text
                )
            )
            return False

    return True


def pre_request_handler(pre_request):
    if "request" not in pre_request or not isinstance(pre_request["request"], dict):
        return False
    expected = (
        pre_request["response"]
        if "response" in pre_request and isinstance(pre_request["response"], dict)
        else {}
    )
    return send_origin_request_and_check_response(pre_request["request"], expected)


def pre_command_handler(pre_command):
    if "command" not in pre_command or not isinstance(pre_command["command"], str):
        return False

    try:
        exec(pre_command["command"])
    except Exception as error:
        logging.error("Some error: {0}".format(error))
        return False
    return True


PRE_OPS_HANDLER_DICT = {
    "PRE_REQUEST": pre_request_handler,
    "PRE_COMMAND": pre_command_handler,
}


def exec_all_before_ops(before_list):
    if not isinstance(before_list, list):
        return False
    for op in before_list:
        if not isinstance(op, dict) or "op_type" not in op:
            return False

        if op["op_type"] not in PRE_OPS_HANDLER_DICT:
            return False

        if not PRE_OPS_HANDLER_DICT[op["op_type"]](op):
            return False
    return True


def boot_envoy(binary_path, config_path, log_level, log_path):
    envoy = subprocess.Popen(
        [binary_path, "-c", config_path, "-l", log_level, "--log-path", log_path]
    )
    logging.info("Envoy is runing...")
    return envoy


def run_envoy_and_check_suite(binary_path, suite):
    with open(suite, "r", encoding="utf-8") as f:
        loaded_suite = json.loads(f.read())
    suite_name = (
        loaded_suite["suite_name"]
        if "suite_name" in loaded_suite and isinstance(loaded_suite["suite_name"], str)
        else "Unknown Suite"
    )

    if "suite_conf" not in loaded_suite or not isinstance(
        loaded_suite["suite_conf"], dict
    ):
        logging.error(
            "No suite configuration provided for suite {0}".format(suite_name)
        )
        return suite_name, 0, 0

    suite_conf = loaded_suite["suite_conf"]

    if "from_completed_yaml" not in suite_conf or not isinstance(
        suite_conf["from_completed_yaml"], str
    ):
        logging.error(
            "'from_completed_yaml' is only way supported now to get envoy config."
        )
        return suite_name, 0, 0

    config_path = os.path.join(
        os.path.dirname(suite), suite_conf["from_completed_yaml"]
    )

    envoy_log_path = (
        "/tmp/" + "envoy-run" + str(int(round(time.time() * 1000))) + ".log"
    )
    envoy = boot_envoy(binary_path, config_path, "debug", envoy_log_path)

    time.sleep(5)

    if "suite_case" not in loaded_suite or not isinstance(
        loaded_suite["suite_case"], list
    ):
        logging.error("No suite case for suite: {0}".format(suite_name))
        return suite_name, 0, 0

    suite_case = loaded_suite["suite_case"]
    suite_case_num, suite_case_ok_num = len(suite_case), 0

    logging.info("Running {0} case in suite {1}...".format(suite_case_num, suite_name))
    for i in range(len(suite_case)):
        case = suite_case[i]
        if not isinstance(case, dict):
            logging.error("Case #{0} is not valid in suite {1}.".format(i, suite_name))
            continue
        case_name = (
            case["case_name"]
            if "case_name" in case and isinstance(case["case_name"], str)
            else "default case name #{0}".format(i)
        )
        logging.info("case: {0}...".format(case_name))
        origin_request = (
            case["request"]
            if "request" in case and isinstance(case["request"], dict)
            else {}
        )
        expected = (
            case["response"]
            if "response" in case and isinstance(case["response"], dict)
            else {}
        )

        if "before" in case:
            if not isinstance(case["before"], list):
                logging.error("Before ops must be array type. Please check.")
                return
            if not exec_all_before_ops(case["before"]):
                logging.error("Some before opts not success. Please check.")
                return

        result = send_origin_request_and_check_response(origin_request, expected)
        if result:
            logging.info("OK.")
            suite_case_ok_num = suite_case_ok_num + 1
        else:
            logging.info("FAILED.")
    envoy.kill()
    time.sleep(5)
    return suite_name, suite_case_num, suite_case_ok_num

def per_filter_check(task, binary_path, example_result):
    logging.info("Running Envoy example task: {0} and check......".format(task))
    task_path = os.path.join(os.getcwd(), "example", task)
    file_name_list, task_result = os.listdir(task_path), []
    for file in [x for x in file_name_list if x.endswith(".json")]:
        result = run_envoy_and_check_suite(args.binary, os.path.join(task_path, file))
        task_result.append(result)
    example_result[task] = task_result

parser = argparse.ArgumentParser(description="A simple empty envoy example script")


parser.add_argument("--binary", "-b", type=str, help="Envoy binary.", required=True)
parser.add_argument(
    "--log_level",
    "-l",
    type=str,
    help="Script log level.(debug/info/error)",
    default="info",
)
parser.add_argument("--target", "-t", type=str, help="Target filter.")
args = parser.parse_args()

log_format_string = "[%(asctime)s %(levelname)s] %(message)s"
if args.log_level == "debug":
    logging.basicConfig(level=logging.DEBUG, format=log_format_string)
elif args.log_level == "info":
    logging.basicConfig(level=logging.INFO, format=log_format_string)
else:
    logging.basicConfig(level=logging.ERROR, format=log_format_string)

example_result = {}

if args.target:
    if args.target in run_run_cfg.example_set:
        per_filter_check(args.target, args.binary, example_result)
    else:
        logging.error("Specified filter is not in example_set")
else:
    for task in run_run_cfg.example_set:
        per_filter_check(task, args.binary, example_result)


logging.info("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++")
logging.info("Envoy example runing result:")
for key, value in example_result.items():
    logging.info("Task {0}:".format(key))
    for res in value:
        logging.info("  Suite: {0} Total: {1} OK: {2}".format(res[0], res[1], res[2]))
logging.info("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++")
