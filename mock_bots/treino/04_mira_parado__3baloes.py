# Bot de referência da fase 04_mira_parado: passa com 3 balões.
# Fica parado, aponta a torre (ângulo absoluto, em graus) para um alvo em que ainda não
# atirou e atira quando o cooldown zera e há energia.
import math

atirados = {}   # id do alvo -> tick do tiro


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def pensar(estado):
    eu = estado["self"]
    tick = estado["tick"]
    alvos = estado["enemies"]
    # Alvo ainda vivo 20 ticks depois do tiro: o tiro errou, pode tentar de novo
    candidatos = [a for a in alvos if tick - atirados.get(a["id"], -999) > 20]
    acao = {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    if not candidatos:
        return acao
    alvo = min(candidatos, key=lambda a: math.hypot(a["x"] - eu["x"], a["y"] - eu["y"]))
    acao["turret_angle"] = angulo_para(eu["x"], eu["y"], alvo["x"], alvo["y"])
    if eu["cooldown"] <= 1e-9 and eu["energy"] >= 20:
        acao["shoot"] = True
        atirados[alvo["id"]] = tick
    return acao
