#----------------------------------------------------------------------------------------------
#  Copyright (c) 2020 - present Alexander Voitenko
#  Licensed under the MIT License. See License.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

#!/bin/bash

trap exit ERR

set -u

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SCRIPT_NAME=$(basename "${BASH_SOURCE[0]}")

source "${SCRIPT_DIR}/env.bash"

"${SCRIPT_DIR}/execute_in_docker.bash" sphinx-docs "cd doc && rm -rf _build && make html"
