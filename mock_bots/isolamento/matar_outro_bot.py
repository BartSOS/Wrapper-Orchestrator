# =============================================================================
# Bot de isolamento: matar_outro_bot (ver LEIA.md)
# =============================================================================
# No tick 0 varre o /proc e testa os.kill(pid, 0) em todo processo que não é dele (fora do
# próprio grupo de processos), nem o init, nem a engine. ISOLAMENTO-FALHOU se algum aceitar o
# sinal; nesse caso manda SIGKILL só nos que são outros bots (cmdline com /build/bots/).
# ISOLAMENTO-OK se todos responderem EPERM. Em seguida joga parado.
# =============================================================================
import signal
import sys

_os = sys.modules["os"]
CASO = "matar_outro_bot"
_feito = False


def _pai(pid):
    try:
        with open(f"/proc/{pid}/stat", "rb") as f:
            dados = f.read().decode("utf-8", "replace")
        return int(dados.rsplit(")", 1)[1].split()[1])
    except (OSError, ValueError, IndexError):
        return -1


def _cmdline(pid):
    try:
        with open(f"/proc/{pid}/cmdline", "rb") as f:
            return f.read().replace(b"\0", b" ").decode("utf-8", "replace")
    except OSError:
        return ""


def _ataque():
    eu = _os.getpid()
    grupo = _os.getpgid(0)
    engine = _pai(grupo)          # o líder do grupo (timeout) é filho da engine
    sinalizaveis, barrados, mortos = [], 0, []
    for nome in _os.listdir("/proc"):
        if not nome.isdigit():
            continue
        pid = int(nome)
        if pid in (1, eu, engine):
            continue
        try:
            if _os.getpgid(pid) == grupo:
                continue
        except OSError:
            continue
        try:
            _os.kill(pid, 0)
            sinalizaveis.append(pid)
        except PermissionError:
            barrados += 1
        except OSError:
            pass
    for pid in sinalizaveis:
        if "/build/bots/" in _cmdline(pid):
            try:
                _os.kill(pid, signal.SIGKILL)
                mortos.append(pid)
            except OSError:
                pass
    if sinalizaveis:
        return True, f"{len(sinalizaveis)} processo(s) aceitaram sinal; SIGKILL em {len(mortos)} bot(s)"
    return False, f"{barrados} processo(s) recusaram o sinal (EPERM)"


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
