#!/usr/bin/env bash

set -e

#-------------------------------------------------------------------------------
regio_generate="$1"; shift
template_dir="$1"; shift
priv_dir="$1"; shift
out_dir="$1"; shift
top_ir="$1"; shift

#-------------------------------------------------------------------------------
cmd=("${regio_generate}")
cmd+=('--file-type=top')
cmd+=('--generator=py')
cmd+=('--recursive')
cmd+=("--template-dir=${template_dir}")
cmd+=("--output-dir=${priv_dir}")
cmd+=("${top_ir}")

mkdir -p "${priv_dir}"
eval "${cmd[@]}"

python_dir="${priv_dir}/python"
pushd "${python_dir}"
poetry build
popd

cp -rv "${python_dir}/dist/." "${out_dir}/."
