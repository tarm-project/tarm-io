#!/bin/echo "Error: This script is for sourcing only!"

trap exit ERR
set -u

DOCKER_REGISTRY_PREFIX=tarmio/builders