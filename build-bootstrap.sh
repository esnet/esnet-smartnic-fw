#!/usr/bin/env bash
# NOTE: This script is also used inside of an alpine linux container and
#       as such, needs to find bash at a different path than where it is
#       in ubuntu.  The indirection through /usr/bin/env is required.

set -e

# Make sure these variables are **only** taken from the .env file
unset SN_HW_BOARD
unset SN_HW_APP_NAME
unset SN_HW_VER

# Read build-time arguments from .env
source .env

OUT=sn-bootstrap
mkdir -p ${OUT}

# Copy the system setup readme
cp sn-stack/README.SYSTEM.SETUP.md ${OUT}

# Build the smartnic-system-setup deb package after fixing up file and directory permissions
dpkg-deb --build --root-owner-group smartnic-system-setup ${OUT}

# Extract the firmware mcs file from the sn-hw artifact
unzip -q \
      -j \
      -c \
      ./sn-hw/artifacts.${SN_HW_BOARD}.${SN_HW_APP_NAME}.${SN_HW_VER}.zip \
      esnet-smartnic-hwapi/firmware/esnet-smartnic.mcs.zst \
  | zstdcat > ${OUT}/esnet-smartnic.${SN_HW_BOARD}.mcs

# Download the xbflash2 .deb file
XRT_XBFLASH2='xrt_202420.2.18.179_22.04-amd64-xbflash2.deb'
wget -q \
     -O- \
     "https://packages.xilinx.com/artifactory/debian-packages/pool/${XRT_XBFLASH2}" \
  | dpkg-deb --fsys-tarfile - \
  | tar -C ${OUT} --strip-components 4 -xf - ./usr/local/bin/xbflash2
