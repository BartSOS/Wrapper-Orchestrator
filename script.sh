#!/usr/bin/env bash
# ==============================================================================
# ORQUESTRADOR DE GAME JAM DE IAS - ARENA TOPCOM BOCA
# Gerencia compilação, inventário pré/pós-execução, isolamento e relatórios.
#
# Uso: ./script.sh [DIR_ENTRADA]        (padrão: submissoes/)
# Ambiente:
#   TOPCOM_SEED           seed da partida (0 a 4294967295); sem ela, a engine sorteia e registra
#   TOPCOM_MAX_BOTS       limite de bots na partida (padrão 20)
#   TOPCOM_COMPILE_JOBS   compilações simultâneas (padrão: min(nproc, memória disponível / 500 MB))
#   TOPCOM_FASE           (fases de treino) caminho do JSON da fase: liga o modo fase
#   TOPCOM_FASE_SEED      (fases de treino) seed do sorteio do cenário (0 a 4294967295)
#   TOPCOM_BOT_UID_BASE   (arena) liga o modo isolado: o script roda como root e cada bot
#                         compila e joga com uid/gid próprio (ver "Modo isolado" abaixo)
#   TOPCOM_BOT_MAX_PROC   (modo isolado) ulimit -u de cada bot na partida (padrão 64)
#   TOPCOM_BOT_MAX_DISCO_MB (modo isolado) disco por bot, em MB (padrão 256)
#   TOPCOM_BOT_MAX_ARQUIVOS (modo isolado) arquivos por bot (padrão 2000)
# Saída: match_frames.json e replays/partida_<id>.json (seed, bots[], excluidos[], frames)
#
# Modo fase (TOPCOM_FASE definida): só 1 bot joga (o primeiro em ordem alfabética), a engine
# recebe --fase <JSON> --fase-seed <N>, o match_frames.json ganha o bloco "fase" (cenário
# sorteado) e, depois da partida, build/avaliar_fase <fase.json> match_frames.json grava
# resultado_fase.json. Se DIR_ENTRADA tiver uma subpasta submissoes/, os bots vêm dela e o
# resultado_fase.json vai para DIR_ENTRADA; senão os bots vêm de DIR_ENTRADA e o resultado
# fica ao lado do match_frames.json (raiz do orquestrador). Sem TOPCOM_FASE nada muda.
#
# Modo isolado (TOPCOM_BOT_UID_BASE definida; só o executor da arena define): o bot de
# índice i (0-based, na ordem de NOMES_BOTS, NPCs incluídos) usa uid = gid = BASE + i.
# Compilação (wrappers/build_bot.sh) e execução passam por setpriv (sem grupos, sem
# capabilities, no_new_privs), cada bot num diretório próprio build/bots/t<N>/ (dono do
# bot, 0700, com home/ e tmp/ para HOME e TMPDIR), umask 077, ulimit -u/-v/-f e o timeout
# de sempre. A engine, o avaliar_fase e o resto do script continuam como root. Um vigia
# encerra o bot que passar do limite de disco/arquivos; ao terminar, todo processo e
# arquivo dos uids dos bots é apagado. O stderr de cada bot vai para build/logs/<bot>.stderr
# (primeiros 64 KB) e as linhas "ISOLAMENTO-OK|FALHOU <caso>" e "[AVISO] <texto>" (avisos do
# wrapper, como um campo da ação que não é número) aparecem no log da partida.
# Sem a variável nada muda (uid único, /tmp comum, stderr dos bots descartado).
# ==============================================================================

set -euo pipefail

# --- Variaveis de Ambiente de Localizacao e Encoding ---
export LANG="C.UTF-8"
export LC_ALL="C.UTF-8"
export PYTHONIOENCODING="utf-8"
export TZ="America/Sao_Paulo"

# --- Configurações de Diretórios e Arquivos ---
DIR_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR_INPUT="${1:-${DIR_ROOT}/submissoes}"
DIR_BUILD="${DIR_ROOT}/build"

DIR_BOTS_BUILD="${DIR_BUILD}/bots"
DIR_LOGS="${DIR_BUILD}/logs"
DIR_REPLAYS="${DIR_ROOT}/replays"

# Diretórios para as bibliotecas locais
DIR_LIBS="${DIR_ROOT}/libs"
GSON_JAR="${DIR_LIBS}/gson-2.10.1.jar"
NLOHMANN_DIR="${DIR_ROOT}/engine/nlohmann"
NLOHMANN_HPP="${NLOHMANN_DIR}/json.hpp"

ENGINE_SRC="${DIR_ROOT}/engine/main.cpp"
ENGINE_BIN="${DIR_BUILD}/engine"
ENGINE_ASSINATURA="${DIR_BUILD}/engine.assinatura"
ENGINE_FLAGS=(-O3 -std=c++20)
BUILD_SCRIPT="${DIR_ROOT}/wrappers/build_bot.sh"

# --- Configurações de Segurança e Limites ---
# Tempo da partida: não há mais limite fixo de relógio por bot (era 30 s e matava
# todos os bots de partidas longas). A engine consulta os bots um por vez, com até
# RESPOSTA_MS por bot por tick (PARTIDA_INICIO_MS no tick 0), por até MAX_TICKS
# ticks; o orçamento da partida é calculado com essas constantes da engine:
#   orçamento = PARTIDA_INICIO_MS + MAX_TICKS x nº de bots x RESPOSTA_MS + FOLGA_PARTIDA_SEG
# Um vigia global (timeout) encerra a engine se o orçamento estourar; cada bot
# recebe o orçamento + FOLGA_BOT_SEG, só como rede de segurança.
# TOPCOM_TIMEOUT_PARTIDA (segundos) sobrepõe o orçamento calculado (ensaios).
FOLGA_PARTIDA_SEG=30
FOLGA_BOT_SEG=15
MEM_LIMIT_KB=2048000
# A JVM padrão reserva mais memória virtual que o ulimit -v acima (heap = 1/4 da RAM +
# 1 GB de "compressed class space") e nem chega a iniciar. Estes limites cabem nos ~2 GB.
JAVA_OPTS_BOT="-Xmx512m -XX:CompressedClassSpaceSize=64m -XX:+UseSerialGC"
MEM_POR_JOB_MB=500            # memória reservada por compilação simultânea (g++ com nlohmann/json)

# Limite de bots por partida (20 no evento; o benchmark pode sobrepor)
MAX_BOTS="${TOPCOM_MAX_BOTS:-20}"
if ! [[ "${MAX_BOTS}" =~ ^[0-9]+$ ]] || [[ "${MAX_BOTS}" -lt 1 ]]; then
    echo "[AVISO] TOPCOM_MAX_BOTS invalido ('${MAX_BOTS}'); usando 20."
    MAX_BOTS=20
fi

# --- Modo isolado (arena): uid próprio por bot. Valor inválido aborta (nunca roda sem isolar) ---
ISOLADO=0
BOT_UID_BASE="${TOPCOM_BOT_UID_BASE:-}"
BOT_MAX_PROC="${TOPCOM_BOT_MAX_PROC:-64}"
BOT_MAX_DISCO_MB="${TOPCOM_BOT_MAX_DISCO_MB:-256}"
BOT_MAX_ARQUIVOS="${TOPCOM_BOT_MAX_ARQUIVOS:-2000}"
COMPILACAO_MAX_PROC=256       # ulimit -u da compilação (javac e g++ criam várias threads/processos)
BOT_STDERR_MAX=65536          # bytes do stderr de cada bot copiados para build/logs/<bot>.stderr
if [[ -n "${BOT_UID_BASE}" ]]; then
    for _v in BOT_UID_BASE BOT_MAX_PROC BOT_MAX_DISCO_MB BOT_MAX_ARQUIVOS; do
        if ! [[ "${!_v}" =~ ^[0-9]{1,10}$ ]] || [[ "${!_v}" -lt 1 ]]; then
            echo "[ERRO] Modo isolado: valor invalido em TOPCOM_${_v} ('${!_v}')."
            exit 1
        fi
    done
    if [[ "${BOT_UID_BASE}" -lt 1000 || "${BOT_UID_BASE}" -gt 2000000000 ]]; then
        echo "[ERRO] TOPCOM_BOT_UID_BASE fora do intervalo 1000..2000000000 ('${BOT_UID_BASE}')."
        exit 1
    fi
    if [[ "$(id -u)" -ne 0 ]]; then
        echo "[ERRO] TOPCOM_BOT_UID_BASE definida, mas o script nao roda como root: o modo isolado exige root."
        exit 1
    fi
    if ! command -v setpriv &> /dev/null; then
        echo "[ERRO] TOPCOM_BOT_UID_BASE definida, mas o setpriv (util-linux) nao esta instalado."
        exit 1
    fi
    ISOLADO=1
fi

