#!/bin/bash

# TODO: need to generalize this script

trap exit ERR

set -u

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
SCRIPT_NAME=$(basename "${BASH_SOURCE[0]}")

IFS=$'\n'
DOCKER_FILES=( $(find ${SCRIPT_DIR} -type f -not -executable | sort) )

for i in "${DOCKER_FILES[@]}"; do
    echo Pushing image "$(basename $i)"
    docker push tarmio/builders:$(basename $i)
done

