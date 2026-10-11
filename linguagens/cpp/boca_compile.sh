#!/bin/bash
# Validador Estrutural BOCA (C++)
SOURCE_FILE="$1"
EXE_FILE="$2"

if ! grep -q "pensar(" "$SOURCE_FILE"; then
    echo "ERRO DE VALIDACAO: A funcao pensar() não foi encontrada no seu bot."
    exit 1
fi

if grep -q "system(" "$SOURCE_FILE"; then
    echo "ERRO DE VALIDACAO: Uso de funcoes de sistema nao permitido."
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HEADER_FILE=""
INC_DIR=""

if [ -f "$SCRIPT_DIR/cpp_header.hpp" ]; then
    HEADER_FILE="$SCRIPT_DIR/cpp_header.hpp"
    INC_DIR="$SCRIPT_DIR"
elif [ -f "$SCRIPT_DIR/../../wrappers/cpp_header.hpp" ]; then
    HEADER_FILE="$SCRIPT_DIR/../../wrappers/cpp_header.hpp"
    INC_DIR="$SCRIPT_DIR/../../engine"
fi

if [ -n "$HEADER_FILE" ]; then
    g++ -O2 -std=c++17 -fsyntax-only -include "$HEADER_FILE" -I"$INC_DIR" "$SOURCE_FILE" || exit 1
else
    g++ -O2 -std=c++17 -fsyntax-only "$SOURCE_FILE" || exit 1
fi
exit 0
