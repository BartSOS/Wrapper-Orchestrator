"""
TOPCOM ARENA - STARTER KIT PYTHON 3

Instruções para o competidor:
- Implemente apenas a função pensar(estado).
- Não utilize input(), print() nem imports de sistema.
- A arena injeta automaticamente o wrapper de comunicação.
"""

def pensar(estado):
    """
    Função invocada a cada turno (tick) da partida.

    Parâmetros:
      estado (dict):
        - self: {"x": float, "y": float, "angle": float, "hp": float, "energy": float, "cooldown": float}
        - enemies: [{"x": float, "y": float, "hp": float}, ...]
        - items: [{"x": float, "y": float, "type": "HEAL"|"ENERGY"}, ...]

    Retorno (dict com comandos da rodada):
      - throttle: float (-1.0 a 1.0) -> Aceleração (frente / ré)
      - turn: float (-1.0 a 1.0)     -> Rotação do chassi (esquerda / direita)
      - turret_angle: float (0 a 360)-> Ângulo da mira da torre
      - shoot: bool (True/False)     -> Disparar projétil
      - shield: bool (True/False)    -> Ativar escudo de defesa
    """
    me = estado.get("self", {})
    meu_angulo = me.get("angle", 0.0)

    # Exemplo de ação inicial: andar para frente e girar suavemente
    return {
        "throttle": 1.0,
        "turn": 0.5,
        "turret_angle": meu_angulo,
        "shoot": False,
        "shield": False
    }
