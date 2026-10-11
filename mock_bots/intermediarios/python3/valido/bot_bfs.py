import math

def pensar(estado):
    me = estado.get("self", {})
    x, y, ang = me.get("x", 0), me.get("y", 0), me.get("angle", 0)
    energia = me.get("energy", 0)
    cooldown = me.get("cooldown", 0)

    acao = {
        "throttle": 0.8,
        "turn": 0.5,
        "turret_angle": ang,
        "shoot": False,
        "shield": False
    }

    # Comportamento: Fuga Extrema (Kiting) e Sobrevivência
    inimigo_mais_proximo = None
    menor_dist = float('inf')
    
    for inimigo in estado.get("enemies", []):
        d = math.hypot(inimigo["x"] - x, inimigo["y"] - y)
        if d < menor_dist:
            menor_dist = d
            inimigo_mais_proximo = inimigo

    if inimigo_mais_proximo:
        # Foge atirando para trás (kiting)
        alvo_ang = math.degrees(math.atan2(inimigo_mais_proximo["y"] - y, inimigo_mais_proximo["x"] - x))
        fuga_ang = (alvo_ang + 180) % 360
        
        # Mira pro inimigo
        acao["turret_angle"] = (alvo_ang + 360) % 360
        
        # Direção de fuga
        diff = (fuga_ang - ang + 180) % 360 - 180
        acao["turn"] = 1.0 if diff > 5 else (-1.0 if diff < -5 else 0.0)
        acao["throttle"] = 1.0
        
        # Atira se possível e seguro
        if menor_dist < 600 and cooldown == 0 and energia >= 15:
            acao["shoot"] = True

        # Escudo de pânico
        if menor_dist < 200 and energia > 30:
            acao["shield"] = True

    return acao