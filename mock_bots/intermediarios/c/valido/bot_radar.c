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

/* Bot de teste (C) - Radar: mira e persegue o inimigo mais próximo. */

Acao pensar(const Estado *e) {
    Acao a = {0};
    a.turret_angle = e->self.turret_angle;

    int alvo = -1;
    double menor = 1e9;
    for (int i = 0; i < e->n_inimigos; i++) {
        double d = distancia(e->self.x, e->self.y, e->inimigos[i].x, e->inimigos[i].y);
        if (d < menor) {
            menor = d;
            alvo = i;
        }
    }

    if (alvo >= 0) {
        const Inimigo *in = &e->inimigos[alvo];
        double ang = angulo_para(e->self.x, e->self.y, in->x, in->y);
        double diff = diferenca_angulo(e->self.angle, ang);
        a.turret_angle = ang;
        a.turn = diff > 5 ? 1.0 : (diff < -5 ? -1.0 : 0.0);
        a.throttle = menor > 200 ? 1.0 : 0.2;
        a.shoot = menor < 450 && e->self.cooldown <= 0 && e->self.energy >= 20;
    } else {
        a.throttle = 0.5;
        a.turn = 0.3;
    }

    a.shield = e->self.hp < 30 && e->self.energy > 60;
    return a;
}
