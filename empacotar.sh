#!/bin/bash
# ==============================================================================
# Game Jam PET - Empacotador (fonte única de engine.zip e dos pacotes de linguagem)
# ==============================================================================
# Gera, a partir do battle-orchestrator, tudo o que o BOCA instala:
#   <saida>/engine.zip                    script.sh + engine/ + wrappers/ + libs/ + fases/*.json
#   <saida>/languages/<ext>.tgz           pacote de validação (c, cpp, java, python3)
#   <saida>/languages/<ext>/              o mesmo pacote já extraído
# Os arquivos são determinísticos: rodar de novo sem mudar os fontes gera os mesmos bytes
# (tar com --sort=name --mtime=@0 --owner=0 --group=0, gzip -n; zip com datas e modos fixos).
#
# Uso:
#   bash empacotar.sh --saida DIR [--engine] [--linguagens]   (sem as duas opções: gera ambos)
#   bash empacotar.sh --sincronizar-kit     copia os arquivos de apoio (arena.h, arena.hpp, Arena.java, arena.py), json.hpp, gson.jar e o verificador para o starter kit
#   bash empacotar.sh --conferir-kit        confere se o starter kit tem as mesmas cópias (código 1 se não)
# O starter kit é o repositório Starter-Kit-Game-Jam, ao lado deste: GAMEJAM_KIT_DIR
# (padrão ../Starter-Kit-Game-Jam, relativo à raiz do battle-orchestrator).
#
# Quem chama: gerenciar_engine.py (--empacotar, --empacotar-linguagens) e o Dockerfile.
# Requer bash, tar, gzip e python3 (o zip é gerado pela biblioteca padrão do Python).
# ==============================================================================
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SAIDA=""
FAZER_ENGINE=0
FAZER_LINGUAGENS=0
FAZER_SINCRONIZAR=0
FAZER_CONFERIR=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --saida) SAIDA="${2:?--saida exige um diretorio}"; shift 2 ;;
        --engine) FAZER_ENGINE=1; shift ;;
        --linguagens) FAZER_LINGUAGENS=1; shift ;;
        --sincronizar-kit) FAZER_SINCRONIZAR=1; shift ;;
        --conferir-kit) FAZER_CONFERIR=1; shift ;;
        -h|--help) sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "Opcao desconhecida: $1" >&2; exit 2 ;;
    esac
done

# Cópias do kit do participante: origem (relativa a esta raiz) -> destino (relativo ao kit)
KIT_DIR="${GAMEJAM_KIT_DIR:-${RAIZ}/../Starter-Kit-Game-Jam}"
KIT_COPIAS=(
    # arquivos de apoio (tipos e constantes em português) que a equipe importa
    "wrappers/arena.h|c/arena.h"
    "wrappers/arena.hpp|cpp/arena.hpp"
    "wrappers/Arena.java|java/Arena.java"
    "wrappers/arena.py|python/arena.py"
    "engine/nlohmann/json.hpp|cpp/nlohmann/json.hpp"
    "libs/gson-2.10.1.jar|java/gson.jar"
    # verificar.py do kit: a mesma montagem e as mesmas checagens da partida
    "wrappers/build_bot.sh|verificador/build_bot.sh"
    "wrappers/checar_fonte.py|verificador/checar_fonte.py"
    "wrappers/checar_python.py|verificador/checar_python.py"
    "wrappers/wrapper.py|verificador/wrapper.py"
    "wrappers/wrapper.java|verificador/wrapper.java"
    "wrappers/cpp_header.hpp|verificador/cpp_header.hpp"
    "wrappers/cpp_footer.cpp|verificador/cpp_footer.cpp"
    "wrappers/c_wrapper.cpp|verificador/c_wrapper.cpp"
)
if [[ $FAZER_SINCRONIZAR -eq 1 || $FAZER_CONFERIR -eq 1 ]] && [[ ! -f "${KIT_DIR}/README.md" ]]; then
    echo "Starter kit nao encontrado em ${KIT_DIR} (clone BartSOS/Starter-Kit-Game-Jam ou defina GAMEJAM_KIT_DIR)" >&2
    exit 2
