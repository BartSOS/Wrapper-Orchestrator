/* =============================================================================
 * NPC "sobrevivente" - nível DIFÍCIL (C99)
 * =============================================================================
 * Estratégia: luta à distância sem se deixar encurralar (kiting). Recua quando o
 * adversário mais próximo chega a menos de 300 (atirando de costas: a torre gira
 * na hora), aproxima quando ele passa de 500 e, no meio-termo, circula trocando
 * o sentido da volta em intervalos irregulares, para não ser previsível. Atira
 * sempre que pode, com mira antecipada (o projétil anda 30 unidades por frame),
 * e guarda energia para o escudo só quando a vida está baixa. Busca cura e
 * energia próximas quando precisa, volta para dentro da zona segura antes de ela
 * fechar (prevê o raio 40 frames à frente) e foge da artilharia e do centro
 * quando há laser.
 * ============================================================================= */

#include "arena.h"

#define VELOCIDADE_PROJETIL 30.0    /* unidades por frame */
#define CENTRO              500.0   /* centro da arena (1000 x 1000) */

/* Estas funções estão no fim do arquivo */
double raio_seguro(int frame);
double sentido_da_volta(int frame, int id);
double mira_com_antecipacao(Tanque meu, Inimigo alvo);
int perto_da_borda(Tanque meu, double folga);
double distancia(double x1, double y1, double x2, double y2);
double angulo_para(double x1, double y1, double x2, double y2);
double diferenca_angulo(double de, double para);


Acao pensar(const Estado *e) {
    Tanque meu = e->eu;

    Acao acao;
    acao.mover = FRENTE;                    /* sempre na velocidade máxima */
    acao.angulo_torre = meu.angulo_torre;
    acao.atirar = 0;
    acao.ligar_escudo = 0;

    /* Sem adversários: volta para o centro */
    acao.direcao = angulo_para(meu.x, meu.y, CENTRO, CENTRO);
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
        /* Distância: recua quando ele chega perto, aproxima quando está longe e, no
         * meio-termo, circula trocando o sentido em intervalos irregulares */
        if (menor < 300.0 && !perto_da_borda(meu, 100.0)) {
            acao.direcao = para_o_alvo + 180.0;
        } else if (menor > 500.0) {
            acao.direcao = para_o_alvo;
        } else {
            acao.direcao = para_o_alvo + 90.0 * sentido_da_volta(e->frame, meu.id);
        }

        /* Atira sempre que pode, com mira antecipada; com a vida baixa, guarda energia
         * para o escudo (só liga acima de 50 e gasta 5 por frame) */
        double reserva = 20.0;
        if (meu.vida <= 50.0 && !meu.escudo_ativo) {
            reserva = 60.0;
        }
        acao.angulo_torre = mira_com_antecipacao(meu, inimigo);
        if (menor < 550.0 && meu.frames_para_atirar == 0 && meu.energia >= reserva) {
            acao.atirar = 1;
        }
        if (meu.escudo_ativo) {
            if (menor < 200.0 && meu.energia > 15.0) {
                acao.ligar_escudo = 1;
            }
        } else if (meu.vida <= 50.0 && menor < 150.0 && meu.energia > 55.0) {
            acao.ligar_escudo = 1;
        }
    }

    /* Itens próximos: cura com a vida baixa, energia com a energia baixa */
    double melhor_item = 1e9;
    for (int i = 0; i < e->n_itens; i++) {
        Item item = e->itens[i];
        int quero = 0;
        if (item.tipo == ITEM_CURA && meu.vida < 60.0) {
            quero = 1;
        }
        if (item.tipo == ITEM_ENERGIA && meu.energia < 40.0) {
            quero = 1;
        }
        double d = distancia(meu.x, meu.y, item.x, item.y);
        if (quero && d < 250.0 && d < melhor_item) {
            melhor_item = d;
            acao.direcao = angulo_para(meu.x, meu.y, item.x, item.y);
        }
    }

    /* Zona: depois do frame 600 volta para dentro antes de ela fechar (raio previsto 40 frames à frente) */
    double do_centro = distancia(meu.x, meu.y, CENTRO, CENTRO);
    if (do_centro > raio_seguro(e->frame + 40) - 40.0) {
        acao.direcao = angulo_para(meu.x, meu.y, CENTRO, CENTRO);
    }

    /* Perigos (prioridade máxima): sai do raio da artilharia e do centro quando há laser */
    for (int i = 0; i < e->n_perigos; i++) {
        Perigo perigo = e->perigos[i];
        if (perigo.tipo == PERIGO_ARTILHARIA
            && distancia(meu.x, meu.y, perigo.x, perigo.y) < perigo.raio + 40.0) {
            acao.direcao = angulo_para(perigo.x, perigo.y, meu.x, meu.y);
            break;
        }
        if (perigo.tipo == PERIGO_LASER && do_centro < 120.0) {
            acao.direcao = angulo_para(CENTRO, CENTRO, meu.x, meu.y);
        }
    }

    return acao;
}


