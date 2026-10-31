/* Bot de teste do modo cenário (frente engine), em C: vai até o primeiro ponto do
 * objetivo (Estado.pontos[0], arena.h) e para dentro do raio. Sem pontos, fica parado. */
#include "arena.h"

Acao pensar(const Estado *e) {
    Acao a = {0.0, 0.0, e->self.turret_angle, false, false};
    if (e->n_pontos < 1) return a;
    const Ponto *p = &e->pontos[0];
    double dx = p->x - e->self.x, dy = p->y - e->self.y;
    if (sqrt(dx * dx + dy * dy) <= p->raio * 0.5) return a;
    double alvo = atan2(dy, dx) * 180.0 / M_PI;
    double erro = fmod(alvo - e->self.angle + 540.0, 360.0) - 180.0;
    a.turn = erro / 9.0 > 1.0 ? 1.0 : (erro / 9.0 < -1.0 ? -1.0 : erro / 9.0);
    a.throttle = fabs(erro) < 30.0 ? 1.0 : 0.2;
    return a;
}
