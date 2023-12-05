#!/usr/bin/env bash

set -e

this_dir=$(dirname $(readlink -f "$0"))

#-------------------------------------------------------------------------------
# Read build-time arguments from .env.
source .env

# Install the Robot Framework directory tree from the upstream pipeline.
artifacts="${this_dir}/sn-hw/artifacts.${SN_HW_BOARD}.${SN_HW_APP_NAME}.${SN_HW_VER}.zip"

test_dir="${this_dir}/sn-stack/test"
hw_test_dir="${test_dir}/hw"

#-------------------------------------------------------------------------------
# NOTE:
#     The busybox version of "unzip -t <zip-file> <target-file>" from the
#     alpine linux container exits with 0 even when <target-file> isn't
#     present in the <zip-file>. To work around this, the extraction is
#     performed unconditionally and the existence of the target is tested
#     afterwards instead.
robot_ar_base='robot-framework-test.tar.bz2'
robot_ar="${hw_test_dir}/${robot_ar_base}"

unzip "${artifacts}" "${robot_ar_base}" -d "${hw_test_dir}"
if [ -e "${robot_ar}" ]; then
    # Extract the HW application's test suites and library.
    tar -xf "${robot_ar}" -C "${hw_test_dir}"
    _rc=$?
    if [ ${_rc} -ne 0 ]; then
        echo "ERROR[${_rc}]: Failed to extract Robot Framework tests."
        exit 1
    fi

    # Cleanup.
    rm "${robot_ar}"
fi

exit 0