# --- Modo fase (fases de treino): 1 bot, cenário do arquivo da fase, avaliação no fim ---
AVALIAR_SRC="${DIR_ROOT}/engine/avaliar_fase.cpp"
AVALIAR_BIN="${DIR_BUILD}/avaliar_fase"
AVALIAR_ASSINATURA="${DIR_BUILD}/avaliar_fase.assinatura"
AVALIAR_PRONTO=0
FASE_ARQ="${TOPCOM_FASE:-}"
FASE_SEED="${TOPCOM_FASE_SEED:-}"
DIR_RESULTADO_FASE="${DIR_ROOT}"
ARGS_FASE=()
if [[ -n "${FASE_ARQ}" ]]; then
    if [[ ! -f "${FASE_ARQ}" ]]; then
        echo "[ERRO] TOPCOM_FASE: arquivo da fase nao encontrado: ${FASE_ARQ}"
        exit 1
    fi
    # A engine roda na raiz do orquestrador: o caminho precisa ser absoluto
    FASE_ARQ="$(cd "$(dirname "${FASE_ARQ}")" && pwd)/$(basename "${FASE_ARQ}")"
    if [[ -d "${DIR_INPUT}/submissoes" ]]; then
        DIR_RESULTADO_FASE="$(cd "${DIR_INPUT}" && pwd)"
        DIR_INPUT="${DIR_INPUT}/submissoes"
    fi
    MAX_BOTS=1
    ARGS_FASE=(--fase "${FASE_ARQ}")
    [[ -n "${FASE_SEED}" ]] && ARGS_FASE+=(--fase-seed "${FASE_SEED}")
fi

# --- Paleta de Cores ANSI e Tags Padronizadas (Sem Emojis) ---
COR_VERDE='\033[1;32m'
COR_AMARELO='\033[1;33m'
COR_AZUL='\033[1;34m'
COR_VERMELHO='\033[1;31m'
COR_CIANO='\033[1;36m'
COR_RESET='\033[0m'

TAG_OK="${COR_VERDE}[OK]${COR_RESET}"
TAG_ERRO="${COR_VERMELHO}[ERRO]${COR_RESET}"
TAG_AVISO="${COR_AMARELO}[AVISO]${COR_RESET}"
TAG_INFO="${COR_CIANO}[INFO]${COR_RESET}"
TAG_BUILD="${COR_AZUL}[BUILD]${COR_RESET}"
TAG_EXEC="${COR_AMARELO}[EXEC]${COR_RESET}"

# ==============================================================================
# 0. COMPILAÇÕES EM PARALELO (POOL DE JOBS)
# ==============================================================================
# Jobs simultâneos: TOPCOM_COMPILE_JOBS ou min(nproc, memória disponível / MEM_POR_JOB_MB), mínimo 1.
COMPILE_JOBS=1
JOBS_ORIGEM=""

calcular_jobs() {
    local n_cpu mem_mb
    n_cpu="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)"
    mem_mb="$(awk '/^MemAvailable:/ {print int($2 / 1024)}' /proc/meminfo 2>/dev/null || true)"
    [[ "${mem_mb}" =~ ^[0-9]+$ ]] || mem_mb="${MEM_POR_JOB_MB}"

    # Em contêiner com limite de memória (cgroup v2 ou v1), vale o que sobra dentro do limite
    local lim="" uso=""
    if [[ -r /sys/fs/cgroup/memory.max ]]; then
        lim="$(cat /sys/fs/cgroup/memory.max 2>/dev/null || true)"
        uso="$(cat /sys/fs/cgroup/memory.current 2>/dev/null || echo 0)"
    elif [[ -r /sys/fs/cgroup/memory/memory.limit_in_bytes ]]; then
        lim="$(cat /sys/fs/cgroup/memory/memory.limit_in_bytes 2>/dev/null || true)"
        uso="$(cat /sys/fs/cgroup/memory/memory.usage_in_bytes 2>/dev/null || echo 0)"
    fi
    if [[ "${lim}" =~ ^[0-9]+$ && "${uso}" =~ ^[0-9]+$ && ${#lim} -lt 19 ]]; then
        local livre_cg=$(( (lim - uso) / 1048576 ))
        if [[ ${livre_cg} -lt ${mem_mb} ]]; then
            mem_mb=${livre_cg}
        fi
    fi

    local auto=$(( mem_mb / MEM_POR_JOB_MB ))
    [[ ${auto} -gt ${n_cpu} ]] && auto=${n_cpu}
    [[ ${auto} -lt 1 ]] && auto=1

    if [[ -n "${TOPCOM_COMPILE_JOBS:-}" ]]; then
        if [[ "${TOPCOM_COMPILE_JOBS}" =~ ^[0-9]+$ && "${TOPCOM_COMPILE_JOBS}" -ge 1 ]]; then
            COMPILE_JOBS="${TOPCOM_COMPILE_JOBS}"
            JOBS_ORIGEM="TOPCOM_COMPILE_JOBS; automatico seria ${auto}"
        else
            COMPILE_JOBS=${auto}
            JOBS_ORIGEM="TOPCOM_COMPILE_JOBS invalido ('${TOPCOM_COMPILE_JOBS}'); automatico"
        fi
    else
        COMPILE_JOBS=${auto}
        JOBS_ORIGEM="automatico: nproc=${n_cpu}, memoria disponivel=${mem_mb} MB, ${MEM_POR_JOB_MB} MB por job"
    fi

    # 'wait -n' (pool) exige bash 4.3+; em versões antigas compila em sequência
    if (( BASH_VERSINFO[0] < 4 || (BASH_VERSINFO[0] == 4 && BASH_VERSINFO[1] < 3) )); then
        COMPILE_JOBS=1
        JOBS_ORIGEM="${JOBS_ORIGEM}; bash ${BASH_VERSION} sem 'wait -n': sequencial"
    fi
}

# Executa "$@" gravando a saída em <log> e "<código de saída> <ms>" em <log>.rc
_job() {
    local log="$1"; shift
    local t0 t1 rc
    t0="$(date +%s%N)"
    set +e
    "$@" > "${log}" 2>&1
    rc=$?
    set -e
    t1="$(date +%s%N)"
    echo "${rc} $(( (t1 - t0) / 1000000 ))" > "${log}.rc"
}

POOL_ATIVOS=0
# pool_rodar <log> comando...: roda em segundo plano, com no máximo COMPILE_JOBS simultâneos
pool_rodar() {
    if [[ ${COMPILE_JOBS} -le 1 ]]; then
        _job "$@"
        return 0
    fi
    _job "$@" &
    POOL_ATIVOS=$((POOL_ATIVOS + 1))
    if [[ ${POOL_ATIVOS} -ge ${COMPILE_JOBS} ]]; then
        wait -n || true
        POOL_ATIVOS=$((POOL_ATIVOS - 1))
    fi
}

pool_esperar() {
    wait || true
    POOL_ATIVOS=0
}

# Lê "<rc> <ms>" de <log>.rc (rc=99 se o job não registrou nada)
JOB_RC=0
JOB_MS=0
resultado_job() {
    JOB_RC=99
    JOB_MS=0
    if [[ -f "$1.rc" ]]; then
        read -r JOB_RC JOB_MS < "$1.rc" || true
    fi
}

# ==============================================================================
# 1. VERIFICAÇÃO DE DEPENDÊNCIAS
# ==============================================================================
verificar_dependencias() {
    echo -e "${COR_AZUL}[1/6] Verificando dependencias essenciais do sistema...${COR_RESET}"
    local deps=("g++" "python3" "timeout")

    for cmd in "${deps[@]}"; do
        if command -v "$cmd" &> /dev/null; then
            local path_cmd
            path_cmd=$(command -v "$cmd")
            echo -e "  ${TAG_OK} Ferramenta '${cmd}' disponivel: ${path_cmd}"
        else
            echo -e "  ${TAG_ERRO} Dependencia obrigatoria nao encontrada: ${cmd}"
            exit 1
        fi
    done

    if command -v gcc &> /dev/null; then
        echo -e "  ${TAG_OK} Compilador C (gcc) disponivel: $(command -v gcc)"
    fi

    if command -v javac &> /dev/null && command -v java &> /dev/null; then
        echo -e "  ${TAG_OK} Ambiente JDK Java disponivel (javac e java detectados)."
    else
        echo -e "  ${TAG_AVISO} javac/java nao detectados. Bots em Java serao ignorados."
    fi

    echo -e "${TAG_OK} Todas as ferramentas essenciais do sistema estao operacionais."
}

# ==============================================================================
# 2. VERIFICAÇÃO / DOWNLOAD DE BIBLIOTECAS (C++ e Java)
# ==============================================================================
baixar_bibliotecas() {
    echo -e "${COR_AZUL}[2/6] Verificando bibliotecas locais (C++ e Java)...${COR_RESET}"
    mkdir -p "${NLOHMANN_DIR}" "${DIR_LIBS}"

    # Nlohmann JSON (C++)
    if [[ ! -f "${NLOHMANN_HPP}" ]]; then
        if command -v curl &> /dev/null; then
            echo -e "  ${TAG_INFO} Baixando nlohmann/json.hpp (C++)..."
            curl -sL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" -o "${NLOHMANN_HPP}" || true
        fi
    fi

    # Gson (Java)
    if [[ ! -f "${GSON_JAR}" ]]; then
        if command -v curl &> /dev/null; then
            echo -e "  ${TAG_INFO} Baixando Gson (Java)..."
            curl -sL "https://repo1.maven.org/maven2/com/google/code/gson/gson/2.10.1/gson-2.10.1.jar" -o "${GSON_JAR}" || true
        fi
    fi

    if [[ -f "${NLOHMANN_HPP}" ]]; then
        echo -e "  ${TAG_OK} Biblioteca C++ (nlohmann/json.hpp) disponivel."
    else
        echo -e "  ${TAG_AVISO} Biblioteca C++ (nlohmann/json.hpp) ausente."
    fi

    if [[ -f "${GSON_JAR}" ]]; then
        echo -e "  ${TAG_OK} Biblioteca Java (Gson) disponivel."
    else
        echo -e "  ${TAG_AVISO} Biblioteca Java (Gson) ausente."
    fi
}

# ==============================================================================
# CANDIDATOS: ARQUIVOS DE CÓDIGO DO DIRETÓRIO DE ENTRADA
# ==============================================================================
declare -a CAND_EQUIPES=()
declare -a CAND_ARQUIVOS=()
declare -a CAND_NOMES=()
declare -a CAND_TAMANHOS=()
declare -a CAND_LANGS=()
declare -a CAND_EXTS=()
declare -a CAND_VALIDOS=()     # índices dos candidatos que vão para a compilação

# Bots que não entram na partida (nome da equipe e motivo, na ordem dos eventos)
declare -a EXCL_NOMES=()
declare -a EXCL_MOTIVOS=()

excluir() {
    EXCL_NOMES+=("$1")
    EXCL_MOTIVOS+=("$2")
}

coletar_candidatos() {
    if [[ ! -d "${DIR_INPUT}" ]]; then
        echo -e "${TAG_ERRO} Diretorio de submissoes nao encontrado: ${DIR_INPUT}"
        return 0
    fi

    local -A vistos=()
    local -a arquivos=()
    readarray -t arquivos < <(find "${DIR_INPUT}" -type f \( -name "*.py" -o -name "*.cpp" -o -name "*.java" -o -name "*.c" \) | sort)
    local arq
    for arq in ${arquivos[@]+"${arquivos[@]}"}; do
        [[ -f "$arq" ]] || continue
        local bname
        bname=$(basename "$arq")
        [[ "$bname" == "submission_meta.json" ]] && continue
        local ext="${bname##*.}"
        [[ "$ext" == "py" || "$ext" == "cpp" || "$ext" == "java" || "$ext" == "c" ]] || continue

        local nome_sem_ext="${bname%.*}"
        local tam_bytes
        tam_bytes=$(wc -c < "$arq" 2>/dev/null || echo 0)
        local tam_fmt="${tam_bytes} B"
        if [[ $tam_bytes -gt 1024 ]]; then
            tam_fmt="$((tam_bytes / 1024)) KB"
        fi

        local lang_nome="Desconhecida"
        case "$ext" in
            py) lang_nome="Python 3" ;;
            cpp) lang_nome="C++20" ;;
            java) lang_nome="Java" ;;
            c) lang_nome="C" ;;
        esac

        local idx=${#CAND_EQUIPES[@]}
        CAND_EQUIPES+=("$nome_sem_ext")
        CAND_ARQUIVOS+=("$arq")
        CAND_NOMES+=("$bname")
        CAND_TAMANHOS+=("$tam_fmt")
        CAND_LANGS+=("$lang_nome")
        CAND_EXTS+=("$ext")

        # O nome do arquivo sem extensão identifica a equipe: só um arquivo por nome
        if [[ -n "${vistos[$nome_sem_ext]+x}" ]]; then
            excluir "$nome_sem_ext" "Nome duplicado: ${bname} ignorado (a equipe ja entrou com ${vistos[$nome_sem_ext]})"
            continue
        fi
        vistos[$nome_sem_ext]="$bname"

        if [[ "$tam_bytes" -eq 0 ]]; then
            excluir "$nome_sem_ext" "Arquivo vazio (${bname})"
            continue
        fi
        CAND_VALIDOS+=("$idx")
    done
}

