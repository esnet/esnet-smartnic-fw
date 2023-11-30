#!/usr/bin/env bash

set -e -o pipefail

# Aguments:
#  Extra command line arguments are passed directly to "docker compose run".

this_dir=$(dirname $(readlink -f "$0"))

test_dir="${this_dir}"
args="${test_dir}/docker-arguments.txt"

# Start the test execution service.
docker compose run --no-deps --rm $(<"${args}") "$@" smartnic-fw-test
