#!/usr/bin/env python3

# Copyed from envoyproxy/envoy. Thanks to the community for the tools.

import argparse
import common
import multiprocessing
import os
import os.path
import pathlib
import re
import subprocess
import stat
import sys
import traceback
import shutil
import paths

EXCLUDED_PREFIXES = (
    "./generated/",
    "./build",
    "./.git/",
    "./bazel-",
    "./.cache",
    "./envoy",
    "./modsecurity",
    "./source/extensions.bzl",
    "./bazel/extensions_build_config",
)
SUFFIXES = ("BUILD", "WORKSPACE", ".bzl", ".cc", ".h", ".m", ".mm", ".proto")
PROTO_SUFFIX = (".proto")

CLANG_FORMAT_PATH = os.getenv("CLANG_FORMAT", "clang-format-11")
BUILDIFIER_PATH = paths.get_buildifier()
BUILDOZER_PATH = paths.get_buildozer()
ENVOY_BUILD_FIXER_PATH = os.path.join(
    os.path.dirname(os.path.abspath(sys.argv[0])), "envoy_build_fixer.py")
HEADER_ORDER_PATH = os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])),
                                 "header_order.py")
SUBDIR_SET = set(common.include_dir_order())
INCLUDE_ANGLE = "#include <"
INCLUDE_ANGLE_LEN = len(INCLUDE_ANGLE)
PROTO_PACKAGE_REGEX = re.compile(r"^package (\S+);\n*", re.MULTILINE)
X_ENVOY_USED_DIRECTLY_REGEX = re.compile(r'.*\"x-envoy-.*\".*')
DESIGNATED_INITIALIZER_REGEX = re.compile(r"\{\s*\.\w+\s*\=")
MANGLED_PROTOBUF_NAME_REGEX = re.compile(
    r"envoy::[a-z0-9_:]+::[A-Z][a-z]\w*_\w*_[A-Z]{2}")
HISTOGRAM_SI_SUFFIX_REGEX = re.compile(
    r"(?<=HISTOGRAM\()[a-zA-Z0-9_]+_(b|kb|mb|ns|us|ms|s)(?=,)")
TEST_NAME_STARTING_LOWER_CASE_REGEX = re.compile(
    r"TEST(_.\(.*,\s|\()[a-z].*\)\s\{")
EXTENSIONS_CODEOWNERS_REGEX = re.compile(r'.*(extensions[^@]*\s+)(@.*)')
CONTRIB_CODEOWNERS_REGEX = re.compile(r'(/contrib/[^@]*\s+)(@.*)')
COMMENT_REGEX = re.compile(r"//|\*")
DURATION_VALUE_REGEX = re.compile(r'\b[Dd]uration\(([0-9.]+)')
PROTO_VALIDATION_STRING = re.compile(r'\bmin_bytes\b')
OLD_MOCK_METHOD_REGEX = re.compile("MOCK_METHOD\d")
# C++17 feature, lacks sufficient support across various libraries / compilers.
FOR_EACH_N_REGEX = re.compile("for_each_n\(")
# Check for punctuation in a terminal ref clause, e.g.
# :ref:`panic mode. <arch_overview_load_balancing_panic_threshold>`
DOT_MULTI_SPACE_REGEX = re.compile("\\. +")
FLAG_REGEX = re.compile("    \"(.*)\",")

# yapf: disable
PROTOBUF_TYPE_ERRORS = {
    # Well-known types should be referenced from the ProtobufWkt namespace.
    "Protobuf::Any":                    "ProtobufWkt::Any",
    "Protobuf::Empty":                  "ProtobufWkt::Empty",
    "Protobuf::ListValue":              "ProtobufWkt::ListValue",
    "Protobuf::NULL_VALUE":             "ProtobufWkt::NULL_VALUE",
    "Protobuf::StringValue":            "ProtobufWkt::StringValue",
    "Protobuf::Struct":                 "ProtobufWkt::Struct",
    "Protobuf::Value":                  "ProtobufWkt::Value",

    # Other common mis-namespacing of protobuf types.
    "ProtobufWkt::Map":                 "Protobuf::Map",
    "ProtobufWkt::MapPair":             "Protobuf::MapPair",
    "ProtobufUtil::MessageDifferencer": "Protobuf::util::MessageDifferencer"
}

