#!/usr/bin/env python3
# =============================================================================
# TOPCOM ARENA - STARTER KIT PYTHON 3
# =============================================================================
# INSTRUÇÕES PARA O PARTICIPANTE:
# - Não utilize stdin/stdout (input/print) para comunicação com a arena.
# - A arena injeta automaticamente o wrapper de comunicação e I/O.
# - Implemente apenas a função pensar(estado), que recebe um dicionário com
#   o estado atual da arena e retorna um dicionário com a sua ação.
# =============================================================================

import math

def pensar(estado):
    """
    Função chamada a cada tick (rodada) da simulação.
    
    Parâmetros:
      estado (dict):
        - "self": {"x": float, "y": float, "angle": float, "hp": float, "energy": float, "cooldown": float}
        - "enemies": [{"x": float, "y": float, "hp": float}, ...]
        - "items": [{"x": float, "y": float, "type": "HEAL"|"ENERGY"}, ...]
        
    Retorno:
      dict com os comandos da rodada:
        - "throttle": float (-1.0 a 1.0)
        - "turn": float (-1.0 a 1.0)
        - "turret_angle": float (0.0 a 360.0)
        - "shoot": bool (True/False)
        - "shield": bool (True/False)
    """
    me = estado.get("self", {})
    minha_x = me.get("x", 0.0)
    minha_y = me.get("y", 0.0)
    meu_angulo = me.get("angle", 0.0)
    minha_energia = me.get("energy", 0.0)
    meu_cooldown = me.get("cooldown", 0.0)

    acao = {
        "throttle": 0.6,
        "turn": 0.3,
        "turret_angle": meu_angulo,
        "shoot": False,
        "shield": False
    }

    # 1. Radar de Inimigos: Encontra o mais próximo
    inimigo_mais_proximo = None
    menor_dist_inimigo = float("inf")
    for inimigo in estado.get("enemies", []):
        dist = math.hypot(inimigo["x"] - minha_x, inimigo["y"] - minha_y)
        if dist < menor_dist_inimigo:
            menor_dist_inimigo = dist
            inimigo_mais_proximo = inimigo

    # 2. Defesa: Se o inimigo estiver muito perto, levanta escudo
    if menor_dist_inimigo < 250 and minha_energia > 20:
        acao["shield"] = True

    # 3. Coleta de Itens
    item_desejado = None
    menor_dist_item = float("inf")
    for item in estado.get("items", []):
        dist = math.hypot(item["x"] - minha_x, item["y"] - minha_y)
        if dist < menor_dist_item:
            menor_dist_item = dist
            item_desejado = item

    # 4. Movimentação inteligente
    angulo_destino = meu_angulo
    if item_desejado:
        angulo_rad = math.atan2(item_desejado["y"] - minha_y, item_desejado["x"] - minha_x)
        angulo_destino = (math.degrees(angulo_rad) + 360) % 360
        acao["throttle"] = 1.0
    elif inimigo_mais_proximo and menor_dist_inimigo < 400:
        # Foge na direção oposta ao inimigo
        angulo_rad = math.atan2(inimigo_mais_proximo["y"] - minha_y, inimigo_mais_proximo["x"] - minha_x)
        angulo_destino = (math.degrees(angulo_rad) + 180) % 360
        acao["throttle"] = 1.0

    # Ajuste de rotação para alinhar com o destino
    diff_ang = (angulo_destino - meu_angulo + 180) % 360 - 180
    if diff_ang > 10:
        acao["turn"] = 1.0
    elif diff_ang < -10:
        acao["turn"] = -1.0
    else:
        acao["turn"] = 0.0

    # 5. Mira e Disparo
    if inimigo_mais_proximo:
        alvo_x = inimigo_mais_proximo["x"]
        alvo_y = inimigo_mais_proximo["y"]
        angulo_rad_alvo = math.atan2(alvo_y - minha_y, alvo_x - minha_x)
        angulo_alvo = (math.degrees(angulo_rad_alvo) + 360) % 360
        acao["turret_angle"] = angulo_alvo

        if menor_dist_inimigo < 500 and meu_cooldown == 0 and minha_energia > 40:
            acao["shoot"] = True

    return acao
