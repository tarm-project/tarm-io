#!/bin/bash

trap exit ERR

set -u

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
SCRIPT_NAME=$(basename "${BASH_SOURCE[0]}")

IMAGE_TO_EXECUTE="tarmio/builders:${1}"
COMMAND_TO_EXECUTE="${2:-bash}"

USER_ID=$(id -u $USER)
GROUP_ID=$(id -g $USER)

docker pull "${IMAGE_TO_EXECUTE}" || true

echo "Running image ${IMAGE_TO_EXECUTE}"

DOCKER_TERMINAL_OPTIONS="-it"

# Domain socket (needed for tests)
CREATE_DOMAIN_SOCKET_COMMAND="python -c \"import socket as s; sock = s.socket(s.AF_UNIX); sock.bind('/var/run/somesocket')\""

#WORK_DIR=/source

# '--privileged' is required for leak sanitizer to work.
docker run ${DOCKER_TERMINAL_OPTIONS} \
           --privileged \
           --cap-add=ALL \
           --rm \
           -v "${SCRIPT_DIR}/../..":"/source" \
           --network=host \
           "${IMAGE_TO_EXECUTE}" \
           bash -c "echo 'You are in docker now!' && \
                    ${CREATE_DOMAIN_SOCKET_COMMAND} && \
                    groupadd -g ${GROUP_ID} ${USER} && \
                    useradd -u ${USER_ID} -g ${USER} ${USER} && \
                    echo '${USER}   ALL = NOPASSWD : ALL' | sudo EDITOR='tee -a' visudo > /dev/null && \
                    sudo PATH=\$PATH -E -u ${USER} bash -c \"${COMMAND_TO_EXECUTE}\""

