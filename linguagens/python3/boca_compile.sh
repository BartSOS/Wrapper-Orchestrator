#!/bin/bash
# Validador Estrutural BOCA (Python)
# Esse script sera empacotado para o BOCA validar sintaxe e regras da maratona
SOURCE_FILE="$1"
EXE_FILE="$2"

if ! grep -q -E "def pensar\s*\(" "$SOURCE_FILE"; then
    echo "ERRO DE VALIDACAO: A funcao pensar(estado) nao foi encontrada no seu bot."
    exit 1
fi

if grep -q -E "import\s+os|from\s+os|import\s+subprocess|from\s+subprocess|eval\s*\(|exec\s*\(" "$SOURCE_FILE"; then
    echo "ERRO DE VALIDACAO: Foram detectados comandos ou imports maliciosos nao permitidos."
    exit 1
fi

python3 -m py_compile "$SOURCE_FILE" || exit 1

cp "$SOURCE_FILE" "$EXE_FILE"
chmod +x "$EXE_FILE"
exit 0
