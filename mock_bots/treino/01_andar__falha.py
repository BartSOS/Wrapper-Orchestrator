# Bot de referência da fase 01_andar: falha de propósito.
# Erro comum: o sinal da conta do giro invertido. O chassi gira para o lado oposto ao ponto
# e o tanque acaba andando para longe dele.
from arena import *
import math


def pensar(estado):
    meu = estado.eu
    ponto = estado.pontos[0]
    falta = diferenca_angulo(meu.angulo_chassi, angulo_para(meu.x, meu.y, ponto.x, ponto.y))
    acao = Acao()
    acao.direcao = meu.angulo_chassi - falta   # ERRO: deveria ser + falta
    acao.mover = FRENTE
    return acao


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
