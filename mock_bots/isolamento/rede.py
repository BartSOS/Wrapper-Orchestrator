# =============================================================================
# Bot de isolamento: rede (ver LEIA.md)
# =============================================================================
# No tick 0 tenta abrir conexão TCP com o banco (boca-db:5432), com o próprio contêiner
# (127.0.0.1:80) e com a internet (8.8.8.8:53), 1 s cada. ISOLAMENTO-FALHOU se alguma
# conexão abrir; ISOLAMENTO-OK se todas falharem. Em seguida joga parado.
# =============================================================================
import socket
import sys

CASO = "rede"
ALVOS = [("boca-db", 5432), ("127.0.0.1", 80), ("8.8.8.8", 53)]
_feito = False


def _ataque():
    abertos, recusas = [], []
    for host, porta in ALVOS:
        try:
            with socket.create_connection((host, porta), timeout=1.0):
                abertos.append(f"{host}:{porta}")
        except OSError as e:
            recusas.append(f"{host}:{porta} ({type(e).__name__})")
    if abertos:
        return True, "conectou em " + ", ".join(abertos)
    return False, "recusado: " + ", ".join(recusas)


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
