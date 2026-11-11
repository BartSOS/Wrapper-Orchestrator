# Bot de referência da fase 05_mira_andando: passa com 3 balões.
# Chassi vai até B (como na fase 1) enquanto a torre atira no alvo mais perto (como na fase 4).
from arena import *
import math

atirados = {}   # id do alvo -> frame em que o tiro já deve ter chegado


def pensar(estado):
    meu = estado.eu
    agora = estado.frame
    acao = Acao()
    # Chassi: até o ponto B
    b = estado.pontos[0]
    if distancia(meu.x, meu.y, b.x, b.y) > b.raio * 0.5:
        para_b = angulo_para(meu.x, meu.y, b.x, b.y)
        acao.direcao = para_b
        acao.mover = FRENTE if abs(diferenca_angulo(meu.angulo_chassi, para_b)) < 45 else PARADO
    # Torre: alvo mais perto em que não há tiro a caminho
    candidatos = [a for a in estado.inimigos if agora > atirados.get(a.id, -1)]
    if candidatos:
        alvo = min(candidatos, key=lambda a: distancia(meu.x, meu.y, a.x, a.y))
        acao.angulo_torre = angulo_para(meu.x, meu.y, alvo.x, alvo.y)
        if meu.frames_para_atirar == 0 and meu.energia >= 20:
            acao.atirar = True
            voo = distancia(meu.x, meu.y, alvo.x, alvo.y) / 30.0   # o projétil anda 30 por frame
            atirados[alvo.id] = agora + int(voo) + 4
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
