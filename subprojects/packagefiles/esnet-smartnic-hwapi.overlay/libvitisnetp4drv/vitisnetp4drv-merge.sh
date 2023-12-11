#!/usr/bin/env bash

set -e

#-------------------------------------------------------------------------------
priv_dir="$1"; shift
out_dir="$1"; shift
input_dir="$1"; shift
lib_base="$1"; shift
ip_names=( "$@" )

lib_name="lib${lib_base}.a"

#-------------------------------------------------------------------------------
mkdir -p "${priv_dir}"
for ip_name in "${ip_names[@]}"; do
    # Copy header files on top of whatever is already present.
    inc_dir="${input_dir}/${ip_name}/include"
    for hdr in $(find "${inc_dir}" -type f -name '*.h'); do
        cp "${hdr}" "${out_dir}/."
    done

    # Extract the object files on top of whatever is already present.
    ar -x --output "${priv_dir}" "${input_dir}/${ip_name}/lib/${lib_name}"
done

# Create a new archive containing all object files from all archives.
ar -rcs "${out_dir}/${lib_name}" $(find "${priv_dir}" -type f -name '*.o')
