# Bot de referência da fase 03_percurso: passa com 1 balão.
# Faz o percurso certo, mas para perto do P4 (fora do raio) e só entra depois do tick 690.
# O tanque não tem inércia (throttle 0 para na hora), então ele termina por volta do tick 700
# em qualquer seed: acima da meta de 2 balões (640) e antes do limite da fase (750).
import math

ESPERA_ATE = 690     # tick a partir do qual ele entra no último ponto
FOLGA_PARADA = 40    # para a esta distância além do raio do último ponto

proximo = 0


def angulo_para(x1, y1, x2, y2):
    return math.degrees(math.atan2(y2 - y1, x2 - x1)) % 360


def diferenca_angulo(de, para):
    d = (para - de) % 360
    return d - 360 if d > 180 else d


def pensar(estado):
    global proximo
    parado = {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
    eu = estado["self"]
    pontos = estado["objetivo"]["pontos"]
    if proximo < len(pontos):
        p = pontos[proximo]
        if math.hypot(p["x"] - eu["x"], p["y"] - eu["y"]) <= p["raio"] * 0.8:
            proximo += 1
    if proximo >= len(pontos):
        return parado
    p = pontos[proximo]
    dist = math.hypot(p["x"] - eu["x"], p["y"] - eu["y"])
    falta = diferenca_angulo(eu["angle"], angulo_para(eu["x"], eu["y"], p["x"], p["y"]))
    turn = max(-1.0, min(1.0, falta / 9.0))
    if proximo == len(pontos) - 1 and estado.get("tick", 0) < ESPERA_ATE and dist <= p["raio"] + FOLGA_PARADA:
        return {"throttle": 0.0, "turn": turn, "shoot": False, "shield": False}
    throttle = 1.0 if abs(falta) < 45 else 0.0
    return {"throttle": throttle, "turn": turn, "shoot": False, "shield": False}
