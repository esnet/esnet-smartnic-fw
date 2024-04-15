#!/usr/bin/env bash

set -e

#---------------------------------------------------------------------------------------------------
pkg_name="$1"; shift
pkg_ver="$1"; shift
priv_dir="$1"; shift
out_dir="$1"; shift
inputs=( "$@" )

pkg_dir="${priv_dir}/${pkg_name}"
pyproject_toml="${priv_dir}/pyproject.toml"

mkdir -p "${pkg_dir}"

# Copy over static Python library files from the source tree.
for input in "${inputs[@]}"; do
    base=$(basename "${input}")
    dst="${pkg_dir}/${base}"
    if ! [[ -e "${dst}" ]]; then
        cp -v "${input}" "${pkg_dir}/."
    fi
done

#---------------------------------------------------------------------------------------------------
cat <<EOF >"${pyproject_toml}"
[tool.poetry]
name = "${pkg_name}"
version = "${pkg_ver}"
description = "Python interface to the SmartNIC Configuration agent."
authors = []

[tool.poetry.dependencies]
click = "^8.1.7"
grpcio = "^1.60.0"
python = "^3.8"
sn_cfg_proto = "^0.1.0"

[tool.poetry.scripts]
sn-cfg = "sn_cfg.client:main"

[build-system]
requires = ["poetry-core"]
build-backend = "poetry.core.masonry.api"
EOF

pushd "${priv_dir}"
poetry build
popd

cp -rv "${priv_dir}/dist/." "${out_dir}/."
