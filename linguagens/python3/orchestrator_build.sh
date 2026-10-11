#!/bin/bash
# Construtor do Orquestrador (Python)
# Anexa o wrapper oficial ao código da submissão do competidor
SOURCE_FILE="$1"
EXE_FILE="$2"

DIR_CURR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR_ROOT="$(cd "${DIR_CURR}/../.." && pwd)"
WRAPPER_FILE="${DIR_ROOT}/wrappers/wrapper.py"

cat "${SOURCE_FILE}" "${WRAPPER_FILE}" > "${EXE_FILE}"
chmod +x "${EXE_FILE}"
exit 0
