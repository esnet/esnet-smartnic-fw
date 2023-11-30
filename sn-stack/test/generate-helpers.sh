#!/usr/bin/env bash

set -e -o pipefail

this_dir=$(dirname $(readlink -f "$0"))

test_dir="${this_dir}"
extra_dir="${test_dir}/extra"

mnt_dir='/test'
mnt_extra_dir="${mnt_dir}/extra"

#-------------------------------------------------------------------------------
# Special handling is needed when using symlinks to external directories for
# adding extra tests:
# - At build-time, the Python package dependencies need to be installed into
#   the container. All pip-requirements.txt files hidden behind symlinks must
#   be found and merged into a single file that will be within scope during a
#   docker build.
#
# - At run-time, the external directories need to be mounted into the container.
#   The path to every symlink target must be fully resolved and passed on the
#   command line as an extra volume mount to bind it to the correct location
#   within the container.
#   NOTE: A quirk of docker requires the symlink target to be relative. When a
#         target is absolute, the mount fails silently but the container is
#         still started.

# Forcefully start with empty files.
docker_args="${test_dir}/docker-arguments.txt"
:>"${docker_args}"

pip_reqs="${test_dir}/pip-requirements.txt"
:>"${pip_reqs}"

for link in $(find "${extra_dir}" -mindepth 1 -maxdepth 1 -type l); do
    name=$(basename "${link}")
    target=$(readlink -f "${link}")

    # Create a volume mount for the symlink.
    echo "-v ${target}:${mnt_extra_dir}/${name}:ro" >>"${docker_args}"

    # Merge the Python package dependencies.
    find "${target}" -type f -name pip-requirements.txt \
        -exec cat '{}' \; >>"${pip_reqs}"
done

#-------------------------------------------------------------------------------
# Create a file containing robot's command line arguments.
robot_args="${test_dir}/robot-arguments.txt"
cat <<EOF >"${robot_args}"
# Auto-generated command line arguments for invoking robot.
--outputdir=/scratch

# Search paths for custom Python library code.
EOF

find -L "${test_dir}" -type d -name library -printf '%P\n' |\
    sed -e "s:^:--pythonpath=${mnt_dir}/:" >>"${robot_args}"

cat <<EOF >>"${robot_args}"

# Paths to top-level test suites.
EOF

find -L "${test_dir}" -type d -regex '.+/suites/[^/]+$' -printf '%P\n' |\
    sed -e "s:^:${mnt_dir}/:" >>"${robot_args}"
