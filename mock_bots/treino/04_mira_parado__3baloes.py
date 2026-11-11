# Bot de referência da fase 04_mira_parado: passa com 3 balões.
# Fica parado, aponta a torre (ângulo absoluto, em graus) para um alvo em que ainda não
# atirou e atira quando pode atirar (frames_para_atirar == 0) e há energia.
from arena import *
import math

atirados = {}   # id do alvo -> frame do tiro


def pensar(estado):
    meu = estado.eu
    agora = estado.frame
    acao = Acao()
    # Alvo ainda vivo 20 frames depois do tiro: o tiro errou, pode tentar de novo
    candidatos = [a for a in estado.inimigos if agora - atirados.get(a.id, -999) > 20]
    if not candidatos:
        return acao
    alvo = min(candidatos, key=lambda a: distancia(meu.x, meu.y, a.x, a.y))
    acao.angulo_torre = angulo_para(meu.x, meu.y, alvo.x, alvo.y)
    if meu.frames_para_atirar == 0 and meu.energia >= 20:
        acao.atirar = True
        atirados[alvo.id] = agora
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360
