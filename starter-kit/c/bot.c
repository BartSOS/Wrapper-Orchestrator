/* =============================================================================
 * GAME JAM PET 2026 - STARTER KIT C (C99)
 * =============================================================================
 * O que você precisa fazer: escrever a função pensar(), mais abaixo.
 * Ela é chamada 10 vezes por segundo e devolve o que o seu tanque vai fazer.
 *
 * Regras do código enviado:
 * - Não crie main(), não leia stdin e não escreva em stdout: a arena já tem a
 *   dela e chama pensar() por você. printf() vai para o log de erros e é
 *   descartado (use fprintf(stderr, ...) se quiser depurar).
 * - A assinatura de pensar() não pode mudar: Acao pensar(const Estado *e)
 * - Variáveis globais e static valem durante a partida inteira (sua memória).
 * - O pensar() abaixo lê todos os campos só para mostrar o que existe: os avisos
 *   de "variável não usada" do compilador somem quando você usar os valores.
 * ============================================================================= */

#include "arena.h"   /* tipos Estado, Acao, Tanque, Inimigo, Hazard e Item */

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


/* =============================================================================
 * SEU CÓDIGO COMEÇA AQUI
 * ============================================================================= */

Acao pensar(const Estado *e) {
    /* --- O jogo --- */
    int t = e->tick;                       /* número do tick atual (a partida tem até 1500) */

    /* --- O seu tanque --- */
    const Tanque *meu = &e->self;
    double x = meu->x;                     /* posição horizontal */
    double y = meu->y;                     /* posição vertical */
    double vx = meu->vx;                   /* velocidade em x (unidades/s) */
    double vy = meu->vy;                   /* velocidade em y (unidades/s) */
    double angulo = meu->angle;            /* para onde o chassi aponta (graus) */
    double torre = meu->turret_angle;      /* para onde a torre aponta (graus) */
    double vida = meu->hp;                 /* vida: chegando a 0, você é eliminado */
    double energia = meu->energy;          /* energia: tiro e escudo gastam */
    double recarga = meu->cooldown;        /* segundos até poder atirar de novo */
    bool escudo = meu->shield;             /* true se o escudo está ligado */

    /* --- Adversários vivos --- */
    for (int i = 0; i < e->n_inimigos; i++) {
        const Inimigo *inimigo = &e->inimigos[i];
        int id = inimigo->id;              /* identificador */
        double ix = inimigo->x;            /* posição */
        double iy = inimigo->y;
        double ivx = inimigo->vx;          /* velocidade */
        double ivy = inimigo->vy;
    }

    /* --- Perigos da arena --- */
    for (int i = 0; i < e->n_hazards; i++) {
        const Hazard *perigo = &e->hazards[i];
        int id = perigo->id;               /* identificador */
        TipoHazard tipo = perigo->tipo;    /* HAZARD_ARTILHARIA ou HAZARD_LASER */
        double px = perigo->x;             /* posição */
        double py = perigo->y;
        double raio = perigo->radius;      /* raio de efeito */
        double prazo = perigo->timer;      /* segundos até o impacto / a ativação */
        double direcao = perigo->angle;    /* direção do feixe (laser) */
        double duracao = perigo->duration; /* segundos de efeito (laser) */
    }

    /* --- Itens no mapa --- */
    for (int i = 0; i < e->n_itens; i++) {
        const Item *item = &e->itens[i];
        int id = item->id;                 /* identificador */
        TipoItem tipo = item->tipo;        /* ITEM_CURA ou ITEM_ENERGIA */
        double ix = item->x;               /* posição */
        double iy = item->y;
    }

    /* --- A ação: andar para a frente girando devagar, torre parada, sem atirar --- */
    Acao a = {0};
    a.throttle = 1.0;                      /* -1.0 (ré) a 1.0 (frente) */
    a.turn = 0.5;                          /* -1.0 a 1.0 (positivo aumenta o ângulo do chassi) */
    a.turret_angle = torre;                /* direção da torre (em C é sempre enviada: passe a atual para mantê-la) */
    a.shoot = false;                       /* true para atirar (precisa de recarga == 0 e energia) */
    a.shield = false;                      /* true para ligar o escudo (gasta energia) */
    return a;
}
