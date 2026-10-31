# Bot de referência da fase 04_mira_parado: passa com 1 balão.
# Erra o primeiro tiro em cada alvo (mira 8 graus ao lado) e acerta na segunda tentativa:
# 6 tiros para 3 alvos.
import math

atirados = {}   # id do alvo -> tick do último tiro
tentativas = {}  # id do alvo -> tiros dados


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def pensar(estado):
    eu = estado["self"]
    tick = estado["tick"]
    candidatos = [a for a in estado["enemies"] if tick - atirados.get(a["id"], -999) > 20]
    acao = {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    if not candidatos:
        return acao
    alvo = min(candidatos, key=lambda a: math.hypot(a["x"] - eu["x"], a["y"] - eu["y"]))
    erro = 8.0 if tentativas.get(alvo["id"], 0) == 0 else 0.0
    acao["turret_angle"] = (angulo_para(eu["x"], eu["y"], alvo["x"], alvo["y"]) + erro) % 360
    if eu["cooldown"] <= 1e-9 and eu["energy"] >= 20:
        acao["shoot"] = True
        atirados[alvo["id"]] = tick
        tentativas[alvo["id"]] = tentativas.get(alvo["id"], 0) + 1
    return acao
