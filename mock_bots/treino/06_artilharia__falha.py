# Bot de referência da fase 06_artilharia: falha de propósito.
# Erro comum: ignora estado["hazards"]. Fica parado e leva todos os bombardeios.


def pensar(estado):
    return {"throttle": 0.0, "turn": 0.0, "shoot": False, "shield": False}
