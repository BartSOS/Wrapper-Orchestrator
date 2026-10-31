# Bot de referência da fase 02_parar: passa com 1 balão.
# Para certo no ponto, mas hesita: espera 170 ticks antes de sair.
import math


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d


def pensar(estado):
    if estado["tick"] < 170:
        return {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    eu = estado["self"]
    ponto = estado["objetivo"]["pontos"][0]
    dist = math.hypot(ponto["x"] - eu["x"], ponto["y"] - eu["y"])
    if dist <= ponto["raio"] * 0.5:
        return {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    falta = diferenca_angulo(eu["angle"], angulo_para(eu["x"], eu["y"], ponto["x"], ponto["y"]))
    turn = max(-1.0, min(1.0, falta / 9.0))
    throttle = min(1.0, dist / 5.0) if abs(falta) < 20 else 0.0
    return {"throttle": throttle, "turn": turn, "shoot": False, "shield": False}
