# Bot de referência da fase 04_mira_parado: passa com 1 balão.
# Erra o primeiro tiro em cada alvo (mira 8 graus ao lado) e acerta na segunda tentativa:
# 6 tiros para 3 alvos.
from arena import *
import math

atirados = {}    # id do alvo -> frame do último tiro
tentativas = {}  # id do alvo -> tiros dados


def pensar(estado):
    meu = estado.eu
    agora = estado.frame
    acao = Acao()
    candidatos = [a for a in estado.inimigos if agora - atirados.get(a.id, -999) > 20]
    if not candidatos:
        return acao
    alvo = min(candidatos, key=lambda a: distancia(meu.x, meu.y, a.x, a.y))
    erro = 8.0 if tentativas.get(alvo.id, 0) == 0 else 0.0
    acao.angulo_torre = (angulo_para(meu.x, meu.y, alvo.x, alvo.y) + erro) % 360
    if meu.frames_para_atirar == 0 and meu.energia >= 20:
        acao.atirar = True
        atirados[alvo.id] = agora
        tentativas[alvo.id] = tentativas.get(alvo.id, 0) + 1
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360
