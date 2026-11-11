# =============================================================================
# GAME JAM PET 2026 - Arquivo de apoio para bots em Python
# =============================================================================
# Este arquivo vem no kit e no zip das fases (deixe-o na mesma pasta do seu
# bot.py) e já existe no servidor: você NÃO envia o arena.py, só o seu bot.py.
# No seu bot, a primeira linha é:
#
#     from arena import *
#
# e você escreve só a função pensar(estado), que devolve uma Acao:
#
#     def pensar(estado):
#         acao = Acao()
#         acao.mover = FRENTE
#         return acao
#
# Ela é chamada 10 vezes por segundo (uma vez por frame).
#
# Unidades:
#   tempo ........ frames (1 frame = 0,1 segundo; 10 frames por segundo)
#   posição ...... unidades da arena (o mapa tem 1000 x 1000)
#   velocidade ... unidades por frame
#   ângulos ...... graus, de 0 a 360; 0 aponta para +x e o ângulo cresce de +x
#                  para +y (90 aponta para +y)
#
# O que tem no estado (todos os nomes em português):
#   estado.frame       número do frame (começa em 0)
#   estado.eu          o seu tanque (Tanque)
#   estado.inimigos    lista de Inimigo
#   estado.perigos     lista de Perigo
#   estado.itens       lista de Item
#   estado.pontos      lista de Ponto (só nas fases de treino; no torneio é vazia)
# =============================================================================

import math
import sys

# O que o "from arena import *" traz para o seu bot
__all__ = ["FRENTE", "PARADO", "RE", "PERIGO_ARTILHARIA", "PERIGO_LASER", "ITEM_CURA", "ITEM_ENERGIA",
           "Estado", "Tanque", "Inimigo", "Perigo", "Item", "Ponto", "Acao"]

# Valores para acao.mover
FRENTE = 1    # anda para a frente: 5 unidades por frame
PARADO = 0    # fica parado (para no mesmo frame, sem inércia)
RE = -1       # anda de ré: 5 unidades por frame para trás

# Tipos de perigo e de item
PERIGO_ARTILHARIA = "ARTILHARIA"
PERIGO_LASER = "LASER"
ITEM_CURA = "CURA"
ITEM_ENERGIA = "ENERGIA"

_SEGUNDOS_POR_FRAME = 0.1   # TICK_DT da engine
_GIRO_POR_FRAME = 9.0       # giro máximo do chassi: 90 graus/s * 0,1 s


def _num(d, chave):
    v = d.get(chave) if isinstance(d, dict) else None
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        return 0.0
    return float(v)


def _frames(d, chave):
    return int(round(_num(d, chave) / _SEGUNDOS_POR_FRAME))


def _por_frame(d, chave):
    return _num(d, chave) * _SEGUNDOS_POR_FRAME


class _Registro:
    """Base das classes do estado: mostra os campos quando impressa (print)."""
    __slots__ = ()

    def __repr__(self):
        campos = ", ".join(f"{n}={getattr(self, n)!r}" for n in self.__slots__)
        return f"{type(self).__name__}({campos})"

    def __getattr__(self, nome):
        _campo_inexistente(self, nome)


# Nomes antigos (em inglês) -> nomes atuais, para a mensagem de erro
_NOMES_NOVOS = {
    "tick": "frame", "self": "eu", "enemies": "inimigos", "hazards": "perigos", "items": "itens",
    "vx": "vel_x", "vy": "vel_y", "angle": "angulo_chassi", "turret_angle": "angulo_torre",
    "hp": "vida", "energy": "energia", "cooldown": "frames_para_atirar", "shield": "escudo_ativo",
    "radius": "raio", "timer": "frames_ate_impacto", "duration": "frames_restantes", "type": "tipo",
}


def _campo_inexistente(objeto, nome):
    classe = type(objeto).__name__
    if nome.startswith("__"):
        raise AttributeError(nome)
    if nome in _NOMES_NOVOS:
        raise AttributeError(f"o campo '{nome}' agora se chama '{_NOMES_NOVOS[nome]}' ({classe}).")
    raise AttributeError(f"{classe} nao tem o campo '{nome}'.")


class Tanque(_Registro):
    """O seu tanque."""
    __slots__ = ("id", "x", "y", "vel_x", "vel_y", "angulo_chassi", "angulo_torre",
                 "vida", "energia", "frames_para_atirar", "escudo_ativo")

    def __init__(self, d):
        self.id = int(_num(d, "id"))
        self.x = _num(d, "x")                          # posição do centro
        self.y = _num(d, "y")
        self.vel_x = _por_frame(d, "vx")               # unidades por frame
        self.vel_y = _por_frame(d, "vy")
        self.angulo_chassi = _num(d, "angle")          # para onde o chassi aponta (graus)
        self.angulo_torre = _num(d, "turret_angle")    # para onde a torre aponta (graus)
        self.vida = _num(d, "hp")                      # 0 a 100
        self.energia = _num(d, "energy")               # 0 a 100
        self.frames_para_atirar = _frames(d, "cooldown")  # 0 = já pode atirar
        self.escudo_ativo = bool(d.get("shield")) if isinstance(d, dict) else False


