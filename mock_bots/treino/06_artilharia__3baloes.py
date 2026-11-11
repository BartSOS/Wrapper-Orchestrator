# Bot de referência da fase 06_artilharia: passa com 3 balões.
# Quando um bombardeio avisado o alcança, testa 24 direções de fuga: para cada uma, estima
# onde o tanque estará quando cada bombardeio explodir (descontando o tempo de giro) e escolhe
# a direção que deixa a maior folga. Anda de frente ou de ré, o que exigir menos giro.
# Sem perigo, volta devagar para perto do centro (longe das paredes).
from arena import *
import math

COMECA_NO_FRAME = 0
LETAL = 80.0 + 20.0 + 8.0   # raio da artilharia + raio do tanque + folga
anterior = None              # direção escolhida no frame anterior (evita ficar indeciso)


def pensar(estado):
    global anterior
    meu = estado.eu
    acao = Acao()
    if estado.frame < COMECA_NO_FRAME:
        return acao                                # PARADO
    perigos = [p for p in estado.perigos if p.tipo == PERIGO_ARTILHARIA]
    ameacas = [p for p in perigos if distancia(meu.x, meu.y, p.x, p.y) < LETAL + 20]
    if not ameacas:
        anterior = None
        if distancia(meu.x, meu.y, 500, 500) > 120:
            para_o_centro = angulo_para(meu.x, meu.y, 500, 500)
            acao.direcao = para_o_centro
            acao.mover = 0.6 if abs(diferenca_angulo(meu.angulo_chassi, para_o_centro)) < 30 else PARADO
        return acao
    melhor, melhor_nota = None, -1e9
    for i in range(24):
        th = i * 15.0
        dif = diferenca_angulo(meu.angulo_chassi, th)
        giro = abs(dif) if abs(dif) <= 90 else 180 - abs(dif)
        nota = 1e9
        for p in perigos:
            andar = max(0.0, p.frames_ate_impacto - giro / 9.0 * 0.5) * 5.0
            px = min(980.0, max(20.0, meu.x + math.cos(math.radians(th)) * andar))
            py = min(980.0, max(20.0, meu.y + math.sin(math.radians(th)) * andar))
            nota = min(nota, distancia(px, py, p.x, p.y) - LETAL)
        if anterior is not None and abs(diferenca_angulo(anterior, th)) < 1:
            nota += 5.0
        if nota > melhor_nota:
            melhor, melhor_nota = th, nota
    anterior = melhor
    if abs(diferenca_angulo(meu.angulo_chassi, melhor)) <= 90:
        acao.direcao = melhor
        acao.mover = FRENTE
    else:
        acao.direcao = (melhor + 180) % 360        # de ré: a traseira aponta para a fuga
        acao.mover = RE
    return acao


def distancia(x1, y1, x2, y2):
    return math.hypot(x2 - x1, y2 - y1)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d
