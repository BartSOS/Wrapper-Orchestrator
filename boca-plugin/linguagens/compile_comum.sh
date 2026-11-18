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
#   - funciona nos dois contêineres: no jail roda como root e rebaixa para bocajail; no web
#     (www-data, sem bocajail) roda como o usuário atual.
# Variáveis: TOPCOM_WRAPPERS_BUILD (cache; padrão /var/cache/gamejam-validador, ou
#            /tmp/gamejam-validador-<uid> quando o padrão não é gravável),
#            GAMEJAM_USUARIO_VALIDADOR, GAMEJAM_TEMPO_COMPILACAO.
# ==============================================================================

# Sem fork a mais: dirname, basename, tr e id viram expansoes do bash ($EUID, ${x%/*}, ${x,,}).
_origem="${BASH_SOURCE[0]}"
_dir=.
[[ "$_origem" == */* ]] && _dir="${_origem%/*}"
PKG_DIR="$(cd "${_dir:-/}" && pwd)"
BUILD_BOT="${PKG_DIR}/wrappers/build_bot.sh"
USUARIO="${GAMEJAM_USUARIO_VALIDADOR:-bocajail}"
LIMITE="${GAMEJAM_TEMPO_COMPILACAO:-50}"

# Cache dos wrappers pré-compilados. Padrão: /var/cache/gamejam-validador (jail: criado e entregue ao
# usuário sem privilégio). Onde não dá para criá-lo (web, rodando como www-data), usa um cache por
# usuário em /tmp, aquecido na primeira validação.
if [[ -z "${TOPCOM_WRAPPERS_BUILD:-}" ]]; then
    TOPCOM_WRAPPERS_BUILD="/var/cache/gamejam-validador"
    if ! { [[ -d "$TOPCOM_WRAPPERS_BUILD" ]] || mkdir -p "$TOPCOM_WRAPPERS_BUILD" 2>/dev/null; } \
        || [[ ! -w "$TOPCOM_WRAPPERS_BUILD" && $EUID -ne 0 ]]; then
        TOPCOM_WRAPPERS_BUILD="/tmp/gamejam-validador-$EUID"
    fi
fi
export TOPCOM_WRAPPERS_BUILD

# Modo de validacao rapida (ligador gold, JVM enxuta no javac/jar): so tem efeito se o wrappers/build_bot.sh
# do pacote o entender, e nao muda veredito nem mensagem. GAMEJAM_VALIDACAO_RAPIDA=0 desliga.
export GAMEJAM_VALIDACAO_RAPIDA="${GAMEJAM_VALIDACAO_RAPIDA:-1}"

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
GID_USUARIO=""
if [[ $EUID -eq 0 ]] && GID_USUARIO="$(id -g "$USUARIO" 2>/dev/null)"; then
    rebaixar=1
fi

# executar_como_usuario <dir_trabalho> <comando...>
executar_como_usuario() {
    local trabalho="$1"; shift
    if [[ $rebaixar -eq 1 ]]; then
        env HOME="$trabalho" TMPDIR="$trabalho" setpriv --reuid="$USUARIO" --regid="$GID_USUARIO" \
            --init-groups --no-new-privs "$@"
    else
        "$@"
    fi
}

# O cache pertence ao usuário da compilação: ele mesmo o reconstrói se a assinatura mudar
preparar_cache() {
    [[ -d "$TOPCOM_WRAPPERS_BUILD" ]] || mkdir -p "$TOPCOM_WRAPPERS_BUILD" 2>/dev/null || true
    if [[ $rebaixar -eq 1 ]]; then
        chown -R "$USUARIO" "$TOPCOM_WRAPPERS_BUILD" 2>/dev/null || true
    fi
    chmod 755 "$TOPCOM_WRAPPERS_BUILD" 2>/dev/null || true
}

# Define TRABALHO (sem subshell); o mktemp -d ja cria o diretorio com modo 700.
novo_trabalho() {
    TRABALHO="$(mktemp -d /tmp/gamejam_val_XXXXXX)" || exit 1
    if [[ $rebaixar -eq 1 ]]; then
        chown "$USUARIO" "$TRABALHO"
    fi
}

# --- Modo de preparação ------------------------------------------------------
if [[ "${1:-}" == "--preparar" ]]; then
    preparar_cache
    novo_trabalho
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
novo_trabalho
trap 'rm -rf -- "${TRABALHO:?}"' EXIT

# Nome do arquivo: o original (mensagens do compilador mais claras) se a extensão bater com a linguagem
NOME="${FONTE##*/}"
EXT_ESPERADA="$LINGUAGEM"
EXT_NOME="${NOME##*.}"
EXT_NOME="${EXT_NOME,,}"
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
