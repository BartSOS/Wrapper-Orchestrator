# =============================================================================
# Bot de isolamento: ler_private (ver LEIA.md)
# =============================================================================
# No tick 0 tenta ler /var/www/boca/src/private/conf.php (senha do banco) e qualquer arquivo
# sob /var/www. ISOLAMENTO-FALHOU se ler pelo menos 1 byte de algum; ISOLAMENTO-OK se nada
# existe ou tudo foi recusado. Em seguida joga parado.
# =============================================================================
import stat
import sys

_os = sys.modules["os"]
CASO = "ler_private"
ALVOS = ["/var/www/boca/src/private/conf.php"]
RAIZ = "/var/www"
MAX_ENTRADAS = 3000
_feito = False


def _ler_um_byte(caminho):
    """True se conseguiu ler 1 byte de um arquivo comum (sem bloquear em FIFO)."""
    try:
        fd = _os.open(caminho, _os.O_RDONLY | _os.O_NONBLOCK)
    except OSError:
        return False
    try:
        if not stat.S_ISREG(_os.fstat(fd).st_mode):
            return False
        _os.read(fd, 1)
        return True
    except OSError:
        return False
    finally:
        _os.close(fd)


def _ataque():
    lidos = [p for p in ALVOS if _ler_um_byte(p)]
    vistos = 0
    for raiz, _dirs, arqs in _os.walk(RAIZ):
        for a in arqs:
            vistos += 1
            p = _os.path.join(raiz, a)
            if p not in lidos and _ler_um_byte(p):
                lidos.append(p)
            if len(lidos) >= 3 or vistos >= MAX_ENTRADAS:
                break
        if len(lidos) >= 3 or vistos >= MAX_ENTRADAS:
            break
    if lidos:
        return True, "leu " + ", ".join(lidos)
    return False, f"nada legivel ({vistos} arquivo(s) vistos sob {RAIZ})"


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
