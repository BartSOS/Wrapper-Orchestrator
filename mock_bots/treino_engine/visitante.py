# Bot de teste do modo cenário (frente engine): visita os pontos da fase (estado.pontos)
# na ordem e depois mira e atira no inimigo mais próximo. Sem pontos (torneio), só atira.
from arena import *
import math

visitados = set()


def pensar(estado):
    meu = estado.eu
    acao = Acao()

    for p in estado.pontos:
        if p.id in visitados:
            continue
        if distancia(meu.x, meu.y, p.x, p.y) <= p.raio * 0.6:
            visitados.add(p.id)
            continue
        para_o_ponto = angulo_para(meu.x, meu.y, p.x, p.y)
        acao.direcao = para_o_ponto
        acao.mover = FRENTE if abs(diferenca_angulo(meu.angulo_chassi, para_o_ponto)) < 30 else 0.2
        return acao

    if estado.inimigos:
        alvo = min(estado.inimigos, key=lambda i: distancia(meu.x, meu.y, i.x, i.y))
        acao.angulo_torre = angulo_para(meu.x, meu.y, alvo.x, alvo.y)
        acao.atirar = True
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
