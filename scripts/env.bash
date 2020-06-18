#!/bin/echo "Error: This script is for sourcing only!"

UNAME_OUT="$(uname -s)"
case "${UNAME_OUT}" in
    Linux*)     MACHINE=Linux;;
    Darwin*)    MACHINE=Mac;;
    CYGWIN*)    MACHINE=Cygwin;;
    MINGW*)     MACHINE=MinGw;;
    *)          MACHINE="UNKNOWN"
esac

if [ "${MACHINE}" = "UNKNOWN" ]; then
    echo "Error. Unknown machine: ${UNAME_OUT}"
    exit 1;
fi

DOCKER_REGISTRY_PREFIX=tarmio/builders
