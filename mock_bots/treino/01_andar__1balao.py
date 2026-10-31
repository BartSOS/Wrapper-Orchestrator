# Bot de referência da fase 01_andar: passa com 1 balão.
# Faz o caminho certo, mas devagar: gira parado e anda com throttle 0.5.
import math


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d


def pensar(estado):
    eu = estado["self"]
    ponto = estado["objetivo"]["pontos"][0]
    falta = diferenca_angulo(eu["angle"], angulo_para(eu["x"], eu["y"], ponto["x"], ponto["y"]))
    turn = max(-1.0, min(1.0, falta / 9.0))
    throttle = 0.5 if abs(falta) < 5 else 0.0
    return {"throttle": throttle, "turn": turn, "shoot": False, "shield": False}
