#!/usr/bin/env bash

set -e -o pipefail

#-------------------------------------------------------------------------------
# Build up the command line to invoke the Robot Framework engine.
cmd=( 'robot' )

# Redirect report and log files.
cmd+=( '--outputdir=/scratch' )

# Generate a report in the XUnit format along with the defaults.
cmd+=( '--xunit=xunit.xml' )

# Give the aggregated test suite a sane name.
cmd+=( "--name=${SN_TEST_SUITE_NAME}" )

# Setup the Python module search paths.
cmd+=( $(find /test -type d -name library | sed -e 's:^:--pythonpath=:') )

# Extra options passed through the docker command line.
cmd+=( "$@" )

# Insert the global suite setup/teardown.
cmd+=( '/test/fw/__init__.robot' )

# Setup the Robot Test Data file search paths.
cmd+=( $(find /test -type d -regex '.+/suites/[^/]+$') )

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
${cmd[@]} | tee /scratch/robot.log

# Publish the test results to the S3 store.
if [[ -e "/scratch/output.xml" ]]; then
    results="/scratch/${TEST_FILENAME:-test-results.tar.gz}"
    tar czf "${results}" -C /scratch output.xml log.html report.html xunit.xml robot.log
fi
