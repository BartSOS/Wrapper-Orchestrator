#!/usr/bin/env bash
# ==============================================================================
# ORQUESTRADOR DE GAME JAM DE IAS - ARENA TOPCOM BOCA
# Gerencia compilação, inventário pré/pós-execução, isolamento e relatórios.
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
DIR_REPLAYS="${DIR_ROOT}/replays"

# Diretórios para as bibliotecas locais
DIR_LIBS="${DIR_ROOT}/libs"
GSON_JAR="${DIR_LIBS}/gson-2.10.1.jar"
NLOHMANN_DIR="${DIR_ROOT}/engine/nlohmann"
NLOHMANN_HPP="${NLOHMANN_DIR}/json.hpp"

ENGINE_SRC="${DIR_ROOT}/engine/main.cpp"
ENGINE_BIN="${DIR_BUILD}/engine"

# --- Configurações de Segurança e Limites ---
TIMEOUT_SEG=30                # Tempo máximo de execução por partida/bot
MEM_LIMIT_KB=2048000
MAX_BOTS="${TOPCOM_MAX_BOTS:-20}"   # Limite de bots por partida (20 no evento; o benchmark pode sobrepor)

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
# 3. COMPILAÇÃO DA ENGINE CENTRAL EM C++
# ==============================================================================
compilar_engine() {
    echo -e "${COR_AZUL}[3/6] Compilando Engine Central TopCom (C++)...${COR_RESET}"
    mkdir -p "${DIR_BUILD}" "${DIR_BOTS_BUILD}" "${DIR_REPLAYS}"

    if [[ ! -f "${ENGINE_SRC}" ]]; then
        echo -e "  ${TAG_ERRO} Arquivo-fonte da engine (${ENGINE_SRC}) nao encontrado!"
        exit 1
    fi

    echo -e "  ${TAG_INFO} Fonte: ${ENGINE_SRC}"
    echo -e "  ${TAG_INFO} Binario de destino: ${ENGINE_BIN}"

    if g++ -O3 -std=c++20 -I"${DIR_ROOT}" -I"${DIR_ROOT}/engine" "${ENGINE_SRC}" -o "${ENGINE_BIN}"; then
        echo -e "${TAG_OK} Engine compilada com sucesso em: ${ENGINE_BIN}"
    else
        echo -e "${TAG_ERRO} Falha ao compilar a Engine Central!"
        exit 1
    fi
}

# ==============================================================================
# 4. INVENTÁRIO PRÉ-COMPILAÇÃO, COMPILAÇÃO E IAS CONFIRMADAS NA PARTIDA
# ==============================================================================
declare -a NOMES_BOTS=()
declare -a COMANDOS_BOTS=()
declare -a ARQS_ENTRARAM=()
declare -a LANGS_ENTRARAM=()
declare -a MOTIVOS_EXCLUSAO=()

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

