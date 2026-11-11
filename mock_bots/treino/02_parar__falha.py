# Bot de referência da fase 02_parar: falha de propósito.
# Erro comum: mover = FRENTE o tempo todo, girando enquanto anda. Passa do ponto, faz a curva
# (o raio da curva é maior que o do ponto) e fica rodando em volta dele.
from arena import *
import math


def pensar(estado):
    meu = estado.eu
    ponto = estado.pontos[0]
    acao = Acao()
    acao.direcao = angulo_para(meu.x, meu.y, ponto.x, ponto.y)
    acao.mover = FRENTE   # ERRO: nunca reduz perto do ponto
    return acao


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360