# Linguagens com wrapper pré-compilado (C e C++) presentes entre os candidatos
langs_com_wrapper() {
    local tem_c=0 tem_cpp=0 i
    for i in ${CAND_VALIDOS[@]+"${CAND_VALIDOS[@]}"}; do
        [[ "${CAND_EXTS[$i]}" == "c" ]] && tem_c=1
        [[ "${CAND_EXTS[$i]}" == "cpp" ]] && tem_cpp=1
    done
    [[ $tem_c -eq 1 ]] && echo -n "c "
    [[ $tem_cpp -eq 1 ]] && echo -n "cpp"
    echo ""
}

# ==============================================================================
# 3. COMPILAÇÃO DA ENGINE CENTRAL EM C++ E DOS WRAPPERS PRÉ-COMPILADOS
# ==============================================================================
compilar_engine_bin() {
    g++ "${ENGINE_FLAGS[@]}" -I"${DIR_ROOT}" -I"${DIR_ROOT}/engine" "${ENGINE_SRC}" -o "${ENGINE_BIN}.tmp" \
        && mv -f "${ENGINE_BIN}.tmp" "${ENGINE_BIN}"
}

# Modo fase: avaliador da fase (engine/avaliar_fase.cpp), com as mesmas flags da engine
compilar_avaliar_bin() {
    g++ "${ENGINE_FLAGS[@]}" -I"${DIR_ROOT}" -I"${DIR_ROOT}/engine" "${AVALIAR_SRC}" -o "${AVALIAR_BIN}.tmp" \
        && mv -f "${AVALIAR_BIN}.tmp" "${AVALIAR_BIN}"
}

compilar_engine() {
    echo -e "${COR_AZUL}[3/6] Compilando Engine Central TopCom (C++) e wrappers pre-compilados...${COR_RESET}"
    mkdir -p "${DIR_BUILD}" "${DIR_BOTS_BUILD}" "${DIR_REPLAYS}"
    rm -rf -- "${DIR_LOGS:?}"
    mkdir -p "${DIR_LOGS}"

    if [[ ! -f "${ENGINE_SRC}" ]]; then
        echo -e "  ${TAG_ERRO} Arquivo-fonte da engine (${ENGINE_SRC}) nao encontrado!"
        exit 1
    fi

    echo -e "  ${TAG_INFO} Fonte: ${ENGINE_SRC}"
    echo -e "  ${TAG_INFO} Binario de destino: ${ENGINE_BIN}"
    echo -e "  ${TAG_INFO} Jobs de compilacao: ${COMPILE_JOBS} (${JOBS_ORIGEM})"

    # A engine só é recompilada quando o fonte, a nlohmann/json, o compilador ou as flags mudam
    local ass_engine
    ass_engine="$( { g++ --version | head -n 1; echo "${ENGINE_FLAGS[*]}"; cat "${ENGINE_SRC}" "${NLOHMANN_HPP}" 2>/dev/null; } | sha256sum | cut -d' ' -f1 )"
    local compilar=1
    if [[ -x "${ENGINE_BIN}" && "$(cat "${ENGINE_ASSINATURA}" 2>/dev/null)" == "${ass_engine}" ]]; then
        compilar=0
    fi

    local log_engine="${DIR_LOGS}/_engine.log"
    local log_wrappers="${DIR_LOGS}/_wrappers"
    if [[ ${compilar} -eq 1 ]]; then
        rm -f "${ENGINE_ASSINATURA}"
        pool_rodar "${log_engine}" compilar_engine_bin
    fi

    # Modo fase: o avaliar_fase compila junto com a engine (e é reaproveitado do mesmo jeito)
    local ass_avaliar="" compilar_avaliar=0 log_avaliar="${DIR_LOGS}/_avaliar_fase.log"
    if [[ -n "${FASE_ARQ}" ]]; then
        if [[ ! -f "${AVALIAR_SRC}" ]]; then
            echo -e "  ${TAG_AVISO} Modo fase: engine/avaliar_fase.cpp ausente; a fase roda sem avaliacao (sem resultado_fase.json)."
        else
            ass_avaliar="$( { g++ --version | head -n 1; echo "${ENGINE_FLAGS[*]}"; cat "${AVALIAR_SRC}" "${NLOHMANN_HPP}" 2>/dev/null; } | sha256sum | cut -d' ' -f1 )"
            if [[ -x "${AVALIAR_BIN}" && "$(cat "${AVALIAR_ASSINATURA}" 2>/dev/null)" == "${ass_avaliar}" ]]; then
                AVALIAR_PRONTO=1
            else
                compilar_avaliar=1
                rm -f "${AVALIAR_ASSINATURA}"
                pool_rodar "${log_avaliar}" compilar_avaliar_bin
            fi
        fi
    fi

    # Wrappers C/C++ (uma vez por partida; reaproveitados se nada mudou), junto com a engine
    local langs lang
    langs="$(langs_com_wrapper)"
    for lang in ${langs}; do
        pool_rodar "${log_wrappers}_${lang}.log" bash "${BUILD_SCRIPT}" --preparar "${lang}"
    done
    pool_esperar

    if [[ ${compilar} -eq 1 ]]; then
        resultado_job "${log_engine}"
        if [[ ${JOB_RC} -ne 0 ]]; then
            cat "${log_engine}" 2>/dev/null || true
            echo -e "${TAG_ERRO} Falha ao compilar a Engine Central!"
            exit 1
        fi
        echo "${ass_engine}" > "${ENGINE_ASSINATURA}"
        echo -e "${TAG_OK} Engine compilada com sucesso em: ${ENGINE_BIN} ($(awk "BEGIN {printf \"%.1f\", ${JOB_MS}/1000}")s)"
    else
        echo -e "${TAG_OK} Engine reaproveitada (fontes, compilador e flags inalterados): ${ENGINE_BIN}"
    fi

    if [[ ${compilar_avaliar} -eq 1 ]]; then
        resultado_job "${log_avaliar}"
        if [[ ${JOB_RC} -eq 0 ]]; then
            echo "${ass_avaliar}" > "${AVALIAR_ASSINATURA}"
            AVALIAR_PRONTO=1
            echo -e "${TAG_OK} Avaliador da fase compilado em: ${AVALIAR_BIN} ($(awk "BEGIN {printf \"%.1f\", ${JOB_MS}/1000}")s)"
        else
            echo -e "  ${TAG_AVISO} Falha ao compilar o avaliador da fase; a fase roda sem avaliacao:"
            grep -i "error" "${log_avaliar}" 2>/dev/null | head -n 5 | sed 's/^/      /' || true
        fi
    elif [[ ${AVALIAR_PRONTO} -eq 1 ]]; then
        echo -e "${TAG_OK} Avaliador da fase reaproveitado: ${AVALIAR_BIN}"
    fi

    for lang in ${langs}; do
        resultado_job "${log_wrappers}_${lang}.log"
        if [[ ${JOB_RC} -eq 0 ]]; then
            echo -e "  ${TAG_OK} Wrapper pre-compilado (${lang}) pronto em build/wrappers/${lang} ($(awk "BEGIN {printf \"%.1f\", ${JOB_MS}/1000}")s)"
        else
            echo -e "  ${TAG_AVISO} Falha ao pre-compilar o wrapper (${lang}); os bots dessa linguagem tentarao de novo:"
            grep -i "error" "${log_wrappers}_${lang}.log" 2>/dev/null | head -n 5 | sed 's/^/      /' || true
        fi
    done
}

