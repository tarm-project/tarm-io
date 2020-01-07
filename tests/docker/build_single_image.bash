#!/bin/bash

trap exit ERR

set -u

DOCKERFILE_PATH=${1}

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
source "${SCRIPT_DIR}/env.bash"

pushd ${SCRIPT_DIR}
docker build -t ${DOCKER_REGISTRY_PREFIX}:$(basename ${DOCKERFILE_PATH}) -f ${DOCKERFILE_PATH} .
popd