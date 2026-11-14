#!/usr/bin/env python3
"""Mini-simulador das fases de treino, só para testar o avaliar_fase sem a engine.

Reproduz a física da engine que importa para as fases (giro de 9 graus por tick, 5 unidades
por tick com throttle 1, velocidade instantânea, tiro com cooldown de 1 s e 20 de energia,
regeneração de 0,5 por tick, projétil de 30 unidades por tick, artilharia com aviso de 3 s e
raio 80) e grava um match_frames.json no formato do contrato C-E3 (bloco "fase" no topo,
alvos em "tanks" com "alvo": true). O sorteio das faixas NÃO é o da engine (outra seed, outro
gerador): serve para cobrir posições variadas, não para reproduzir uma tentativa real.

Uso como módulo: simular(fase, pensar, seed) -> dict (o replay).
Uso direto:     python3 simulador.py <fase.json> <bot.py> <seed> <saida.json>
"""
import json
import math
import os
import random
import sys
import types

TANQUE = 20.0
PROJ_RAIO = 4.0
PROJ_VEL = 300.0
DT = 0.1


def _sortear(v, rng):
    if isinstance(v, list):
        return rng.uniform(float(v[0]), float(v[1]))
    return float(v)


WRAPPERS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))),
                        "wrappers")


def carregar_bot(caminho):
    """Carrega um bot.py (API em português, from arena import *) e devolve uma função
    estado_da_engine (dict) -> ação da engine (dict), com a mesma tradução do wrapper real
    (wrappers/arena.py: Estado na entrada, _traduzir_acao na saída)."""
    if WRAPPERS not in sys.path:
        sys.path.insert(0, WRAPPERS)
    import arena  # noqa: E402  (wrappers/arena.py, o mesmo arquivo de apoio do servidor)
    with open(caminho, encoding="utf-8") as f:
        fonte = f.read()
    mod = types.ModuleType("bot")
    exec(compile(fonte, caminho, "exec"), mod.__dict__)
    pensar = mod.__dict__["pensar"]

    def traduzido(bruto):
        estado = arena.Estado(bruto)
        return arena._traduzir_acao(pensar(estado), estado)
    return traduzido


