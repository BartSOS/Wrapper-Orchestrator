# Bot de referência da fase 02_parar: passa com 1 balão.
# Para certo no ponto, mas hesita: espera 170 frames antes de sair.
from arena import *
import math


def pensar(estado):
    acao = Acao()
    if estado.frame < 170:
        return acao                                # PARADO
    meu = estado.eu
    ponto = estado.pontos[0]
    dist = distancia(meu.x, meu.y, ponto.x, ponto.y)
    if dist <= ponto.raio * 0.5:
        return acao
    para_o_ponto = angulo_para(meu.x, meu.y, ponto.x, ponto.y)
    falta = diferenca_angulo(meu.angulo_chassi, para_o_ponto)
    acao.direcao = para_o_ponto
    acao.mover = min(1.0, dist / 5.0) if abs(falta) < 20 else PARADO
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
