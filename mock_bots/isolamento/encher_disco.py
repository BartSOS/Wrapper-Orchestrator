# =============================================================================
# Bot de isolamento: encher_disco (ver LEIA.md)
# =============================================================================
# No tick 0 grava arquivos de 32 MB em HOME, TMPDIR e no diretório atual até dar erro
# (ENOSPC, EFBIG...), até 3 GB no total, ou até o disco ficar com menos de 256 MB livres
# (margem de segurança). Depois apaga tudo o que gravou.
# ISOLAMENTO-FALHOU se conseguiu gravar 1 GB ou mais (não há limite por bot);
# ISOLAMENTO-OK se parou antes. No modo isolado o vigia do script.sh costuma encerrar o bot
# antes do fim (limite de disco por bot): aí não sai linha nenhuma e o log da partida mostra
# "[ISOLAMENTO] uid ...: ... passou do limite". Em seguida (se vivo) joga parado.
# NÃO rode fora do modo isolado: ele enche o disco de quem estiver rodando.
# =============================================================================
import sys

_os = sys.modules["os"]
CASO = "encher_disco"
BLOCO = b"\0" * (1 << 20)
POR_ARQUIVO_MB = 32
MAX_TOTAL_MB = 3 * 1024
LIMITE_FALHA_MB = 1024
MARGEM_LIVRE = 256 << 20
_feito = False


def _livre(d):
    try:
        st = _os.statvfs(d)
        return st.f_bavail * st.f_frsize
    except OSError:
        return 0


def _ataque():
    destinos = []
    for d in (_os.environ.get("HOME"), _os.environ.get("TMPDIR"), _os.getcwd()):
        if d and _os.path.isdir(d) and _os.path.realpath(d) not in [_os.path.realpath(x) for x in destinos]:
            destinos.append(d)
    gravados, total_mb, motivos = [], 0, []
    try:
        for d in destinos:
            parar_dir = False
            n = 0
            while not parar_dir and total_mb < MAX_TOTAL_MB:
                caminho = _os.path.join(d, f".encher_{_os.getpid()}_{n}")
                n += 1
                try:
                    fd = _os.open(caminho, _os.O_WRONLY | _os.O_CREAT | _os.O_TRUNC, 0o600)
                except OSError as e:
                    motivos.append(f"{d}: {e.strerror}")
                    break
                gravados.append(caminho)
                try:
                    for _ in range(POR_ARQUIVO_MB):
                        if total_mb >= MAX_TOTAL_MB:
                            break
                        if _livre(d) < MARGEM_LIVRE:
                            motivos.append(f"{d}: menos de 256 MB livres")
                            parar_dir = True
                            break
                        _os.write(fd, BLOCO)
                        total_mb += 1
                except OSError as e:
                    motivos.append(f"{d}: {e.strerror}")
                    parar_dir = True
                finally:
                    _os.close(fd)
    finally:
        for caminho in gravados:
            try:
                _os.unlink(caminho)
            except OSError:
                pass
    detalhe = f"gravou {total_mb} MB em {len(gravados)} arquivo(s)" + (f"; parou: {motivos[0]}" if motivos else "")
    return total_mb >= LIMITE_FALHA_MB, detalhe


def _relatar(conseguiu, detalhe):
    print(f"ISOLAMENTO-{'FALHOU' if conseguiu else 'OK'} {CASO}", file=sys.stderr, flush=True)
    print(f"  detalhe: {detalhe}", file=sys.stderr, flush=True)


def pensar(estado):
    global _feito
    if not _feito:
        _feito = True
        try:
            conseguiu, detalhe = _ataque()
        except Exception as e:  # noqa: BLE001
            conseguiu, detalhe = False, f"excecao {type(e).__name__}: {e}"
        _relatar(conseguiu, detalhe)
    return {}
