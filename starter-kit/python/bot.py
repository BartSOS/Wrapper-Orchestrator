# =============================================================================
# GAME JAM PET 2026 - STARTER KIT PYTHON 3
# =============================================================================
# O que você precisa fazer: escrever a função pensar(estado), mais abaixo.
# Ela é chamada 10 vezes por segundo e devolve o que o seu tanque vai fazer.
#
# Regras do código enviado:
# - Não crie laço de leitura, não use input() nem sys.stdin: a arena cuida da
#   comunicação e chama pensar() por você.
# - Só a biblioteca padrão do Python 3 (import math, random, collections...).
#   Bibliotecas externas (numpy etc.) dão Compilation Error.
# - print() não chega à arena: vai para o log de erros e é descartado.
# - Variáveis globais valem durante a partida inteira (guardam a sua memória).
# =============================================================================

import math

# -----------------------------------------------------------------------------
# AJUDANTES (opcional: renomeie, altere ou apague)
# -----------------------------------------------------------------------------
# Eles só facilitam o acesso aos atributos do estado e não decidem nada por
# você. Se preferir, apague esta seção inteira e leia o dicionário 'estado'
# direto (veja o formato em README.md / enunciado).

HAZARD_ARTILHARIA = 0   # explode em (x, y) com raio 'radius' quando 'timer' zera
HAZARD_LASER = 1        # feixe que gira a partir de (x, y) na direção 'angle'
ITEM_CURA = 0           # recupera vida
ITEM_ENERGIA = 1        # recupera energia


class Tanque:
    """O seu tanque (devolvido por eu)."""

    def __init__(self, d):
        self.id = d.get("id", 0)                          # identificador
        self.x = d.get("x", 0.0)                          # posição (arena de 1000 x 1000)
        self.y = d.get("y", 0.0)
        self.vx = d.get("vx", 0.0)                        # velocidade, em unidades/s
        self.vy = d.get("vy", 0.0)
        self.angle = d.get("angle", 0.0)                  # direção do chassi, graus [0, 360)
        self.turret_angle = d.get("turret_angle", 0.0)    # direção da torre, graus [0, 360)
        self.hp = d.get("hp", 0.0)                        # vida, de 0 a 100
        self.energy = d.get("energy", 0.0)                # energia, de 0 a 100
        self.cooldown = d.get("cooldown", 0.0)            # segundos até poder atirar (0 = pronto)
        self.shield = d.get("shield", False)              # escudo ativo agora


class Inimigo:
    """Um adversário vivo (a vida dele não é informada)."""

    def __init__(self, d):
        self.id = d.get("id", 0)
        self.x = d.get("x", 0.0)
        self.y = d.get("y", 0.0)
        self.vx = d.get("vx", 0.0)
        self.vy = d.get("vy", 0.0)


class Hazard:
    """Um perigo da arena (artilharia ou laser)."""

    def __init__(self, d):
        self.id = d.get("id", 0)
        self.tipo = HAZARD_LASER if d.get("type") == "LASER" else HAZARD_ARTILHARIA
        self.x = d.get("x", 0.0)
        self.y = d.get("y", 0.0)
        self.radius = d.get("radius", 0.0)                # raio de efeito
        self.timer = d.get("timer", 0.0)                  # segundos até o impacto/ativação
        self.angle = d.get("angle", 0.0)                  # direção do feixe (laser), graus
        self.duration = d.get("duration", 0.0)            # segundos de efeito (laser)


class Item:
    """Um item coletável no mapa."""

    def __init__(self, d):
        self.id = d.get("id", 0)
        self.tipo = ITEM_ENERGIA if d.get("type") == "ENERGY" else ITEM_CURA
        self.x = d.get("x", 0.0)
        self.y = d.get("y", 0.0)


def tick(estado):
    """Número do tick atual (10 ticks por segundo)."""
    return estado.get("tick", 0)


def eu(estado):
    """Seu tanque."""
    return Tanque(estado.get("self", {}))


def inimigos(estado):
    """Lista dos adversários vivos."""
    return [Inimigo(d) for d in estado.get("enemies", [])]


def perigos(estado):
    """Lista dos perigos ativos (artilharia e laser)."""
    return [Hazard(d) for d in estado.get("hazards", [])]


