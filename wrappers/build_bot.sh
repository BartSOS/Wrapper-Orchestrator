#!/bin/bash
# ==============================================================================
# TopCom Battle Orchestrator - Construtor de Bots
# ==============================================================================
# Compila e injeta o wrapper oficial em códigos C, C++, Python e Java.
# Uso: bash wrappers/build_bot.sh <arquivo_fonte> <arquivo_saida>
#      bash wrappers/build_bot.sh --preparar c|cpp ...   # só pré-compila os wrappers
#
# Os wrappers de C e C++ são pré-compilados uma vez e reaproveitados enquanto os
# fontes, o compilador e as flags não mudarem (assinatura SHA-256). Ficam em
# TOPCOM_WRAPPERS_BUILD (padrão: build/wrappers):
#   C   -> c/c_wrapper.o: main + JSON -> structs de arena.h. Cada bot só compila
#          o próprio .c (gcc -std=c99 -include arena.h) e é ligado a ele.
#   C++ -> cpp/cpp_header.hpp.gch (cabeçalho pré-compilado com nlohmann/json) e
#          cpp/cpp_footer.o (main + E/S JSON). Cada bot compila com
#          -include cpp_header.hpp e é ligado ao cpp_footer.o.
# O contrato do aluno não muda: C++ escreve json pensar(const json& estado) sem
# main nem includes; C escreve Acao pensar(const Estado *e).
# O script.sh chama --preparar antes de compilar os bots em paralelo.
# ==============================================================================
set -euo pipefail

DIR_WRAPPERS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR_ROOT="$(cd "${DIR_WRAPPERS}/.." && pwd)"
DIR_PREBUILT="${TOPCOM_WRAPPERS_BUILD:-${DIR_ROOT}/build/wrappers}"
NLOHMANN_INC="${DIR_ROOT}/engine"

CXX_FLAGS=(-O2 -std=c++20)
CC_FLAGS=(-O2 -std=c99)

# Arquivos temporários removidos ao sair (sucesso ou erro)
TEMPORARIOS=()
limpar() {
    if [[ ${#TEMPORARIOS[@]} -gt 0 ]]; then
        rm -rf -- "${TEMPORARIOS[@]}"
    fi
}
trap limpar EXIT

# Assinatura dos wrappers pré-compilados: compilador + flags + fontes
assinatura() {
    { g++ --version | head -n 1; echo "${CXX_FLAGS[*]} | ${CC_FLAGS[*]}"; cat "$@"; } | sha256sum | cut -d' ' -f1
}

# Compila para um arquivo temporário e o move para o destino (troca atômica)
compilar_para() {
    local destino="$1"; shift
    local tmp="${destino}.tmp.$$"
    TEMPORARIOS+=("$tmp")
    "$@" -o "$tmp"
    mv -f "$tmp" "$destino"
}

preparar_c() {
    local dir="${DIR_PREBUILT}/c"
    local ass
    ass="$(assinatura "${DIR_WRAPPERS}/c_wrapper.cpp" "${DIR_WRAPPERS}/arena.h" "${NLOHMANN_INC}/nlohmann/json.hpp")"
    if [[ -f "${dir}/c_wrapper.o" && "$(cat "${dir}/assinatura" 2>/dev/null)" == "$ass" ]]; then
        return 0
    fi
    mkdir -p "$dir"
    rm -f "${dir}/assinatura"
    compilar_para "${dir}/c_wrapper.o" \
        g++ "${CXX_FLAGS[@]}" -I"${DIR_WRAPPERS}" -I"${NLOHMANN_INC}" -c "${DIR_WRAPPERS}/c_wrapper.cpp"
    echo "$ass" > "${dir}/assinatura"
}

preparar_cpp() {
    local dir="${DIR_PREBUILT}/cpp"
    local ass
    ass="$(assinatura "${DIR_WRAPPERS}/cpp_header.hpp" "${DIR_WRAPPERS}/cpp_footer.cpp" "${NLOHMANN_INC}/nlohmann/json.hpp")"
    if [[ -f "${dir}/cpp_header.hpp.gch" && -f "${dir}/cpp_footer.o" && \
          "$(cat "${dir}/assinatura" 2>/dev/null)" == "$ass" ]]; then
        return 0
    fi
    mkdir -p "$dir"
    rm -f "${dir}/assinatura"
    # O .gch só é usado quando está ao lado do cabeçalho passado em -include
    cp "${DIR_WRAPPERS}/cpp_header.hpp" "${dir}/cpp_header.hpp"
    compilar_para "${dir}/cpp_header.hpp.gch" \
        g++ "${CXX_FLAGS[@]}" -I"${NLOHMANN_INC}" -x c++-header "${dir}/cpp_header.hpp"
    compilar_para "${dir}/cpp_footer.o" \
        g++ "${CXX_FLAGS[@]}" -I"${NLOHMANN_INC}" -include "${dir}/cpp_header.hpp" -Winvalid-pch \
            -c "${DIR_WRAPPERS}/cpp_footer.cpp"
    echo "$ass" > "${dir}/assinatura"
}

# --- Modo de preparação: bash build_bot.sh --preparar c cpp ---
if [[ "${1:-}" == "--preparar" ]]; then
    shift
    for lang in "$@"; do
        case "$lang" in
            c) preparar_c ;;
            cpp) preparar_cpp ;;
            *) ;;  # Python e Java não têm wrapper pré-compilado
        esac
    done
    exit 0
