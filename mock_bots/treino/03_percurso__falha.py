# Bot de referência da fase 03_percurso: falha de propósito.
# Erro comum: não guarda estado e vai sempre ao ponto mais perto. Chega ao P4 (o do centro,
# fora da vez) e fica nele, porque o mais perto passa a ser ele mesmo.
from arena import *
import math


def pensar(estado):
    meu = estado.eu
    acao = Acao()
    p = min(estado.pontos, key=lambda q: distancia(meu.x, meu.y, q.x, q.y))
    if distancia(meu.x, meu.y, p.x, p.y) <= p.raio * 0.5:
        return acao                                # PARADO
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