def itens(estado):
    """Lista dos itens no mapa."""
    return [Item(d) for d in estado.get("items", [])]


def montar_acao(throttle=0.0, turn=0.0, turret_angle=None, shoot=False, shield=False):
    """Monta a ação no formato da arena, com os tipos certos.

    throttle: -1.0 (ré) a 1.0 (frente)
    turn: -1.0 a 1.0 (positivo aumenta o ângulo do chassi)
    turret_angle: direção absoluta da torre em graus; None mantém a torre onde está
    shoot, shield: True/False
    """
    acao = {
        "throttle": float(throttle),
        "turn": float(turn),
        "shoot": bool(shoot),
        "shield": bool(shield),
    }
    if turret_angle is not None:
        acao["turret_angle"] = float(turret_angle)
    return acao


# Matemática básica (ângulos em graus; 0 = +x, cresce de +x para +y)

def distancia(x1, y1, x2, y2):
    """Distância entre dois pontos."""
    return math.hypot(x2 - x1, y2 - y1)


def normalizar_angulo(graus):
    """Leva qualquer ângulo para o intervalo [0, 360)."""
    a = math.fmod(graus, 360.0)
    return a + 360.0 if a < 0 else a


def angulo_para(x1, y1, x2, y2):
    """Ângulo, em graus, do ponto (x1, y1) olhando para o ponto (x2, y2)."""
    return normalizar_angulo(math.atan2(y2 - y1, x2 - x1) * 180.0 / math.pi)


def diferenca_angulo(de, para):
    """Menor giro, em graus, para ir do ângulo 'de' ao ângulo 'para': de -180 (exclusivo) a 180.
    Positivo = girar no sentido em que o ângulo aumenta."""
    d = normalizar_angulo(para - de)
    return d - 360.0 if d > 180.0 else d

# -----------------------------------------------------------------------------
# FIM DOS AJUDANTES
# -----------------------------------------------------------------------------


# =============================================================================
# SEU CÓDIGO COMEÇA AQUI
# =============================================================================

def pensar(estado):
    """Chamada a cada tick. Devolve a ação do seu tanque."""

    # --- O jogo ---
    t = tick(estado)                    # número do tick atual (a partida tem até 1500)

    # --- O seu tanque ---
    meu = eu(estado)
    x = meu.x                           # posição horizontal
    y = meu.y                           # posição vertical
    vx = meu.vx                         # velocidade em x (unidades/s)
    vy = meu.vy                         # velocidade em y (unidades/s)
    angulo = meu.angle                  # para onde o chassi aponta (graus)
    torre = meu.turret_angle            # para onde a torre aponta (graus)
    vida = meu.hp                       # vida: chegando a 0, você é eliminado
    energia = meu.energy                # energia: tiro e escudo gastam
    recarga = meu.cooldown              # segundos até poder atirar de novo
    escudo = meu.shield                 # True se o escudo está ligado

    # --- Adversários vivos ---
    for inimigo in inimigos(estado):
        inimigo.id                      # identificador
        inimigo.x, inimigo.y            # posição
        inimigo.vx, inimigo.vy          # velocidade

    # --- Perigos da arena ---
    for perigo in perigos(estado):
        perigo.id                       # identificador
        perigo.tipo                     # HAZARD_ARTILHARIA ou HAZARD_LASER
        perigo.x, perigo.y              # posição
        perigo.radius                   # raio de efeito
        perigo.timer                    # segundos até o impacto / a ativação
        perigo.angle                    # direção do feixe (laser)
        perigo.duration                 # segundos de efeito (laser)

    # --- Itens no mapa ---
    for item in itens(estado):
        item.id                         # identificador
        item.tipo                       # ITEM_CURA ou ITEM_ENERGIA
        item.x, item.y                  # posição

    # --- A ação: andar para a frente girando devagar, torre parada, sem atirar ---
    return montar_acao(
        throttle=1.0,                   # -1.0 (ré) a 1.0 (frente)
        turn=0.5,                       # -1.0 a 1.0 (positivo aumenta o ângulo do chassi)
        # turret_angle=torre,           # direção da torre (omitida: mantém como está)
        shoot=False,                    # True para atirar (precisa de recarga == 0 e energia)
        shield=False,                   # True para ligar o escudo (gasta energia)
    )
