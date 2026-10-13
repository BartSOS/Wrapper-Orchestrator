#!/bin/bash
# ==============================================================================
# TopCom Battle Orchestrator - Construtor de Bots
# ==============================================================================
# Compila e injeta o wrapper oficial em códigos C, C++, Python e Java.
# Uso: ./wrappers/build_bot.sh <arquivo_fonte> <arquivo_saida>
# ==============================================================================
set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Uso: $0 <arquivo_fonte> <arquivo_saida>" >&2
    exit 1
fi

SOURCE_FILE="$1"
EXE_FILE="$2"

DIR_WRAPPERS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR_ROOT="$(cd "${DIR_WRAPPERS}/.." && pwd)"
EXT="${SOURCE_FILE##*.}"
EXT=$(echo "$EXT" | tr '[:upper:]' '[:lower:]')

case "$EXT" in
    cpp)
        HEADER="${DIR_WRAPPERS}/cpp_header.hpp"
        FOOTER="${DIR_WRAPPERS}/cpp_footer.cpp"
        TMP_SRC="${EXE_FILE}_combined.cpp"
        
        cat "${HEADER}" "${SOURCE_FILE}" "${FOOTER}" > "${TMP_SRC}"
        g++ -O2 -std=c++20 -I"${DIR_ROOT}/engine" "${TMP_SRC}" -o "${EXE_FILE}" -lm
        rm -f "${TMP_SRC}"
        ;;

    c)
        # Código do competidor em C99 com arena.h injetado; wrapper de I/O em C++20
        OBJ_BOT="${EXE_FILE}_bot.o"
        gcc -O2 -std=c99 -include "${DIR_WRAPPERS}/arena.h" -c "${SOURCE_FILE}" -o "${OBJ_BOT}"
        g++ -O2 -std=c++20 -I"${DIR_WRAPPERS}" -I"${DIR_ROOT}/engine" \
            "${DIR_WRAPPERS}/c_wrapper.cpp" "${OBJ_BOT}" -o "${EXE_FILE}" -lm
        rm -f "${OBJ_BOT}"
        ;;

    py)
        WRAPPER_FILE="${DIR_WRAPPERS}/wrapper.py"
        cat "${SOURCE_FILE}" "${WRAPPER_FILE}" > "${EXE_FILE}"
        chmod +x "${EXE_FILE}"
        ;;

    java)
        GSON_JAR="${DIR_ROOT}/libs/gson-2.10.1.jar"
        WRAPPER_SRC="${DIR_WRAPPERS}/wrapper.java"
        BUILD_TMP="$(mktemp -d /tmp/java_build_XXXXXX)"
        trap 'rm -rf "${BUILD_TMP}"' EXIT

        CLASS_NAME=$(grep -oE '(public\s+)?class\s+[A-Za-z0-9_]+' "$SOURCE_FILE" | head -n1 | awk '{print $NF}')
        if [ -z "$CLASS_NAME" ]; then
            CLASS_NAME="Solution"
        fi

        cp "$SOURCE_FILE" "${BUILD_TMP}/${CLASS_NAME}.java"
        sed "s/CLASS_PLACEHOLDER/${CLASS_NAME}/g" "$WRAPPER_SRC" > "${BUILD_TMP}/JavaRunner.java"
        javac -cp "${GSON_JAR}" "${BUILD_TMP}/${CLASS_NAME}.java" "${BUILD_TMP}/JavaRunner.java"

        (cd "${BUILD_TMP}" && jar -xf "${GSON_JAR}")
        rm -f "${BUILD_TMP}/${CLASS_NAME}.java" "${BUILD_TMP}/JavaRunner.java"
        jar cfe "${EXE_FILE}" JavaRunner -C "${BUILD_TMP}" .
        chmod +x "${EXE_FILE}"
        ;;

    *)
        echo "Extensao nao suportada: .${EXT}" >&2
        exit 1
        ;;
esac