# ==============================================================================
# 3b. MODO ISOLADO (TOPCOM_BOT_UID_BASE): UID, DIRETÓRIO E LIMITES PRÓPRIOS POR BOT
# ==============================================================================
N_UIDS=0                      # uids reservados: BASE .. BASE+N_UIDS-1 (um por candidato compilável)
VIGIA_PID=""
DIRS_COMUNS=()                # diretórios graváveis por todos (fora do build/bots) que o vigia e a limpeza olham
SETPRIV_OPCOES=(--clear-groups --no-new-privs --inh-caps=-all --bounding-set=-all)

# Há processo com uid real $1?
uid_tem_processos() {
    if command -v pgrep &> /dev/null; then
        pgrep -U "$1" > /dev/null 2>&1
    else
        awk -v u="$1" '$1 == "Uid:" && $2 == u { achou = 1; exit } END { exit !achou }' /proc/[0-9]*/status 2> /dev/null
    fi
}

# Encerra todos os processos do uid $1. "kill -KILL -1" rodando COMO o próprio uid atinge só os
# processos dele (inclusive os que saíram do grupo com setsid); repete por causa de fork bomb.
matar_uid() {
    local u="$1" t
    for t in 1 2 3 4 5 6 7 8 9 10; do
        uid_tem_processos "$u" || return 0
        setpriv --reuid="$u" --regid="$u" --clear-groups bash -c 'kill -KILL -1' > /dev/null 2>&1 || true
        sleep 0.05
    done
    if uid_tem_processos "$u"; then
        echo -e "  ${TAG_AVISO} Modo isolado: o uid ${u} ainda tem processos depois de 10 tentativas de encerramento."
    fi
}

# Apaga os arquivos do uid $1 nos diretórios comuns (/tmp, /var/tmp, /dev/shm...)
limpar_arquivos_uid() {
    if [[ ${#DIRS_COMUNS[@]} -gt 0 ]]; then
        find "${DIRS_COMUNS[@]}" -xdev -mindepth 1 -uid "$1" -delete 2> /dev/null || true
    fi
}

# Vigia (segundo plano): soma disco e arquivos de cada uid de bot (build/bots e diretórios comuns);
# quem passar do limite tem os processos encerrados e os arquivos apagados.
isolamento_vigia() {
    set +e
    trap - EXIT TERM INT
    local pai="$1"
    local lim_kb=$(( BOT_MAX_DISCO_MB * 1024 )) umin="${BOT_UID_BASE}" umax=$(( BOT_UID_BASE + N_UIDS - 1 ))
    local u kb n
    while kill -0 "${pai}" 2> /dev/null; do
        while read -r u kb n; do
            [[ "$u" =~ ^[0-9]+$ ]] || continue
            matar_uid "$u"
            find "${DIR_BOTS_BUILD}" -xdev -mindepth 2 -uid "$u" ! -type d -delete 2> /dev/null
            find "${DIR_BOTS_BUILD}" -xdev -mindepth 3 -uid "$u" -type d -delete 2> /dev/null
            limpar_arquivos_uid "$u"
            echo -e "  ${TAG_AVISO} [ISOLAMENTO] uid ${u}: $(( kb / 1024 )) MB em ${n} arquivo(s) passou do limite (${BOT_MAX_DISCO_MB} MB / ${BOT_MAX_ARQUIVOS} arquivos); processos do bot encerrados e arquivos apagados."
        done < <(find "${DIR_BOTS_BUILD}" ${DIRS_COMUNS[@]+"${DIRS_COMUNS[@]}"} -xdev -printf '%U %k\n' 2> /dev/null \
                 | awk -v a="${umin}" -v b="${umax}" -v lk="${lim_kb}" -v la="${BOT_MAX_ARQUIVOS}" \
                       '$1 >= a && $1 <= b { kb[$1] += $2; n[$1]++ }
                        END { for (u in kb) if (kb[u] > lk || n[u] > la) print u, kb[u], n[u] }')
        sleep 0.25
    done
}

# Permissões para o uid do bot: travessia até a raiz, leitura só do que a compilação usa
# (wrappers/, libs/, engine/nlohmann/, build/wrappers/); o resto (submissões, fontes da engine,
# logs, fases, NPCs) fica só para o root.
isolamento_permissoes() {
    local a="${DIR_ROOT}" e
    while [[ "$a" != "/" && -n "$a" ]]; do
        a="$(dirname "$a")"
        [[ "$a" == "/" ]] && break
        if [[ ! -L "$a" ]] && (( (8#$(stat -c '%a' "$a") & 1) == 0 )); then
            chmod o+x -- "$a"
        fi
    done
    chmod 0711 -- "${DIR_ROOT}"
    for e in "${DIR_ROOT}"/* "${DIR_ROOT}"/.[!.]* "${DIR_ROOT}"/..?*; do
        [[ -e "$e" && ! -L "$e" ]] || continue
        case "${e##*/}" in
            wrappers|libs) chmod -R a+rX,go-w -- "$e" ;;
            engine|build) chmod 0711 -- "$e" ;;
            *) chmod go-rwx -- "$e" ;;
        esac
    done
    for e in "${DIR_ROOT}/engine"/*; do
        [[ -e "$e" && ! -L "$e" ]] || continue
        if [[ "${e##*/}" == "nlohmann" ]]; then chmod -R a+rX,go-w -- "$e"; else chmod go-rwx -- "$e"; fi
    done
    for e in "${DIR_BUILD}"/*; do
        [[ -e "$e" && ! -L "$e" ]] || continue
        case "${e##*/}" in
            wrappers) chmod -R a+rX,go-w -- "$e" ;;
            bots) chmod 0711 -- "$e" ;;
            *) chmod go-rwx -- "$e" ;;
        esac
    done
    if [[ -d "${DIR_INPUT}" && ! -L "${DIR_INPUT}" ]]; then
        chmod go-rwx -- "${DIR_INPUT}"
    fi
}

