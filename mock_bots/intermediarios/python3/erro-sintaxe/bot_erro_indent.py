#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Bot de teste (Python 3) - ERRO DE INDENTAÇÃO PROPOSITAL.
# Implementa pensar(estado) com a assinatura correta, para passar pela checagem
# estrutural do validador e chegar ao py_compile, que deve recusar com IndentationError.


def pensar(estado):
    me = estado.get("self", {})
    acao = {"throttle": 0.5, "turn": 0.0, "turret_angle": me.get("angle", 0.0), "shoot": False, "shield": False}
    if me.get("hp", 100) < 30:
    acao["shield"] = True
        acao["throttle"] = -1.0
    return acao
