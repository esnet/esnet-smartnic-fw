#!/usr/bin/env bash
# NOTE: This script is also used inside of an alpine linux container and
#       as such, needs to find bash at a different path than where it is
#       in ubuntu.  The indirection through /usr/bin/env is required.

this_dir=$(dirname $(readlink -f "$0"))

# By default, point to a user dev image in the local registry
DEFAULT_IMAGE_URI="esnet-smartnic-fw:${USER}-dev"

if [ $# -lt 1 ] ; then
    # Use a default container URI
    IMAGE_URI=${DEFAULT_IMAGE_URI}
elif [ $# -eq 1 ] ; then
    # User has provided the desired URI for the image we're building
    IMAGE_URI="$1"
else
    # Too many command line parameters
    echo "ERROR: incorrect number of parameters provided"
    echo
    echo "Usage: $0 [<ImageURI>]"
    echo "  ImageURI: optional Container Image URI to use as a tag for this image"
    echo "    See: https://github.com/opencontainers/.github/blob/master/docs/docs/introduction/digests.md#unique-resource-identifiers"
    echo "    e.g.: <registry>/<project>/<container>:<tag>@<digest>"
    echo "    e.g.: wharf.es.net/ht/esnet-smartnic-fw:33228-gf4c89e64"
    echo "  When no ImageURI is provided a default of '${DEFAULT_IMAGE_URI}' will be used"
    echo
    exit 1
fi
echo "Building container '${IMAGE_URI}'"

# Make sure these variables are **only** taken from the .env file
unset SMARTNIC_DPDK_IMAGE_URI
unset LABTOOLS_IMAGE_URI
unset SN_HW_GROUP
unset SN_HW_REPO
unset SN_HW_BRANCH
unset SN_HW_BOARD
unset SN_HW_APP_NAME
unset SN_HW_VER
unset SN_FW_GROUP
unset SN_FW_REPO
unset SN_FW_BRANCH
unset SN_FW_APP_NAME
unset SN_FW_VER

# Read build-time arguments from .env
source .env

# Check for missing mandatory build-arguments and fill in default values for optional arguments
SMARTNIC_DPDK_IMAGE_URI=${SMARTNIC_DPDK_IMAGE_URI:-smartnic-dpdk-docker:${USER}-dev}
LABTOOLS_IMAGE_URI=${LABTOOLS_IMAGE_URI:-xilinx-labtools-docker:${USER}-dev}

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

SN_FW_GROUP="${SN_FW_GROUP:-unset}"
SN_FW_REPO="${SN_FW_REPO:-unset}"
SN_FW_BRANCH="${SN_FW_BRANCH:-unset}"
SN_FW_APP_NAME="${SN_FW_APP_NAME:-unset}"
SN_FW_VER="${SN_FW_VER:-unset}"

# Install the Robot Framework directory tree from the upstream pipeline.
artifacts="${this_dir}/sn-hw/artifacts.${SN_HW_BOARD}.${SN_HW_APP_NAME}.${SN_HW_VER}.zip"

test_dir="${this_dir}/sn-stack/test"
extra_dir="${test_dir}/extra"
app_dir="${extra_dir}/${SN_HW_APP_NAME}"

# If the application's test directory is a symlink, assume a developer is
# actively working on tests and ignore any extras from the artifacts. If
# the path exists and isn't a symlink, assume it's left over from a previous
# build and replace it.
if [ ! -h "${app_dir}" ]; then
    rm -rf "${app_dir}"

    # NOTE:
    #     The busybox version of "unzip -t <zip-file> <target-file>" from the
    #     alpine linux container exits with 0 even when <target-file> isn't
    #     present in the <zip-file>. To work around this, the extraction is
    #     performed unconditionally and the existence of the target is tested
    #     afterwards instead.
    robot_ar_base='robot-framework-test.tar.bz2'
    robot_ar="${extra_dir}/${robot_ar_base}"

    unzip "${artifacts}" "${robot_ar_base}" -d "${extra_dir}"
    if [ -e "${robot_ar}" ]; then
        # Extract the application's test suites and library.
        mkdir "${app_dir}"
        tar -xf "${robot_ar}" -C "${app_dir}"
        _rc=$?
        if [ ${_rc} -ne 0 ]; then
            echo "ERROR[${_rc}]: Failed to extract Robot Framework tests."
            exit 1
        fi

        # Cleanup.
        rm "${robot_ar}"
    fi
fi

# Generate helper files for executing tests.
${test_dir}/generate-helpers.sh

# Build the image
export DOCKER_BUILDKIT=1
docker build \
	--progress=plain \
	--build-arg SMARTNIC_DPDK_IMAGE_URI=${SMARTNIC_DPDK_IMAGE_URI} \
	--build-arg SN_HW_GROUP=${SN_HW_GROUP} \
	--build-arg SN_HW_REPO=${SN_HW_REPO} \
	--build-arg SN_HW_BRANCH=${SN_HW_BRANCH} \
	--build-arg SN_HW_BOARD=${SN_HW_BOARD} \
	--build-arg SN_HW_APP_NAME=${SN_HW_APP_NAME} \
	--build-arg SN_HW_VER=${SN_HW_VER} \
	--build-arg SN_FW_GROUP=${SN_FW_GROUP} \
	--build-arg SN_FW_REPO=${SN_FW_REPO} \
	--build-arg SN_FW_BRANCH=${SN_FW_BRANCH} \
	--build-arg SN_FW_APP_NAME=${SN_FW_APP_NAME} \
	--build-arg SN_FW_VER=${SN_FW_VER} \
	-t ${IMAGE_URI} .
if [ $? -ne 0 ] ; then
    echo "ERROR: Failed to build container"
    exit 1
fi

# Rewrite a cleaned-up, simplified version of the input .env file used to build this image
# The buildinfo.env file will be available to users that embed the sn-stack directory into a larger compose stack
#
# NOTE: No blank lines or comment lines are allowed in this file since it may be used by downstream gitlab-ci builds
#       which limits the format: https://docs.gitlab.com/ee/ci/yaml/artifacts_reports.html#artifactsreportsdotenv
#
cat <<_EOF > sn-stack/buildinfo.env
SMARTNIC_FW_IMAGE_URI=${IMAGE_URI}
LABTOOLS_IMAGE_URI=${LABTOOLS_IMAGE_URI}
SMARTNIC_DPDK_IMAGE_URI=${SMARTNIC_DPDK_IMAGE_URI}
SN_FW_GROUP=${SN_FW_GROUP}
SN_FW_REPO=${SN_FW_REPO}
SN_FW_BRANCH=${SN_FW_BRANCH}
SN_FW_APP_NAME=${SN_FW_APP_NAME}
SN_FW_VER=${SN_FW_VER}
SN_HW_GROUP=${SN_HW_GROUP}
SN_HW_REPO=${SN_HW_REPO}
SN_HW_BRANCH=${SN_HW_BRANCH}
SN_HW_BOARD=${SN_HW_BOARD}
SN_HW_APP_NAME=${SN_HW_APP_NAME}
SN_HW_VER=${SN_HW_VER}
_EOF

# Update or create the user's sn-stack/.env file to refer to the container image URLs used during this build
if [ ! -e sn-stack/.env ] ; then
    # No previous .env file exists, initialize it from the example.env file
    cp sn-stack/example.env sn-stack/.env
fi

ENV_BLOCK_TOP="# block-start auto-generated by build"
ENV_BLOCK_BOT="# block-end   auto-generated by build"

# Check if we have a full marker block in the file already
#   Quit immediately with exit-code of 0 if block is found
#   else quit with exit-code of 1 on end of file
if egrep -q "^${ENV_BLOCK_TOP}\$" sn-stack/.env ; then
    # Found the marker block, update it in place
    sed -i -r \
        -e "/${ENV_BLOCK_TOP}/,/${ENV_BLOCK_BOT}/c\
${ENV_BLOCK_TOP}\n\
SMARTNIC_FW_IMAGE_URI=${IMAGE_URI}\n\
LABTOOLS_IMAGE_URI=${LABTOOLS_IMAGE_URI}\n\
SMARTNIC_DPDK_IMAGE_URI=${SMARTNIC_DPDK_IMAGE_URI}\n\
${ENV_BLOCK_BOT}" \
	sn-stack/.env
else
    # No marker block found, append it to the end of the file
    cat >>sn-stack/.env <<EOF

${ENV_BLOCK_TOP}
SMARTNIC_FW_IMAGE_URI=${IMAGE_URI}
LABTOOLS_IMAGE_URI=${LABTOOLS_IMAGE_URI}
SMARTNIC_DPDK_IMAGE_URI=${SMARTNIC_DPDK_IMAGE_URI}
${ENV_BLOCK_BOT}
EOF
fi

exit 0