isolamento_preparar() {
    [[ ${ISOLADO} -eq 1 ]] || return 0
    N_UIDS=${#CAND_VALIDOS[@]}
    [[ ${N_UIDS} -ge 1 ]] || N_UIDS=1
    local d u
    for d in /tmp /var/tmp /dev/shm /run/lock /var/lib/php/sessions; do
        if [[ -d "$d" && ! -L "$d" ]]; then DIRS_COMUNS+=("$d"); fi
    done
    echo -e "  ${TAG_INFO} Modo isolado: uids ${BOT_UID_BASE}..$(( BOT_UID_BASE + N_UIDS - 1 )), ulimit -u ${BOT_MAX_PROC}, ${BOT_MAX_DISCO_MB} MB e ${BOT_MAX_ARQUIVOS} arquivos por bot"

    # O setpriv precisa trocar de uid (CAP_SETUID/CAP_SETGID). --bounding-set pede CAP_SETPCAP: sem ela,
    # segue sem esvaziar o bounding set (com no_new_privs e uid sem privilégio ele não dá nada ao bot).
    if ! setpriv --reuid="${BOT_UID_BASE}" --regid="${BOT_UID_BASE}" "${SETPRIV_OPCOES[@]}" true 2> "${DIR_LOGS}/_setpriv.log"; then
        local sem_bounding=(--clear-groups --no-new-privs --inh-caps=-all)
        if grep -q "bounding set" "${DIR_LOGS}/_setpriv.log" 2> /dev/null \
           && setpriv --reuid="${BOT_UID_BASE}" --regid="${BOT_UID_BASE}" "${sem_bounding[@]}" true 2>> "${DIR_LOGS}/_setpriv.log"; then
            SETPRIV_OPCOES=("${sem_bounding[@]}")
            echo -e "  ${TAG_AVISO} Modo isolado: sem CAP_SETPCAP o bounding set dos bots nao e esvaziado (uid proprio e no_new_privs continuam)."
        else
            echo -e "${TAG_ERRO} Modo isolado: o setpriv falhou ($(head -n 1 "${DIR_LOGS}/_setpriv.log" 2>/dev/null)). Partida abortada."
            exit 1
        fi
    fi

    trap isolamento_encerrar EXIT
    trap 'exit 143' TERM
    trap 'exit 130' INT
    # Sobras de uma execução anterior na mesma réplica (os uids se repetem entre pedidos)
    for ((u=BOT_UID_BASE; u<BOT_UID_BASE+N_UIDS; u++)); do
        matar_uid "$u"
        limpar_arquivos_uid "$u"
    done
    isolamento_permissoes
    rm -rf -- "${DIR_BOTS_BUILD:?}"
    mkdir -p "${DIR_BOTS_BUILD}"
    chmod 0711 -- "${DIR_BOTS_BUILD}"
    # Vigia fora dos jobs do script (subshell dupla): o "wait" do pool de compilação não o espera.
    # Fica no grupo de processos do script (o executor derruba o grupo) e sai sozinho se o script sumir.
    local arq_pid="${DIR_BUILD}/.vigia.pid"
    rm -f -- "${arq_pid}"
    ( isolamento_vigia "$$" & echo "$!" > "${arq_pid}" )
    VIGIA_PID="$(cat "${arq_pid}" 2> /dev/null || true)"
    rm -f -- "${arq_pid}"
}

# Fim do script (EXIT): para o vigia, encerra processos e apaga arquivos de todos os uids dos bots
isolamento_encerrar() {
    local rc=$?
    set +e
    trap - EXIT
    if [[ "${VIGIA_PID}" =~ ^[0-9]+$ ]]; then
        kill "${VIGIA_PID}" 2> /dev/null
        local t
        for t in 1 2 3 4 5 6 7 8 9 10; do
            kill -0 "${VIGIA_PID}" 2> /dev/null || break
            sleep 0.05
        done
        VIGIA_PID=""
    fi
    local u
    for ((u=BOT_UID_BASE; u<BOT_UID_BASE+N_UIDS; u++)); do
        matar_uid "$u"
        limpar_arquivos_uid "$u"
    done
    rm -rf -- "${DIR_BOTS_BUILD:?}"
    exit "${rc}"
}

# Diretório do bot (dono uid $2, 0700) com a cópia do fonte, home/, tmp/ e stderr.log
preparar_dir_bot() {
    local d="$1" u="$2" fonte="$3" nome="$4"
    rm -rf -- "${d:?}"
    mkdir -m 0700 -- "$d" "$d/home" "$d/tmp"
    cp -- "${fonte}" "$d/${nome}"
    : > "$d/stderr.log"
    chmod 0600 -- "$d/${nome}" "$d/stderr.log"
    chown -R -h "${u}:${u}" -- "$d"
}

# Compila como o uid do bot (chamado pelo pool): build_bot.sh <fonte> <saída>
compilar_como_bot() {
    local u="$1" d="$2" fonte="$3" saida="$4"
    (
        ulimit -u "${COMPILACAO_MAX_PROC}" || exit 1
        umask 077
        cd "$d" || exit 1
        export HOME="$d/home" TMPDIR="$d/tmp"
        exec setpriv --reuid="$u" --regid="$u" "${SETPRIV_OPCOES[@]}" bash "${BUILD_SCRIPT}" "${fonte}" "${saida}"
    )
}

# Depois da partida: encerra os bots e copia o stderr de cada um (primeiros 64 KB) para
# build/logs/<bot>.stderr; as linhas dos bots de isolamento vão para o log da partida.
isolamento_relatorio() {
    [[ ${ISOLADO} -eq 1 ]] || return 0
    local i d saida linha
    for ((i=0; i<${#NOMES_BOTS[@]}; i++)); do
        matar_uid "$(( BOT_UID_BASE + i ))"
    done
    for ((i=0; i<${#NOMES_BOTS[@]}; i++)); do
        d="${DIRS_BOTS[$i]}"
        saida="${DIR_LOGS}/${NOMES_BOTS[$i]}.stderr"
        rm -f -- "${saida}"
        [[ -f "$d/stderr.log" && ! -L "$d/stderr.log" ]] || continue
        dd if="$d/stderr.log" of="${saida}" iflag=nofollow,count_bytes count="${BOT_STDERR_MAX}" status=none 2> /dev/null || true
        while IFS= read -r linha; do
            echo "  [ISOLAMENTO] ${NOMES_BOTS[$i]}: ${linha}"
        done < <(grep -aE '^ISOLAMENTO-(OK|FALHOU) [a-z_]{1,40}$' "${saida}" 2> /dev/null | head -n 20)
        # Avisos da camada de tradução do wrapper (campo inválido na ação, resposta vazia):
        # a equipe vê no resultado do treino (TreinoService).
        while IFS= read -r linha; do
            echo "  [AVISO] ${NOMES_BOTS[$i]}: ${linha#\[AVISO\] }"
        done < <(grep -aE '^\[AVISO\] .' "${saida}" 2> /dev/null | head -n 10 | cut -c1-200 | LC_ALL=C tr -cd '[:print:]\n')
    done
}

# ==============================================================================
# 4. INVENTÁRIO PRÉ-COMPILAÇÃO, COMPILAÇÃO E IAS CONFIRMADAS NA PARTIDA
# ==============================================================================
declare -a NOMES_BOTS=()
declare -a COMANDOS_BOTS=()
declare -a DIRS_BOTS=()       # modo isolado: diretório de cada bot (paralelo a NOMES_BOTS)
declare -a CAND_DIR=()        # modo isolado: diretório de compilação de cada candidato
declare -a ARQS_ENTRARAM=()
declare -a LANGS_ENTRARAM=()

formatar_tamanho() {
    local bytes="$1"
    if [[ "$bytes" -ge 1048576 ]]; then
        echo "$(awk "BEGIN {printf \"%.1f MB\", $bytes/1048576}")"
    elif [[ "$bytes" -ge 1024 ]]; then
        echo "$(awk "BEGIN {printf \"%.1f KB\", $bytes/1024}")"
    else
        echo "${bytes} B"
    fi
}

# Primeira linha de erro do log de compilação (sem os caminhos do servidor)
resumo_erro() {
    local log="$1"
    local resumo
    resumo="$(grep -E "error:|undefined reference|Error:" "$log" 2>/dev/null | grep -v "collect2" | head -n 1 || true)"
    [[ -n "$resumo" ]] || resumo="$(head -n 2 "$log" 2>/dev/null | tr '\n' ' ')"
    resumo="${resumo//"${DIR_INPUT}/"/}"
    resumo="${resumo//"${DIR_BOTS_BUILD}/"/}"
    resumo="${resumo//"${DIR_ROOT}/"/}"
    # java_build_* fica no TMPDIR (/tmp ou, no modo isolado, o tmp/ do bot); t<N>/ é o diretório do bot isolado
    if [[ ${ISOLADO} -eq 1 ]]; then
        echo "$resumo" | sed -E -e 's#[^[:space:](]*/java_build_[A-Za-z0-9]+/##g' -e 's#^t[0-9]+/##; s#([[:space:](])t[0-9]+/#\1#g'
    else
        echo "$resumo" | sed -E 's#/tmp/java_build_[A-Za-z0-9]+/##g'
    fi
}

preparar_bots() {
    echo -e "${COR_AZUL}[4/6] Analisando codigos das equipes e preparando IAs para a Arena...${COR_RESET}"

    # SUBSEÇÃO 4.1: INVENTÁRIO PRÉ-COMPILAÇÃO
    # --------------------------------------------------------------------------
    echo ""
    echo -e "${COR_CIANO}================================================================================${COR_RESET}"
    echo -e "${COR_CIANO}    INVENTARIO PRE-COMPILACAO: ARQUIVOS DE CODIGO DETECTADOS NO AMBIENTE       ${COR_RESET}"
    echo -e "${COR_CIANO}================================================================================${COR_RESET}"
    printf "${COR_AMARELO}%-18s | %-26s | %-10s | %-20s${COR_RESET}\n" "EQUIPE" "ARQUIVO FONTE" "TAMANHO" "LINGUAGEM DETECTADA"
    echo -e "-------------------|----------------------------|------------|----------------------"

    if [[ ${#CAND_EQUIPES[@]} -eq 0 ]]; then
        echo -e "  (Nenhum arquivo de codigo fonte detectado antes da compilacao)"
    else
        for ((i=0; i<${#CAND_EQUIPES[@]}; i++)); do
            printf "%-18s | %-26s | %-10s | %-20s\n" \
                "${CAND_EQUIPES[$i]}" "${CAND_NOMES[$i]}" "${CAND_TAMANHOS[$i]}" "${CAND_LANGS[$i]}"
        done
    fi
    echo -e "-------------------|----------------------------|------------|----------------------"
    echo -e "Total de arquivos candidatos antes da compilacao: ${COR_VERDE}${#CAND_EQUIPES[@]}${COR_RESET}"
    echo -e "${COR_CIANO}================================================================================${COR_RESET}"
    echo ""

    # --------------------------------------------------------------------------
    # SUBSEÇÃO 4.2: COMPILAÇÃO E VALIDAÇÃO DOS CANDIDATOS
    # --------------------------------------------------------------------------
    # O limite é aplicado ANTES de compilar: cada lote compila só as vagas restantes, na
    # ordem alfabética. Se algum falhar, o lote seguinte tenta os próximos candidatos.
    # Os lotes compilam em paralelo; o log de cada bot é impresso na ordem dos nomes.
    echo -e "${COR_AZUL}Iniciando validacao e compilacao dos candidatos (${COMPILE_JOBS} jobs de compilacao em paralelo)...${COR_RESET}"

    if [[ ! -f "${BUILD_SCRIPT}" ]]; then
        echo -e "    ${TAG_ERRO} Script wrappers/build_bot.sh nao encontrado."
        local i
        for i in ${CAND_VALIDOS[@]+"${CAND_VALIDOS[@]}"}; do
            excluir "${CAND_EQUIPES[$i]}" "Script wrappers/build_bot.sh ausente"
        done
        CAND_VALIDOS=()
    fi
    mkdir -p "${DIR_BOTS_BUILD}"
    if [[ ${ISOLADO} -eq 0 ]]; then
        # Sobras de uma execução isolada interrompida (build/bots/t<N>/) atrapalhariam o build_bot.sh
        find "${DIR_BOTS_BUILD}" -mindepth 1 -maxdepth 1 -type d -name 't[0-9]*' -exec rm -rf -- {} + 2> /dev/null || true
    fi

    local prox=0 lote_n=0 tentativa=0
    local total_validos=${#CAND_VALIDOS[@]}
    while [[ ${#NOMES_BOTS[@]} -lt ${MAX_BOTS} && ${prox} -lt ${total_validos} ]]; do
        local vagas=$(( MAX_BOTS - ${#NOMES_BOTS[@]} ))
        local -a lote=("${CAND_VALIDOS[@]:prox:vagas}")
        prox=$(( prox + ${#lote[@]} ))
        lote_n=$(( lote_n + 1 ))
        if [[ ${lote_n} -gt 1 ]]; then
            echo -e "  ${TAG_INFO} Lote ${lote_n}: ${#lote[@]} candidato(s) para as vagas restantes."
        fi

        local t_lote
        t_lote="$(date +%s%N)"
        local i
        for i in "${lote[@]}"; do
            if [[ ${ISOLADO} -eq 1 ]]; then
                # Modo isolado: cada tentativa de compilação tem uid e diretório próprios
                local u_c=$(( BOT_UID_BASE + tentativa ))
                CAND_DIR[$i]="${DIR_BOTS_BUILD}/t${tentativa}"
                tentativa=$(( tentativa + 1 ))
                preparar_dir_bot "${CAND_DIR[$i]}" "${u_c}" "${CAND_ARQUIVOS[$i]}" "${CAND_NOMES[$i]}"
                pool_rodar "${DIR_LOGS}/${CAND_EQUIPES[$i]}.log" \
                    compilar_como_bot "${u_c}" "${CAND_DIR[$i]}" "${CAND_DIR[$i]}/${CAND_NOMES[$i]}" "${CAND_DIR[$i]}/bot"
                continue
            fi
            # Chamado via bash: a permissão +x se perde quando o BOCA extrai o engine.zip
            pool_rodar "${DIR_LOGS}/${CAND_EQUIPES[$i]}.log" \
                bash "${BUILD_SCRIPT}" "${CAND_ARQUIVOS[$i]}" "${DIR_BOTS_BUILD}/${CAND_EQUIPES[$i]}"
        done
        pool_esperar
        echo -e "  ${TAG_INFO} ${#lote[@]} bot(s) processado(s) em $(awk "BEGIN {printf \"%.1f\", ($(date +%s%N) - ${t_lote}) / 1e9}")s"

        for i in "${lote[@]}"; do
            local team="${CAND_EQUIPES[$i]}"
            local bname="${CAND_NOMES[$i]}"
            local lang_name="${CAND_LANGS[$i]}"
            local bin_out="${DIR_BOTS_BUILD}/${team}"
            local java_extra=""
            if [[ ${ISOLADO} -eq 1 ]]; then
                bin_out="${CAND_DIR[$i]}/bot"
                # Sem hsperfdata no /tmp comum; temporários do Java no tmp/ do bot
                java_extra=" -XX:-UsePerfData -Djava.io.tmpdir=\"${CAND_DIR[$i]}/tmp\""
            fi
            local log_bot="${DIR_LOGS}/${team}.log"
            local cmd_exec=""
            case "${CAND_EXTS[$i]}" in
                py) cmd_exec="python3 \"${bin_out}\"" ;;
                cpp|c) cmd_exec="\"${bin_out}\"" ;;
                java) cmd_exec="java ${JAVA_OPTS_BOT}${java_extra} -jar \"${bin_out}\"" ;;
            esac

            echo -e "  ${TAG_BUILD} [${lang_name}] Compilando equipe '${team}' (${bname})..."
            resultado_job "${log_bot}"
            if [[ ${JOB_RC} -eq 0 && -f "${bin_out}" ]]; then
                NOMES_BOTS+=("$team")
                COMANDOS_BOTS+=("${cmd_exec}")
                ARQS_ENTRARAM+=("$bname")
                LANGS_ENTRARAM+=("${lang_name}")
                if [[ ${ISOLADO} -eq 1 ]]; then DIRS_BOTS+=("${CAND_DIR[$i]}"); fi
                echo -e "    ${TAG_OK} Preparacao '${lang_name}' concluida em $(awk "BEGIN {printf \"%.1f\", ${JOB_MS}/1000}")s -> ${bin_out}"
            else
                local err_resumo
                err_resumo="$(resumo_erro "${log_bot}")"
                excluir "$team" "Erro de compilacao modular (${err_resumo})"
                echo -e "    ${TAG_ERRO} Falha na compilacao para '${team}': ${err_resumo}"
                if [[ ${ISOLADO} -eq 1 ]]; then rm -rf -- "${CAND_DIR[$i]:?}"; fi
            fi
        done
    done

    # Modo isolado: nenhum processo da compilação sobrevive, e o bot i passa a ter o uid BASE+i
    if [[ ${ISOLADO} -eq 1 ]]; then
        local u
        for ((u=BOT_UID_BASE; u<BOT_UID_BASE+tentativa; u++)); do
            matar_uid "$u"
            limpar_arquivos_uid "$u"
        done
        for ((i=0; i<${#NOMES_BOTS[@]}; i++)); do
            u=$(( BOT_UID_BASE + i ))
            chown -R -h "${u}:${u}" -- "${DIRS_BOTS[$i]}"
        done
    fi

    # Excedentes do limite: não chegam a ser compilados
    for ((; prox<total_validos; prox++)); do
        excluir "${CAND_EQUIPES[${CAND_VALIDOS[$prox]}]}" "Excedeu limite maximo da partida (${MAX_BOTS} bots)"
    done


    # --------------------------------------------------------------------------
    # SUBSEÇÃO 4.3: INVENTÁRIO PÓS-COMPILAÇÃO (QUAIS ENTRARAM NA PARTIDA)
    # --------------------------------------------------------------------------
    echo ""
    echo -e "${COR_VERDE}================================================================================${COR_RESET}"
    echo -e "${COR_VERDE}    IAS CONFIRMADAS NA PARTIDA (ENTRARAM EFETIVAMENTE NA ARENA)                 ${COR_RESET}"
    echo -e "${COR_VERDE}================================================================================${COR_RESET}"
    printf "${COR_AMARELO}%-10s | %-18s | %-24s | %-12s | %-10s${COR_RESET}\n" "SLOT ARENA" "EQUIPE" "ARQUIVO ORIGINAL" "LINGUAGEM" "STATUS"
    echo -e "-----------|--------------------|--------------------------|--------------|-----------"

    if [[ ${#NOMES_BOTS[@]} -eq 0 ]]; then
        echo -e "  ${TAG_ERRO} Nenhuma IA foi aprovada para entrar na partida!"
    else
        for ((i=0; i<${#NOMES_BOTS[@]}; i++)); do
            local slot_id=$((i + 1))
            printf "%-10s | %-18s | %-24s | %-12s | ${COR_VERDE}%-10s${COR_RESET}\n" \
                "Bot ${slot_id}" "${NOMES_BOTS[$i]}" "${ARQS_ENTRARAM[$i]}" "${LANGS_ENTRARAM[$i]}" "Confirmado"
        done
    fi
    echo -e "--------------------------------------------------------------------------------"
    echo -e "Total de IAs que entraram na partida: ${COR_VERDE}${#NOMES_BOTS[@]}${COR_RESET} / ${MAX_BOTS} max"

    if [[ ${#EXCL_NOMES[@]} -gt 0 ]]; then
        echo ""
        echo -e "${COR_AMARELO}IAs nao participantes / desqualificadas nesta rodada:${COR_RESET}"
        for ((i=0; i<${#EXCL_NOMES[@]}; i++)); do
            echo -e "  • ${EXCL_NOMES[$i]}: ${EXCL_MOTIVOS[$i]}"
        done
    fi
    echo -e "${COR_VERDE}================================================================================${COR_RESET}"
    echo ""

    if [[ ${#NOMES_BOTS[@]} -eq 0 ]]; then
        echo -e "${TAG_ERRO} Abortando execucao da partida: nenhuma IA valida disponivel."
        exit 1
    fi
}

# ==============================================================================
# 5. EXECUÇÃO PROTEGIDA (ULIMIT + TIMEOUT) VIA PIPES
# ==============================================================================
# Lê uma constante inteira da engine (constexpr int NOME = valor;) ou devolve o padrão.
const_engine() {
    local v
    v="$(sed -nE "s/.*constexpr[[:space:]]+int[[:space:]]+$1[[:space:]]*=[[:space:]]*([0-9]+).*/\1/p" "${ENGINE_SRC}" 2>/dev/null | head -n 1)"
    if [[ "${v}" =~ ^[0-9]+$ ]] && [[ "${v}" -gt 0 ]]; then echo "${v}"; else echo "$2"; fi
}

# Orçamento de relógio da partida em segundos para N bots (ver Configurações).
orcamento_partida_seg() {
    local n="$1"
    if [[ "${TOPCOM_TIMEOUT_PARTIDA:-}" =~ ^[0-9]+$ ]] && [[ "${TOPCOM_TIMEOUT_PARTIDA}" -gt 0 ]]; then
        echo "${TOPCOM_TIMEOUT_PARTIDA}"
        return
    fi
    local ticks resp inicio
    ticks="$(const_engine MAX_TICKS 1500)"
    resp="$(const_engine RESPOSTA_MS 50)"
    inicio="$(const_engine PARTIDA_INICIO_MS 5000)"
    local total_ms=$(( inicio + ticks * n * resp ))
    echo $(( (total_ms + 999) / 1000 + FOLGA_PARTIDA_SEG ))
}

executar_batalha() {
    echo -e "${COR_AZUL}[5/6] Conectando IAs e iniciando simulacao na Arena TopCom...${COR_RESET}"

    local orcamento timeout_bot
    orcamento="$(orcamento_partida_seg "${#NOMES_BOTS[@]}")"
    timeout_bot=$(( orcamento + FOLGA_BOT_SEG ))

    local comandos_protegidos=() cmd i=0
    for cmd in "${COMANDOS_BOTS[@]}"; do
        local wrapper
        if [[ ${ISOLADO} -eq 1 ]]; then
            # Limites aplicados como root e herdados; o setpriv troca para o uid do bot no exec.
            # O stderr do bot vai para o stderr.log do próprio diretório (conta no limite de disco).
            local u=$(( BOT_UID_BASE + i )) d="${DIRS_BOTS[$i]}"
            wrapper="timeout -k 3s ${timeout_bot}s bash -c 'ulimit -v ${MEM_LIMIT_KB} 2>/dev/null || true; ulimit -u ${BOT_MAX_PROC} || exit 1; ulimit -f $(( BOT_MAX_DISCO_MB * 1024 )) || exit 1; umask 077; cd \"${d}\" || exit 1; export HOME=\"${d}/home\" TMPDIR=\"${d}/tmp\"; exec setpriv --reuid=${u} --regid=${u} ${SETPRIV_OPCOES[*]} ${cmd} 2>>\"${d}/stderr.log\"'"
        else
            wrapper="timeout -k 3s ${timeout_bot}s bash -c 'ulimit -v ${MEM_LIMIT_KB} 2>/dev/null || true; exec ${cmd}'"
        fi
        comandos_protegidos+=("$wrapper")
        i=$(( i + 1 ))
    done

    echo -e "  ${TAG_INFO} Bots conectados via Pipes IPC: ${#NOMES_BOTS[@]}"
    echo -e "  ${TAG_INFO} Parametros de isolamento: orcamento da partida=${orcamento}s (vigia global), limite por bot=${timeout_bot}s, ulimit_mem=${MEM_LIMIT_KB}KB"
    if [[ ${ISOLADO} -eq 1 ]]; then
        echo -e "  ${TAG_INFO} Modo isolado: ulimit -u ${BOT_MAX_PROC}, disco ${BOT_MAX_DISCO_MB} MB por bot; uids:"
        for ((i=0; i<${#NOMES_BOTS[@]}; i++)); do
            echo "      Bot $(( i + 1 )): ${NOMES_BOTS[$i]} -> uid $(( BOT_UID_BASE + i ))"
        done
    fi
    if [[ -n "${FASE_ARQ}" ]]; then
        echo -e "  ${TAG_INFO} Modo fase: $(basename "${FASE_ARQ}") (TOPCOM_FASE_SEED=${FASE_SEED:-sorteada})"
        rm -f "${DIR_RESULTADO_FASE}/resultado_fase.json"
    elif [[ -n "${TOPCOM_SEED:-}" ]]; then
        echo -e "  ${TAG_INFO} Seed solicitada: TOPCOM_SEED=${TOPCOM_SEED}"
    else
        echo -e "  ${TAG_INFO} Seed: sorteada pela engine (defina TOPCOM_SEED para reproduzir a partida)"
    fi
    echo -e "  ${TAG_EXEC} Batalha em andamento..."

    # A engine grava match_frames.json e replays/ no diretório atual: roda na raiz do orquestrador.
    # PYTHONHASHSEED fixo deixa a ordem de sets/dicts dos bots Python igual entre execuções.
    rm -f "${DIR_ROOT}/match_frames.json"
    # Vigia global: a engine (e, por ela, os bots) é encerrada se a partida estourar o orçamento.
    local rc=0
    ( cd "${DIR_ROOT}" && PYTHONHASHSEED=0 timeout -k 10s "${orcamento}s" "${ENGINE_BIN}" ${ARGS_FASE[@]+"${ARGS_FASE[@]}"} "${comandos_protegidos[@]}" ) || rc=$?

    if [[ -n "${FASE_ARQ}" && ${rc} -eq 2 ]]; then
        echo -e "${TAG_ERRO} Modo fase: a engine recusou o arquivo da fase (cenario invalido; veja a mensagem acima)."
        rm -f "${DIR_ROOT}/match_frames.json"
        return 0
    fi

    if [[ ${rc} -eq 124 || ${rc} -eq 137 ]]; then
        echo -e "${TAG_ERRO} Vigia da partida: a partida excedeu o orcamento de ${orcamento}s e a engine foi encerrada."
        rm -f "${DIR_ROOT}/match_frames.json"
        return 0
    fi
    echo -e "${TAG_OK} Simulacao encerrada pela Engine Central."
}

# ==============================================================================
# 6. CONSOLIDAÇÃO DE RELATÓRIO JSON E RESUMO ESTATÍSTICO SEM EMOJIS
# ==============================================================================
exibir_resumo_e_gerar_json() {
    echo -e "${COR_AZUL}[6/6] Consolidando replay completo da partida (match_frames.json)...${COR_RESET}"

    python3 - << 'EOF' "$DIR_ROOT" \
        "${#NOMES_BOTS[@]}" ${NOMES_BOTS[@]+"${NOMES_BOTS[@]}"} \
        "${#LANGS_ENTRARAM[@]}" ${LANGS_ENTRARAM[@]+"${LANGS_ENTRARAM[@]}"} \
        "${#EXCL_NOMES[@]}" ${EXCL_NOMES[@]+"${EXCL_NOMES[@]}"} \
        "${#EXCL_MOTIVOS[@]}" ${EXCL_MOTIVOS[@]+"${EXCL_MOTIVOS[@]}"}
import sys, os, json, time

dir_root = sys.argv[1]
_args = sys.argv[2:]


def ler_lista():
    """Lê '<n> item1 ... itemN' dos argumentos."""
    global _args
    n = int(_args[0])
    itens, _args = _args[1:1 + n], _args[1 + n:]
    return itens


nomes_bots = ler_lista()      # slot i -> id i+1 na engine (ordem alfabética)
langs_bots = ler_lista()
excl_nomes = ler_lista()
excl_motivos = ler_lista()

match_frames_path = os.path.join(dir_root, "match_frames.json")
resultado_legado_path = os.path.join(dir_root, "resultado_partida.json")

# Remove resultado_partida.json se existir para manter estritamente apenas match_frames.json
if os.path.isfile(resultado_legado_path):
    try:
        os.remove(resultado_legado_path)
    except Exception:
        pass

# 1. Carrega o match_frames.json gerado pela engine (seed, ranking por id e frames)
if not os.path.isfile(match_frames_path):
    print("\033[1;31m[ERRO] A engine nao gerou o match_frames.json: a partida falhou.\033[0m")
    sys.exit(2)
try:
    with open(match_frames_path, 'r', encoding='utf-8') as f:
        dados = json.load(f)
except Exception as e:
    print(f"\033[1;31m[ERRO] match_frames.json da engine ilegivel: {e}\033[0m")
    sys.exit(2)

frames_lista = dados.get("frames", [])
ranking_engine = {b.get("id"): b for b in dados.get("bots", []) if isinstance(b, dict)}

# 2. Ranking com os nomes reais: o id da engine é o slot (id = posição na lista + 1)
bots_info = []
for i, nome in enumerate(nomes_bots):
    r = ranking_engine.get(i + 1, {})
    bots_info.append({
        "id": i + 1,
        "nome": nome,
        "lang": langs_bots[i] if i < len(langs_bots) else "Desconhecida",
        "posicao": r.get("posicao"),
        "vivo": bool(r.get("vivo", False)),
        "kills": int(r.get("kills", 0)),
        "hp": float(r.get("hp", 0.0)),
        "vencedor": bool(r.get("vencedor", False)),
        "tick_eliminacao": r.get("tick_eliminacao"),
        "abatido_por": r.get("abatido_por"),
        "causa_eliminacao": r.get("causa_eliminacao"),
        "dano_causado": float(r.get("dano_causado", 0.0)),
    })

# Tanques da partida (equipes e alvos): o rótulo das equipes vira o nome real
nome_por_id = {b["id"]: b["nome"] for b in bots_info}
tanques = []
for t in dados.get("tanques", []):
    if not isinstance(t, dict):
        continue
    t = dict(t)
    if t.get("controle") == "equipe" and t.get("id") in nome_por_id:
        t["rotulo"] = nome_por_id[t["id"]]
    tanques.append(t)

# Dono do replay: quem disparou a partida (o BOCA passa TOPCOM_DONO_*). Equipe: o tanque
# dela (tank_id), para a Unity mostrar VITÓRIA ou DERROTA; juiz/admin: tank_id null.
dono_tipo = os.environ.get("TOPCOM_DONO_TIPO", "").strip()
dono = None
if dono_tipo in ("equipe", "juiz", "admin"):
    dono_bot = os.environ.get("TOPCOM_DONO_BOT", "").strip()
    dono = {
        "tipo": dono_tipo,
        "usuario": os.environ.get("TOPCOM_DONO_USUARIO", "").strip(),
        "tank_id": next((b["id"] for b in bots_info if dono_bot and b["nome"] == dono_bot), None),
    }

vencedor_real = next((b["nome"] for b in bots_info if b["posicao"] == 1), "Empate")
excluidos = [{"nome": n, "motivo": m} for n, m in zip(excl_nomes, excl_motivos)]

partida_id = str(dados.get("partida_id") or time.time_ns())
turnos = dados.get("turnos", len(frames_lista))
duracao_ms = dados.get("duracao_ms", int(len(frames_lista) * 100))
data_hora = time.strftime("%d/%m/%Y %H:%M:%S")

resumo = {
    "partida_id": partida_id,
    "data_hora": data_hora,
    "status": dados.get("status", "concluido"),
    "seed": dados.get("seed"),
    "vencedor": vencedor_real,
    "turnos": turnos,
    "duracao_ms": duracao_ms,
    "bots": bots_info,
    "excluidos": excluidos,
}

# 3. match_frames.json unificado (resumo + config + frames), gravado de forma atômica
#    Modo fase: o bloco "fase" da engine (cenário sorteado) vai no topo; no torneio ele não existe.
resultado_final = {"config": dados.get("config", {}), "dono": dono, **resumo, "tanques": tanques, "frames": frames_lista}
if isinstance(dados.get("fase"), dict):
    resultado_final = {"fase": dados["fase"], **resultado_final}
tmp_path = match_frames_path + ".tmp"
with open(tmp_path, 'w', encoding='utf-8') as f:
    json.dump(resultado_final, f, indent=2, ensure_ascii=False)
os.replace(tmp_path, match_frames_path)

# 4. Resumo em replays/partida_<id>.json (sem frames)
replay_path = os.path.join(dir_root, "replays", f"partida_{partida_id}.json")
try:
    with open(replay_path, 'w', encoding='utf-8') as f:
        json.dump(resumo, f, indent=2, ensure_ascii=False)
except Exception as e:
    print(f"[AVISO] Nao foi possivel gravar {replay_path}: {e}")

tamanho_bytes = os.path.getsize(match_frames_path)
if tamanho_bytes >= 1048576:
    tam_str = f"{tamanho_bytes / 1048576:.1f} MB"
elif tamanho_bytes >= 1024:
    tam_str = f"{tamanho_bytes / 1024:.1f} KB"
else:
    tam_str = f"{tamanho_bytes} B"

print("\n\033[1;36m====================================================================\033[0m")
print("\033[1;36m                     RESUMO GERAL DA PARTIDA                        \033[0m")
print("\033[1;36m====================================================================\033[0m")
print(f"\033[1;33m{'PARTIDA ID':<22} | {'VENCEDOR':<15} | {'TURNOS':<8} | {'DURACAO':<10}\033[0m")
print("-----------------------|-----------------|----------|-----------")
print(f"{partida_id[:22]:<22} | \033[1;32m{vencedor_real:<15}\033[0m | {turnos:<8} | {duracao_ms} ms")
print("-----------------------|-----------------|----------|-----------")
print(f"Seed: {resumo['seed']}")
print(f"\033[1;32m[STATUS] Gravacao concluida: match_frames.json ({tam_str})\033[0m")
print(f"\033[1;32m[INFO] Total de frames salvos: {len(frames_lista)} frames\033[0m")
print("\033[1;36mClassificacao (vivo > abates > vida):\033[0m")
for p in sorted(bots_info, key=lambda b: (b["posicao"] is None, b["posicao"] or 0)):
    venc_tag = " \033[1;32m[Vencedor]\033[0m" if p['vencedor'] else ""
    estado = "VIVO" if p["vivo"] else f"eliminado no tick {p['tick_eliminacao']}"
    print(f"  {str(p['posicao']) + 'o':>4} Bot {p['id']}: {p['nome']} ({p['lang']}) | {estado} | "
          f"abates {p['kills']} | vida {p['hp']:.1f}{venc_tag}")
if excluidos:
    print(f"\033[1;33mExcluidos da partida: {len(excluidos)}\033[0m")
print("\033[1;36m====================================================================\033[0m")
EOF
}

# ==============================================================================
# 7. MODO FASE: AVALIAÇÃO (build/avaliar_fase <fase.json> match_frames.json)
# ==============================================================================
avaliar_fase() {
    [[ -n "${FASE_ARQ}" ]] || return 0
    echo -e "${COR_AZUL}[fase] Avaliando a fase (resultado_fase.json)...${COR_RESET}"
    local saida="${DIR_RESULTADO_FASE}/resultado_fase.json"
    if [[ ${AVALIAR_PRONTO} -ne 1 ]]; then
        echo -e "  ${TAG_AVISO} Avaliador da fase indisponivel: resultado_fase.json nao gerado."
        return 0
    fi
    if [[ ! -f "${DIR_ROOT}/match_frames.json" ]]; then
        echo -e "  ${TAG_AVISO} Sem match_frames.json: a fase nao foi avaliada."
        return 0
    fi
    local rc=0 log_avaliar="${DIR_LOGS}/_avaliar_fase_execucao.log"
    timeout -k 5s 60s "${AVALIAR_BIN}" "${FASE_ARQ}" "${DIR_ROOT}/match_frames.json" > "${saida}.tmp" 2> "${log_avaliar}" || rc=$?
    if [[ ${rc} -ne 0 ]]; then
        rm -f "${saida}.tmp"
        echo -e "  ${TAG_ERRO} avaliar_fase terminou com codigo ${rc}; resultado_fase.json nao gerado:"
        head -n 5 "${log_avaliar}" 2>/dev/null | sed 's/^/      /' || true
        return 0
    fi
    mv -f "${saida}.tmp" "${saida}"
    python3 - "${saida}" << 'EOF' || true
import json, sys
try:
    r = json.load(open(sys.argv[1], encoding="utf-8"))
except Exception as e:
    print(f"  [AVISO] resultado_fase.json ilegivel: {e}")
    sys.exit(0)
veredito = "PASSOU" if r.get("passou") else "NAO PASSOU"
print(f"  Fase {r.get('fase')}: {veredito} | balões {r.get('baloes')} | {r.get('motivo', '')}")
EOF
    # fase.pontos[].consumido_tick no replay: tick em que o ponto passou a contar (-1 = nunca; sem null,
    # que o JsonUtility da Unity não lê num int), para o visualizador esconder o ponto a partir dali;
    # e fase.titulo, fase.limite_frame e fase.progresso[] para o HUD (VISUALIZADOR_WEB.md)
    python3 - "${saida}" "${DIR_ROOT}/match_frames.json" << 'EOF' || true
import json, os, sys
try:
    r = json.load(open(sys.argv[1], encoding="utf-8"))
    with open(sys.argv[2], encoding="utf-8") as f:
        replay = json.load(f)
    consumidos = r.get("pontos_consumidos") or {}
    fase = replay.get("fase")
    pontos = (fase or {}).get("pontos") or []
    for p in pontos:
        p["consumido_tick"] = consumidos.get(p.get("id"), -1)
    # HUD do visualizador: título, prazo e progresso por frame, calculados pelo avaliador
    if isinstance(fase, dict):
        fase["titulo"] = r.get("titulo") or fase.get("id", "")
        fase["limite_frame"] = r.get("limite_frame", -1)
        fase["progresso"] = r.get("progresso") or []
    # O progresso só serve ao replay: sai do resultado_fase.json, que o BOCA guarda por tentativa
    if "progresso" in r:
        del r["progresso"]
        tmp = sys.argv[1] + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(r, f, ensure_ascii=False)
        os.replace(tmp, sys.argv[1])
    if isinstance(fase, dict):
        tmp = sys.argv[2] + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(replay, f, ensure_ascii=False, indent=2)
        os.replace(tmp, sys.argv[2])
except Exception as e:
    print(f"  [AVISO] consumido_tick nao anotado no replay: {e}")
EOF
    echo -e "${TAG_OK} Resultado da fase gravado em: ${saida}"
}

# --- Execução Principal ---
calcular_jobs
verificar_dependencias
baixar_bibliotecas
coletar_candidatos
compilar_engine
isolamento_preparar
preparar_bots
executar_batalha
isolamento_relatorio
exibir_resumo_e_gerar_json
avaliar_fase