class Inimigo(_Registro):
    """Um inimigo vivo."""
    __slots__ = ("id", "x", "y", "vel_x", "vel_y")

    def __init__(self, d):
        self.id = int(_num(d, "id"))
        self.x = _num(d, "x")
        self.y = _num(d, "y")
        self.vel_x = _por_frame(d, "vx")
        self.vel_y = _por_frame(d, "vy")


class Perigo(_Registro):
    """Perigo da arena: PERIGO_ARTILHARIA ou PERIGO_LASER."""
    __slots__ = ("id", "tipo", "x", "y", "raio", "frames_ate_impacto", "angulo", "frames_restantes")

    def __init__(self, d):
        self.id = int(_num(d, "id"))
        self.tipo = PERIGO_LASER if d.get("type") == "LASER" else PERIGO_ARTILHARIA
        self.x = _num(d, "x")
        self.y = _num(d, "y")
        self.raio = _num(d, "radius")                     # alcance do efeito
        self.frames_ate_impacto = _frames(d, "timer")     # 0 = já está ativo
        self.angulo = _num(d, "angle")                    # direção do feixe do laser (graus)
        self.frames_restantes = _frames(d, "duration")    # frames que o laser ainda fica ligado


class Item(_Registro):
    """Item que o tanque coleta passando por cima: ITEM_CURA ou ITEM_ENERGIA."""
    __slots__ = ("id", "tipo", "x", "y")

    def __init__(self, d):
        self.id = int(_num(d, "id"))
        self.tipo = ITEM_ENERGIA if d.get("type") == "ENERGY" else ITEM_CURA
        self.x = _num(d, "x")
        self.y = _num(d, "y")


class Ponto(_Registro):
    """Ponto do objetivo de uma fase de treino (ex.: o ponto "B")."""
    __slots__ = ("id", "x", "y", "raio")

    def __init__(self, d):
        self.id = str(d.get("id", ""))
        self.x = _num(d, "x")
        self.y = _num(d, "y")
        self.raio = _num(d, "raio")   # o tanque está no ponto se o centro dele estiver a até raio


def _lista(d, chave, classe):
    v = d.get(chave)
    if not isinstance(v, list):
        return []
    return [classe(x) for x in v if isinstance(x, dict)]


class Estado(dict):
    """O que o seu tanque enxerga neste frame (os nomes estão no topo deste arquivo).

    Também é um dict com o JSON original da arena (estado["self"], estado["tick"]...),
    para os bots escritos antes dos nomes em português."""

    def __init__(self, bruto):
        super().__init__(bruto if isinstance(bruto, dict) else {})
        self.frame = int(_num(self, "tick"))
        self.eu = Tanque(self.get("self") if isinstance(self.get("self"), dict) else {})
        self.inimigos = _lista(self, "enemies", Inimigo)
        self.perigos = _lista(self, "hazards", Perigo)
        self.itens = _lista(self, "items", Item)
        objetivo = self.get("objetivo")
        self.pontos = _lista(objetivo, "pontos", Ponto) if isinstance(objetivo, dict) else []
        self.n_inimigos = len(self.inimigos)
        self.n_perigos = len(self.perigos)
        self.n_itens = len(self.itens)
        self.n_pontos = len(self.pontos)

    def __getattr__(self, nome):
        _campo_inexistente(self, nome)


class Acao:
    """O que o seu tanque faz neste frame.

    mover          FRENTE, PARADO ou RE (0.5 = metade da velocidade)
    direcao        para onde o chassi deve apontar (graus). O tanque gira até 9 graus
                   por frame até chegar lá. None = mantém a direção atual.
    angulo_torre   para onde a torre aponta (graus); muda na hora. None = mantém.
    atirar         True (ou 1) atira; False (ou 0) não atira
    ligar_escudo   True (ou 1) liga o escudo neste frame; False (ou 0) desliga
    """
    __slots__ = ("mover", "direcao", "angulo_torre", "atirar", "ligar_escudo")

    def __init__(self, mover=PARADO, direcao=None, angulo_torre=None, atirar=False, ligar_escudo=False):
        self.mover = mover
        self.direcao = direcao
        self.angulo_torre = angulo_torre
        self.atirar = atirar
        self.ligar_escudo = ligar_escudo

    def __setattr__(self, nome, valor):
        if nome not in Acao.__slots__:
            if nome in _CHAVES_ANTIGAS:
                raise AttributeError(f"o campo '{nome}' da Acao agora se chama '{_CHAVES_ANTIGAS[nome]}'.")
            raise AttributeError(f"a Acao nao tem o campo '{nome}'. Os campos sao: "
                                 "mover, direcao, angulo_torre, atirar, ligar_escudo.")
        object.__setattr__(self, nome, valor)

    def __repr__(self):
        return (f"Acao(mover={self.mover!r}, direcao={self.direcao!r}, angulo_torre={self.angulo_torre!r}, "
                f"atirar={self.atirar!r}, ligar_escudo={self.ligar_escudo!r})")


