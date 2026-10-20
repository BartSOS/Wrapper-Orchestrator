/* =============================================================================
 * NPC "sobrevivente" - nível DIFÍCIL (C99)
 * =============================================================================
 * Estratégia: sobreviver primeiro. O movimento é a soma de forças (campos de
 * potencial): foge da artilharia e do centro do laser, mantém distância dos
 * adversários, busca cura quando a vida cai e energia quando ela acaba, e volta
 * para o centro quando a zona de radiação fecha (tick > 600). Só atira quando é
 * seguro, com mira antecipada (o projétil anda a 300 u/s) e sem parar de se
 * mover. Liga o escudo com a vida baixa e adversário perto.
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

#define VELOCIDADE_PROJETIL 300.0   /* unidades por segundo */

/* Soma ao vetor (fx, fy) um empurrão de intensidade 'peso' na direção 'angulo' (graus) */
static void empurrar(double *fx, double *fy, double angulo, double peso) {
    *fx += cos(angulo * M_PI / 180.0) * peso;
    *fy += sin(angulo * M_PI / 180.0) * peso;
}

/* Direção da torre para acertar um adversário em movimento: resolve o instante t
 * em que o projétil (velocidade constante) encontra o alvo andando em linha reta. */
static double mira_com_antecipacao(const Tanque *meu, const Inimigo *alvo) {
    double dx = alvo->x - meu->x, dy = alvo->y - meu->y;
    double a = alvo->vx * alvo->vx + alvo->vy * alvo->vy - VELOCIDADE_PROJETIL * VELOCIDADE_PROJETIL;
    double b = 2.0 * (dx * alvo->vx + dy * alvo->vy);
    double c = dx * dx + dy * dy;
    double t = 0.0;
    if (fabs(a) < 1e-9) {
        if (fabs(b) > 1e-9) t = -c / b;
    } else {
        double disc = b * b - 4.0 * a * c;
        if (disc >= 0.0) {
            double s = sqrt(disc);
            double t1 = (-b - s) / (2.0 * a), t2 = (-b + s) / (2.0 * a);
            if (t1 > 0 && t2 > 0) t = t1 < t2 ? t1 : t2;
            else t = t1 > t2 ? t1 : t2;
        }
    }
    if (t < 0.0) t = 0.0;
    if (t > 3.0) t = 3.0;
    return angulo_para(meu->x, meu->y, alvo->x + alvo->vx * t, alvo->y + alvo->vy * t);
}

Acao pensar(const Estado *e) {
    const Tanque *eu = &e->self;
    Acao a = {0};
    a.turret_angle = eu->turret_angle;
    double fx = 0.0, fy = 0.0;

    /* Adversários: o mais próximo vira alvo; todos empurram para longe quando estão perto */
    int alvo = -1;
    double menor = 1e9;
    for (int i = 0; i < e->n_inimigos; i++) {
        const Inimigo *in = &e->inimigos[i];
        double d = distancia(eu->x, eu->y, in->x, in->y);
        if (d < menor) {
            menor = d;
            alvo = i;
        }
        if (d < 300) {
            empurrar(&fx, &fy, angulo_para(in->x, in->y, eu->x, eu->y), (300 - d) / 100.0);
        }
    }
    if (alvo >= 0) {
        a.turret_angle = mira_com_antecipacao(eu, &e->inimigos[alvo]);
        a.shoot = menor < 550 && eu->cooldown <= 0 && eu->energy >= 30;
        a.shield = menor < 200 && eu->hp < 50 && eu->energy > 60;
    }

    /* Itens: cura quando a vida cai, energia quando ela acaba */
    for (int i = 0; i < e->n_itens; i++) {
        const Item *it = &e->itens[i];
        int quero = (it->tipo == ITEM_CURA && eu->hp < 80) || (it->tipo == ITEM_ENERGIA && eu->energy < 60);
        double d = distancia(eu->x, eu->y, it->x, it->y);
        if (quero && d < 400) {
            empurrar(&fx, &fy, angulo_para(eu->x, eu->y, it->x, it->y), 2.0);
        }
    }

    /* Perigos: artilharia (raio + folga) e centro do laser */
    for (int i = 0; i < e->n_hazards; i++) {
        const Hazard *h = &e->hazards[i];
        double d = distancia(eu->x, eu->y, h->x, h->y);
        double fugir = angulo_para(h->x, h->y, eu->x, eu->y);
        if (h->tipo == HAZARD_ARTILHARIA && d < h->radius + 70) empurrar(&fx, &fy, fugir, 5.0);
        if (h->tipo == HAZARD_LASER && d < 180) empurrar(&fx, &fy, fugir, 3.0);
    }

    /* Zona de radiação: depois do tick 600 o raio seguro encolhe a partir do centro */
    double raio_seguro = e->tick > 600 ? 550.0 - (e->tick - 600) * 0.7 : 550.0;
    if (raio_seguro < 80.0) raio_seguro = 80.0;
    double do_centro = distancia(eu->x, eu->y, 500.0, 500.0);
    if (do_centro > raio_seguro - 100.0) {
        empurrar(&fx, &fy, angulo_para(eu->x, eu->y, 500.0, 500.0), 4.0);
    }
    /* Ronda o centro a uns 300 de distância (o laser nasce no centro), trocando o sentido
     * da volta a cada 3 s: andar sempre do mesmo jeito deixa o adversário acertar. */
    double para_o_centro = angulo_para(eu->x, eu->y, 500.0, 500.0);
    double sentido = (e->tick / 30) % 2 == 0 ? 90.0 : -90.0;
    if (do_centro > 360.0) empurrar(&fx, &fy, para_o_centro, 1.0);
    else if (do_centro < 240.0) empurrar(&fx, &fy, para_o_centro + 180.0, 1.0);
    else empurrar(&fx, &fy, para_o_centro + sentido, 1.0);

    /* Segue a força resultante: gira até a direção (9 graus por tick no máximo) e anda */
    double direcao = (fx == 0.0 && fy == 0.0) ? eu->angle + 30.0 : angulo_para(0.0, 0.0, fx, fy);
    double dif = diferenca_angulo(eu->angle, direcao);
    a.turn = dif / 9.0;
    if (a.turn > 1.0) a.turn = 1.0;
    if (a.turn < -1.0) a.turn = -1.0;
    a.throttle = fabs(dif) < 60 ? 1.0 : 0.2;
    return a;
}
