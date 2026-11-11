# =============================================================================
# INFRAESTRUTURA DE I/O E COMUNICAÇÃO (WRAPPER OFICIAL DO BOCA)
# O build_bot.sh monta o executável do bot assim:
#     #!/usr/bin/env python3
#     _FONTE_DA_EQUIPE = "<código da equipe em base64>"
#     _FONTE_ARENA = "<wrappers/arena.py em base64>"
#     <este arquivo>
# A saída é protegida ANTES de o código da equipe rodar: um print no nível do
# arquivo (ou em sys.__stdout__) vai para o stderr e nunca chega à engine.
# O arena.py vira o módulo "arena" (o "from arena import *" da equipe). Cada
# estado chega ao pensar() como arena.Estado (nomes em português, e também o
# dict original), e a Acao devolvida é traduzida para o protocolo da engine
# (mover -> throttle, direcao -> turn, ...). Chaves antigas (throttle, turn...)
# continuam aceitas, com um [AVISO] no stderr.
# Toda linha de estado recebe exatamente uma resposta: se o pensar() falhar ou
# devolver algo que não é uma Acao (nem um dict), a resposta é a ação vazia {}.
# =============================================================================

import base64
import json
import os
import sys
import traceback
import types

# 1. Proteção da saída: a engine lê o fd 1. Ele passa a ser o stderr para todo o
#    resto do processo, e o fd original fica privado, só para as respostas.
_FD_ENGINE = os.dup(1)
os.dup2(2, 1)
sys.stdout = sys.stderr
_saida_engine = os.fdopen(_FD_ENGINE, "w", encoding="utf-8", buffering=1)


def enviar_comando(texto):
    _saida_engine.write(texto + "\n")
    _saida_engine.flush()


def _erro(titulo):
    print(f"[ERRO NO BOT PYTHON] {titulo}", file=sys.stderr)
    traceback.print_exc(file=sys.stderr)
    sys.stderr.flush()


# 2. Arquivo de apoio (arena.py), registrado como módulo "arena" antes do código da equipe
def _carregar_arena():
    modulo = types.ModuleType("arena")
    modulo.__file__ = "arena.py"
    sys.modules["arena"] = modulo
    fonte = base64.b64decode(_FONTE_ARENA).decode("utf-8-sig")
    exec(compile(fonte, "arena.py", "exec"), modulo.__dict__)
    return modulo


# 3. Código da equipe, num módulo de verdade ("bot"), com o nome de arquivo e os
#    números de linha originais nos tracebacks
def _carregar_bot():
    modulo = types.ModuleType("bot")
    modulo.__file__ = "bot.py"
    sys.modules["bot"] = modulo
    try:
        fonte = base64.b64decode(_FONTE_DA_EQUIPE).decode("utf-8-sig")
        exec(compile(fonte, "bot.py", "exec"), modulo.__dict__)
    except BaseException:  # noqa: BLE001 - o bot não pode derrubar o canal com a engine
        _erro("ao carregar o arquivo do bot")
    return modulo.__dict__.get("pensar")


# 4. Laço: uma resposta por estado, sempre
def main_loop(arena, pensar):
    for linha in sys.stdin:
        linha = linha.strip()
        if not linha:
            continue
        texto = "{}"
        try:
            estado = arena.Estado(json.loads(linha))
            if pensar is None:
                raise NameError("a função pensar(estado) não foi definida")
            acao = pensar(estado)
            texto = json.dumps(arena._traduzir_acao(acao, estado), allow_nan=False)
        except (Exception, SystemExit):  # noqa: BLE001
            _erro("no pensar(): neste frame o tanque ficou parado")
        enviar_comando(texto)


_ARENA = _carregar_arena()
main_loop(_ARENA, _carregar_bot())
