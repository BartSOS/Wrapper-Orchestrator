# =============================================================================
# NPC "guloso" - nível FÁCIL (Python 3)
# =============================================================================
# Estratégia: vai atrás do item mais próximo; sem itens, persegue o adversário
# mais próximo. Mira a torre no adversário mais próximo e atira de perto.
# Não desvia de artilharia nem de laser e não usa o escudo.
# Serve de adversário para quem está começando e de exemplo de uso dos ajudantes.
# =============================================================================

from arena import *
import math


def pensar(estado):
    meu = estado.eu

    acao = Acao()
    acao.mover = FRENTE                     # sempre na velocidade máxima
    acao.direcao = meu.angulo_chassi
    acao.angulo_torre = meu.angulo_torre
    acao.atirar = False
    acao.ligar_escudo = False

    # A torre fica no adversário mais próximo; atira quando está perto e pronta
    alvo = mais_proximo(meu, estado.inimigos)
    if alvo is not None:
        acao.angulo_torre = angulo_para(meu.x, meu.y, alvo.x, alvo.y)
        perto = distancia(meu.x, meu.y, alvo.x, alvo.y) < 450
        if perto and meu.frames_para_atirar == 0 and meu.energia >= 20:
            acao.atirar = True

    # O chassi vai atrás do item mais próximo; sem itens, atrás do adversário
    item = mais_proximo(meu, estado.itens)
    if item is not None:
        acao.direcao = angulo_para(meu.x, meu.y, item.x, item.y)
    elif alvo is not None:
        acao.direcao = acao.angulo_torre

    return acao


# O elemento da lista mais perto do meu tanque (ou None se a lista é vazia)
def mais_proximo(meu, lista):
    melhor = None
    menor = 0
    for coisa in lista:
        d = distancia(meu.x, meu.y, coisa.x, coisa.y)
        if melhor is None or d < menor:
            melhor = coisa
            menor = d
    return melhor


# -----------------------------------------------------------------------------
# AJUDANTES (opcional: use, mude ou apague)
# -----------------------------------------------------------------------------
# Matemática básica, com ângulos em graus (0 aponta para +x; 90, para +y).
# Elas só calculam: não decidem nada por você.
# -----------------------------------------------------------------------------

# Distância entre o ponto (x1, y1) e o ponto (x2, y2)
def distancia(x1, y1, x2, y2):
    dx = x2 - x1                            # quanto falta em x
    dy = y2 - y1                            # quanto falta em y
    return math.sqrt(dx * dx + dy * dy)     # Pitágoras


# Ângulo, em graus (de 0 a 360), de quem está em (x1, y1) olhando para (x2, y2)
def angulo_para(x1, y1, x2, y2):
    radianos = math.atan2(y2 - y1, x2 - x1)     # atan2 devolve radianos, de -pi a pi
    graus = radianos * 180.0 / math.pi          # em graus: de -180 a 180
    if graus < 0:
        graus = graus + 360.0                   # passa para 0 a 360
    return graus


# Quanto girar para ir do ângulo "de" ao ângulo "para" pelo caminho mais
# curto: de -180 a 180 graus. Positivo = o ângulo aumenta; negativo = diminui.
def diferenca_angulo(de, para):
    d = para - de
    while d > 180.0:
        d = d - 360.0                       # mais de meia volta: vai pelo outro lado
    while d <= -180.0:
        d = d + 360.0
    return d

# -----------------------------------------------------------------------------
# FIM DOS AJUDANTES
# -----------------------------------------------------------------------------
