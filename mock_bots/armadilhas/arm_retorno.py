# Armadilha: o pensar() lança exceção ou devolve algo que não é um objeto JSON em
# alguns ticks (None, lista, NaN). Antes, um único tick assim dessincronizava o bot,
# que era eliminado por timeout 20 ticks depois. Agora esse tick vale como {} (parado).
def pensar(estado):
    t = estado["tick"]
    if t % 13 == 1:
        raise ValueError("erro proposital")
    if t % 13 == 4:
        return None
    if t % 13 == 7:
        return [1, 2, 3]
    if t % 13 == 10:
        return {"throttle": float("nan")}
    return {"throttle": 0.8, "turn": -0.2}
