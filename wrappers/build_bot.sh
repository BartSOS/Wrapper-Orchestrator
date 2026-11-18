#!/bin/bash
# ==============================================================================
# Game Jam PET - Construtor de Bots
# ==============================================================================
# Compila e injeta o wrapper oficial em códigos C, C++, Python e Java. É a MESMA
# montagem usada pela partida (script.sh) e pelo validador do BOCA (compile.sh dos
# pacotes de linguagem): quem recebe YES no validador compila aqui também.
#
# Uso: bash wrappers/build_bot.sh <arquivo_fonte> <arquivo_saida>
#      bash wrappers/build_bot.sh --preparar c|cpp ...   # só pré-compila os wrappers
#
# Verificações antes de compilar (mensagens "ERRO DE VALIDACAO: ..."):
#   C/C++  pensar() presente, sem system(), sem #include com caminho absoluto ou ".."
#   Java   pensar() presente, sem Runtime.getRuntime/ProcessBuilder/System.exit, sem package;
#          a classe principal é a pública (ou a que contém pensar)
#   Python AST (wrappers/checar_python.py): sintaxe, def pensar(estado), só biblioteca padrão,
#          sem os/subprocess/eval/exec/__import__/importlib, sem input()/sys.stdin no topo,
#          sem if __name__ == "__main__"
# Depois da compilação, erros conhecidos (main() extra, assinatura de pensar errada...)
# viram uma mensagem clara na primeira linha; o resto da saída do compilador vem em seguida
# apenas quando o erro não foi reconhecido.
#
# Os wrappers de C e C++ são pré-compilados uma vez e reaproveitados enquanto os
# fontes, o compilador e as flags não mudarem (assinatura SHA-256). Ficam em
# TOPCOM_WRAPPERS_BUILD (padrão: build/wrappers; o validador usa
# /var/cache/gamejam-validador). Se o diretório não for gravável, usa um temporário.
#   C   -> c/c_wrapper.o: main + JSON -> structs de arena.h. Cada bot só compila
#          o próprio .c (gcc -std=c99 -include arena.h) e é ligado a ele.
#   C++ -> cpp/cpp_header.hpp.gch (cabeçalho pré-compilado com arena.hpp e nlohmann/json),
#          cpp/cpp_footer.o (main + E/S JSON -> Estado de arena.hpp).
#          Cada bot compila com -include cpp_header.hpp e é ligado ao rodapé.
# Contrato da equipe (sem main, só a função pensar, nomes em português):
#   C      Acao pensar(const Estado *e)        #include "arena.h"
#   C++    Acao pensar(const Estado& e)        #include "arena.hpp"
#   Java   public static Acao pensar(Estado e)  na classe "public class X extends Arena"
#   Python def pensar(estado)                    from arena import *
# Os arquivos de apoio (arena.h, arena.hpp, Arena.java, arena.py) ficam aqui em
# wrappers/ e vão no kit; a equipe envia só o próprio arquivo.
# O script.sh chama --preparar antes de compilar os bots em paralelo.
# ==============================================================================
set -euo pipefail

DIR_WRAPPERS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIR_ROOT="$(cd "${DIR_WRAPPERS}/.." && pwd)"
DIR_PREBUILT="${TOPCOM_WRAPPERS_BUILD:-${DIR_ROOT}/build/wrappers}"
NLOHMANN_INC="${DIR_ROOT}/engine"
GSON_JAR="${DIR_ROOT}/libs/gson-2.10.1.jar"
# Temporários no TMPDIR (o modo isolado do script.sh dá um tmp/ próprio a cada bot); sem ele, /tmp
DIR_TMP="${TMPDIR:-/tmp}"
DIR_TMP="${DIR_TMP%/}"
[[ -n "${DIR_TMP}" && -d "${DIR_TMP}" && -w "${DIR_TMP}" ]] || DIR_TMP="/tmp"

CXX_FLAGS=(-O2 -std=c++20)
CC_FLAGS=(-O2 -std=c99)

