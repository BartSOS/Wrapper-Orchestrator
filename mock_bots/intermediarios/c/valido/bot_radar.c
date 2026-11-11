/* Bot de teste (C) - Radar: mira e persegue o inimigo mais próximo. */
#include "arena.h"

double distancia(double x1, double y1, double x2, double y2);
double angulo_para(double x1, double y1, double x2, double y2);

Acao pensar(const Estado *e) {
    Tanque meu = e->eu;
    Acao acao;
    acao.mover = PARADO;
    acao.direcao = meu.angulo_chassi;
    acao.angulo_torre = meu.angulo_torre;
    acao.atirar = 0;
    acao.ligar_escudo = 0;

    int alvo = -1;
    double menor = 1e9;
    for (int i = 0; i < e->n_inimigos; i++) {
        Inimigo in = e->inimigos[i];
        double d = distancia(meu.x, meu.y, in.x, in.y);
        if (d < menor) {
            menor = d;
            alvo = i;
        }
    }

    if (alvo >= 0) {
        Inimigo in = e->inimigos[alvo];
        double para_o_alvo = angulo_para(meu.x, meu.y, in.x, in.y);
        acao.angulo_torre = para_o_alvo;
        acao.direcao = para_o_alvo;
        if (menor > 200) {
            acao.mover = FRENTE;
        } else {
            acao.mover = 0.2;
        }
        if (menor < 450 && meu.frames_para_atirar == 0 && meu.energia >= 20) {
            acao.atirar = 1;
        }
    } else {
        acao.mover = 0.5;                     /* ninguém à vista: anda em círculo */
        acao.direcao = meu.angulo_chassi + 3;
    }

    if (meu.vida < 30 && meu.energia > 60) {
        acao.ligar_escudo = 1;
    }
    return acao;
}

double distancia(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

double angulo_para(double x1, double y1, double x2, double y2) {
    double graus = atan2(y2 - y1, x2 - x1) * 180.0 / M_PI;
    if (graus < 0) {
        graus = graus + 360.0;
    }
    return graus;
}
