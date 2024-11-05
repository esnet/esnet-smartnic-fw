#!/usr/bin/env bash

set -e

#-------------------------------------------------------------------------------
PROG=$(basename "$0")
this_dir=$(dirname $(readlink -f "$0"))

#-------------------------------------------------------------------------------
print_usage() {
    echo "Usage: ${PROG} [options] <robot-arguments>"
}

#-------------------------------------------------------------------------------
print_help() {
    print_usage
    echo ""
    echo "    Execute Robot Framework tests."
    echo ""
    echo "Options:"
    echo "    -c ARG, --compose-arg=ARG"
    echo "        Specify extra arguments to pass to 'docker compose'."
    echo "    -h, --help"
    echo "        Show this help."
    echo "    -p, --pip-install"
    echo "        Allow installing Python package dependencies from pypi.org"
    echo "        prior to executing tests. Intended for use in development."
    echo "    -r ARG, --run-arg=ARG"
    echo "        Specify extra arguments to pass to 'docker compose run'."
    echo "    -s, --shell"
    echo "        Enter the container using the Bash shell as entrypoint."
    echo "    --"
    echo "        Stop processing options. All remaining arguments will be"
    echo "        passed to the Robot Framework."
}

#-------------------------------------------------------------------------------
compose_args=()
run_args=( '--no-deps' '--rm' )

# Parse command line arguments.
arguments=$(getopt -n "${PROG}" \
                -o 'c:hpr:s' \
                -l compose-arg: -l help -l pip-install -l run-arg: -l shell \
                -- "$@")
eval set -- "${arguments}"

# Process optional arguments.
while true; do
    opt="$1"
    shift

    case "${opt}" in
        --)
            break
            ;;

        -c | --compose-arg)
            compose_args+=( "$1" )
            shift
            ;;

        -h | --help)
            print_help
            exit 0
            ;;

        -p | --pip-install)
            run_args+=( "--env=TEST_DO_PIP_INSTALL=y" )
            ;;

        -r | --run-arg)
            run_args+=( "$1" )
            shift
            ;;

        -s | --shell)
            run_args+=( "--entrypoint=bash" )
            ;;

        *)
            echo "ERROR: Unknown option '${opt}'." >&2
            exit 1
            ;;
    esac
done

# Capture all remaining arguments for the Robot command line.
robot_args+=( "$@" )

#-------------------------------------------------------------------------------
# Start the test execution service.
docker \
    compose "${compose_args[@]}" \
    run "${run_args[@]}" \
    smartnic-fw-test \
    "${robot_args[@]}"
