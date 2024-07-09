#!/usr/bin/env bash

set -e

#---------------------------------------------------------------------------------------------------
pkg_name="$1"; shift
pkg_ver="$1"; shift
pkg_desc="$1"; shift
priv_dir="$1"; shift
out_dir="$1"; shift
src_dir="$1"; shift
inputs=( "$@" )

pkg_dir="${priv_dir}/${pkg_name}"
pyproject_toml="${priv_dir}/pyproject.toml"

mkdir -p "${pkg_dir}"

# Copy over static Python library files from the source tree.
for input in "${inputs[@]}"; do
    base="${input#${src_dir}}"

    # Filter-out the protobuf/grpc generated files.
    if [[ "${base}" =~ '_pb2' ]]; then
        continue;
    fi

    dst="${pkg_dir}/${base}"
    dst_dir=$(dirname "${dst}")
    if ! [[ -e "${dst_dir}" ]]; then
        mkdir -p "${dst_dir}"
    fi
    cp -v "${input}" "${dst}"
done

#---------------------------------------------------------------------------------------------------
cat <<EOF >"${pyproject_toml}"
[tool.poetry]
name = "${pkg_name}"
version = "${pkg_ver}"
description = "Python interface to ${pkg_desc} protobuf structures."
authors = []

include = ["*_pb2*"] # Include the protobuf/grpc generated files into the package.

[tool.poetry.dependencies]
grpcio = "^1.60.0"
python = "^3.8"

[build-system]
requires = ["poetry-core"]
build-backend = "poetry.core.masonry.api"
EOF

pushd "${priv_dir}"
poetry build
popd

cp -rv "${priv_dir}/dist/." "${out_dir}/."
