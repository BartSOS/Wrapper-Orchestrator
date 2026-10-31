# Bot de referência da fase 06_artilharia: passa com 1 balão.
# É o bot de 3 balões que acorda tarde: fica parado até o tick 100 e leva os 2 primeiros
# bombardeios (80 de dano); depois foge de todos.
# Quando um bombardeio avisado o alcança, testa 24 direções de fuga: para cada uma, estima
# onde o tanque estará quando cada bombardeio explodir (descontando o tempo de giro) e escolhe
# a direção que deixa a maior folga. Anda de frente ou de ré, o que exigir menos giro.
# Sem perigo, volta devagar para perto do centro (longe das paredes).
import math

COMECA_NO_TICK = 100
LETAL = 80.0 + 20.0 + 8.0   # raio da artilharia + raio do tanque + folga
anterior = None              # direção escolhida no tick anterior (evita ficar indeciso)


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d


def pensar(estado):
    global anterior
    eu = estado["self"]
    parado = {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    if estado["tick"] < COMECA_NO_TICK:
        return parado
    perigos = [h for h in estado["hazards"] if h["type"] == "ARTILLERY"]
    ameacas = [h for h in perigos if math.hypot(eu["x"] - h["x"], eu["y"] - h["y"]) < LETAL + 20]
    if not ameacas:
        anterior = None
        if math.hypot(500 - eu["x"], 500 - eu["y"]) > 120:
            falta = diferenca_angulo(eu["angle"], angulo_para(eu["x"], eu["y"], 500, 500))
            return {"throttle": 0.6 if abs(falta) < 30 else 0.0, "turn": max(-1.0, min(1.0, falta / 9.0)),
                    "shoot": False, "shield": False}
        return parado
    melhor, melhor_nota = None, -1e9
    for i in range(24):
        th = i * 15.0
        dif = diferenca_angulo(eu["angle"], th)
        giro = abs(dif) if abs(dif) <= 90 else 180 - abs(dif)
        nota = 1e9
        for h in perigos:
            ticks = h["timer"] * 10.0
            andar = max(0.0, ticks - giro / 9.0 * 0.5) * 5.0
            px = min(980.0, max(20.0, eu["x"] + math.cos(math.radians(th)) * andar))
            py = min(980.0, max(20.0, eu["y"] + math.sin(math.radians(th)) * andar))
            nota = min(nota, math.hypot(px - h["x"], py - h["y"]) - LETAL)
        if anterior is not None and abs(diferenca_angulo(anterior, th)) < 1:
            nota += 5.0
        if nota > melhor_nota:
            melhor, melhor_nota = th, nota
    anterior = melhor
    falta = diferenca_angulo(eu["angle"], melhor)
    if abs(falta) <= 90:
        return {"throttle": 1.0, "turn": max(-1.0, min(1.0, falta / 9.0)), "shoot": False, "shield": False}
    falta_re = diferenca_angulo(eu["angle"], (melhor + 180) % 360)
    return {"throttle": -1.0, "turn": max(-1.0, min(1.0, falta_re / 9.0)), "shoot": False, "shield": False}
