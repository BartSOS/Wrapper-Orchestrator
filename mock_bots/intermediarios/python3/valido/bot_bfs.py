# Bot de teste (Python 3) - Fuga (kiting): foge do inimigo mais próximo atirando para trás.
from arena import *
import math


def pensar(estado):
    meu = estado.eu
    acao = Acao()
    acao.mover = 0.8
    acao.direcao = meu.angulo_chassi + 4.5      # sem inimigo: anda em círculo

    mais_proximo = None
    menor = float("inf")
    for inimigo in estado.inimigos:
        d = distancia(meu.x, meu.y, inimigo.x, inimigo.y)
        if d < menor:
            menor = d
            mais_proximo = inimigo

    if mais_proximo is not None:
        para_o_inimigo = angulo_para(meu.x, meu.y, mais_proximo.x, mais_proximo.y)
        acao.angulo_torre = para_o_inimigo       # mira no inimigo
        acao.direcao = (para_o_inimigo + 180) % 360  # e anda para o lado oposto
        acao.mover = FRENTE
        if menor < 600 and meu.frames_para_atirar == 0 and meu.energia >= 15:
            acao.atirar = True
        if menor < 200 and meu.energia > 30:     # escudo de pânico
            acao.ligar_escudo = True
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360
