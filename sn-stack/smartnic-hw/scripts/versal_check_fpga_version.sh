#!/bin/bash

set -e

HW_SERVER_URL="$1"
HW_TARGET_SERIAL="$2"
IMAGE_PATH="$3"

JSON_PATH='/tmp/v80.jtag.registers.json'

# Collect all of the JTAG register values for the currently loaded FPGA device
vivado_lab \
    -nolog \
    -nojournal \
    -tempDir /tmp/ \
    -mode batch \
    -notrace \
    -quiet \
    -source /scripts/read_jtag_registers.tcl \
    -tclargs "${HW_SERVER_URL}" "${HW_TARGET_SERIAL}" "${JSON_PATH}"

## echo '========================================'
## echo 'JTAG Registers JSON:'
## echo '----------------------------------------'
## cat "${JSON_PATH}"
## echo '========================================'

# Grab the USERCODE register value (ie. which FPGA is *currently* loaded)
USERCODE_HEX=$(cat "${JSON_PATH}" | jq -r '.["REGISTER.USERCODE"]')
USERCODE=$((${USERCODE_HEX}))
echo "Found JTAG USERCODE=${USERCODE_HEX} (${USERCODE})"

# Read the build ID field out of the header in the new target bit file
OPT_DATA=$(file -m /scripts/versal.magic "${IMAGE_PATH}" | sed -e 's@.* <ESnet optional data: \([^>]\+\)>.*@\1@')
echo "Found optional data: '${OPT_DATA}'"

BUILD_ID=$(echo "${OPT_DATA}" | sed -e 's@.*build_id=\([[:digit:]]\+\).*@\1@')
echo "Found Target Build ID=${BUILD_ID}"

# Compare the current with the target FPGA versions
if [[ "${USERCODE}" != "${BUILD_ID}" ]] ; then
    # Running version does not match target version
    exit 1
fi
