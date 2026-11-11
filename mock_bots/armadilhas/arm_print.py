# Armadilha: print no nível do arquivo e escrita direta em sys.__stdout__.
# Antes do wrapper novo, a linha abaixo virava a "resposta" do frame 0 e todas as
# respostas seguintes chegavam um frame atrasadas. Agora vai para o stderr.
from arena import *
import sys

print('{"mover": 1, "direcao": 90}')
sys.__stdout__.write("lixo no canal da engine\n")


def pensar(estado):
    print("debug do frame", estado.frame)
    acao = Acao()
    acao.mover = FRENTE
    acao.direcao = estado.eu.angulo_chassi + 3
    acao.atirar = estado.frame % 10 == 0
    return acao
