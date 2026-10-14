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
# Saída: match_frames.json e replays/partida_<id>.json (seed, bots[], excluidos[], frames)
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
TIMEOUT_SEG=30                # Tempo máximo de execução por partida/bot
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
# 4. INVENTÁRIO PRÉ-COMPILAÇÃO, COMPILAÇÃO E IAS CONFIRMADAS NA PARTIDA
# ==============================================================================
declare -a NOMES_BOTS=()
declare -a COMANDOS_BOTS=()
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
    echo "$resumo" | sed -E 's#/tmp/java_build_[A-Za-z0-9]+/##g'
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

    local prox=0 lote_n=0
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
            local log_bot="${DIR_LOGS}/${team}.log"
            local cmd_exec=""
            case "${CAND_EXTS[$i]}" in
                py) cmd_exec="python3 \"${bin_out}\"" ;;
                cpp|c) cmd_exec="\"${bin_out}\"" ;;
                java) cmd_exec="java ${JAVA_OPTS_BOT} -jar \"${bin_out}\"" ;;
            esac

            echo -e "  ${TAG_BUILD} [${lang_name}] Compilando equipe '${team}' (${bname})..."
            resultado_job "${log_bot}"
            if [[ ${JOB_RC} -eq 0 && -f "${bin_out}" ]]; then
                NOMES_BOTS+=("$team")
                COMANDOS_BOTS+=("${cmd_exec}")
                ARQS_ENTRARAM+=("$bname")
                LANGS_ENTRARAM+=("${lang_name}")
                echo -e "    ${TAG_OK} Preparacao '${lang_name}' concluida em $(awk "BEGIN {printf \"%.1f\", ${JOB_MS}/1000}")s -> ${bin_out}"
            else
                local err_resumo
                err_resumo="$(resumo_erro "${log_bot}")"
                excluir "$team" "Erro de compilacao modular (${err_resumo})"
                echo -e "    ${TAG_ERRO} Falha na compilacao para '${team}': ${err_resumo}"
            fi
        done
    done

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
executar_batalha() {
    echo -e "${COR_AZUL}[5/6] Conectando IAs e iniciando simulacao na Arena TopCom...${COR_RESET}"

    local comandos_protegidos=()
    for cmd in "${COMANDOS_BOTS[@]}"; do
        local wrapper="timeout -k 3s ${TIMEOUT_SEG}s bash -c 'ulimit -v ${MEM_LIMIT_KB} 2>/dev/null || true; exec ${cmd}'"
        comandos_protegidos+=("$wrapper")
    done

    echo -e "  ${TAG_INFO} Bots conectados via Pipes IPC: ${#NOMES_BOTS[@]}"
    echo -e "  ${TAG_INFO} Parametros de isolamento: timeout=${TIMEOUT_SEG}s, ulimit_mem=${MEM_LIMIT_KB}KB"
    if [[ -n "${TOPCOM_SEED:-}" ]]; then
        echo -e "  ${TAG_INFO} Seed solicitada: TOPCOM_SEED=${TOPCOM_SEED}"
    else
        echo -e "  ${TAG_INFO} Seed: sorteada pela engine (defina TOPCOM_SEED para reproduzir a partida)"
    fi
    echo -e "  ${TAG_EXEC} Batalha em andamento..."

    # A engine grava match_frames.json e replays/ no diretório atual: roda na raiz do orquestrador.
    # PYTHONHASHSEED fixo deixa a ordem de sets/dicts dos bots Python igual entre execuções.
    rm -f "${DIR_ROOT}/match_frames.json"
    ( cd "${DIR_ROOT}" && PYTHONHASHSEED=0 "${ENGINE_BIN}" "${comandos_protegidos[@]}" ) || true

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
    })

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
resultado_final = {"config": dados.get("config", {}), **resumo, "frames": frames_lista}
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

# --- Execução Principal ---
calcular_jobs
verificar_dependencias
baixar_bibliotecas
coletar_candidatos
compilar_engine
preparar_bots
executar_batalha
exibir_resumo_e_gerar_json
