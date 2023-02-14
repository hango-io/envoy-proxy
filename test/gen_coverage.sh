#!/bin/bash

set -e
# set -x

[[ -z "${SRCDIR}" ]] && SRCDIR="$(pwd)"

read -ra BAZEL_BUILD_OPTIONS <<<"${BAZEL_BUILD_OPTIONS:-}"

# This is the target that will be run to generate coverage data. It can be overridden by consumer
# projects that want to run coverage on a different/combined target.
# Command-line arguments take precedence over ${COVERAGE_TARGET}.
if [[ $# -gt 0 ]]; then
    COVERAGE_TARGETS=$*
elif [[ -n "${COVERAGE_TARGET}" ]]; then
    COVERAGE_TARGETS=${COVERAGE_TARGET}
else
    COVERAGE_TARGETS=//test/...
fi

COVERAGE_DIR="${SRCDIR}"/generated/coverage
rm -rf "${COVERAGE_DIR}"
mkdir -p "${COVERAGE_DIR}"

echo "Starting gen_coverage.sh..."
echo "    SOURCE=${SRCDIR}"
echo "    TARGET=${COVERAGE_TARGETS}"
echo "    OUTPUT=${COVERAGE_DIR}"

echo "Generating coverage data..."
bazel coverage "${BAZEL_BUILD_OPTIONS[@]}" "${COVERAGE_TARGETS[@]}" --test_output=errors |
    tee "${COVERAGE_DIR}"/gen_coverage.log

# Collecting profile and testlogs
[[ -z "${ENVOY_BUILD_PROFILE}" ]] || cp -f "$(bazel info output_base)/command.profile.gz" "${ENVOY_BUILD_PROFILE}/coverage.profile.gz" || true
[[ -z "${ENVOY_BUILD_DIR}" ]] || find bazel-testlogs/ -name test.log | tar zcf "${ENVOY_BUILD_DIR}/testlogs.tar.gz" -T -

COVERAGE_DATA="${COVERAGE_DIR}/coverage.dat"
cp bazel-out/_coverage/_coverage_report.dat "${COVERAGE_DATA}"

COVERAGE_VALUE="$(genhtml --prefix "${PWD}" --output "${COVERAGE_DIR}" "${COVERAGE_DATA}" | tee /dev/stderr | grep lines... | cut -d ' ' -f 4)"
COVERAGE_VALUE=${COVERAGE_VALUE%?}

[[ -z "${ENVOY_COVERAGE_ARTIFACT}" ]] || tar zcf "${ENVOY_COVERAGE_ARTIFACT}" -C "${COVERAGE_DIR}" --transform 's/^\./coverage/' .
