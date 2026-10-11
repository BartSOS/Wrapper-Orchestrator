def pensar(estado):
    if not hasattr(pensar, "mem"):
        pensar.mem = []
    
    # Aloca muita memória repetidamente
    try:
        for _ in range(20):
            pensar.mem.append("A" * 10_000_000) # 10MB por string
    except MemoryError:
        pass

    return {
        "throttle": 0.0,
        "turn": 0.0,
        "turret_angle": 0.0,
        "shoot": False,
        "shield": False
    }

