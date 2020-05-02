#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

#!/bin/echo "Error: This script is for sourcing only!"

trap exit ERR
set -u

DOCKER_REGISTRY_PREFIX=tarmio/builders