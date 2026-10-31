# Bot de referência da fase 04_mira_parado: falha de propósito.
# Erro comum: manda para turret_angle o valor de atan2 em RADIANOS. A torre fica quase em
# 0 grau e os tiros saem para o lado errado.
import math


def pensar(estado):
    eu = estado["self"]
    acao = {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    if not estado["enemies"]:
        return acao
    alvo = estado["enemies"][0]
    acao["turret_angle"] = math.atan2(alvo["y"] - eu["y"], alvo["x"] - eu["x"])   # ERRO: faltou converter para graus
    acao["shoot"] = eu["cooldown"] <= 1e-9 and eu["energy"] >= 20
    return acao