# Arquivos temporários removidos ao sair (sucesso ou erro)
TEMPORARIOS=()
limpar() {
    if [[ ${#TEMPORARIOS[@]} -gt 0 ]]; then
        rm -rf -- "${TEMPORARIOS[@]}"
    fi
}
trap limpar EXIT

# Recusa o bot com a mensagem que a equipe verá no [Ver Erro]
recusar() {
    echo "ERRO DE VALIDACAO: $*" >&2
    exit 1
}

# Assinatura dos wrappers pré-compilados: compilador + flags + fontes
assinatura() {
    { g++ --version | head -n 1; echo "${CXX_FLAGS[*]} | ${CC_FLAGS[*]}"; cat "$@"; } | sha256sum | cut -d' ' -f1
}

# Compila para um arquivo temporário e o move para o destino (troca atômica)
compilar_para() {
    local destino="$1"; shift
    local tmp="${destino}.tmp.$$"
    TEMPORARIOS+=("$tmp")
    LC_ALL=C "$@" -o "$tmp"
    mv -f "$tmp" "$destino"
}

# Diretório do cache para uma linguagem; se não der para gravar, usa um temporário
dir_cache_gravavel() {
    local dir="${DIR_PREBUILT}/$1"
    if mkdir -p "$dir" 2>/dev/null && [[ -w "$dir" ]]; then
        echo "$dir"
        return
    fi
    local tmp
    tmp="$(mktemp -d "${DIR_TMP}/gamejam_wrappers_XXXXXX")"
    TEMPORARIOS+=("$tmp")
    mkdir -p "${tmp}/$1"
    echo "${tmp}/$1"
}

CACHE_C=""
CACHE_CPP=""

# Cache entre partidas (só o script.sh da arena define TOPCOM_WRAPPERS_CACHE, na etapa do root, antes de
# qualquer bot rodar; vazio = sem cache, tudo igual a antes). Entrada de primeiro nível (a faxina do executor
# apaga por ela): wrappers-<lang>-<assinatura>/{arquivos,sha256sums}.
WRAPPERS_CACHE="${TOPCOM_WRAPPERS_CACHE:-}"

# restaurar_do_cache <lang> <assinatura> <arquivos...>: copia os arquivos do cache (sha256 conferido) para o
# diretório de build, grava a assinatura e deixa o diretório em RESTAURADO_DIR. Falha = compilar normalmente.
RESTAURADO_DIR=""
restaurar_do_cache() {
    local lang="$1" ass="$2"; shift 2
    [[ -n "${WRAPPERS_CACHE}" ]] || return 1
    local src="${WRAPPERS_CACHE}/wrappers-${lang}-${ass}" f dir
    [[ -d "$src" && ! -L "$src" && -f "$src/sha256sums" ]] || return 1
    for f in "$@"; do
        [[ -f "$src/$f" && ! -L "$src/$f" ]] || return 1
    done
    (cd "$src" && sha256sum -c --quiet sha256sums > /dev/null 2>&1) || return 1
    dir="$(dir_cache_gravavel "$lang")"
    rm -f "${dir}/assinatura"
    for f in "$@"; do
        cp -- "$src/$f" "${dir}/$f.cache.tmp" 2> /dev/null && mv -f "${dir}/$f.cache.tmp" "${dir}/$f" || return 1
    done
    echo "$ass" > "${dir}/assinatura"
    touch -- "$src" 2> /dev/null || true
    RESTAURADO_DIR="$dir"
}

# gravar_no_cache <lang> <assinatura> <diretório de build> <arquivos...>: troca atômica; mantém as 2 mais novas
gravar_no_cache() {
    local lang="$1" ass="$2" dir="$3"; shift 3
    [[ -n "${WRAPPERS_CACHE}" ]] || return 0
    local base="${WRAPPERS_CACHE}" tmp f
    [[ -d "${base}/wrappers-${lang}-${ass}" ]] && return 0
    tmp="$(mktemp -d "${base}/.novo.XXXXXX" 2> /dev/null)" || return 0
    for f in "$@"; do
        cp -- "${dir}/$f" "${tmp}/$f" 2> /dev/null || { rm -rf -- "${tmp:?}"; return 0; }
    done
    (cd "$tmp" && sha256sum -- "$@" > sha256sums) 2> /dev/null || { rm -rf -- "${tmp:?}"; return 0; }
    chmod 0700 -- "$tmp" 2> /dev/null || true
    if mv -T -- "$tmp" "${base}/wrappers-${lang}-${ass}" 2> /dev/null; then
        ls -1t "$base" 2> /dev/null | grep -E "^wrappers-${lang}-[0-9a-f]{64}\$" | tail -n +3 | while read -r velho; do
            rm -rf -- "${base:?}/${velho:?}"
        done
    else
        rm -rf -- "${tmp:?}"
    fi
    return 0
}

preparar_c() {
    [[ -z "$CACHE_C" ]] || return 0
    local dir="${DIR_PREBUILT}/c"
    local ass
    ass="$(assinatura "${DIR_WRAPPERS}/c_wrapper.cpp" "${DIR_WRAPPERS}/arena.h" "${NLOHMANN_INC}/nlohmann/json.hpp")"
    if [[ -f "${dir}/c_wrapper.o" && "$(cat "${dir}/assinatura" 2>/dev/null)" == "$ass" ]]; then
        CACHE_C="$dir"
        return 0
    fi
    if restaurar_do_cache c "$ass" c_wrapper.o; then
        CACHE_C="${RESTAURADO_DIR}"
        return 0
    fi
    dir="$(dir_cache_gravavel c)"
    rm -f "${dir}/assinatura"
    compilar_para "${dir}/c_wrapper.o" \
        g++ "${CXX_FLAGS[@]}" -I"${DIR_WRAPPERS}" -I"${NLOHMANN_INC}" -c "${DIR_WRAPPERS}/c_wrapper.cpp"
    echo "$ass" > "${dir}/assinatura"
    gravar_no_cache c "$ass" "$dir" c_wrapper.o
    CACHE_C="$dir"
}

preparar_cpp() {
    [[ -z "$CACHE_CPP" ]] || return 0
    local dir="${DIR_PREBUILT}/cpp"
    local ass
    ass="$(assinatura "${DIR_WRAPPERS}/cpp_header.hpp" "${DIR_WRAPPERS}/arena.hpp" "${DIR_WRAPPERS}/cpp_footer.cpp" \
        "${NLOHMANN_INC}/nlohmann/json.hpp")"
    if [[ -f "${dir}/cpp_header.hpp.gch" && -f "${dir}/cpp_footer.o" && \
          "$(cat "${dir}/assinatura" 2>/dev/null)" == "$ass" ]]; then
        CACHE_CPP="$dir"
        return 0
    fi
    if restaurar_do_cache cpp "$ass" cpp_header.hpp cpp_header.hpp.gch cpp_footer.o; then
        CACHE_CPP="${RESTAURADO_DIR}"
        return 0
    fi
    dir="$(dir_cache_gravavel cpp)"
    rm -f "${dir}/assinatura"
    # O .gch só é usado quando está ao lado do cabeçalho passado em -include
    cp "${DIR_WRAPPERS}/cpp_header.hpp" "${dir}/cpp_header.hpp"
    compilar_para "${dir}/cpp_header.hpp.gch" \
        g++ "${CXX_FLAGS[@]}" -w -I"${NLOHMANN_INC}" -I"${DIR_WRAPPERS}" -x c++-header "${dir}/cpp_header.hpp"
    compilar_para "${dir}/cpp_footer.o" \
        g++ "${CXX_FLAGS[@]}" -I"${NLOHMANN_INC}" -I"${DIR_WRAPPERS}" -include "${dir}/cpp_header.hpp" -Winvalid-pch \
            -c "${DIR_WRAPPERS}/cpp_footer.cpp"
    echo "$ass" > "${dir}/assinatura"
    gravar_no_cache cpp "$ass" "$dir" cpp_header.hpp cpp_header.hpp.gch cpp_footer.o
    CACHE_CPP="$dir"
}

# --- Modo de preparação: bash build_bot.sh --preparar c cpp ---
if [[ "${1:-}" == "--preparar" ]]; then
    shift
    for lang in "$@"; do
        case "$lang" in
            c) preparar_c ;;
            cpp) preparar_cpp ;;
            *) ;;  # Python e Java não têm wrapper pré-compilado
        esac
    done
    exit 0
