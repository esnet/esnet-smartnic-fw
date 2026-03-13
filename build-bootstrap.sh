#!/usr/bin/env bash
# NOTE: This script is also used inside of an alpine linux container and
#       as such, needs to find bash at a different path than where it is
#       in ubuntu.  The indirection through /usr/bin/env is required.

set -e

# Make sure these variables are **only** taken from the .env file
unset SN_HW_GROUP
unset SN_HW_REPO
unset SN_HW_BRANCH
unset SN_HW_BOARD
unset SN_HW_APP_NAME
unset SN_HW_VER
unset SN_HW_COMMIT
unset SN_FW_GROUP
unset SN_FW_REPO
unset SN_FW_BRANCH
unset SN_FW_APP_NAME
unset SN_FW_VER
unset SN_FW_COMMIT

# Read build-time arguments from .env
source .env

# Check for missing mandatory build-arguments and fill in default values for optional arguments
SN_HW_GROUP="${SN_HW_GROUP:-unset}"
SN_HW_REPO="${SN_HW_REPO:-unset}"
SN_HW_BRANCH="${SN_HW_BRANCH:-unset}"
if [ ! -v SN_HW_BOARD ] ; then
	echo "ERROR: Missing environment variable 'SN_HW_BOARD' which is required"
	exit 1
fi

if [ ! -v SN_HW_APP_NAME ] ; then
	echo "ERROR: Missing environment variable 'SN_HW_APP_NAME' which is required"
	exit 1
fi

if [ ! -v SN_HW_VER ] ; then
	echo "ERROR: Missing environment variable 'SN_HW_VER' which is required"
	exit 1
fi
SN_HW_COMMIT="${SN_HW_COMMIT:-unset}"

SN_FW_GROUP="${SN_FW_GROUP:-unset}"
SN_FW_REPO="${SN_FW_REPO:-unset}"
SN_FW_BRANCH="${SN_FW_BRANCH:-unset}"
SN_FW_APP_NAME="${SN_FW_APP_NAME:-unset}"
SN_FW_VER="${SN_FW_VER:-unset}"
SN_FW_COMMIT="${SN_FW_COMMIT:-unset}"

OUT=sn-bootstrap
mkdir -p ${OUT}

# Rewrite a cleaned-up, simplified version of the input .env file used to build this image
#
cat <<_EOF > ${OUT}/buildinfo.env
SN_FW_GROUP=${SN_FW_GROUP}
SN_FW_REPO=${SN_FW_REPO}
SN_FW_BRANCH=${SN_FW_BRANCH}
SN_FW_APP_NAME=${SN_FW_APP_NAME}
SN_FW_VER=${SN_FW_VER}
SN_FW_COMMIT=${SN_FW_COMMIT}
SN_HW_GROUP=${SN_HW_GROUP}
SN_HW_REPO=${SN_HW_REPO}
SN_HW_BRANCH=${SN_HW_BRANCH}
SN_HW_BOARD=${SN_HW_BOARD}
SN_HW_APP_NAME=${SN_HW_APP_NAME}
SN_HW_VER=${SN_HW_VER}
SN_HW_COMMIT=${SN_HW_COMMIT}
_EOF

# Copy the system setup readme
cp sn-stack/README.SYSTEM.SETUP.md ${OUT}
cp sn-stack/README.SYSTEM.DECOM.md ${OUT}

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