fi

if [[ $FAZER_SINCRONIZAR -eq 1 ]]; then
    for par in "${KIT_COPIAS[@]}"; do
        origem="${par%%|*}"; destino="${par##*|}"
        mkdir -p "$(dirname "${KIT_DIR}/${destino}")"
        cp -- "${RAIZ}/${origem}" "${KIT_DIR}/${destino}"
        echo "[kit] ${destino} <- ${origem}"
    done
    [[ -n "$SAIDA" || $FAZER_ENGINE -eq 1 || $FAZER_LINGUAGENS -eq 1 || $FAZER_CONFERIR -eq 1 ]] || exit 0
fi

if [[ $FAZER_CONFERIR -eq 1 ]]; then
    divergentes=0
    for par in "${KIT_COPIAS[@]}"; do
        origem="${par%%|*}"; destino="${par##*|}"
        if cmp -s -- "${RAIZ}/${origem}" "${KIT_DIR}/${destino}"; then
            echo "[kit] OK        ${destino}"
        else
            echo "[kit] DIVERGE   ${destino} (rode: bash empacotar.sh --sincronizar-kit)" >&2
            divergentes=1
        fi
    done
    [[ $divergentes -eq 0 ]] || exit 1
    [[ -n "$SAIDA" || $FAZER_ENGINE -eq 1 || $FAZER_LINGUAGENS -eq 1 ]] || exit 0
fi

if [[ -z "$SAIDA" ]]; then
    echo "Uso: bash empacotar.sh --saida DIR [--engine] [--linguagens]" >&2
    exit 2
fi
if [[ $FAZER_ENGINE -eq 0 && $FAZER_LINGUAGENS -eq 0 ]]; then
    FAZER_ENGINE=1
    FAZER_LINGUAGENS=1
fi
mkdir -p "$SAIDA"
SAIDA="$(cd "$SAIDA" && pwd)"

