import math

def pensar(estado):
    me = estado.get("self", {})
    x, y, ang = me.get("x", 0), me.get("y", 0), me.get("angle", 0)
    
    acao = {
        "throttle": 1.0,
        "turn": 0.0,
        "turret_angle": ang,
        "shoot": False,
        "shield": False
    }

    # Comportamento: Kamikaze / Guloso por itens
    item_mais_proximo = None
    menor_dist = float('inf')
    
    for item in estado.get("items", []):
        d = math.hypot(item["x"] - x, item["y"] - y)
        if d < menor_dist:
            menor_dist = d
            item_mais_proximo = item

    if item_mais_proximo:
        alvo_ang = math.degrees(math.atan2(item_mais_proximo["y"] - y, item_mais_proximo["x"] - x))
        diff = (alvo_ang - ang + 180) % 360 - 180
        acao["turn"] = 1.0 if diff > 5 else (-1.0 if diff < -5 else 0.0)
    else:
        # Se nao tem item, vai atras do inimigo com tudo
        for inimigo in estado.get("enemies", []):
            alvo_ang = math.degrees(math.atan2(inimigo["y"] - y, inimigo["x"] - x))
            diff = (alvo_ang - ang + 180) % 360 - 180
            acao["turn"] = 1.0 if diff > 5 else (-1.0 if diff < -5 else 0.0)
            acao["turret_angle"] = (alvo_ang + 360) % 360
            acao["shoot"] = True # Spray and pray
            break

    return acao
