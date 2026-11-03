# =============================================================================
# Bot de isolamento: ler_outro_bot (ver LEIA.md)
# =============================================================================
# No tick 0 procura arquivos de outros bots e códigos das equipes:
#   - caminhos de outros bots que aparecem no /proc/<pid>/cmdline (que é público);
#   - /proc/<pid>/cwd, /proc/<pid>/environ e /proc/<pid>/fd dos outros processos;
#   - a partir do próprio diretório, de "..", "../.." etc. (até 4 níveis), inclusive submissoes/.
# ISOLAMENTO-FALHOU se ler 1 byte de um arquivo de outro bot (build/bots/<outro>/...), de um
# código-fonte fora das pastas públicas (wrappers/, libs/, engine/nlohmann/) ou de qualquer
# coisa dentro de submissoes/, ou se abrir o cwd/environ/fd de outro processo.
# ISOLAMENTO-OK se tudo foi recusado. Em seguida joga parado.
# =============================================================================
import re
import stat
import sys

_os = sys.modules["os"]
CASO = "ler_outro_bot"
FONTES = (".py", ".c", ".cpp", ".java", ".jar")
PUBLICAS = ("/wrappers/", "/libs/", "/nlohmann/")
MAX_ENTRADAS = 4000
_feito = False


def _ler_um_byte(caminho):
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


def _proprio(p, meu_dir):
    return p == meu_dir or p.startswith(meu_dir + "/")


def _sensivel(p):
    """Arquivo que um bot não pode ler: de outro bot, das submissões ou código fora das pastas públicas."""
    if "/build/bots/" in p or "/submissoes/" in p:
        return True
    return p.endswith(FONTES) and not any(x in p for x in PUBLICAS)


def _meu_grupo():
    try:
        return _os.getpgid(0)
    except OSError:
        return -1


def _outros_pids():
    grupo, eu = _meu_grupo(), _os.getpid()
    for nome in _os.listdir("/proc"):
        if not nome.isdigit():
            continue
        pid = int(nome)
        if pid in (1, eu):
            continue
        try:
            if _os.getpgid(pid) == grupo:
                continue
        except OSError:
            continue
        yield pid


def _ataque():
    # O que é meu: no modo isolado, o diretório do bot (build/bots/t<N>/, binário "bot");
    # fora dele, só o próprio executável (build/bots/<equipe>)
    exe = _os.path.realpath(sys.argv[0]) if sys.argv and sys.argv[0] else _os.path.realpath(_os.getcwd())
    meu_dir = _os.path.dirname(exe) if _os.path.basename(exe) == "bot" else exe
    lidos, abertos = [], []

    # 1. /proc: caminhos dos outros bots (cmdline é público) e cwd/environ/fd dos outros processos
    candidatos = set()
    for pid in _outros_pids():
        try:
            with open(f"/proc/{pid}/cmdline", "rb") as f:
                texto = f.read().replace(b"\0", b" ").decode("utf-8", "replace")
        except OSError:
            continue
        for p in re.findall(r"/[^\s\"';]+", texto):
            if "/build/bots/" in p:
                candidatos.add(p)
        for extra in ("cwd", "fd"):
            try:
                _os.listdir(f"/proc/{pid}/{extra}")
                abertos.append(f"/proc/{pid}/{extra}")
            except OSError:
                pass
        if _ler_um_byte(f"/proc/{pid}/environ"):
            abertos.append(f"/proc/{pid}/environ")
    for p in sorted(candidatos):
        p = _os.path.realpath(p)
        if _proprio(p, meu_dir):
            continue
        if _ler_um_byte(p):
            lidos.append(p)
        for base in (p, _os.path.dirname(p)):
            try:
                for e in _os.listdir(base):
                    q = _os.path.join(base, e)
                    if not _proprio(q, meu_dir) and _ler_um_byte(q):
                        lidos.append(q)
            except OSError:
                pass

    # 2. Subindo a partir do próprio diretório (e submissoes/ de cada nível)
    vistos = 0
    nivel = _os.path.join(_os.path.realpath(_os.getcwd()), "x")   # o 1o dirname() devolve o próprio cwd
    for _ in range(5):
        nivel = _os.path.dirname(nivel)
        for raiz in (nivel, _os.path.join(nivel, "submissoes")):
            for r, dirs, arqs in _os.walk(raiz):
                if _proprio(r, meu_dir):
                    dirs[:] = []
                    continue
                dirs[:] = [d for d in dirs if d not in ("proc", "sys", "dev")]
                for a in arqs:
                    vistos += 1
                    q = _os.path.join(r, a)
                    if _sensivel(q) and not _proprio(q, meu_dir) and _ler_um_byte(q):
                        lidos.append(q)
                    if vistos >= MAX_ENTRADAS:
                        break
                if vistos >= MAX_ENTRADAS or len(lidos) >= 5:
                    break

    lidos = sorted(set(lidos))
    if lidos or abertos:
        return True, "leu " + ", ".join((lidos + abertos)[:5])
    return False, f"nada de outros bots legivel ({len(candidatos)} caminho(s) de bots no /proc, {vistos} arquivo(s) vistos)"


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