/* Raio da zona segura no frame dado: fecha 0,7 por frame a partir do frame 600, até 80 */
double raio_seguro(int frame) {
    double r = 550.0;
    if (frame > 600) {
        r = 550.0 - (frame - 600) * 0.7;
    }
    if (r < 80.0) {
        r = 80.0;
    }
    return r;
}

/* Sentido da volta em torno do alvo (+1 ou -1): troca em intervalos de 12 a 33 frames,
 * sorteados por um embaralhamento simples do número do bloco (difícil de prever) */
double sentido_da_volta(int frame, int id) {
    unsigned bloco = 0;
    unsigned inicio = 0;
    while (1) {
        unsigned h = (bloco + 1u) * 2654435761u ^ (unsigned)id * 40503u;
        unsigned tamanho = 12u + (h >> 7) % 22u;   /* 12 a 33 frames */
        if ((unsigned)frame < inicio + tamanho) {
            if (h & 1u) {
                return 1.0;
            }
            return -1.0;
        }
        inicio = inicio + tamanho;
        bloco++;
    }
}

/* Direção da torre para acertar um adversário em movimento: resolve o instante t
 * (em frames) em que o projétil encontra o alvo andando em linha reta. */
double mira_com_antecipacao(Tanque meu, Inimigo alvo) {
    double dx = alvo.x - meu.x;
    double dy = alvo.y - meu.y;
    double a = alvo.vel_x * alvo.vel_x + alvo.vel_y * alvo.vel_y
               - VELOCIDADE_PROJETIL * VELOCIDADE_PROJETIL;
    double b = 2.0 * (dx * alvo.vel_x + dy * alvo.vel_y);
    double c = dx * dx + dy * dy;
    double t = 0.0;
    if (fabs(a) < 1e-9) {
        if (fabs(b) > 1e-9) {
            t = -c / b;
        }
    } else {
        double disc = b * b - 4.0 * a * c;
        if (disc >= 0.0) {
            double s = sqrt(disc);
            double t1 = (-b - s) / (2.0 * a);
            double t2 = (-b + s) / (2.0 * a);
            if (t1 > 0 && t2 > 0) {
                t = t1;
                if (t2 < t1) {
                    t = t2;
                }
            } else {
                t = t1;
                if (t2 > t1) {
                    t = t2;
                }
            }
        }
    }
    if (t < 0.0) {
        t = 0.0;
    }
    if (t > 30.0) {
        t = 30.0;
    }
    return angulo_para(meu.x, meu.y, alvo.x + alvo.vel_x * t, alvo.y + alvo.vel_y * t);
}

/* Perto da parede (folga em unidades): recuar para lá encurralaria o tanque */
int perto_da_borda(Tanque meu, double folga) {
    if (meu.x < folga || meu.x > 1000.0 - folga) {
        return 1;
    }
    if (meu.y < folga || meu.y > 1000.0 - folga) {
        return 1;
    }
    return 0;
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
