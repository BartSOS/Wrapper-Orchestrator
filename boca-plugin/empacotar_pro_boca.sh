#!/bin/bash
# ==============================================================================
# TopCom BOCA Plugin - Empacotador de Linguagens para o BOCA (.tgz)
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

mkdir -p "${SCRIPT_DIR}/out_boca_packages"
cd "${SCRIPT_DIR}/linguagens"

for lang_dir in */; do
    lang=${lang_dir%/}
    compile_src="${SCRIPT_DIR}/linguagens/${lang}/compile.sh"
    
    if [ -d "$lang" ] && [ -f "$compile_src" ]; then
        echo "[BOCA-PLUGIN] Empacotando linguagem: $lang"
        
        PKG_TMP="$(mktemp -d /tmp/boca_pkg_XXXXXX)"
        
        # O script de validacao vira tanto compile quanto compile.sh
        cp "$compile_src" "${PKG_TMP}/compile.sh"
        cp "$compile_src" "${PKG_TMP}/compile"
        chmod +x "${PKG_TMP}/compile.sh" "${PKG_TMP}/compile"
        
        # O run do BOCA sera vazio (exit 0)
        printf '#!/bin/bash\nexit 0\n' > "${PKG_TMP}/run.sh"
        printf '#!/bin/bash\nexit 0\n' > "${PKG_TMP}/run"
        chmod +x "${PKG_TMP}/run.sh" "${PKG_TMP}/run"
        
        # C++: header com o tipo json + nlohmann/json.hpp
        if [ "$lang" = "cpp" ]; then
            cp "${ROOT_DIR}/wrappers/cpp_header.hpp" "${PKG_TMP}/"
            mkdir -p "${PKG_TMP}/nlohmann"
            cp "${ROOT_DIR}/engine/nlohmann/json.hpp" "${PKG_TMP}/nlohmann/"
        fi

        # C: interface arena.h (structs Estado/Acao)
        if [ "$lang" = "c" ]; then
            cp "${ROOT_DIR}/wrappers/arena.h" "${PKG_TMP}/"
        fi

        # Se Java, copia a lib gson
        if [ "$lang" = "java" ] && [ -f "${ROOT_DIR}/libs/gson-2.10.1.jar" ]; then
            cp "${ROOT_DIR}/libs/gson-2.10.1.jar" "${PKG_TMP}/gson.jar"
        fi

        # Gera o TGZ com todos os arquivos
        tar -czf "${SCRIPT_DIR}/out_boca_packages/${lang}_boca.tgz" -C "${PKG_TMP}" .
        cp "${SCRIPT_DIR}/out_boca_packages/${lang}_boca.tgz" "${SCRIPT_DIR}/out_boca_packages/${lang}.tgz"
        
        # Aliases para compatibilidade com extensoes comuns
        if [ "$lang" = "python3" ]; then
            cp "${SCRIPT_DIR}/out_boca_packages/${lang}_boca.tgz" "${SCRIPT_DIR}/out_boca_packages/py.tgz"
            cp "${SCRIPT_DIR}/out_boca_packages/${lang}_boca.tgz" "${SCRIPT_DIR}/out_boca_packages/py3.tgz"
        fi
        
        # Limpa o temp
        rm -rf "${PKG_TMP}"
    fi
done

echo "[BOCA-PLUGIN] Pacotes gerados com sucesso em boca-plugin/out_boca_packages/:"
ls -lh "${SCRIPT_DIR}/out_boca_packages/"
