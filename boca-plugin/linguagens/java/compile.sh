#!/bin/bash
# Validador Estrutural BOCA (Java)
SOURCE_FILE="$1"
EXE_FILE="$2"

if ! grep -q "pensar\s*(" "$SOURCE_FILE"; then
    echo "ERRO DE VALIDACAO: O metodo pensar(JsonObject estado) nao foi encontrado no seu bot."
    exit 1
fi

if grep -q -E "Runtime\.getRuntime|ProcessBuilder|System\.exit" "$SOURCE_FILE"; then
    echo "ERRO DE VALIDACAO: Chamada de execucao de sistema nao permitida."
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GSON_JAR=""

if [ -f "$SCRIPT_DIR/gson.jar" ]; then
    GSON_JAR="$SCRIPT_DIR/gson.jar"
elif [ -f "$SCRIPT_DIR/../../../libs/gson-2.10.1.jar" ]; then
    GSON_JAR="$SCRIPT_DIR/../../../libs/gson-2.10.1.jar"
fi

if [ -n "$GSON_JAR" ]; then
    javac -cp ".:$GSON_JAR" "$SOURCE_FILE" || exit 1
fi

cp "$SOURCE_FILE" "$EXE_FILE"
exit 0

