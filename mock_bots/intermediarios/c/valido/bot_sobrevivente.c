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

/* Bot de teste (C) - Sobrevivente: foge de perigos e fica perto do centro. */

#define CENTRO 500.0

Acao pensar(const Estado *e) {
    Acao a = {0};
    double alvo_x = CENTRO, alvo_y = CENTRO;

    /* Se estiver dentro de algum perigo, foge na direção oposta */
    for (int i = 0; i < e->n_hazards; i++) {
        const Hazard *h = &e->hazards[i];
        double d = distancia(e->self.x, e->self.y, h->x, h->y);
        if (h->tipo == HAZARD_ARTILHARIA && d < h->radius + 30) {
            alvo_x = e->self.x + (e->self.x - h->x);
            alvo_y = e->self.y + (e->self.y - h->y);
            a.shield = e->self.energy > 60;
            break;
        }
    }

    double ang = angulo_para(e->self.x, e->self.y, alvo_x, alvo_y);
    double diff = diferenca_angulo(e->self.angle, ang);
    a.turn = diff > 10 ? 1.0 : (diff < -10 ? -1.0 : 0.0);
    a.throttle = distancia(e->self.x, e->self.y, alvo_x, alvo_y) > 60 ? 1.0 : 0.0;

    /* Atira no inimigo mais próximo sem parar de se mover */
    if (e->n_inimigos > 0) {
        a.turret_angle = angulo_para(e->self.x, e->self.y, e->inimigos[0].x, e->inimigos[0].y);
        a.shoot = e->self.cooldown <= 0 && e->self.energy >= 40;
    } else {
        a.turret_angle = e->self.turret_angle;
    }
    return a;
}
