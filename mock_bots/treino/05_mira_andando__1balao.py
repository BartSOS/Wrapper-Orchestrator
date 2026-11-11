# Bot de referência da fase 05_mira_andando: passa com 1 balão.
# Anda e atira ao mesmo tempo, mas erra o primeiro tiro nos 3 primeiros alvos (10 graus
# ao lado): 8 tiros e espera para juntar energia.
from arena import *
import math

atirados = {}    # id do alvo -> frame em que o tiro já deve ter chegado
errados = set()  # alvos em que já errou de propósito


def pensar(estado):
    meu = estado.eu
    agora = estado.frame
    acao = Acao()
    b = estado.pontos[0]
    if distancia(meu.x, meu.y, b.x, b.y) > b.raio * 0.5:
        para_b = angulo_para(meu.x, meu.y, b.x, b.y)
        acao.direcao = para_b
        acao.mover = FRENTE if abs(diferenca_angulo(meu.angulo_chassi, para_b)) < 45 else PARADO
    candidatos = [a for a in estado.inimigos if agora > atirados.get(a.id, -1)]
    if candidatos:
        alvo = min(candidatos, key=lambda a: distancia(meu.x, meu.y, a.x, a.y))
        errar = alvo.id not in errados and len(errados) < 3
        acao.angulo_torre = (angulo_para(meu.x, meu.y, alvo.x, alvo.y) + (10.0 if errar else 0.0)) % 360
        if meu.frames_para_atirar == 0 and meu.energia >= 20:
            acao.atirar = True
            if errar:
                errados.add(alvo.id)
            voo = distancia(meu.x, meu.y, alvo.x, alvo.y) / 30.0
            atirados[alvo.id] = agora + int(voo) + 4
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