fi

if [ $# -lt 2 ]; then
    echo "Uso: $0 <arquivo_fonte> <arquivo_saida>" >&2
    echo "     $0 --preparar c|cpp ..." >&2
    exit 1
fi

SOURCE_FILE="$1"
EXE_FILE="$2"

EXT="${SOURCE_FILE##*.}"
EXT=$(echo "$EXT" | tr '[:upper:]' '[:lower:]')

# Uma falha de compilação não pode deixar o binário de uma partida anterior no lugar
rm -f "${EXE_FILE}"

# Log da compilação (stdout + stderr dos compiladores)
LOG_COMPILACAO="$(mktemp "${DIR_TMP}/gamejam_build_XXXXXX.log")"
TEMPORARIOS+=("${LOG_COMPILACAO}")
DIR_FONTE="$(cd "$(dirname "${SOURCE_FILE}")" && pwd)"

# Roda um comando juntando a saída no log. Mensagens do compilador em inglês/ASCII
# (aparecem no [Ver Erro]); o resultado é o código de saída do comando.
compilar() {
    LC_ALL=C "$@" >> "${LOG_COMPILACAO}" 2>&1
}

# Erro conhecido de compilação/ligação -> mensagem clara (vazio se não reconhecer)
mensagem_amigavel() {
    local lang="$1"
    if grep -qE "multiple definition of .main.|redefinition of .(int )?main" "${LOG_COMPILACAO}"; then
        echo "nao crie a funcao main(): a arena ja tem a dela. Apague a sua main()."
        return
    fi
    case "$lang" in
        c)
            if grep -q "conflicting types for 'pensar'" "${LOG_COMPILACAO}"; then
                echo "a assinatura de pensar esta errada: use exatamente 'Acao pensar(const Estado *e)'."
            elif grep -q "undefined reference to .pensar'" "${LOG_COMPILACAO}"; then
                echo "a funcao 'Acao pensar(const Estado *e)' nao esta definida (ela so aparece em comentario ou foi apenas declarada)."
            fi
            ;;
        cpp)
            if grep -qE "undefined reference to .pensar\(|ambiguating new declaration of .*pensar|call of overloaded .pensar|conflicting declaration .*pensar" "${LOG_COMPILACAO}"; then
                echo "a assinatura de pensar esta errada: use exatamente 'Acao pensar(const Estado& e)'."
            fi
            ;;
        java)
            if grep -q "JavaRunner.java:[0-9]*: error" "${LOG_COMPILACAO}"; then
                echo "o metodo pensar esta errado ou fora da classe principal: use exatamente 'public static Acao pensar(Estado e)', dentro da classe publica do arquivo (que comeca com 'public class MeuBot extends Arena')."
            fi
            ;;
    esac
}

