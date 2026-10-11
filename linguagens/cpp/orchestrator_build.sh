#!/bin/bash
# Construtor do Orquestrador (C++)
# Une o cabeçalho oficial (JSON), o código do competidor e o loop principal de I/O
SOURCE_FILE="$1"
EXE_FILE="$2"

DIR_CURR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR_ROOT="$(cd "${DIR_CURR}/../.." && pwd)"

HEADER="${DIR_ROOT}/wrappers/cpp_header.hpp"
FOOTER="${DIR_ROOT}/wrappers/cpp_footer.cpp"
TMP_SRC="${EXE_FILE}_combined.cpp"

cat "${HEADER}" "${SOURCE_FILE}" "${FOOTER}" > "${TMP_SRC}"

g++ -O2 -std=c++17 -I"${DIR_ROOT}/engine" "${TMP_SRC}" -o "${EXE_FILE}" -lm
STATUS=$?

rm -f "${TMP_SRC}"
exit ${STATUS}
