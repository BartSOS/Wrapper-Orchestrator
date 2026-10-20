/* =============================================================================
 * NPC "radar" - nível MÉDIO (C99)
 * =============================================================================
 * Estratégia: mira no adversário mais próximo e o persegue, segurando uma
 * distância de tiro (aproxima se está longe, recua se está colado). Atira quando
 * a recarga permite, sai do raio de uma artilharia e liga o escudo com a vida
 * baixa. Não antecipa o movimento do adversário e não trata o laser nem a zona.
 * ============================================================================= */

#include "arena.h"

/* -----------------------------------------------------------------------------
 * AJUDANTES (opcional: renomeie, altere ou apague)
 * -----------------------------------------------------------------------------
 * Em C o estado já chega tipado (arena.h), então não há acessores: use direto
 *
 *   e->tick                           número do tick atual
 *   e->self                           seu tanque (Tanque)
 *   e->inimigos[i], i < e->n_inimigos adversários vivos (Inimigo)
 *   e->hazards[i],  i < e->n_hazards  perigos da arena (Hazard)
 *   e->itens[i],    i < e->n_itens    itens no mapa (Item)
 *
 * e devolva uma Acao com os campos throttle, turn, turret_angle, shoot e
 * shield (veja o pensar() abaixo). As funções abaixo são só matemática básica
 * (ângulos em graus; 0 = +x, cresce de +x para +y). Elas não decidem nada por
 * você.
 * ----------------------------------------------------------------------------- */

/* Distância entre dois pontos */
static inline double distancia(double x1, double y1, double x2, double y2) {
    return hypot(x2 - x1, y2 - y1);
}

/* Leva qualquer ângulo para o intervalo [0, 360) */
static inline double normalizar_angulo(double graus) {
    double a = fmod(graus, 360.0);
    return a < 0 ? a + 360.0 : a;
}

/* Ângulo, em graus, do ponto (x1, y1) olhando para o ponto (x2, y2) */
static inline double angulo_para(double x1, double y1, double x2, double y2) {
    return normalizar_angulo(atan2(y2 - y1, x2 - x1) * 180.0 / M_PI);
}

/* Menor giro, em graus, para ir do ângulo 'de' ao ângulo 'para': de -180 (exclusivo) a 180.
 * Positivo = girar no sentido em que o ângulo aumenta. */
static inline double diferenca_angulo(double de, double para) {
    double d = normalizar_angulo(para - de);
    return d > 180.0 ? d - 360.0 : d;
}

/* -----------------------------------------------------------------------------
 * FIM DOS AJUDANTES
 * ----------------------------------------------------------------------------- */

/* Gira o chassi na direção do alvo (-1, 0 ou 1), com folga de 5 graus */
static double girar_para(double angulo_atual, double angulo_alvo) {
    double dif = diferenca_angulo(angulo_atual, angulo_alvo);
    return dif > 5 ? 1.0 : (dif < -5 ? -1.0 : 0.0);
}

Acao pensar(const Estado *e) {
    const Tanque *eu = &e->self;
    Acao a = {0};
    a.turret_angle = eu->turret_angle;
    a.throttle = 0.5;
    double direcao = eu->angle + 20.0;   /* sem adversários: gira devagar */

    /* Adversário mais próximo */
    int alvo = -1;
    double menor = 1e9;
    for (int i = 0; i < e->n_inimigos; i++) {
        double d = distancia(eu->x, eu->y, e->inimigos[i].x, e->inimigos[i].y);
        if (d < menor) {
            menor = d;
            alvo = i;
        }
    }

    if (alvo >= 0) {
        const Inimigo *in = &e->inimigos[alvo];
        double para_o_alvo = angulo_para(eu->x, eu->y, in->x, in->y);
        a.turret_angle = para_o_alvo;
        a.shoot = menor < 450 && eu->cooldown <= 0 && eu->energy >= 20;
        if (menor > 300) {
            direcao = para_o_alvo;             /* aproxima */
            a.throttle = 1.0;
        } else if (menor < 180) {
            direcao = para_o_alvo + 180.0;     /* recua */
            a.throttle = 1.0;
        } else {
            direcao = para_o_alvo;             /* mantém a distância de tiro */
            a.throttle = 0.2;
        }
    }

    /* Dentro do raio de uma artilharia: sai dele (prioridade máxima) */
    for (int i = 0; i < e->n_hazards; i++) {
        const Hazard *h = &e->hazards[i];
        if (h->tipo == HAZARD_ARTILHARIA && distancia(eu->x, eu->y, h->x, h->y) < h->radius + 40) {
            direcao = angulo_para(h->x, h->y, eu->x, eu->y);
            a.throttle = 1.0;
            break;
        }
    }

    a.turn = girar_para(eu->angle, direcao);
    a.shield = eu->hp < 35 && eu->energy > 60;
    return a;
}
