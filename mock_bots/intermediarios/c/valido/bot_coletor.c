#include "arena.h"

/* Matematica basica (as mesmas funcoes do AJUDANTES do starter kit; antes ficavam no arena.h) */
static inline double distancia(double x1, double y1, double x2, double y2) {
    return hypot(x2 - x1, y2 - y1);
}

static inline double normalizar_angulo(double graus) {
    double a = fmod(graus, 360.0);
    return a < 0 ? a + 360.0 : a;
}

static inline double angulo_para(double x1, double y1, double x2, double y2) {
    return normalizar_angulo(atan2(y2 - y1, x2 - x1) * 180.0 / M_PI);
}

static inline double diferenca_angulo(double de, double para) {
    double d = normalizar_angulo(para - de);
    return d > 180.0 ? d - 360.0 : d;
}

/* Bot de teste (C) - Coletor: prioriza itens de cura/energia quando precisa. */

Acao pensar(const Estado *e) {
    Acao a = {0};
    a.turret_angle = e->self.turret_angle;

    TipoItem desejado = e->self.hp < 60 ? ITEM_CURA : ITEM_ENERGIA;
    int melhor = -1;
    double menor = 1e9;
    for (int i = 0; i < e->n_itens; i++) {
        if (e->itens[i].tipo != desejado) continue;
        double d = distancia(e->self.x, e->self.y, e->itens[i].x, e->itens[i].y);
        if (d < menor) {
            menor = d;
            melhor = i;
        }
    }

    if (melhor >= 0) {
        double ang = angulo_para(e->self.x, e->self.y, e->itens[melhor].x, e->itens[melhor].y);
        double diff = diferenca_angulo(e->self.angle, ang);
        a.turn = diff > 5 ? 1.0 : (diff < -5 ? -1.0 : 0.0);
        a.throttle = fabs(diff) < 45 ? 1.0 : 0.3;
    } else {
        a.throttle = 0.6;
        a.turn = (e->tick / 30) % 2 == 0 ? 0.5 : -0.5;
    }

    if (e->n_inimigos > 0) {
        a.turret_angle = angulo_para(e->self.x, e->self.y, e->inimigos[0].x, e->inimigos[0].y);
        a.shoot = e->self.cooldown <= 0 && e->self.energy >= 50;
    }
    return a;
}
