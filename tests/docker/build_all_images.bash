#!/bin/bash

trap exit ERR

set -u

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
SCRIPT_NAME=$(basename "${BASH_SOURCE[0]}")

IFS=$'\n'
DOCKER_FILES=( $(find ${SCRIPT_DIR} -type f -not -executable -and -not -name "*.template" | sort) )

for i in "${DOCKER_FILES[@]}"; do
    echo Building image "$i"
    echo $(basename $i)
    docker build -t tarmio/builders:$(basename $i) -f $i .
done
