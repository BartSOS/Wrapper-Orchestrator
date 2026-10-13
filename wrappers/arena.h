/* =============================================================================
 * TOPCOM ARENA - Interface oficial para bots em C (C99)
 * =============================================================================
 * Este cabeçalho é injetado automaticamente na compilação do bot.
 * O competidor implementa APENAS:
 *
 *     Acao pensar(const Estado *e);
 *
 * Não escreva main(), não leia stdin e não escreva em stdout:
 * a organização converte o estado da arena para as structs abaixo e envia
 * a Acao devolvida para a engine. printf() vai para o log de erros (stderr).
 *
 * Unidades: posições/raios em unidades da arena (1000 x 1000), velocidades em
 * unidades/s, ângulos em graus [0, 360), tempos em segundos.
 * ============================================================================= */
#ifndef TOPCOM_ARENA_H
#define TOPCOM_ARENA_H

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {   /* permite que o wrapper interno (C++) chame pensar() */
#endif

#define MAX_INIMIGOS 32   /* adversários vivos (a arena comporta até 20 bots) */
#define MAX_HAZARDS  64   /* perigos ativos; excedentes são descartados       */
#define MAX_ITENS    64   /* itens no mapa;  excedentes são descartados       */

/* Seu próprio tanque */
typedef struct {
    int    id;
    double x, y;          /* posição                                   */
    double vx, vy;        /* velocidade                                */
    double angle;         /* direção do chassi                         */
    double turret_angle;  /* direção da torre                          */
    double hp;            /* vida (0 a 100)                            */
    double energy;        /* energia (0 a 100)                         */
    double cooldown;      /* segundos até poder atirar de novo         */
    bool   shield;        /* escudo ativo neste momento                */
} Tanque;

/* Adversário vivo */
typedef struct {
    int    id;
    double x, y;
    double vx, vy;
} Inimigo;

typedef enum { HAZARD_ARTILHARIA = 0, HAZARD_LASER = 1 } TipoHazard;

/* Perigo da arena */
typedef struct {
    int        id;
    TipoHazard tipo;
    double     x, y;
    double     radius;    /* raio de efeito                             */
    double     timer;     /* segundos até o impacto/ativação            */
    double     angle;     /* direção atual do feixe (laser)             */
    double     duration;  /* segundos restantes de efeito (laser)       */
} Hazard;

typedef enum { ITEM_CURA = 0, ITEM_ENERGIA = 1 } TipoItem;

/* Item coletável */
typedef struct {
    int      id;
    TipoItem tipo;
    double   x, y;
} Item;

/* Estado da arena recebido a cada tick */
typedef struct {
    int     tick;
    Tanque  self;
    int     n_inimigos;
    Inimigo inimigos[MAX_INIMIGOS];
    int     n_hazards;
    Hazard  hazards[MAX_HAZARDS];
    int     n_itens;
    Item    itens[MAX_ITENS];
} Estado;

/* Ação devolvida a cada tick */
typedef struct {
    double throttle;      /* -1.0 (ré) a 1.0 (frente)                  */
    double turn;          /* -1.0 a 1.0 (giro do chassi)               */
    double turret_angle;  /* 0 a 360 graus (direção absoluta da torre)  */
    bool   shoot;         /* disparar                                  */
    bool   shield;        /* ligar o escudo                            */
} Acao;

/* Função que o competidor implementa */
Acao pensar(const Estado *e);

/* ----------------------------------------------------------------------------
 * Funções auxiliares (opcionais)
 * ---------------------------------------------------------------------------- */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Distância entre dois pontos */
static inline double distancia(double x1, double y1, double x2, double y2) {
    return hypot(x2 - x1, y2 - y1);
}

/* Normaliza um ângulo para [0, 360) */
static inline double normalizar_angulo(double graus) {
    double a = fmod(graus, 360.0);
    return a < 0 ? a + 360.0 : a;
}

/* Ângulo (graus) do ponto (x1,y1) para o ponto (x2,y2), mesma convenção da arena */
static inline double angulo_para(double x1, double y1, double x2, double y2) {
    return normalizar_angulo(atan2(y2 - y1, x2 - x1) * 180.0 / M_PI);
}

/* Menor diferença de 'de' para 'para', em graus, no intervalo (-180, 180] */
static inline double diferenca_angulo(double de, double para) {
    double d = fmod(para - de + 540.0, 360.0) - 180.0;
    return d == -180.0 ? 180.0 : d;
}

#ifdef __cplusplus
}
#endif

#endif /* TOPCOM_ARENA_H */
