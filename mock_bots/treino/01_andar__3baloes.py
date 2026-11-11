# Bot de referência da fase 01_andar: passa com 3 balões.
# Gira no lugar até o chassi apontar para o ponto e anda com mover = FRENTE.
from arena import *
import math


def pensar(estado):
    meu = estado.eu
    ponto = estado.pontos[0]
    para_o_ponto = angulo_para(meu.x, meu.y, ponto.x, ponto.y)
    falta = diferenca_angulo(meu.angulo_chassi, para_o_ponto)
    acao = Acao()
    acao.direcao = para_o_ponto
    acao.mover = FRENTE if abs(falta) < 45 else PARADO
    return acao


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
