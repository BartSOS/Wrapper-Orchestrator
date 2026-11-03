# =============================================================================
# Bot de isolamento: ler_fila (ver LEIA.md)
# =============================================================================
# No tick 0 tenta listar e gravar na fila da arena (/fila no executor, /var/www/boca/arena
# no web). ISOLAMENTO-FALHOU se conseguir listar ou criar um arquivo em alguma delas;
# ISOLAMENTO-OK se não existem ou foram recusadas. Em seguida joga parado.
# =============================================================================
import sys

_os = sys.modules["os"]
CASO = "ler_fila"
ALVOS = ["/fila", "/var/www/boca/arena"]
_feito = False


def _ataque():
    falhas, detalhes = [], []
    for alvo in ALVOS:
        if not _os.path.exists(alvo):
            detalhes.append(f"{alvo}: nao existe")
            continue
        try:
            entradas = _os.listdir(alvo)
            falhas.append(f"listou {alvo} ({len(entradas)} entradas)")
        except OSError as e:
            detalhes.append(f"{alvo}: listar recusado ({e.strerror})")
        teste = _os.path.join(alvo, f".isolamento_{_os.getpid()}")
        try:
            fd = _os.open(teste, _os.O_WRONLY | _os.O_CREAT | _os.O_EXCL, 0o600)
            _os.close(fd)
            _os.unlink(teste)
            falhas.append(f"gravou em {alvo}")
        except OSError as e:
            detalhes.append(f"{alvo}: gravar recusado ({e.strerror})")
    if falhas:
        return True, "; ".join(falhas)
    return False, "; ".join(detalhes)


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