LIBCXX_REPLACEMENTS = {
    "absl::make_unique<": "std::make_unique<",
}

CODE_CONVENTION_REPLACEMENTS = {
    # We can't just remove Times(1) everywhere, since .Times(1).WillRepeatedly
    # is a legitimate pattern. See
    # https://github.com/google/googletest/blob/master/googlemock/docs/for_dummies.md#cardinalities-how-many-times-will-it-be-called
    ".Times(1);": ";",
    # These may miss some cases, due to line breaks, but should reduce the
    # Times(1) noise.
    ".Times(1).WillOnce": ".WillOnce",
    ".Times(1).WillRepeatedly": ".WillOnce",
}
# yapf: enable


class FormatChecker:

    def __init__(self, args):
        self.operation_type = args.operation_type
        self.target_path = args.target_path
        self.api_prefix = args.api_prefix
        self.namespace_check = args.namespace_check
        self.include_dir_order = args.include_dir_order

    # Map a line transformation function across each line of a file,
    # writing the result lines as requested.
    # If there is a clang format nesting or mismatch error, return the first occurrence
    def evaluate_lines(self, path, line_xform, write=True):
        error_message = None
        format_flag = True
        output_lines = []
        for line_number, line in enumerate(self.read_lines(path)):
            if line.find("// clang-format off") != -1:
                if not format_flag and error_message is None:
                    error_message = "%s:%d: %s" % (path, line_number + 1,
                                                   "clang-format nested off")
                format_flag = False
            if line.find("// clang-format on") != -1:
                if format_flag and error_message is None:
                    error_message = "%s:%d: %s" % (path, line_number + 1,
                                                   "clang-format nested on")
                format_flag = True
            if format_flag:
                output_lines.append(line_xform(line, line_number))
            else:
                output_lines.append(line)
        # We used to use fileinput in the older Python 2.7 script, but this doesn't do
        # inplace mode and UTF-8 in Python 3, so doing it the manual way.
        if write:
            pathlib.Path(path).write_text('\n'.join(output_lines),
                                          encoding='utf-8')
        if not format_flag and error_message is None:
            error_message = "%s:%d: %s" % (path, line_number + 1,
                                           "clang-format remains off")
        return error_message

    # Obtain all the lines in a given file.
    def read_lines(self, path):
        return self.read_file(path).split('\n')

    # Read a UTF-8 encoded file as a str.
    def read_file(self, path):
        return pathlib.Path(path).read_text(encoding='utf-8')

    # look_path searches for the given executable in all directories in PATH
    # environment variable. If it cannot be found, empty string is returned.
    def look_path(self, executable):
        if executable is None:
            return ''
        return shutil.which(executable) or ''

    # path_exists checks whether the given path exists. This function assumes that
    # the path is absolute and evaluates environment variables.
    def path_exists(self, executable):
        if executable is None:
            return False
        return os.path.exists(os.path.expandvars(executable))

    # executable_by_others checks whether the given path has execute permission for
    # others.
    def executable_by_others(self, executable):
        st = os.stat(os.path.expandvars(executable))
        return bool(st.st_mode & stat.S_IXOTH)

    # Check whether all needed external tools (clang-format, buildifier, buildozer) are
    # available.
    def check_tools(self):
        error_messages = []

        clang_format_abs_path = self.look_path(CLANG_FORMAT_PATH)
        if clang_format_abs_path:
            if not self.executable_by_others(clang_format_abs_path):
                error_messages.append(
                    "command {} exists, but cannot be executed by other "
                    "users".format(CLANG_FORMAT_PATH))
        else:
            error_messages.append(
                "Command {} not found. If you have clang-format in version 10.x.x "
                "installed, but the binary name is different or it's not available in "
                "PATH, please use CLANG_FORMAT environment variable to specify the path. "
                "Examples:\n"
                "    export CLANG_FORMAT=clang-format-11.0.1\n"
                "    export CLANG_FORMAT=/opt/bin/clang-format-11\n"
                "    export CLANG_FORMAT=/usr/local/opt/llvm@11/bin/clang-format"
                .format(CLANG_FORMAT_PATH))

        def check_bazel_tool(name, path, var):
            bazel_tool_abs_path = self.look_path(path)
            if bazel_tool_abs_path:
                if not self.executable_by_others(bazel_tool_abs_path):
                    error_messages.append(
                        "command {} exists, but cannot be executed by other "
                        "users".format(path))
            elif self.path_exists(path):
                if not self.executable_by_others(path):
                    error_messages.append(
                        "command {} exists, but cannot be executed by other "
                        "users".format(path))
            else:

                error_messages.append(
                    "Command {} not found. If you have {} installed, but the binary "
                    "name is different or it's not available in $GOPATH/bin, please use "
                    "{} environment variable to specify the path. Example:\n"
                    "    export {}=`which {}`\n"
                    "If you don't have {} installed, you can install it by:\n"
                    "    go get -u github.com/bazelbuild/buildtools/{}".format(
                        path, name, var, var, name, name, name))

        check_bazel_tool('buildifier', BUILDIFIER_PATH, 'BUILDIFIER_BIN')
        check_bazel_tool('buildozer', BUILDOZER_PATH, 'BUILDOZER_BIN')

        return error_messages

    def check_namespace(self, file_path):
        nolint = "NOLINT(namespace-%s)" % self.namespace_check.lower()
        text = self.read_file(file_path)
        if not re.search("^\s*namespace\s+%s\s*{" % self.namespace_check, text, re.MULTILINE) and \
                not nolint in text:
            return [
                "Unable to find %s namespace or %s for file: %s" %
                (self.namespace_check, nolint, file_path)
            ]
        return []

    def package_name_for_proto(self, file_path):
        package_name = None
        error_message = []
        result = PROTO_PACKAGE_REGEX.search(self.read_file(file_path))
        if result is not None and len(result.groups()) == 1:
            package_name = result.group(1)
        if package_name is None:
            error_message = [
                "Unable to find package name for proto file: %s" % file_path
            ]

        return [package_name, error_message]

    def is_api_file(self, file_path):
        return file_path.startswith(self.api_prefix)

    def is_build_file(self, file_path):
        basename = os.path.basename(file_path)
        if basename in {"BUILD", "BUILD.bazel"} or basename.endswith(".BUILD"):
            return True
        return False

    def is_external_build_file(self, file_path):
        return self.is_build_file(file_path) and (
            file_path.startswith("./bazel/external/")
            or file_path.startswith("./tools/clang_tools"))

    def is_starlark_file(self, file_path):
        return file_path.endswith(".bzl")

    def is_workspace_file(self, file_path):
        return os.path.basename(file_path) == "WORKSPACE"

    def has_invalid_angle_bracket_directory(self, line):
        if not line.startswith(INCLUDE_ANGLE):
            return False
        path = line[INCLUDE_ANGLE_LEN:]
        slash = path.find("/")
        if slash == -1:
            return False
        subdir = path[0:slash]
        return subdir in SUBDIR_SET

    def check_file_contents(self, file_path, checker):
        error_messages = []

        def check_format_errors(line, line_number):

            def report_error(message):
                error_messages.append("%s:%d: %s" %
                                      (file_path, line_number + 1, message))

            checker(line, file_path, report_error)

        evaluate_failure = self.evaluate_lines(file_path, check_format_errors,
                                               False)
        if evaluate_failure is not None:
            error_messages.append(evaluate_failure)

        return error_messages

    def fix_source_line(self, line, line_number):
        # Strip double space after '.'  This may prove overenthusiastic and need to
        # be restricted to comments and metadata files but works for now.
        line = re.sub(DOT_MULTI_SPACE_REGEX, ". ", line)

        if self.has_invalid_angle_bracket_directory(line):
            line = line.replace("<", '"').replace(">", '"')

        # Fix incorrect protobuf namespace references.
        for invalid_construct, valid_construct in PROTOBUF_TYPE_ERRORS.items():
            line = line.replace(invalid_construct, valid_construct)

        # Use recommended cpp stdlib
        for invalid_construct, valid_construct in LIBCXX_REPLACEMENTS.items():
            line = line.replace(invalid_construct, valid_construct)

        # Fix code conventions violations.
        for invalid_construct, valid_construct in CODE_CONVENTION_REPLACEMENTS.items(
        ):
            line = line.replace(invalid_construct, valid_construct)

        return line

    # Determines whether the filename is either in the specified subdirectory, or
    # at the top level. We consider files in the top level for the benefit of
    # the check_format testcases in tools/testdata/check_format.
    def is_in_subdir(self, filename, *subdirs):
        # Skip this check for check_format's unit-tests.
        if filename.count("/") <= 1:
            return True
        for subdir in subdirs:
            if filename.startswith('./' + subdir + '/'):
                return True
        return False

    # Determines if given token exists in line without leading or trailing token characters
    # e.g. will return True for a line containing foo() but not foo_bar() or baz_foo
    def token_in_line(self, token, line):
        index = 0
        while True:
            index = line.find(token, index)
            # the following check has been changed from index < 1 to index < 0 because
            # this function incorrectly returns false when the token in question is the
            # first one in a line. The following line returns false when the token is present:
            # (no leading whitespace) violating_symbol foo;
            if index < 0:
                break
            if index == 0 or not (line[index - 1].isalnum()
                                  or line[index - 1] == '_'):
                if index + len(token) >= len(line) or not (
                        line[index + len(token)].isalnum()
                        or line[index + len(token)] == '_'):
                    return True
            index = index + 1
        return False

    def check_source_line(self, line, file_path, report_error):
        # Check fixable errors. These may have been fixed already.
        if line.find(".  ") != -1:
            report_error("over-enthusiastic spaces")
        # if self.is_in_subdir(
        #         file_path, 'source',
        #         'include') and X_ENVOY_USED_DIRECTLY_REGEX.match(line):
        #     report_error(
        #         "Please do not use the raw literal x-envoy in source code.  See Envoy::Http::PrefixValue."
        #     )
        if self.has_invalid_angle_bracket_directory(line):
            report_error("envoy includes should not have angle brackets")
        for invalid_construct, valid_construct in PROTOBUF_TYPE_ERRORS.items():
            if invalid_construct in line:
                report_error("incorrect protobuf type reference %s; "
                             "should be %s" %
                             (invalid_construct, valid_construct))
        for invalid_construct, valid_construct in LIBCXX_REPLACEMENTS.items():
            if invalid_construct in line:
                report_error(
                    "term %s should be replaced with standard library term %s"
                    % (invalid_construct, valid_construct))
        for invalid_construct, valid_construct in CODE_CONVENTION_REPLACEMENTS.items(
        ):
            if invalid_construct in line:
                report_error(
                    "term %s should be replaced with preferred term %s" %
                    (invalid_construct, valid_construct))
        # Do not include the virtual_includes headers.
        if re.search("#include.*/_virtual_includes/", line):
            report_error("Don't include the virtual includes headers.")

        if line.startswith("#include <mutex>") or line.startswith(
                "#include <condition_variable"):
            # We don't check here for std::mutex because that may legitimately show up in
            # comments, for example this one.
            report_error(
                "Don't use <mutex> or <condition_variable*>, switch to "
                "Thread::MutexBasicLockable in source/common/common/thread.h")
        if line.startswith("#include <shared_mutex>"):
            # We don't check here for std::shared_timed_mutex because that may
            # legitimately show up in comments, for example this one.
            report_error(
                "Don't use <shared_mutex>, use absl::Mutex for reader/writer locks."
            )
        duration_arg = DURATION_VALUE_REGEX.search(line)
        if duration_arg and duration_arg.group(
                1) != "0" and duration_arg.group(1) != "0.0":
            # Matching duration(int-const or float-const) other than zero
            report_error(
                "Don't use ambiguous duration(value), use an explicit duration type, e.g. Event::TimeSystem::Milliseconds(value)"
            )
        # Check that we use the absl::Time library
        if self.token_in_line("std::get_time", line):
            if "test/" in file_path:
                report_error(
                    "Don't use std::get_time; use TestUtility::parseTime in tests"
                )
            else:
                report_error(
                    "Don't use std::get_time; use the injectable time system")
        if self.token_in_line("std::put_time", line):
            report_error(
                "Don't use std::put_time; use absl::Time equivalent instead")
        if self.token_in_line("gmtime", line):
            report_error("Don't use gmtime; use absl::Time equivalent instead")
        if self.token_in_line("mktime", line):
            report_error("Don't use mktime; use absl::Time equivalent instead")
        if self.token_in_line("localtime", line):
            report_error(
                "Don't use localtime; use absl::Time equivalent instead")
        if self.token_in_line("strftime", line):
            report_error("Don't use strftime; use absl::FormatTime instead")
        if self.token_in_line("strptime", line):
            report_error("Don't use strptime; use absl::FormatTime instead")
        if self.token_in_line("strerror", line):
            report_error("Don't use strerror; use Envoy::errorDetails instead")
        # Prefer using abseil hash maps/sets over std::unordered_map/set for performance optimizations and
        # non-deterministic iteration order that exposes faulty assertions.
        # See: https://abseil.io/docs/cpp/guides/container#hash-tables
        # if "std::unordered_map" in line:
        #     report_error(
        #         "Don't use std::unordered_map; use absl::flat_hash_map instead or "
        #         "absl::node_hash_map if pointer stability of keys/values is required"
        #     )
        # if "std::unordered_set" in line:
        #     report_error(
        #         "Don't use std::unordered_set; use absl::flat_hash_set instead or "
        #         "absl::node_hash_set if pointer stability of keys/values is required"
        #     )
        if "std::atomic_" in line:
            # The std::atomic_* free functions are functionally equivalent to calling
            # operations on std::atomic<T> objects, so prefer to use that instead.
            report_error(
                "Don't use free std::atomic_* functions, use std::atomic<T> members instead."
            )
        # Block usage of certain std types/functions as iOS 11 and macOS 10.13
        # do not support these at runtime.
        # See: https://github.com/envoyproxy/envoy/issues/12341
        if self.token_in_line("std::any", line):
            report_error("Don't use std::any; use absl::any instead")
        if self.token_in_line("std::get_if", line):
            report_error("Don't use std::get_if; use absl::get_if instead")
        if self.token_in_line("std::holds_alternative", line):
            report_error(
                "Don't use std::holds_alternative; use absl::holds_alternative instead"
            )
        if self.token_in_line("std::make_optional", line):
            report_error(
                "Don't use std::make_optional; use absl::make_optional instead"
            )
        if self.token_in_line("std::monostate", line):
            report_error(
                "Don't use std::monostate; use absl::monostate instead")
        if self.token_in_line("std::optional", line):
            report_error("Don't use std::optional; use absl::optional instead")
        if not "NOLINT(std::string_view)" in line:
            if self.token_in_line("std::string_view",
                                  line) or self.token_in_line(
                                      "toStdStringView", line):
                report_error(
                    "Don't use std::string_view or toStdStringView; use absl::string_view instead"
                )
        if self.token_in_line("std::variant", line):
            report_error("Don't use std::variant; use absl::variant instead")
        if self.token_in_line("std::visit", line):
            report_error("Don't use std::visit; use absl::visit instead")
        if "__attribute__((packed))" in line and file_path != "./envoy/common/platform.h":
            # __attribute__((packed)) is not supported by MSVC, we have a PACKED_STRUCT macro that
            # can be used instead
            report_error(
                "Don't use __attribute__((packed)), use the PACKED_STRUCT macro defined "
                "in envoy/common/platform.h instead")
        if DESIGNATED_INITIALIZER_REGEX.search(line):
            # Designated initializers are not part of the C++14 standard and are not supported
            # by MSVC
            report_error(
                "Don't use designated initializers in struct initialization, "
                "they are not part of C++14")
        if " ?: " in line:
            # The ?: operator is non-standard, it is a GCC extension
            report_error(
                "Don't use the '?:' operator, it is a non-standard GCC extension"
            )
        if line.startswith("using testing::Test;"):
            report_error(
                "Don't use 'using testing::Test;, elaborate the type instead")
        if line.startswith("using testing::TestWithParams;"):
            report_error(
                "Don't use 'using testing::Test;, elaborate the type instead")
        if TEST_NAME_STARTING_LOWER_CASE_REGEX.search(line):
            # Matches variants of TEST(), TEST_P(), TEST_F() etc. where the test name begins
            # with a lowercase letter.
            report_error(
                "Test names should be CamelCase, starting with a capital letter"
            )
        if OLD_MOCK_METHOD_REGEX.search(line):
            report_error(
                "The MOCK_METHODn() macros should not be used, use MOCK_METHOD() instead"
            )
        if FOR_EACH_N_REGEX.search(line):
            report_error(
                "std::for_each_n should not be used, use an alternative for loop instead"
            )

        if self.is_in_subdir(file_path, 'source') and file_path.endswith('.cc') and \
            ('.counterFromString(' in line or '.gaugeFromString(' in line or
             '.histogramFromString(' in line or '.textReadoutFromString(' in line or
             '->counterFromString(' in line or '->gaugeFromString(' in line or
                '->histogramFromString(' in line or '->textReadoutFromString(' in line):
            report_error(
                "Don't lookup stats by name at runtime; use StatName saved during construction"
            )

        if MANGLED_PROTOBUF_NAME_REGEX.search(line):
            report_error("Don't use mangled Protobuf names for enum constants")

        # if "lua_pushlightuserdata" in line:
        #     report_error(
        #         "Don't use lua_pushlightuserdata, since it can cause unprotected error in call to"
        #         +
        #         "Lua API (bad light userdata pointer) on ARM64 architecture. See "
        #         +
        #         "https://github.com/LuaJIT/LuaJIT/issues/450#issuecomment-433659873 for details."
        #     )

        if file_path.endswith(PROTO_SUFFIX):
            exclude_path = ['v1', 'v2']
            result = PROTO_VALIDATION_STRING.search(line)
            if result is not None:
                if not any(x in file_path for x in exclude_path):
                    report_error("min_bytes is DEPRECATED, Use min_len.")

    def check_build_line(self, line, file_path, report_error):
        if "@bazel_tools" in line and not (self.is_starlark_file(file_path)
                                           or file_path.startswith("./bazel/")
                                           or "python/runfiles" in line):
            report_error(
                "unexpected @bazel_tools reference, please indirect via a definition in //bazel"
            )
        if '"protobuf"' in line:
            report_error(
                "unexpected direct external dependency on protobuf, use "
                "//source/common/protobuf instead.")

    def fix_build_path(self, file_path):
        error_messages = []

        if not self.is_api_file(file_path) and not self.is_starlark_file(
                file_path) and not self.is_workspace_file(file_path):
            if os.system("%s %s %s" %
                         (ENVOY_BUILD_FIXER_PATH, file_path, file_path)) != 0:
                error_messages += [
                    "envoy_build_fixer rewrite failed for file: %s" % file_path
                ]

        if os.system("%s -lint=fix -mode=fix %s" %
                     (BUILDIFIER_PATH, file_path)) != 0:
            error_messages += [
                "buildifier rewrite failed for file: %s" % file_path
            ]
        return error_messages

    def check_build_path(self, file_path):
        error_messages = []

        if self.is_build_file(file_path) and file_path.startswith(
                self.api_prefix + "proxy"):
            found = False
            for line in self.read_lines(file_path):
                if "api_proto_package(" in line:
                    found = True
                    break
            if not found:
                error_messages += [
                    "API build file does not provide api_proto_package()"
                ]

        command = "%s -mode=diff %s" % (BUILDIFIER_PATH, file_path)
        error_messages += self.execute_command(command,
                                               "buildifier check failed",
                                               file_path)
        error_messages += self.check_file_contents(file_path,
                                                   self.check_build_line)
        return error_messages

    def fix_source_path(self, file_path):
        self.evaluate_lines(file_path, self.fix_source_line)

        error_messages = []

        if not file_path.endswith(PROTO_SUFFIX):
            error_messages += self.fix_header_order(file_path)
        error_messages += self.clang_format(file_path)
        if file_path.endswith(PROTO_SUFFIX) and self.is_api_file(file_path):
            package_name, error_message = self.package_name_for_proto(
                file_path)
            if package_name is None:
                error_messages += error_message
        return error_messages

    def check_source_path(self, file_path):
        error_messages = self.check_file_contents(file_path,
                                                  self.check_source_line)

        if not file_path.endswith(PROTO_SUFFIX):
            error_messages += self.check_namespace(file_path)
            command = ("%s --include_dir_order %s --path %s | diff %s -" %
                       (HEADER_ORDER_PATH, self.include_dir_order, file_path,
                        file_path))
            error_messages += self.execute_command(
                command, "header_order.py check failed", file_path)
        command = ("%s %s | diff %s -" %
                   (CLANG_FORMAT_PATH, file_path, file_path))
        error_messages += self.execute_command(command,
                                               "clang-format check failed",
                                               file_path)

        if file_path.endswith(PROTO_SUFFIX) and self.is_api_file(file_path):
            package_name, error_message = self.package_name_for_proto(
                file_path)
            if package_name is None:
                error_messages += error_message
        return error_messages

    # Example target outputs are:
    #   - "26,27c26"
    #   - "12,13d13"
    #   - "7a8,9"
    def execute_command(
        self,
        command,
        error_message,
        file_path,
        regex=re.compile(r"^(\d+)[a|c|d]?\d*(?:,\d+[a|c|d]?\d*)?$")):
        try:
            output = subprocess.check_output(command,
                                             shell=True,
                                             stderr=subprocess.STDOUT).strip()
            if output:
                return output.decode('utf-8').split("\n")
            return []
        except subprocess.CalledProcessError as e:
            if (e.returncode != 0 and e.returncode != 1):
                return [
                    "ERROR: something went wrong while executing: %s" % e.cmd
                ]
            # In case we can't find any line numbers, record an error message first.
            error_messages = ["%s for file: %s" % (error_message, file_path)]
            for line in e.output.decode('utf-8').splitlines():
                for num in regex.findall(line):
                    error_messages.append("  %s:%s" % (file_path, num))
            return error_messages

    def fix_header_order(self, file_path):
        command = "%s --rewrite --include_dir_order %s --path %s" % (
            HEADER_ORDER_PATH, self.include_dir_order, file_path)
        if os.system(command) != 0:
            return ["header_order.py rewrite error: %s" % (file_path)]
        return []

    def clang_format(self, file_path):
        command = "%s -i %s" % (CLANG_FORMAT_PATH, file_path)
        if os.system(command) != 0:
            return ["clang-format rewrite error: %s" % (file_path)]
        return []

    def check_format(self, file_path):
        error_messages = []
        # Apply fixes first, if asked, and then run checks. If we wind up attempting to fix
        # an issue, but there's still an error, that's a problem.
        try_to_fix = self.operation_type == "fix"
        if self.is_build_file(file_path) or self.is_starlark_file(
                file_path) or self.is_workspace_file(file_path):
            if try_to_fix:
                error_messages += self.fix_build_path(file_path)
            error_messages += self.check_build_path(file_path)
        else:
            if try_to_fix:
                error_messages += self.fix_source_path(file_path)
            error_messages += self.check_source_path(file_path)

        if error_messages:
            return ["From %s" % file_path] + error_messages
        return error_messages

    def check_format_return_trace_on_error(self, file_path):
        """Run check_format and return the traceback of any exception."""
        try:
            return self.check_format(file_path)
        except:
            return traceback.format_exc().split("\n")

    def check_format_visitor(self, pool, result_list, dir_name, names):
        """Run check_format in parallel for the given files.
    Args:
      pool: multiprocessing tasks pool.
      result_list: task result list.
      dir_name: the parent directory of the given files.
      names: a list of file names.
    """
        for file_name in names:
            result = pool.apply_async(self.check_format_return_trace_on_error,
                                      args=(dir_name + "/" + file_name, ))
            result_list.append(result)

    # check_error_messages iterates over the list with error messages and prints
    # errors and returns a bool based on whether there were any errors.
    def check_error_messages(self, error_messages):
        if error_messages:
            for e in error_messages:
                print("ERROR: %s" % e)
            return True
        return False


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Check or fix file format.")
    parser.add_argument(
        "operation_type",
        type=str,
        choices=["check", "fix"],
        help="specify if the run should 'check' or 'fix' format.")
    parser.add_argument(
        "target_path",
        type=str,
        nargs="?",
        default=".",
        help=
        "specify the root directory for the script to recurse over. Default '.'."
    )
    parser.add_argument(
        "--num-workers",
        type=int,
        default=multiprocessing.cpu_count(),
        help="number of worker processes to use; defaults to one per core.")
    parser.add_argument("--api-prefix",
                        type=str,
                        default="./api/",
                        help="path of the API tree.")
    parser.add_argument(
        "--namespace_check",
        type=str,
        nargs="?",
        default="Proxy",
        help="specify namespace check string. Default 'Proxy'.")
    parser.add_argument(
        "--include_dir_order",
        type=str,
        default=",".join(common.include_dir_order()),
        help="specify the header block include directory order.")

    args = parser.parse_args()
    format_checker = FormatChecker(args)

    # Check whether all needed external tools are available.
    ct_error_messages = format_checker.check_tools()
    if format_checker.check_error_messages(ct_error_messages):
        sys.exit(1)

    # Calculate the list of owned directories once per run.
    error_messages = []

    if os.path.isfile(args.target_path):
        # All of our EXCLUDED_PREFIXES start with "./", but the provided
        # target path argument might not. Add it here if it is missing,
        # and use that normalized path for both lookup and `check_format`.
        normalized_target_path = args.target_path
        if not normalized_target_path.startswith("./"):
            normalized_target_path = "./" + normalized_target_path
        if not normalized_target_path.startswith(
                EXCLUDED_PREFIXES) and normalized_target_path.endswith(
                    SUFFIXES):
            error_messages += format_checker.check_format(
                normalized_target_path)
    else:
        results = []

        def pooled_check_format(path_predicate):
            pool = multiprocessing.Pool(processes=args.num_workers)
            # For each file in target_path, start a new task in the pool and collect the
            # results (results is passed by reference, and is used as an output).
            for root, _, files in os.walk(args.target_path):
                _files = []
                for filename in files:
                    file_path = os.path.join(root, filename)
                    check_file = (path_predicate(filename) and
                                  not file_path.startswith(EXCLUDED_PREFIXES)
                                  and file_path.endswith(SUFFIXES))
                    if check_file:
                        _files.append(filename)
                if not _files:
                    continue
                format_checker.check_format_visitor(pool, results, root,
                                                    _files)

            # Close the pool to new tasks, wait for all of the running tasks to finish,
            # then collect the error messages.
            pool.close()
            pool.join()

        # We first run formatting on non-BUILD files, since the BUILD file format
        # requires analysis of srcs/hdrs in the BUILD file, and we don't want these
        # to be rewritten by other multiprocessing pooled processes.
        pooled_check_format(lambda f: not format_checker.is_build_file(f))
        pooled_check_format(lambda f: format_checker.is_build_file(f))

        error_messages += sum((r.get() for r in results), [])

    if format_checker.check_error_messages(error_messages):
        print(
            "ERROR: check format failed. run 'tools/code_format/check_format.py fix'"
        )
        sys.exit(1)

    if args.operation_type == "check":
        print("PASS")
