#!/bin/bash
# Construtor do Orquestrador (Java)
# Detecta a classe do participante, compila e gera um JAR executável independente
SOURCE_FILE="$1"
EXE_FILE="$2"

DIR_CURR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR_ROOT="$(cd "${DIR_CURR}/../.." && pwd)"
GSON_JAR="${DIR_ROOT}/libs/gson-2.10.1.jar"
WRAPPER_SRC="${DIR_ROOT}/wrappers/wrapper.java"

BUILD_TMP="$(mktemp -d /tmp/java_build_XXXXXX)"
trap 'rm -rf "${BUILD_TMP}"' EXIT

# Descobre o nome da classe principal do participante
CLASS_NAME=$(grep -oE '(public\s+)?class\s+[A-Za-z0-9_]+' "$SOURCE_FILE" | head -n1 | awk '{print $NF}')
if [ -z "$CLASS_NAME" ]; then
    CLASS_NAME="Solution"
fi

# Copia o código do participante com o nome correto
cp "$SOURCE_FILE" "${BUILD_TMP}/${CLASS_NAME}.java"

# Injeta o nome da classe no Runner
sed "s/CLASS_PLACEHOLDER/${CLASS_NAME}/g" "$WRAPPER_SRC" > "${BUILD_TMP}/JavaRunner.java"

# Compila as classes vinculando Gson
javac -cp "${GSON_JAR}" "${BUILD_TMP}/${CLASS_NAME}.java" "${BUILD_TMP}/JavaRunner.java"
STATUS=$?
if [ $STATUS -ne 0 ]; then
    exit $STATUS
fi

# Descompacta o Gson para montar o Fat JAR auto-contido
(cd "${BUILD_TMP}" && jar -xf "${GSON_JAR}")

# Remove os arquivos fonte antes de empacotar
rm -f "${BUILD_TMP}/${CLASS_NAME}.java" "${BUILD_TMP}/JavaRunner.java"

# Empacota no JAR de destino
jar cfe "${EXE_FILE}" JavaRunner -C "${BUILD_TMP}" .
STATUS=$?

chmod +x "${EXE_FILE}"
exit $STATUS