def simular(fase, pensar, seed, morrer_no_tick=None):
    """morrer_no_tick: simula um bot que para de responder (timeout) naquele tick."""
    rng = random.Random(seed)
    cen = fase["cenario"]
    max_ticks = int(cen.get("max_ticks", 1500))
    j = cen.get("jogador", {})
    eu = {"id": 1, "x": _sortear(j.get("x", 500), rng), "y": _sortear(j.get("y", 500), rng),
          "angle": _sortear(j.get("angulo", 0), rng) % 360, "hp": float(j.get("hp", 100)),
          "energy": float(j.get("energia", 100)), "cooldown": 0.0, "alive": True, "turret": 0.0,
          "vx": 0.0, "vy": 0.0, "kills": 0}
    eu["turret"] = eu["angle"]
    pontos = [{"id": p["id"], "x": _sortear(p["x"], rng), "y": _sortear(p["y"], rng), "raio": float(p["raio"])}
              for p in cen.get("pontos", [])]
    alvos = []
    for i, a in enumerate(cen.get("alvos", [])):
        alvos.append({"id": a["id"], "tank_id": 2 + i, "x": _sortear(a["x"], rng), "y": _sortear(a["y"], rng),
                      "hp": float(a.get("hp", 100)), "alive": True})
    art_prog = list(cen.get("artilharia", [])) if isinstance(cen.get("artilharia"), list) else []
    art_efetiva = []
    hazards = []
    projeteis = []
    prox_id = 100
    frames = []
    for tick in range(max_ticks):
        eventos = []
        # 1. artilharia programada
        for a in art_prog:
            if int(a["tick"]) != tick:
                continue
            if a.get("no_jogador"):
                d = _sortear(a.get("desvio", 0), rng)
                ang = rng.uniform(0, 2 * math.pi)
                x = min(1000.0, max(0.0, eu["x"] + d * math.cos(ang)))
                y = min(1000.0, max(0.0, eu["y"] + d * math.sin(ang)))
            else:
                x, y = _sortear(a["x"], rng), _sortear(a["y"], rng)
            hazards.append({"type": "ARTILLERY", "x": x, "y": y, "radius": 80.0, "timer": 3.0,
                            "angle": 0.0, "duration": 0.0, "id": prox_id})
            prox_id += 1
            art_efetiva.append({"tick": tick, "x": x, "y": y, "no_jogador": bool(a.get("no_jogador"))})
            eventos.append({"type": "ARTILLERY_WARN", "x": x, "y": y, "source_id": -1, "target_id": -1})
        # 2. comando do bot
        if eu["alive"]:
            if morrer_no_tick is not None and tick >= morrer_no_tick:
                eu["alive"] = False
                eu["hp"] = 0.0
            else:
                estado = {
                    "tick": tick,
                    "self": {"id": 1, "x": eu["x"], "y": eu["y"], "vx": eu["vx"], "vy": eu["vy"], "angle": eu["angle"],
                             "turret_angle": eu["turret"], "hp": eu["hp"], "energy": eu["energy"],
                             "cooldown": eu["cooldown"], "shield": False},
                    "enemies": [{"id": a["tank_id"], "x": a["x"], "y": a["y"], "vx": 0.0, "vy": 0.0} for a in alvos if a["alive"]],
                    "hazards": [dict(h) for h in hazards],
                    "items": [],
                    "objetivo": {"pontos": [dict(p) for p in pontos]},
                }
                acao = pensar(estado) or {}
                thr = max(-1.0, min(1.0, float(acao.get("throttle", 0.0))))
                turn = max(-1.0, min(1.0, float(acao.get("turn", 0.0))))
                eu["angle"] = (eu["angle"] + turn * 9.0) % 360
                if acao.get("turret_angle") is not None:
                    eu["turret"] = float(acao["turret_angle"]) % 360
                rad = math.radians(eu["angle"])
                eu["vx"], eu["vy"] = math.cos(rad) * thr * 50, math.sin(rad) * thr * 50
                if acao.get("shoot") is True and eu["cooldown"] <= 0 and eu["energy"] >= 20:
                    eu["cooldown"] = 1.0
                    eu["energy"] -= 20
                    tr = math.radians(eu["turret"])
                    px, py = eu["x"] + math.cos(tr) * 25, eu["y"] + math.sin(tr) * 25
                    projeteis.append({"id": prox_id, "x": px, "y": py, "owner_id": 1,
                                      "vx": math.cos(tr) * PROJ_VEL, "vy": math.sin(tr) * PROJ_VEL})
                    prox_id += 1
                    eventos.append({"type": "SHOOT", "x": px, "y": py, "source_id": 1, "target_id": -1})
        # 3. física
        if eu["alive"]:
            eu["x"] = min(1000 - TANQUE, max(TANQUE, eu["x"] + eu["vx"] * DT))
            eu["y"] = min(1000 - TANQUE, max(TANQUE, eu["y"] + eu["vy"] * DT))
            if eu["cooldown"] > 0:
                eu["cooldown"] = max(0.0, eu["cooldown"] - DT) if eu["cooldown"] - DT > 1e-9 else 0.0
            eu["energy"] = min(100.0, eu["energy"] + 0.5)
        vivos = []
        for p in projeteis:
            p["x"] += p["vx"] * DT
            p["y"] += p["vy"] * DT
            if not (0 <= p["x"] <= 1000 and 0 <= p["y"] <= 1000):
                continue
            acertou = False
            for a in alvos:
                if a["alive"] and math.hypot(a["x"] - p["x"], a["y"] - p["y"]) <= TANQUE + PROJ_RAIO:
                    a["hp"] -= 25
                    eventos.append({"type": "HIT", "x": a["x"], "y": a["y"], "source_id": 1, "target_id": a["tank_id"]})
                    if a["hp"] <= 0:
                        a["alive"] = False
                        eu["kills"] += 1
                        eventos.append({"type": "EXPLOSION", "x": a["x"], "y": a["y"], "source_id": 1, "target_id": a["tank_id"]})
                    acertou = True
                    break
            if not acertou:
                vivos.append(p)
        projeteis = vivos
        restantes = []
        for h in hazards:
            h["timer"] -= DT
            if h["timer"] <= 1e-9:
                eventos.append({"type": "EXPLOSION", "x": h["x"], "y": h["y"], "source_id": -1, "target_id": -1})
                if eu["alive"] and math.hypot(eu["x"] - h["x"], eu["y"] - h["y"]) <= 80 + TANQUE:
                    eu["hp"] -= 40
                    if eu["hp"] <= 0:
                        eu["alive"] = False
            else:
                restantes.append(h)
        hazards = restantes
        # 4. frame
        tanks = [{"id": 1, "x": eu["x"], "y": eu["y"], "angle": eu["angle"], "turret": eu["turret"], "hp": eu["hp"],
                  "energy": eu["energy"], "shield": False, "kills": eu["kills"], "alive": eu["alive"]}]
        for a in alvos:
            tanks.append({"id": a["tank_id"], "x": a["x"], "y": a["y"], "angle": 0.0, "turret": 0.0, "hp": a["hp"],
                          "energy": 100.0, "shield": False, "kills": 0, "alive": a["alive"]})
        frames.append({"tick": tick, "tanks": tanks, "projectiles": [dict(p) for p in projeteis],
                       "hazards": [dict(h) for h in hazards], "items": [], "events": eventos})
        if not eu["alive"]:
            break
    return {
        "fase": {"id": fase["id"], "seed": seed, "pontos": pontos,
                 "alvos": [{"id": a["id"], "tank_id": a["tank_id"], "x": a["x"], "y": a["y"]} for a in alvos],
                 "artilharia": art_efetiva},
        "config": {}, "frames": frames, "seed": seed, "turnos": len(frames),
        "tanques": [{"id": 1, "controle": "equipe", "rotulo": "Bot_1"}] +
                   [{"id": a["tank_id"], "controle": "alvo", "rotulo": a["id"]} for a in alvos],
    }


if __name__ == "__main__":
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(2)
    with open(sys.argv[1], encoding="utf-8") as f:
        fase = json.load(f)
    replay = simular(fase, carregar_bot(sys.argv[2]), int(sys.argv[3]))
    with open(sys.argv[4], "w", encoding="utf-8", newline="") as f:
        json.dump(replay, f)