# Falha de compilação: mensagem clara (se houver) ou a saída do compilador sem os caminhos do servidor
falhar_compilacao() {
    local lang="$1" amigavel
    amigavel="$(mensagem_amigavel "$lang")"
    if [[ -n "$amigavel" ]]; then
        echo "ERRO DE VALIDACAO: ${amigavel}" >&2
    else
        echo "ERRO DE COMPILACAO: o compilador recusou o seu codigo." >&2
        sed -e "s#${DIR_FONTE}/##g" -e "s#${DIR_TMP}/java_build_[A-Za-z0-9]*/##g" "${LOG_COMPILACAO}" >&2
    fi
    exit 1
}

# Avisos do compilador numa compilação que deu certo
mostrar_avisos() {
    if [[ -s "${LOG_COMPILACAO}" ]]; then
        sed -e "s#${DIR_FONTE}/##g" -e "s#${DIR_TMP}/java_build_[A-Za-z0-9]*/##g" "${LOG_COMPILACAO}" >&2
    fi
}

case "$EXT" in
    cpp)
        python3 -I "${DIR_WRAPPERS}/checar_fonte.py" cpp "${SOURCE_FILE}" || exit 1
        preparar_cpp
        OBJ_BOT="${EXE_FILE}_bot.o"
        TEMPORARIOS+=("${OBJ_BOT}")

        # Código do aluno com o cabeçalho pré-compilado (mesmo -include do validador do BOCA)
        compilar g++ "${CXX_FLAGS[@]}" -I"${NLOHMANN_INC}" -I"${DIR_WRAPPERS}" \
            -include "${CACHE_CPP}/cpp_header.hpp" -Winvalid-pch \
            -x c++ -c "${SOURCE_FILE}" -o "${OBJ_BOT}" || falhar_compilacao cpp

        # Liga ao main pré-compilado. A tentativa que falha vai para um log à parte:
        # só aparece se a segunda também falhar.
        LOG_LIGACAO="${LOG_COMPILACAO}.ligacao"
        TEMPORARIOS+=("${LOG_LIGACAO}")
        ligar() { LC_ALL=C "$@" >> "${LOG_LIGACAO}" 2>&1; }
        if ! ligar g++ "${OBJ_BOT}" "${CACHE_CPP}/cpp_footer.o" -o "${EXE_FILE}" -lm; then
            # Compilou, mas não ligou ao main pré-compilado (ex.: pensar() declarada
            # inline): repete com cabeçalho + código + rodapé numa única unidade.
            TMP_SRC="${EXE_FILE}_combined.cpp"
            TEMPORARIOS+=("${TMP_SRC}")
            cat "${DIR_WRAPPERS}/cpp_header.hpp" "${SOURCE_FILE}" "${DIR_WRAPPERS}/cpp_footer.cpp" > "${TMP_SRC}"
            if ! ligar g++ "${CXX_FLAGS[@]}" -I"${NLOHMANN_INC}" -I"${DIR_WRAPPERS}" "${TMP_SRC}" -o "${EXE_FILE}" -lm; then
                cat "${LOG_LIGACAO}" >> "${LOG_COMPILACAO}"
                falhar_compilacao cpp
            fi
        fi
        mostrar_avisos
        ;;

    c)
        # Código do competidor em C99 com arena.h injetado; wrapper de I/O (C++20) pré-compilado
        python3 -I "${DIR_WRAPPERS}/checar_fonte.py" c "${SOURCE_FILE}" || exit 1
        preparar_c
        OBJ_BOT="${EXE_FILE}_bot.o"
        TEMPORARIOS+=("${OBJ_BOT}")
        compilar gcc "${CC_FLAGS[@]}" -I"${DIR_WRAPPERS}" -include "${DIR_WRAPPERS}/arena.h" \
            -x c -c "${SOURCE_FILE}" -o "${OBJ_BOT}" || falhar_compilacao c
        compilar g++ "${OBJ_BOT}" "${CACHE_C}/c_wrapper.o" -o "${EXE_FILE}" -lm || falhar_compilacao c
        mostrar_avisos
        ;;

    py)
        # Análise estática pela AST (sem executar o código): sintaxe, pensar(estado), imports e afins.
        python3 -I "${DIR_WRAPPERS}/checar_python.py" "${SOURCE_FILE}" || exit 1
        WRAPPER_FILE="${DIR_WRAPPERS}/wrapper.py"
        # O código da equipe vai embutido (base64) e só roda depois que o wrapper protege
        # a saída: um print no nível do arquivo não chega mais à engine. O arquivo de apoio
        # (arena.py, o "from arena import *" da equipe) vai embutido do mesmo jeito.
        {
            echo '#!/usr/bin/env python3'
            printf '_FONTE_DA_EQUIPE = "%s"\n' "$(base64 -w0 < "${SOURCE_FILE}")"
            printf '_FONTE_ARENA = "%s"\n' "$(base64 -w0 < "${DIR_WRAPPERS}/arena.py")"
            cat "${WRAPPER_FILE}"
        } > "${EXE_FILE}"
        chmod +x "${EXE_FILE}"
        ;;

    java)
        python3 -I "${DIR_WRAPPERS}/checar_fonte.py" java "${SOURCE_FILE}" > "${LOG_COMPILACAO}.classe" || exit 1
        TEMPORARIOS+=("${LOG_COMPILACAO}.classe")
        CLASS_NAME="$(cat "${LOG_COMPILACAO}.classe")"

        WRAPPER_SRC="${DIR_WRAPPERS}/wrapper.java"
        BUILD_TMP="$(mktemp -d "${DIR_TMP}/java_build_XXXXXX")"
        TEMPORARIOS+=("${BUILD_TMP}")

        # Arquivo de apoio (Estado, Acao, FRENTE...) compilado junto: a equipe não o envia
        cp "$SOURCE_FILE" "${BUILD_TMP}/${CLASS_NAME}.java"
        cp "${DIR_WRAPPERS}/Arena.java" "${BUILD_TMP}/Arena.java"
        sed "s/CLASS_PLACEHOLDER/${CLASS_NAME}/g" "$WRAPPER_SRC" > "${BUILD_TMP}/JavaRunner.java"
        javac_bot() {
            compilar javac -encoding UTF-8 -cp "${GSON_JAR}" -d "${BUILD_TMP}" "${BUILD_TMP}/${CLASS_NAME}.java" \
                "${BUILD_TMP}/Arena.java" "${BUILD_TMP}/JavaRunner.java"
        }
        javac_bot || falhar_compilacao java

        (cd "${BUILD_TMP}" && jar -xf "${GSON_JAR}")
        rm -f "${BUILD_TMP}/${CLASS_NAME}.java" "${BUILD_TMP}/JavaRunner.java" "${BUILD_TMP}/Arena.java"
        jar cfe "${EXE_FILE}" JavaRunner -C "${BUILD_TMP}" .
        chmod +x "${EXE_FILE}"
        mostrar_avisos
        ;;

    *)
        echo "Extensao nao suportada: .${EXT}" >&2
        exit 1
        ;;
esac
