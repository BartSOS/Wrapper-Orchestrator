# Armadilha: print no nível do arquivo e escrita direta em sys.__stdout__.
# Antes do wrapper novo, a linha abaixo virava a "resposta" do tick 0 e todas as
# respostas seguintes chegavam um tick atrasadas. Agora vai para o stderr.
import sys

print('{"throttle": 1, "turn": 1}')
sys.__stdout__.write("lixo no canal da engine\n")


def pensar(estado):
    print("debug do tick", estado["tick"])
    return {"throttle": 1.0, "turn": 0.3, "shoot": estado["tick"] % 10 == 0}
