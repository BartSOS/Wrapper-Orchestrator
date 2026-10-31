# Bot de referência da fase 03_percurso: passa com 1 balão.
# Guarda o próximo ponto certo, mas para 70 ticks em cada ponto antes de seguir.
import math

proximo = 0
espera = 0


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d


def pensar(estado):
    global proximo, espera
    parado = {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    if espera > 0:
        espera -= 1
        return parado
    eu = estado["self"]
    pontos = estado["objetivo"]["pontos"]
    if proximo < len(pontos):
        p = pontos[proximo]
        if math.hypot(p["x"] - eu["x"], p["y"] - eu["y"]) <= p["raio"] * 0.8:
            proximo += 1
            espera = 70
            return parado
    if proximo >= len(pontos):
        return parado
    p = pontos[proximo]
    falta = diferenca_angulo(eu["angle"], angulo_para(eu["x"], eu["y"], p["x"], p["y"]))
    turn = max(-1.0, min(1.0, falta / 9.0))
    throttle = 1.0 if abs(falta) < 45 else 0.0
    return {"throttle": throttle, "turn": turn, "shoot": False, "shield": False}
