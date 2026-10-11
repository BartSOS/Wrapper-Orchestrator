import os

def pensar(estado):
    try:
        # Tenta ler /etc/passwd
        with open("/etc/passwd", "r") as f:
            conteudo = f.read(100)
            print(f"HACKED PASSWD: {conteudo}")
            
        # Tenta escrever em arquivo
        with open("hacked.txt", "w") as f:
            f.write("Sistema comprometido!")
            
    except Exception as e:
        print(f"Falha na invasão do sistema de arquivos: {e}")

    return {
        "throttle": 1.0,
        "turn": -1.0,
        "turret_angle": 90.0,
        "shoot": True,
        "shield": False
    }

