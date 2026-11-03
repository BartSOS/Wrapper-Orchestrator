# =============================================================================
# Bot de isolamento: fork_bomb (ver LEIA.md)
# =============================================================================
# No tick 0 tenta criar 256 processos. Barrado (ISOLAMENTO-OK) se o fork for recusado antes
# disso (ulimit -u do modo isolado ou pids do contêiner); ISOLAMENTO-FALHOU se criar todos.
# Os filhos só dormem e são encerrados logo depois. Em seguida joga parado.
# Usa sys.modules["os"] (o wrapper da arena já importou o os): o checar_python barra só o
# "import os"; quem segura o bot de verdade é o isolamento.
# =============================================================================
import signal
import sys
import time

_os = sys.modules["os"]
CASO = "fork_bomb"
LIMITE = 256
_feito = False


def _ataque():
    filhos = []
    erro = ""
    try:
        for _ in range(LIMITE):
            try:
                pid = _os.fork()
            except OSError as e:
                erro = f"fork recusado depois de {len(filhos)} processos: {e.strerror}"
                break
            if pid == 0:
                # Filho: não escreve nada (o canal com a engine é herdado); dorme e sai
                try:
                    time.sleep(10)
                finally:
                    _os._exit(0)
            filhos.append(pid)
    finally:
        for pid in filhos:
            try:
                _os.kill(pid, signal.SIGKILL)
            except OSError:
                pass
        for pid in filhos:
            try:
                _os.waitpid(pid, 0)
            except OSError:
                pass
    return len(filhos) >= LIMITE, erro or f"{len(filhos)} processos criados"


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
