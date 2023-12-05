#!/usr/bin/env bash

set -e

#-------------------------------------------------------------------------------
PROG=$(basename "$0")

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
    echo "    -d ARG, --docker-arg=ARG"
    echo "        Specify extra arguments to pass to 'docker compose run'."
    echo "    -h, --help"
    echo "        Show this help."
    echo "    --"
    echo "        Stop processing options. All remaining arguments will be"
    echo "        passed to the Robot Framework."
}

#-------------------------------------------------------------------------------
docker_args=( '--no-deps' '--rm' )

# Parse command line arguments.
arguments=$(getopt -n "${PROG}" -o 'd:h' -l docker-arg: -l help -- "$@")
eval set -- "${arguments}"

# Process optional arguments.
while true; do
    opt="$1"
    shift

    case "${opt}" in
        --)
            break
            ;;

        -d | --docker-arg)
            docker_args+=( "$1" )
            shift
            ;;

        -h | --help)
            print_help
            exit 0
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
docker compose run "${docker_args[@]}" smartnic-fw-test "${robot_args[@]}"
