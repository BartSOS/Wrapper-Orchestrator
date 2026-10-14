#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Bot de teste (Python 3) - ERRO DE SINTAXE PROPOSITAL.
# Implementa pensar(estado) com a assinatura correta, para passar pela checagem
# estrutural do validador e chegar ao py_compile, que deve recusar:
# falta o ':' no if e há um parêntese não fechado.

import math


def pensar(estado):
    me = estado.get("self", {})
    x, y = me.get("x", 0), me.get("y", 0)
    acao = {"throttle": 1.0, "turn": 0.0, "turret_angle": 0.0, "shoot": False, "shield": False}

    for inimigo in estado.get("enemies", []):
        if math.hypot(inimigo["x"] - x, inimigo["y"] - y) < 300
            acao["shoot"] = True
            acao["turret_angle"] = (math.degrees(math.atan2(inimigo["y"] - y, inimigo["x"] - x) + 360) % 360

    return acao
