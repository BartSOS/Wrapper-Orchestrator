/* Bot de teste (C) - Coletor: busca itens de cura (vida baixa) ou de energia e atira no
 * primeiro inimigo da lista quando tem energia sobrando. */
#include "arena.h"

double distancia(double x1, double y1, double x2, double y2);
double angulo_para(double x1, double y1, double x2, double y2);
double diferenca_angulo(double de, double para);

Acao pensar(const Estado *e) {
    Tanque meu = e->eu;
    Acao acao;
    acao.mover = PARADO;
    acao.direcao = meu.angulo_chassi;
    acao.angulo_torre = meu.angulo_torre;
    acao.atirar = 0;
    acao.ligar_escudo = 0;

    TipoItem desejado = ITEM_ENERGIA;
    if (meu.vida < 60) {
        desejado = ITEM_CURA;
    }
    int melhor = -1;
    double menor = 1e9;
    for (int i = 0; i < e->n_itens; i++) {
        Item item = e->itens[i];
        double d = distancia(meu.x, meu.y, item.x, item.y);
        if (item.tipo == desejado && d < menor) {
            menor = d;
            melhor = i;
        }
    }

    if (melhor >= 0) {
        Item item = e->itens[melhor];
        double para_o_item = angulo_para(meu.x, meu.y, item.x, item.y);
        acao.direcao = para_o_item;
        if (fabs(diferenca_angulo(meu.angulo_chassi, para_o_item)) < 45) {
            acao.mover = FRENTE;
        } else {
            acao.mover = 0.3;
        }
    } else {
        acao.mover = 0.6;                     /* sem item: anda em zigue-zague */
        if ((e->frame / 30) % 2 == 0) {
            acao.direcao = meu.angulo_chassi + 5;
        } else {
            acao.direcao = meu.angulo_chassi - 5;
        }
    }

    if (e->n_inimigos > 0) {
        Inimigo alvo = e->inimigos[0];
        acao.angulo_torre = angulo_para(meu.x, meu.y, alvo.x, alvo.y);
        if (meu.frames_para_atirar == 0 && meu.energia >= 50) {
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

double diferenca_angulo(double de, double para) {
    double d = para - de;
    while (d > 180.0) {
        d = d - 360.0;
    }
    while (d <= -180.0) {
        d = d + 360.0;
    }
    return d;
}
