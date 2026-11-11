# Bot de referência da fase 04_mira_parado: falha de propósito.
# Erro comum: manda para angulo_torre o valor de atan2 em RADIANOS. A torre fica quase em
# 0 grau e os tiros saem para o lado errado.
from arena import *
import math


def pensar(estado):
    meu = estado.eu
    acao = Acao()
    if not estado.inimigos:
        return acao
    alvo = estado.inimigos[0]
    acao.angulo_torre = math.atan2(alvo.y - meu.y, alvo.x - meu.x)   # ERRO: faltou converter para graus
    acao.atirar = meu.frames_para_atirar == 0 and meu.energia >= 20
    return acao
