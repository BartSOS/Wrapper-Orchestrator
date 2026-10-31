# Bot de referência da fase 05_mira_andando: passa com 3 balões.
# Chassi vai até B (como na fase 1) enquanto a torre atira no alvo mais perto (como na fase 4).
import math

atirados = {}   # id do alvo -> tick em que o tiro já deve ter chegado


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d


def pensar(estado):
    eu = estado["self"]
    tick = estado["tick"]
    acao = {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    # Chassi: até o ponto B
    b = estado["objetivo"]["pontos"][0]
    if math.hypot(b["x"] - eu["x"], b["y"] - eu["y"]) > b["raio"] * 0.5:
        falta = diferenca_angulo(eu["angle"], angulo_para(eu["x"], eu["y"], b["x"], b["y"]))
        acao["turn"] = max(-1.0, min(1.0, falta / 9.0))
        acao["throttle"] = 1.0 if abs(falta) < 45 else 0.0
    # Torre: alvo mais perto em que não há tiro a caminho
    candidatos = [a for a in estado["enemies"] if tick > atirados.get(a["id"], -1)]
    if candidatos:
        alvo = min(candidatos, key=lambda a: math.hypot(a["x"] - eu["x"], a["y"] - eu["y"]))
        acao["turret_angle"] = angulo_para(eu["x"], eu["y"], alvo["x"], alvo["y"])
        if eu["cooldown"] <= 1e-9 and eu["energy"] >= 20:
            acao["shoot"] = True
            voo = math.hypot(alvo["x"] - eu["x"], alvo["y"] - eu["y"]) / 30.0
            atirados[alvo["id"]] = tick + int(voo) + 4
    return acao
