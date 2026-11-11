# =============================================================================
# NPC "bfs" - nível MÉDIO (Python 3)
# =============================================================================
# Estratégia: kiting. Mira no adversário mais próximo e mantém a distância:
# recua atirando quando ele chega perto, aproxima quando está longe e circula
# no meio-termo. Corre para uma cura próxima se a vida está baixa, sai do raio
# de uma artilharia e liga o escudo se o adversário encosta com a vida baixa.
# Não antecipa o movimento do adversário e não trata o laser nem a zona.
# =============================================================================

from arena import *
import math


def pensar(estado):
    meu = estado.eu

    acao = Acao()
    acao.mover = FRENTE                                     # sempre na velocidade máxima
    acao.direcao = angulo_para(meu.x, meu.y, 500, 500)     # sem adversários: volta ao centro
    acao.angulo_torre = meu.angulo_torre
    acao.atirar = False
    acao.ligar_escudo = False

    alvo = mais_proximo(meu, estado.inimigos)
    if alvo is not None:
        d = distancia(meu.x, meu.y, alvo.x, alvo.y)
        para_o_alvo = angulo_para(meu.x, meu.y, alvo.x, alvo.y)
        acao.angulo_torre = para_o_alvo
        if d < 550 and meu.frames_para_atirar == 0 and meu.energia >= 20:
            acao.atirar = True
        if d < 300 and not perto_da_borda(meu, 100):
            acao.direcao = para_o_alvo + 180        # recua atirando
        elif d > 500:
            acao.direcao = para_o_alvo              # aproxima
        else:
            acao.direcao = para_o_alvo + 90         # circula
        if d < 150 and meu.vida < 60 and meu.energia > 60:
            acao.ligar_escudo = True

    # Vida baixa: corre para a cura mais próxima, se estiver perto
    if meu.vida < 60:
        curas = []
        for item in estado.itens:
            if item.tipo == ITEM_CURA:
                curas.append(item)
        cura = mais_proximo(meu, curas)
        if cura is not None and distancia(meu.x, meu.y, cura.x, cura.y) < 250:
            acao.direcao = angulo_para(meu.x, meu.y, cura.x, cura.y)

    # Dentro do raio de uma artilharia: sai dele (prioridade máxima)
    for perigo in estado.perigos:
        if perigo.tipo == PERIGO_ARTILHARIA and distancia(meu.x, meu.y, perigo.x, perigo.y) < perigo.raio + 40:
            acao.direcao = angulo_para(perigo.x, perigo.y, meu.x, meu.y)
            break

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


# Perto da parede (folga em unidades): recuar para lá encurralaria o tanque
def perto_da_borda(meu, folga):
    return meu.x < folga or meu.x > 1000 - folga or meu.y < folga or meu.y > 1000 - folga


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
