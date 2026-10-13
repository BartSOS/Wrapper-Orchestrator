#!/bin/bash
# Validador Estrutural BOCA (C99)
# O bot implementa Acao pensar(const Estado *e); arena.h é injetado na compilação.
SOURCE_FILE="$1"
EXE_FILE="$2"

if ! grep -q "pensar(" "$SOURCE_FILE"; then
    echo "ERRO DE VALIDACAO: A funcao Acao pensar(const Estado *e) nao foi encontrada no seu bot."
    exit 1
fi

if grep -q "system(" "$SOURCE_FILE"; then
    echo "ERRO DE VALIDACAO: Uso de funcoes de sistema nao permitido."
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARENA_H=""
if [ -f "$SCRIPT_DIR/arena.h" ]; then
    ARENA_H="$SCRIPT_DIR/arena.h"
elif [ -f "$SCRIPT_DIR/../../../wrappers/arena.h" ]; then
    ARENA_H="$SCRIPT_DIR/../../../wrappers/arena.h"
else
    echo "ERRO INTERNO: arena.h nao encontrado no pacote da linguagem C."
    exit 1
fi

gcc -O2 -std=c99 -fsyntax-only -include "$ARENA_H" "$SOURCE_FILE" || exit 1
exit 0
