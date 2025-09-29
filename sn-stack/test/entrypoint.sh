#!/usr/bin/env bash

set -e -o pipefail

#-------------------------------------------------------------------------------
# Build up the command line to invoke the Robot Framework engine.
cmd=( 'robot' )

# Redirect report and log files.
cmd+=( '--outputdir=/scratch' )

# Give the aggregated test suite a sane name.
cmd+=( "--name=esnet-smartnic-fw-${SN_HW_BOARD}-${SN_HW_APP_NAME}-${SN_HW_VER}" )

# Setup the Python module search paths.
cmd+=( $(find /test -type d -name library | sed -e 's:^:--pythonpath=:') )

# Insert the global suite setup/teardown.
cmd+=( '/test/fw/__init__.robot' )

# Setup the Robot Test Data file search paths.
cmd+=( $(find /test -type d -regex '.+/suites/[^/]+$') )

# Extra options passed to the docker command line.
cmd+=( "$@" )

#-------------------------------------------------------------------------------
# Install extra Python package dependencies if the package index is available.
# This step is only needed when developing tests. For normal execution, the
# dependencies are already installed at build-time and pip would have nothing
# to do at run-time.
if [[ "${TEST_DO_PIP_INSTALL}" != "" ]]; then
    if curl --silent --output /dev/null 'https://pypi.org'; then
        for req in $(find /test -type f -name pip-requirements.txt); do
            pip3 install --quiet --no-deps --requirement="${req}"
        done
    fi
fi

#-------------------------------------------------------------------------------
# Stop Python from writing compiled byte code files. Since test libraries are
# mounted into the container, the .pyc files would be written back to the host.
export PYTHONDONTWRITEBYTECODE=1

# Run the tests.
exec "${cmd[@]}"
