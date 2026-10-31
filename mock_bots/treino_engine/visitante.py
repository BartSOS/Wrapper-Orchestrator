# Bot de teste do modo cenário (frente engine): visita os pontos de estado["objetivo"]
# na ordem e depois mira e atira no inimigo mais próximo. Sem "objetivo" (torneio), só atira.
import math

_visitados = set()


def _diferenca(a, b):
    return (a - b + 180.0) % 360.0 - 180.0


def pensar(estado):
    eu = estado["self"]
    acao = {"throttle": 0.0, "turn": 0.0, "turret_angle": eu["turret_angle"], "shoot": False, "shield": False}

    for p in estado.get("objetivo", {}).get("pontos", []):
        if p["id"] in _visitados:
            continue
        if math.hypot(p["x"] - eu["x"], p["y"] - eu["y"]) <= p["raio"] * 0.6:
            _visitados.add(p["id"])
            continue
        alvo = math.degrees(math.atan2(p["y"] - eu["y"], p["x"] - eu["x"])) % 360.0
        erro = _diferenca(alvo, eu["angle"])
        acao["turn"] = max(-1.0, min(1.0, erro / 9.0))
        acao["throttle"] = 1.0 if abs(erro) < 30 else 0.2
        return acao

    inimigos = estado.get("enemies", [])
    if inimigos:
        e = min(inimigos, key=lambda i: math.hypot(i["x"] - eu["x"], i["y"] - eu["y"]))
        acao["turret_angle"] = math.degrees(math.atan2(e["y"] - eu["y"], e["x"] - eu["x"])) % 360.0
        acao["shoot"] = True
    return acao