TEMPORARIOS=()
limpar() {
    if [[ ${#TEMPORARIOS[@]} -gt 0 ]]; then
        rm -rf -- "${TEMPORARIOS[@]}"
    fi
}
trap limpar EXIT

# ------------------------------------------------------------------------------
# engine.zip
# ------------------------------------------------------------------------------
empacotar_engine() {
    [[ -f "${RAIZ}/script.sh" && -f "${RAIZ}/engine/main.cpp" ]] || {
        echo "ERRO: script.sh ou engine/main.cpp ausentes em ${RAIZ}" >&2
        exit 1
    }
    python3 -I - "$RAIZ" "${SAIDA}/engine.zip" <<'PY'
import os
import sys
import zipfile

raiz, destino = sys.argv[1], sys.argv[2]
arquivos = ["script.sh"]
# fases/: só os arquivos de fase (fases/<id>.json), que o BOCA lista lendo o engine.zip
for pasta in ("engine", "wrappers", "libs", "fases"):
    for dirpath, dirnames, nomes in os.walk(os.path.join(raiz, pasta)):
        dirnames[:] = sorted(d for d in dirnames if d != "__pycache__")
        if pasta == "fases":
            dirnames[:] = []
        for nome in sorted(nomes):
            if nome.endswith((".pyc", ".gch", ".o")):
                continue
            if pasta == "fases" and not nome.endswith(".json"):
                continue
            arquivos.append(os.path.relpath(os.path.join(dirpath, nome), raiz).replace(os.sep, "/"))
arquivos = ["script.sh"] + sorted(set(arquivos) - {"script.sh"})

tmp = destino + ".tmp"
with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for rel in arquivos:
        with open(os.path.join(raiz, rel), "rb") as f:
            dados = f.read()
        if rel.endswith(".sh"):
            dados = dados.replace(b"\r\n", b"\n")
        info = zipfile.ZipInfo(rel, date_time=(1980, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = (0o755 if rel.endswith(".sh") else 0o644) << 16
        z.writestr(info, dados, compresslevel=9)
os.replace(tmp, destino)
print(f"[engine] engine.zip: {len(arquivos)} arquivos, {os.path.getsize(destino) / 1024:.0f} KB")
PY
}

# ------------------------------------------------------------------------------
# Pacotes de linguagem
# ------------------------------------------------------------------------------
# Cada pacote leva: compile, compile.sh, compile_comum.sh, run, run.sh, wrappers/ e as bibliotecas
# que a linguagem usa. O compile.sh de cada pasta de boca-plugin/linguagens/ define LINGUAGEM.
# pasta|arquivos de wrappers/|engine/nlohmann/json.hpp?|libs/gson?
PACOTES=(
    "c|build_bot.sh arena.h c_wrapper.cpp checar_fonte.py|sim|nao"
    "cpp|build_bot.sh arena.hpp cpp_header.hpp cpp_footer.cpp checar_fonte.py|sim|nao"
    "java|build_bot.sh Arena.java wrapper.java checar_fonte.py|nao|sim"
    "python3|build_bot.sh arena.py wrapper.py checar_python.py|nao|nao"
)

empacotar_linguagens() {
    local plugin="${RAIZ}/boca-plugin/linguagens"
    local stage
    stage="$(mktemp -d /tmp/gamejam_pacotes_XXXXXX)"
    TEMPORARIOS+=("$stage")
    mkdir -p "${SAIDA}/languages"

    local item
    for item in "${PACOTES[@]}"; do
        IFS='|' read -r pasta wrappers json gson <<< "$item"
        local pkg="${stage}/${pasta}"
        mkdir -p "${pkg}/wrappers"

        cp -- "${plugin}/${pasta}/compile.sh" "${pkg}/compile.sh"
        cp -- "${plugin}/${pasta}/compile.sh" "${pkg}/compile"
        cp -- "${plugin}/compile_comum.sh" "${pkg}/compile_comum.sh"
        printf '#!/bin/bash\nexit 0\n' > "${pkg}/run.sh"
        cp -- "${pkg}/run.sh" "${pkg}/run"
        local arq
        for arq in $wrappers; do
            cp -- "${RAIZ}/wrappers/${arq}" "${pkg}/wrappers/${arq}"
        done
        if [[ "$json" == "sim" ]]; then
            mkdir -p "${pkg}/engine/nlohmann"
            cp -- "${RAIZ}/engine/nlohmann/json.hpp" "${pkg}/engine/nlohmann/json.hpp"
        fi
        if [[ "$gson" == "sim" ]]; then
            mkdir -p "${pkg}/libs"
            cp -- "${RAIZ}/libs/gson-2.10.1.jar" "${pkg}/libs/gson-2.10.1.jar"
        fi

        # Scripts em LF e permissões fixas (o tar guarda os modos)
        find "$pkg" -type f -name '*.sh' -exec sed -i 's/\r$//' {} +
        find "$pkg" -type d -exec chmod 755 {} +
        find "$pkg" -type f -exec chmod 644 {} +
        chmod 755 "${pkg}/compile" "${pkg}/compile.sh" "${pkg}/run" "${pkg}/run.sh" "${pkg}/wrappers/build_bot.sh"

        local tgz="${SAIDA}/languages/${pasta}.tgz"
        tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -cf - -C "$pkg" . | gzip -n -9 > "${tgz}.tmp"
        mv -f -- "${tgz}.tmp" "$tgz"

        # Pasta extraída (o BOCA só extrai o .tgz se a pasta não existir)
        local destino="${SAIDA}/languages/${pasta}"
        rm -rf -- "${destino:?}"
        mkdir -p "$destino"
        tar -xzf "$tgz" -C "$destino" --no-same-owner
        echo "[linguagens] ${pasta}: $(du -k "$tgz" | cut -f1) KB"
    done
}

[[ $FAZER_ENGINE -eq 0 ]] || empacotar_engine
[[ $FAZER_LINGUAGENS -eq 0 ]] || empacotar_linguagens
echo "[empacotar] pronto em ${SAIDA}"
