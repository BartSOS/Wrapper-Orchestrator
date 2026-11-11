/* =============================================================================
 * NPC "radar" - nível MÉDIO (C99)
 * =============================================================================
 * Estratégia: mira no adversário mais próximo e o persegue, segurando uma
 * distância de tiro (aproxima se está longe, recua se está colado). Atira quando
 * a recarga permite, sai do raio de uma artilharia e liga o escudo com a vida
 * baixa. Não antecipa o movimento do adversário e não trata o laser nem a zona.
 * ============================================================================= */

#include "arena.h"

/* Estas funções estão no fim do arquivo (AJUDANTES) */
double distancia(double x1, double y1, double x2, double y2);
double angulo_para(double x1, double y1, double x2, double y2);
double diferenca_angulo(double de, double para);


Acao pensar(const Estado *e) {
    Tanque meu = e->eu;

    Acao acao;
    acao.mover = 0.5;
    acao.direcao = meu.angulo_chassi + 20;  /* sem adversários: gira devagar */
    acao.angulo_torre = meu.angulo_torre;
    acao.atirar = 0;
    acao.ligar_escudo = 0;

    /* Adversário mais próximo */
    int alvo = -1;
    double menor = 1e9;
    for (int i = 0; i < e->n_inimigos; i++) {
        Inimigo inimigo = e->inimigos[i];
        double d = distancia(meu.x, meu.y, inimigo.x, inimigo.y);
        if (d < menor) {
            menor = d;
            alvo = i;
        }
    }

    if (alvo >= 0) {
        Inimigo inimigo = e->inimigos[alvo];
        double para_o_alvo = angulo_para(meu.x, meu.y, inimigo.x, inimigo.y);
        acao.angulo_torre = para_o_alvo;
        if (menor < 450 && meu.frames_para_atirar == 0 && meu.energia >= 20) {
            acao.atirar = 1;
        }
        if (menor > 300) {
            acao.direcao = para_o_alvo;         /* aproxima */
            acao.mover = FRENTE;
        } else if (menor < 180) {
            acao.direcao = para_o_alvo + 180;   /* recua */
            acao.mover = FRENTE;
        } else {
            acao.direcao = para_o_alvo;         /* mantém a distância de tiro */
            acao.mover = 0.2;
        }
    }

    /* Dentro do raio de uma artilharia: sai dele (prioridade máxima) */
    for (int i = 0; i < e->n_perigos; i++) {
        Perigo perigo = e->perigos[i];
        if (perigo.tipo == PERIGO_ARTILHARIA
            && distancia(meu.x, meu.y, perigo.x, perigo.y) < perigo.raio + 40) {
            acao.direcao = angulo_para(perigo.x, perigo.y, meu.x, meu.y);
            acao.mover = FRENTE;
            break;
        }
    }

    if (meu.vida < 35 && meu.energia > 60) {
        acao.ligar_escudo = 1;
    }
    return acao;
}


/* -----------------------------------------------------------------------------
 * AJUDANTES (opcional: use, mude ou apague)
 * -----------------------------------------------------------------------------
 * Matemática básica, com ângulos em graus (0 aponta para +x; 90, para +y).
 * Elas só calculam: não decidem nada por você.
 * ----------------------------------------------------------------------------- */

/* Distância entre o ponto (x1, y1) e o ponto (x2, y2) */
double distancia(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;                    /* quanto falta em x */
    double dy = y2 - y1;                    /* quanto falta em y */
    return sqrt(dx * dx + dy * dy);         /* Pitágoras */
}

/* Ângulo, em graus (de 0 a 360), de quem está em (x1, y1) olhando para (x2, y2) */
double angulo_para(double x1, double y1, double x2, double y2) {
    double radianos = atan2(y2 - y1, x2 - x1);  /* atan2 devolve radianos, de -pi a pi */
    double graus = radianos * 180.0 / M_PI;     /* em graus: de -180 a 180 */
    if (graus < 0) {
        graus = graus + 360.0;                  /* passa para 0 a 360 */
    }
    return graus;
}

/* Quanto girar para ir do ângulo "de" ao ângulo "para" pelo caminho mais
 * curto: de -180 a 180 graus. Positivo = o ângulo aumenta; negativo = diminui. */
double diferenca_angulo(double de, double para) {
    double d = para - de;
    while (d > 180.0) {
        d = d - 360.0;                      /* mais de meia volta: vai pelo outro lado */
    }
    while (d <= -180.0) {
        d = d + 360.0;
    }
    return d;
}

/* -----------------------------------------------------------------------------
 * FIM DOS AJUDANTES
 * ----------------------------------------------------------------------------- */
