"""
TOPCOM ARENA - STARTER KIT PYTHON 3

Instruções para o competidor:
- Implemente apenas a função pensar(estado).
- Não utilize input() nem imports de sistema (os, subprocess, eval, exec).
- print() não chega à arena: é desviado para o log de erros e descartado.
- A arena injeta automaticamente o wrapper de comunicação.
"""

def pensar(estado):
    """
    Função invocada a cada turno (tick) da partida (10 ticks por segundo).

    Parâmetros:
      estado (dict):
        - tick: int (número do tick atual)
        - self: {"id": int, "x": float, "y": float, "vx": float, "vy": float,
                 "angle": float, "turret_angle": float, "hp": float, "energy": float,
                 "cooldown": float, "shield": bool}
        - enemies: [{"id": int, "x": float, "y": float, "vx": float, "vy": float}, ...]
                   (só adversários vivos; a vida deles não é informada)
        - hazards: [{"id": int, "type": "ARTILLERY"|"LASER", "x": float, "y": float,
                     "radius": float, "timer": float, "angle": float, "duration": float}, ...]
                   ARTILLERY explode em (x, y) com raio "radius" quando "timer" chega a 0.
                   LASER gira a partir de (x, y) na direção "angle"; queima por "duration"
                   segundos depois que "timer" chega a 0.
        - items: [{"id": int, "type": "HEAL"|"ENERGY", "x": float, "y": float}, ...]
      Arena de 1000 x 1000; ângulos em graus [0, 360); velocidades em unidades/s;
      cooldown, timer e duration em segundos.

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