fi

if [ $# -lt 2 ]; then
    echo "Uso: $0 <arquivo_fonte> <arquivo_saida>" >&2
    echo "     $0 --preparar c|cpp ..." >&2
    exit 1
fi

SOURCE_FILE="$1"
EXE_FILE="$2"

EXT="${SOURCE_FILE##*.}"
EXT=$(echo "$EXT" | tr '[:upper:]' '[:lower:]')

# Uma falha de compilação não pode deixar o binário de uma partida anterior no lugar
rm -f "${EXE_FILE}"

case "$EXT" in
    cpp)
        preparar_cpp
        PCH_DIR="${DIR_PREBUILT}/cpp"
        OBJ_BOT="${EXE_FILE}_bot.o"
        TEMPORARIOS+=("${OBJ_BOT}")

        # Código do aluno com o cabeçalho pré-compilado (mesmo -include do validador do BOCA)
        g++ "${CXX_FLAGS[@]}" -I"${NLOHMANN_INC}" -include "${PCH_DIR}/cpp_header.hpp" -Winvalid-pch \
            -x c++ -c "${SOURCE_FILE}" -o "${OBJ_BOT}"

        if ! g++ "${OBJ_BOT}" "${PCH_DIR}/cpp_footer.o" -o "${EXE_FILE}" -lm; then
            # Compilou, mas não ligou ao main pré-compilado (ex.: pensar() declarada inline):
            # repete como antes, com cabeçalho + código + rodapé numa única unidade.
            echo "[build_bot] Ligacao com o main pre-compilado falhou; compilando em unidade unica." >&2
            TMP_SRC="${EXE_FILE}_combined.cpp"
            TEMPORARIOS+=("${TMP_SRC}")
            cat "${DIR_WRAPPERS}/cpp_header.hpp" "${SOURCE_FILE}" "${DIR_WRAPPERS}/cpp_footer.cpp" > "${TMP_SRC}"
            g++ "${CXX_FLAGS[@]}" -I"${NLOHMANN_INC}" "${TMP_SRC}" -o "${EXE_FILE}" -lm
        fi
        ;;

    c)
        # Código do competidor em C99 com arena.h injetado; wrapper de I/O (C++20) pré-compilado
        preparar_c
        OBJ_BOT="${EXE_FILE}_bot.o"
        TEMPORARIOS+=("${OBJ_BOT}")
        gcc "${CC_FLAGS[@]}" -include "${DIR_WRAPPERS}/arena.h" -x c -c "${SOURCE_FILE}" -o "${OBJ_BOT}"
        g++ "${OBJ_BOT}" "${DIR_PREBUILT}/c/c_wrapper.o" -o "${EXE_FILE}" -lm
        ;;

    py)
        # Confere a sintaxe já aqui (como o validador do BOCA), sem gravar .pyc: um erro de
        # sintaxe vira exclusão com motivo, e não uma desconexão no tick 0
        python3 -c 'import sys; compile(open(sys.argv[1], "rb").read(), sys.argv[1], "exec")' "${SOURCE_FILE}"
        WRAPPER_FILE="${DIR_WRAPPERS}/wrapper.py"
        cat "${SOURCE_FILE}" "${WRAPPER_FILE}" > "${EXE_FILE}"
        chmod +x "${EXE_FILE}"
        ;;

    java)
        GSON_JAR="${DIR_ROOT}/libs/gson-2.10.1.jar"
        WRAPPER_SRC="${DIR_WRAPPERS}/wrapper.java"
        BUILD_TMP="$(mktemp -d /tmp/java_build_XXXXXX)"
        TEMPORARIOS+=("${BUILD_TMP}")

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
