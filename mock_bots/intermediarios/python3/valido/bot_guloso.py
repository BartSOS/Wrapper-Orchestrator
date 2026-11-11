# Bot de teste (Python 3) - Guloso: corre para o item mais próximo; sem item, vai para
# cima do primeiro inimigo atirando sem parar.
from arena import *
import math


def pensar(estado):
    meu = estado.eu
    acao = Acao()
    acao.mover = FRENTE

    mais_proximo = None
    menor = float("inf")
    for item in estado.itens:
        d = distancia(meu.x, meu.y, item.x, item.y)
        if d < menor:
            menor = d
            mais_proximo = item

    if mais_proximo is not None:
        acao.direcao = angulo_para(meu.x, meu.y, mais_proximo.x, mais_proximo.y)
    elif estado.inimigos:
        alvo = estado.inimigos[0]
        para_o_alvo = angulo_para(meu.x, meu.y, alvo.x, alvo.y)
        acao.direcao = para_o_alvo
        acao.angulo_torre = para_o_alvo
        acao.atirar = True                       # atira sempre que pode
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360
