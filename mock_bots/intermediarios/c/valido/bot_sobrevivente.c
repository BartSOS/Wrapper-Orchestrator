/* Bot de teste (C) - Sobrevivente: foge da artilharia, fica perto do centro e atira no
 * primeiro inimigo da lista sem parar de se mover. */
#include "arena.h"

#define CENTRO 500.0

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
    double alvo_x = CENTRO;
    double alvo_y = CENTRO;

    /* Dentro de uma artilharia: foge na direção oposta ao centro dela */
    int fugindo = 0;
    for (int i = 0; i < e->n_perigos; i++) {
        Perigo p = e->perigos[i];
        double d = distancia(meu.x, meu.y, p.x, p.y);
        if (fugindo == 0 && p.tipo == PERIGO_ARTILHARIA && d < p.raio + 30) {
            alvo_x = meu.x + (meu.x - p.x);
            alvo_y = meu.y + (meu.y - p.y);
            if (meu.energia > 60) {
                acao.ligar_escudo = 1;
            }
            fugindo = 1;
        }
    }

    if (distancia(meu.x, meu.y, alvo_x, alvo_y) > 60) {
        acao.direcao = angulo_para(meu.x, meu.y, alvo_x, alvo_y);
        acao.mover = FRENTE;
    }

    if (e->n_inimigos > 0) {
        Inimigo in = e->inimigos[0];
        acao.angulo_torre = angulo_para(meu.x, meu.y, in.x, in.y);
        if (meu.frames_para_atirar == 0 && meu.energia >= 40) {
            acao.atirar = 1;
        }
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