preparar_bots() {
    echo -e "${COR_AZUL}[4/6] Analisando codigos das equipes e preparando IAs para a Arena...${COR_RESET}"
    
    local CP_SEP=":"
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
        CP_SEP=";"
    fi

    # Coleta arquivos do diretório de entrada
    local -a CAND_EQUIPES=()
    local -a CAND_ARQUIVOS=()
    local -a CAND_NOMES=()
    local -a CAND_TAMANHOS=()
    local -a CAND_LANGS=()
    local -a CAND_EXTS=()

    if [[ -d "${DIR_INPUT}" ]]; then
        readarray -t arquivos < <(find "${DIR_INPUT}" -type f \( -name "*.py" -o -name "*.cpp" -o -name "*.java" -o -name "*.c" \) | sort)
        for arq in "${arquivos[@]}"; do
            [[ -f "$arq" ]] || continue
            local bname=$(basename "$arq")
            [[ "$bname" == "submission_meta.json" ]] && continue
            local ext="${bname##*.}"
            if [[ "$ext" == "py" || "$ext" == "cpp" || "$ext" == "java" || "$ext" == "c" ]]; then
                local nome_sem_ext="${bname%.*}"
                local tam_bytes=$(wc -c < "$arq" 2>/dev/null || echo 0)
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

                CAND_EQUIPES+=("$nome_sem_ext")
                CAND_ARQUIVOS+=("$arq")
                CAND_NOMES+=("$bname")
                CAND_TAMANHOS+=("$tam_fmt")
                CAND_LANGS+=("$lang_nome")
                CAND_EXTS+=("$ext")
            fi
        done
    else
        echo -e "${TAG_ERRO} Diretorio de submissoes nao encontrado: ${DIR_INPUT}"
    fi

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
    echo -e "${COR_AZUL}Iniciando validacao e compilacao dos candidatos de forma modular...${COR_RESET}"

    for ((i=0; i<${#CAND_EQUIPES[@]}; i++)); do
        local team="${CAND_EQUIPES[$i]}"
        local arq="${CAND_ARQUIVOS[$i]}"
        local bname="${CAND_NOMES[$i]}"
        local ext="${CAND_EXTS[$i]}"

        # Limite máximo de bots por partida na arena
        if [[ ${#NOMES_BOTS[@]} -ge ${MAX_BOTS} ]]; then
            MOTIVOS_EXCLUSAO+=("${team}: Excedeu limite maximo da partida (${MAX_BOTS} bots)")
            continue
        fi

        local lang_name=""
        local cmd_exec=""
        local bin_out="${DIR_BOTS_BUILD}/${team}"

        case "$ext" in
            py) 
                lang_name="Python 3"
                cmd_exec="python3 \"${bin_out}\""
                ;;
            cpp) 
                lang_name="C++20"
                cmd_exec="\"${bin_out}\""
                ;;
            java) 
                lang_name="Java"
                cmd_exec="java -jar \"${bin_out}\""
                ;;
            c)
                lang_name="C"
                cmd_exec="\"${bin_out}\""
                ;;
            *) 
                MOTIVOS_EXCLUSAO+=("${team}: Extensao nao suportada (${ext})")
                continue
                ;;
        esac

        local build_script="${DIR_ROOT}/wrappers/build_bot.sh"
        # Chamado via bash: a permissão +x se perde quando o BOCA extrai o engine.zip
        if [[ ! -f "$build_script" ]]; then
            MOTIVOS_EXCLUSAO+=("${team}: Script wrappers/build_bot.sh ausente")
            echo -e "    ${TAG_ERRO} Script wrappers/build_bot.sh nao encontrado."
            continue
        fi

        echo -e "  ${TAG_BUILD} [${lang_name}] Compilando equipe '${team}' (${bname})..."
        mkdir -p "${DIR_BOTS_BUILD}"

        # Executa compilacao utilizando o build_bot.sh da raiz
        local err_comp=""
        local exit_comp=0
        if ! err_comp=$(bash "${build_script}" "$arq" "$bin_out" 2>&1); then
            exit_comp=1
        fi

        if [[ $exit_comp -eq 0 ]]; then
            NOMES_BOTS+=("$team")
            COMANDOS_BOTS+=("${cmd_exec}")
            ARQS_ENTRARAM+=("$bname")
            LANGS_ENTRARAM+=("${lang_name}")
            echo -e "    ${TAG_OK} Preparacao '${lang_name}' concluida -> ${bin_out}"
        else
            local err_resumo=$(echo "$err_comp" | head -n 2 | tr '\n' ' ')
            MOTIVOS_EXCLUSAO+=("${team}: Erro de compilacao modular (${err_resumo})")
            echo -e "    ${TAG_ERRO} Falha na compilacao para '${team}': ${err_resumo}"
        fi
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

    if [[ ${#MOTIVOS_EXCLUSAO[@]} -gt 0 ]]; then
        echo ""
        echo -e "${COR_AMARELO}IAs nao participantes / desqualificadas nesta rodada:${COR_RESET}"
        for mot in "${MOTIVOS_EXCLUSAO[@]}"; do
            echo -e "  • ${mot}"
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
    echo -e "  ${TAG_EXEC} Batalha em andamento..."
    
    "${ENGINE_BIN}" "${comandos_protegidos[@]}" || true
    
    echo -e "${TAG_OK} Simulacao encerrada pela Engine Central."
}

# ==============================================================================
# 6. CONSOLIDAÇÃO DE RELATÓRIO JSON E RESUMO ESTATÍSTICO SEM EMOJIS
# ==============================================================================
exibir_resumo_e_gerar_json() {
    echo -e "${COR_AZUL}[6/6] Consolidando replay completo da partida (match_frames.json)...${COR_RESET}"
    
    python3 - << 'EOF' "$DIR_ROOT" "${#NOMES_BOTS[@]}" "${NOMES_BOTS[@]}" "${LANGS_ENTRARAM[@]}"
import sys, os, glob, json, time

dir_root = sys.argv[1]
n_bots = int(sys.argv[2])
nomes_bots = sys.argv[3:3 + n_bots]
langs_bots = sys.argv[3 + n_bots:3 + 2 * n_bots]

replays_dir = os.path.join(dir_root, "replays")
replay_files = sorted(glob.glob(os.path.join(replays_dir, "partida_*.json")), key=os.path.getmtime, reverse=True)
match_frames_path = os.path.join(dir_root, "match_frames.json")
resultado_legado_path = os.path.join(dir_root, "resultado_partida.json")

# Remove resultado_partida.json se existir para manter estritamente apenas match_frames.json
if os.path.isfile(resultado_legado_path):
    try:
        os.remove(resultado_legado_path)
    except Exception:
        pass

# 1. Carrega o match_frames.json gerado pela engine
data_frames = {}
frames_lista = []
config_arena = {}
if os.path.isfile(match_frames_path):
    try:
        with open(match_frames_path, 'r', encoding='utf-8') as f:
            data_frames = json.load(f)
            frames_lista = data_frames.get("frames", [])
            config_arena = data_frames.get("config", {})
    except Exception as e:
        print(f"[AVISO] Erro ao ler match_frames.json da engine: {e}")

# 2. Carrega o resumo da partida da engine
data_resumo = {}
if replay_files:
    try:
        with open(replay_files[0], 'r', encoding='utf-8') as f:
            data_resumo = json.load(f)
    except Exception as e:
        print(f"[AVISO] Erro ao ler resumo do replay: {e}")

# Mapeia nomes genéricos Bot_1, Bot_2 para os nomes reais das equipes
def mapear_nome(bot_str):
    if not bot_str: return "Empate"
    for i, nome in enumerate(nomes_bots):
        tag = f"Bot_{i+1}"
        if tag in bot_str:
            return bot_str.replace(tag, nome)
        tag_id = f"Bot ID [{i+1}]"
        if tag_id in bot_str:
            return bot_str.replace(tag_id, nome)
    return bot_str

vencedor_original = data_resumo.get("vencedor", data_frames.get("vencedor", "Empate"))
vencedor_real = mapear_nome(vencedor_original)

# Associação prévia canônica: id -> nome, linguagem e status
bots_info = []
for i in range(n_bots):
    nome = nomes_bots[i]
    lang = langs_bots[i] if i < len(langs_bots) else "Desconhecido"
    bots_info.append({
        "id": i + 1,
        "nome": nome,
        "lang": lang,
        "vencedor": (vencedor_real == nome)
    })

partida_id = data_resumo.get("partida_id", data_frames.get("partida_id", str(int(time.time()))))
turnos = data_resumo.get("turnos", data_frames.get("turnos", len(frames_lista)))
duracao_ms = data_resumo.get("duracao_ms", data_frames.get("duracao_ms", int(len(frames_lista) * 100)))

# 3. Monta o JSON unificado match_frames.json (SEM destaques, com associação prévia de bots)
resultado_final = {
    "config": config_arena,
    "partida_id": partida_id,
    "data_hora": time.strftime("%d/%m/%Y %H:%M:%S"),
    "status": "concluido",
    "vencedor": vencedor_real,
    "turnos": turnos,
    "duracao_ms": duracao_ms,
    "bots": bots_info,
    "frames": frames_lista
}

# Salva exclusivamente o arquivo canônico match_frames.json
with open(match_frames_path, 'w', encoding='utf-8') as f:
    json.dump(resultado_final, f, indent=2, ensure_ascii=False)

# 4. Atualiza o replay salvo na pasta replays/ com o nome real e sem destaques
if replay_files:
    try:
        replay_clean = {
            "partida_id": partida_id,
            "data_hora": time.strftime("%d/%m/%Y %H:%M:%S"),
            "status": "concluido",
            "vencedor": vencedor_real,
            "turnos": turnos,
            "duracao_ms": duracao_ms,
            "bots": bots_info
        }
        with open(replay_files[0], 'w', encoding='utf-8') as f:
            json.dump(replay_clean, f, indent=2, ensure_ascii=False)
    except Exception:
        pass

tamanho_bytes = os.path.getsize(match_frames_path) if os.path.isfile(match_frames_path) else 0
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
partida_id_curto = str(resultado_final['partida_id'])[:22]
print(f"{partida_id_curto:<22} | \033[1;32m{vencedor_real:<15}\033[0m | {resultado_final['turnos']:<8} | {resultado_final['duracao_ms']} ms")
print("-----------------------|-----------------|----------|-----------")
print(f"\033[1;32m[STATUS] Gravacao concluida: match_frames.json ({tam_str})\033[0m")
print(f"\033[1;32m[INFO] Total de frames salvos: {len(frames_lista)} frames\033[0m")
print("\033[1;36mEquipes participantes:\033[0m")
for p in bots_info:
    venc_tag = " \033[1;32m[Vencedor]\033[0m" if p['vencedor'] else ""
    print(f"  • Bot {p['id']}: {p['nome']} ({p['lang']}){venc_tag}")
print("\033[1;36m====================================================================\033[0m")
EOF
}

# --- Execução Principal ---
verificar_dependencias
baixar_bibliotecas
compilar_engine
preparar_bots
executar_batalha
exibir_resumo_e_gerar_json