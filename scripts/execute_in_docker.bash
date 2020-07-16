#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

#!/bin/bash

trap exit ERR

set -u

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SCRIPT_NAME=$(basename "${BASH_SOURCE[0]}")

function usage()  {
    echo "Help:"
    echo "    Run tarmio/builders docker container from an image with optional command (default is shell)"
    echo "    Usage: ./${SCRIPT_NAME} <docker image name> [command to execute]"
    echo "    Example1: ./${SCRIPT_NAME} sphinx-docs"
    echo "    Example2: ./${SCRIPT_NAME} sphinx-docs \"cd doc && make html\""
}

if [ $# == 0 ]; then
    usage
    exit 1
fi

source "${SCRIPT_DIR}/env.bash"

IMAGE_TO_EXECUTE="${DOCKER_REGISTRY_PREFIX}:$(echo ${1} | sed "s|${DOCKER_REGISTRY_PREFIX}:||")"
COMMAND_TO_EXECUTE="${2:-bash}"

PORT=1234
NETWORK_CONFIG="--network=host"
USER_ID=$(id -u $USER)
if [ "${MACHINE}" = "Mac" ]; then
    GROUP_ID=1000
    NETWORK_CONFIG="-p ${PORT}:${PORT}/tcp -p ${PORT}:${PORT}/udp"
    echo "Binding port from host ${PORT} to container ${PORT}"
else
    GROUP_ID=$(id -g $USER)
fi

docker pull "${IMAGE_TO_EXECUTE}" || true

echo "Running image ${IMAGE_TO_EXECUTE}"

DOCKER_TERMINAL_OPTIONS="-it"
if [ ! -z ${GITHUB_ACTION:-} ]; then
    DOCKER_TERMINAL_OPTIONS=""
fi

# Domain socket (needed for tests)
CREATE_DOMAIN_SOCKET_COMMAND="python -c \"import socket as s; sock = s.socket(s.AF_UNIX); sock.bind('/var/run/somesocket')\""

if [[ ${IMAGE_TO_EXECUTE} == *"alpine"* ]]; then
    echo "Alpine Linux detected!"
    CREATE_GROUP="true" 
    CREATE_USER="adduser --disabled-password --uid ${USER_ID} --shell /bin/bash -g ${USER} ${USER}"
else
    CREATE_GROUP="groupadd -g ${GROUP_ID} ${USER}"
    CREATE_USER="useradd -u ${USER_ID} -g ${USER} --create-home -s /bin/bash ${USER}"
fi

# '--privileged' is required for leak sanitizer(and other tools) to work.
docker run ${DOCKER_TERMINAL_OPTIONS} \
           --privileged \
           --cap-add=ALL \
           --rm \
           -v "${SCRIPT_DIR}/..":"/source" \
           ${NETWORK_CONFIG} \
           "${IMAGE_TO_EXECUTE}" \
           bash -c "echo 'You are in docker now!' && \
                    ${CREATE_DOMAIN_SOCKET_COMMAND} && \
                    export HOME=/home/${USER} && \
                    ${CREATE_GROUP} && \
                    ${CREATE_USER} && \
                    echo '${USER}   ALL = NOPASSWD : ALL' | sudo EDITOR='tee -a' visudo > /dev/null && \
                    sudo PATH=\$PATH -E -u ${USER} bash -c \"${COMMAND_TO_EXECUTE}\""

