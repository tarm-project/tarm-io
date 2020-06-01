#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

#!/bin/bash

trap exit ERR

set -u

SCRIPT_DIR=$(realpath "$(dirname "${BASH_SOURCE[0]}")")
SCRIPT_NAME=$(basename "${BASH_SOURCE[0]}")

IMAGE_TO_EXECUTE="tarmio/builders:$(echo ${1} | sed 's|tarmio/builders:||')"
COMMAND_TO_EXECUTE="${2:-bash}"

USER_ID=$(id -u $USER)
GROUP_ID=$(id -g $USER)

docker pull "${IMAGE_TO_EXECUTE}" || true

echo "Running image ${IMAGE_TO_EXECUTE}"

DOCKER_TERMINAL_OPTIONS="-it"
if [ ! -z ${GITHUB_ACTION:-} ]; then
    DOCKER_TERMINAL_OPTIONS=""
fi

# Domain socket (needed for tests)
CREATE_DOMAIN_SOCKET_COMMAND="python -c \"import socket as s; sock = s.socket(s.AF_UNIX); sock.bind('/var/run/somesocket')\""

#CREATE_GROUP="groupadd -g ${GROUP_ID} ${USER}"
#CREATE_USER="useradd -u ${USER_ID} -g ${USER} --create-home -s /bin/bash ${USER}"

if [[ ${IMAGE_TO_EXECUTE} == *"alpine"* ]]; then
    echo "Alpine Linux detected!"
    CREATE_GROUP="true" 
    CREATE_USER="adduser --disabled-password --uid ${USER_ID} --shell /bin/bash -g ${USER} ${USER}"
else
    CREATE_GROUP="groupadd -g ${GROUP_ID} ${USER}"
    CREATE_USER="useradd -u ${USER_ID} -g ${USER} --create-home -s /bin/bash ${USER}"
fi

#CREATE_GROUP="addgroup -S ${USER}"
#CREATE_USER="adduser --shell /bin/bash -S -G ${USER} ${USER}"

# '--privileged' is required for leak sanitizer(and other tools) to work.
docker run ${DOCKER_TERMINAL_OPTIONS} \
           --privileged \
           --cap-add=ALL \
           --rm \
           -v "${SCRIPT_DIR}/..":"/source" \
           --network=host \
           "${IMAGE_TO_EXECUTE}" \
           bash -c "echo 'You are in docker now!' && \
                    ${CREATE_DOMAIN_SOCKET_COMMAND} && \
                    export HOME=/home/${USER} && \
                    ${CREATE_GROUP} && \
                    ${CREATE_USER} && \
                    echo '${USER}   ALL = NOPASSWD : ALL' | sudo EDITOR='tee -a' visudo > /dev/null && \
                    sudo PATH=\$PATH -E -u ${USER} bash -c \"${COMMAND_TO_EXECUTE}\""

