# Bot de referência da fase 02_parar: falha de propósito.
# Erro comum: throttle 1 o tempo todo, girando enquanto anda. Passa do ponto, faz a curva
# (o raio da curva é maior que o do ponto) e fica rodando em volta dele.
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
    throttle = 1.0   # ERRO: nunca reduz perto do ponto
    return {"throttle": throttle, "turn": turn, "shoot": False, "shield": False}
