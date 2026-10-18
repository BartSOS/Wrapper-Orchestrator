#!/bin/bash
# ==============================================================================
# Game Jam PET - Validador BOCA: lógica comum dos pacotes de linguagem
# ==============================================================================
# Carregado (source) pelo compile.sh de cada pacote, que define LINGUAGEM
# (c | cpp | java | py). O validador usa a MESMA montagem da partida
# (wrappers/build_bot.sh): quem recebe YES aqui compila na arena.
#
# Uso (pelo fvalidator.php do BOCA, com o diretório do pacote como cwd):
#   compile.sh <fonte> <saida>     valida uma submissão (0 = YES, outro = Compilation Error)
#   compile.sh --preparar          pré-compila os wrappers no cache (chamado pelo jail/init.sh)
#
# Isolamento:
#   - a compilação roda num diretório temporário próprio (cwd), nunca na pasta do pacote;
#   - como root (jail), é rebaixada para o usuário sem privilégio (padrão: bocajail);
#   - tempo máximo próprio (padrão 50 s), abaixo do timeout do PHP (60 s).
# Variáveis: TOPCOM_WRAPPERS_BUILD (cache; padrão /var/cache/gamejam-validador),
#            GAMEJAM_USUARIO_VALIDADOR, GAMEJAM_TEMPO_COMPILACAO.
# ==============================================================================

PKG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_BOT="${PKG_DIR}/wrappers/build_bot.sh"
USUARIO="${GAMEJAM_USUARIO_VALIDADOR:-bocajail}"
LIMITE="${GAMEJAM_TEMPO_COMPILACAO:-50}"
export TOPCOM_WRAPPERS_BUILD="${TOPCOM_WRAPPERS_BUILD:-/var/cache/gamejam-validador}"

case "${LINGUAGEM:-}" in
    c|cpp|java|py) ;;
    *) echo "ERRO INTERNO: pacote de linguagem sem LINGUAGEM definida." >&2; exit 1 ;;
esac

if [[ ! -f "$BUILD_BOT" ]]; then
    echo "ERRO INTERNO: wrappers/build_bot.sh nao encontrado no pacote da linguagem." >&2
    exit 1
fi

# Roda como root dentro do jail? Então rebaixa o privilégio da compilação.
rebaixar=0
if [[ "$(id -u)" == "0" ]] && id "$USUARIO" >/dev/null 2>&1; then
    rebaixar=1
fi

# executar_como_usuario <dir_trabalho> <comando...>
executar_como_usuario() {
    local trabalho="$1"; shift
    if [[ $rebaixar -eq 1 ]]; then
        env HOME="$trabalho" TMPDIR="$trabalho" setpriv --reuid="$USUARIO" --regid="$(id -g "$USUARIO")" \
            --init-groups --no-new-privs "$@"
    else
        "$@"
    fi
}

# O cache pertence ao usuário da compilação: ele mesmo o reconstrói se a assinatura mudar
preparar_cache() {
    mkdir -p "$TOPCOM_WRAPPERS_BUILD" 2>/dev/null || true
    if [[ $rebaixar -eq 1 ]]; then
        chown "$USUARIO" "$TOPCOM_WRAPPERS_BUILD" 2>/dev/null || true
        chown -R "$USUARIO" "$TOPCOM_WRAPPERS_BUILD" 2>/dev/null || true
    fi
    chmod 755 "$TOPCOM_WRAPPERS_BUILD" 2>/dev/null || true
}

novo_trabalho() {
    local t
    t="$(mktemp -d /tmp/gamejam_val_XXXXXX)" || exit 1
    chmod 700 "$t"
    if [[ $rebaixar -eq 1 ]]; then
        chown "$USUARIO" "$t"
    fi
    echo "$t"
}

# --- Modo de preparação ------------------------------------------------------
if [[ "${1:-}" == "--preparar" ]]; then
    preparar_cache
    TRABALHO="$(novo_trabalho)"
    trap 'rm -rf -- "${TRABALHO:?}"' EXIT
    ( cd "$TRABALHO" && executar_como_usuario "$TRABALHO" timeout -k 3 "${GAMEJAM_TEMPO_PREPARO:-300}" \
        bash "$BUILD_BOT" --preparar "$LINGUAGEM" )
    rc=$?
    if [[ $rc -eq 0 ]]; then
        echo "[validador] ${LINGUAGEM}: wrappers prontos em ${TOPCOM_WRAPPERS_BUILD}"
    else
        echo "[validador] ${LINGUAGEM}: falha ao preparar os wrappers (codigo ${rc})" >&2
    fi
    exit $rc
fi

# --- Modo de validação -------------------------------------------------------
FONTE="${1:-}"
if [[ -z "$FONTE" || ! -f "$FONTE" ]]; then
    echo "ERRO INTERNO: arquivo-fonte nao informado ou inexistente." >&2
    exit 1
fi

preparar_cache
TRABALHO="$(novo_trabalho)"
trap 'rm -rf -- "${TRABALHO:?}"' EXIT

# Nome do arquivo: o original (mensagens do compilador mais claras) se a extensão bater com a linguagem
NOME="$(basename -- "$FONTE")"
EXT_ESPERADA="$LINGUAGEM"
EXT_NOME="$(echo "${NOME##*.}" | tr '[:upper:]' '[:lower:]')"
if [[ "$NOME" != *.* || "$EXT_NOME" != "$EXT_ESPERADA" ]]; then
    NOME="solution.${EXT_ESPERADA}"
fi
cp -- "$FONTE" "${TRABALHO}/${NOME}"
if [[ $rebaixar -eq 1 ]]; then
    chown -R "$USUARIO" "$TRABALHO"
fi

( cd "$TRABALHO" && executar_como_usuario "$TRABALHO" timeout -k 3 "$LIMITE" \
    bash "$BUILD_BOT" "${TRABALHO}/${NOME}" "${TRABALHO}/exec_out" )
exit $?