# -----------------------------------------------------------------------------
# Daqui para baixo: tradução da Acao para o comando da engine (uso interno da arena)
# -----------------------------------------------------------------------------
_CHAVES_ANTIGAS = {"throttle": "mover", "turn": "direcao", "turret_angle": "angulo_torre",
                   "shoot": "atirar", "shield": "ligar_escudo"}
_CHAVES = ("mover", "direcao", "angulo_torre", "atirar", "ligar_escudo")
_avisados = set()


def _avisar(chave, texto):
    """Escreve o aviso no log do bot uma vez só (stderr; na arena ele vai para o log da partida)."""
    if chave in _avisados:
        return
    _avisados.add(chave)
    print(f"[AVISO] {texto}", file=sys.stderr)
    sys.stderr.flush()


def _numero(valor, nome):
    """Número finito ou None (com aviso)."""
    if valor is None:
        return None
    if isinstance(valor, bool):
        return 1.0 if valor else 0.0
    if isinstance(valor, (int, float)) and math.isfinite(valor):
        return float(valor)
    _avisar("tipo_" + nome, f"'{nome}' recebeu {valor!r}, que nao e um numero: ignorado neste e nos proximos frames.")
    return None


def _turn_para(direcao, angulo_atual):
    """direcao desejada -> turn da engine (diferença no menor sentido / 9, entre -1 e 1)."""
    dif = math.fmod(direcao - angulo_atual, 360.0)
    if dif > 180.0:
        dif -= 360.0
    if dif < -180.0:
        dif += 360.0
    return max(-1.0, min(1.0, dif / _GIRO_POR_FRAME))


def _traduzir_acao(acao, estado):
    """Acao (ou dict com as chaves novas ou antigas) -> dict do protocolo da engine."""
    if isinstance(acao, Acao):
        campos = {n: getattr(acao, n) for n in _CHAVES}
    elif isinstance(acao, dict):
        campos = {}
        for chave, valor in acao.items():
            if chave in _CHAVES:
                campos[chave] = valor
            elif chave in _CHAVES_ANTIGAS:
                nova = _CHAVES_ANTIGAS[chave]
                if chave == "turn":
                    _avisar(chave, "use 'direcao' (para onde o tanque deve apontar, em graus) em vez de 'turn'. "
                                   "A chave antiga ainda funciona.")
                else:
                    _avisar(chave, f"use '{nova}' em vez de '{chave}' na acao. A chave antiga ainda funciona.")
                campos.setdefault("_" + chave, valor)
            else:
                _avisar("desconhecida_" + str(chave),
                        f"a acao tem a chave '{chave}', que nao existe e foi ignorada. "
                        "Use: mover, direcao, angulo_torre, atirar, ligar_escudo.")
    else:
        raise TypeError(f"pensar() devolveu {type(acao).__name__}; devolva uma Acao (faltou 'return acao'?)")

    eu = estado.eu if isinstance(estado, Estado) else Estado(estado).eu
    saida = {}

    mover = _numero(campos.get("mover", campos.get("_throttle")), "mover")
    saida["throttle"] = max(-1.0, min(1.0, mover if mover is not None else 0.0))

    direcao = _numero(campos.get("direcao"), "direcao")
    if direcao is not None:
        saida["turn"] = _turn_para(direcao, eu.angulo_chassi)
    else:
        turn = _numero(campos.get("_turn"), "turn")
        saida["turn"] = max(-1.0, min(1.0, turn)) if turn is not None else 0.0

    torre = _numero(campos.get("angulo_torre", campos.get("_turret_angle")), "angulo_torre")
    if torre is not None:
        saida["turret_angle"] = torre

    saida["shoot"] = bool(campos.get("atirar", campos.get("_shoot", False)))
    saida["shield"] = bool(campos.get("ligar_escudo", campos.get("_shield", False)))
    return saida
