# Bot de referência da fase 03_percurso: passa com 1 balão.
# Faz o percurso certo, mas para perto do P4 (fora do raio) e só entra depois do frame 690.
# O tanque não tem inércia (PARADO para na hora), então ele termina por volta do frame 700
# em qualquer seed: acima da meta de 2 balões (640) e antes do limite da fase (750).
from arena import *
import math

ESPERA_ATE = 690     # frame a partir do qual ele entra no último ponto
FOLGA_PARADA = 40    # para a esta distância além do raio do último ponto
proximo = 0


def pensar(estado):
    global proximo
    meu = estado.eu
    pontos = estado.pontos
    acao = Acao()
    if proximo < len(pontos):
        p = pontos[proximo]
        if distancia(meu.x, meu.y, p.x, p.y) <= p.raio * 0.8:
            proximo += 1
    if proximo >= len(pontos):
        return acao                                # PARADO
    p = pontos[proximo]
    dist = distancia(meu.x, meu.y, p.x, p.y)
    para_o_ponto = angulo_para(meu.x, meu.y, p.x, p.y)
    falta = diferenca_angulo(meu.angulo_chassi, para_o_ponto)
    acao.direcao = para_o_ponto
    if proximo == len(pontos) - 1 and estado.frame < ESPERA_ATE and dist <= p.raio + FOLGA_PARADA:
        return acao                                # espera, só girando
    acao.mover = FRENTE if abs(falta) < 45 else PARADO
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
