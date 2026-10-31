# Bot de referência da fase 03_percurso: falha de propósito.
# Erro comum: não guarda estado e vai sempre ao ponto mais perto. Chega ao P4 (o do centro,
# fora da vez) e fica nele, porque o mais perto passa a ser ele mesmo.
import math


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d


def pensar(estado):
    eu = estado["self"]
    pontos = estado["objetivo"]["pontos"]
    p = min(pontos, key=lambda q: math.hypot(q["x"] - eu["x"], q["y"] - eu["y"]))
    if math.hypot(p["x"] - eu["x"], p["y"] - eu["y"]) <= p["raio"] * 0.5:
        return {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    falta = diferenca_angulo(eu["angle"], angulo_para(eu["x"], eu["y"], p["x"], p["y"]))
    turn = max(-1.0, min(1.0, falta / 9.0))
    throttle = 1.0 if abs(falta) < 45 else 0.0
    return {"throttle": throttle, "turn": turn, "shoot": False, "shield": False}
