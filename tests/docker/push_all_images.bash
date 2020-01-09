#!/bin/bash

trap exit ERR

set -u

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
source "${SCRIPT_DIR}/env.bash"

PUSH_PREFIX="${1:-}"

IMAGES_TO_PUSH=($(docker images | grep ${DOCKER_REGISTRY_PREFIX} | grep "${PUSH_PREFIX}" | awk '{print $2}'))

for i in "${IMAGES_TO_PUSH[@]}"; do
	echo "Pushing image: ${DOCKER_REGISTRY_PREFIX}:$i"
    docker push ${DOCKER_REGISTRY_PREFIX}:$i
done

