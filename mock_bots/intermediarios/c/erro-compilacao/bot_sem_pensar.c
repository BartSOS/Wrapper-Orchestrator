/* Bot de teste (C) - ERRO PROPOSITAL: não implementa a função de decisão exigida. */

Acao decidir(const Estado *e) {
    Acao a = {0};
    a.throttle = e->self.hp > 50 ? 1.0 : -1.0;
    return a;
}
