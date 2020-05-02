#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

#!/bin/bash

set -u

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
source "${SCRIPT_DIR}/env.bash"

PLACEHOLDER_FILE=${1}
VERSION_TO_SUBSTITUTE=${2}

RESULTING_IMAGE_TAG=$(echo "${PLACEHOLDER_FILE}" | sed "s|.template|${VERSION_TO_SUBSTITUTE}|")

echo "Building image: ${RESULTING_IMAGE_TAG}"

cp "${SCRIPT_DIR}/${PLACEHOLDER_FILE}" "/tmp/${RESULTING_IMAGE_TAG}"
sed -i "s|__OPENSSL_VERSION__|${VERSION_TO_SUBSTITUTE}|" "/tmp/${RESULTING_IMAGE_TAG}"

docker build -t ${DOCKER_REGISTRY_PREFIX}:${RESULTING_IMAGE_TAG} -f "/tmp/${RESULTING_IMAGE_TAG}" .