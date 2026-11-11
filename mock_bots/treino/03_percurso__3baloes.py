# Bot de referência da fase 03_percurso: passa com 3 balões.
# Guarda o índice do próximo ponto numa variável global e anda até ele.
from arena import *
import math

proximo = 0


def pensar(estado):
    global proximo
    meu = estado.eu
    pontos = estado.pontos
    acao = Acao()
    if proximo < len(pontos):
        p = pontos[proximo]
        if distancia(meu.x, meu.y, p.x, p.y) <= p.raio * 0.8:
            proximo += 1
    if proximo >= len(pontos):
        return acao                                # PARADO
    p = pontos[proximo]
    para_o_ponto = angulo_para(meu.x, meu.y, p.x, p.y)
    falta = diferenca_angulo(meu.angulo_chassi, para_o_ponto)
    acao.direcao = para_o_ponto
    acao.mover = FRENTE if abs(falta) < 45 else PARADO
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
