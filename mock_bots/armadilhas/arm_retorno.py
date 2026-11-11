# Armadilha: o pensar() lança exceção ou devolve algo que não é uma Acao em alguns
# frames (None, lista, NaN). Antes, um único frame assim dessincronizava o bot, que era
# eliminado por timeout 20 frames depois. Agora esse frame vale como parado.
# No frame 12 de cada ciclo devolve um dict com as chaves antigas da engine (throttle,
# turn): o wrapper aceita e escreve um [AVISO] no stderr (uma vez por chave).
from arena import *


def pensar(estado):
    t = estado.frame
    if t % 13 == 1:
        raise ValueError("erro proposital")
    if t % 13 == 4:
        return None
    if t % 13 == 7:
        return [1, 2, 3]
    if t % 13 == 10:
        return Acao(mover=float("nan"))
    if t % 13 == 12:
        return {"throttle": 0.8, "turn": -0.2}
    acao = Acao()
    acao.mover = 0.8
    acao.direcao = estado.eu.angulo_chassi - 2
    return acao
