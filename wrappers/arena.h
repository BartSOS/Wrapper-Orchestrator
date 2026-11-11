/* =============================================================================
 * GAME JAM PET 2026 - Arquivo de apoio para bots em C (C99)
 * =============================================================================
 * Este arquivo vem no kit e no zip das fases (deixe-o na mesma pasta do seu
 * bot.c) e já existe no servidor: você NÃO envia o arena.h, só o seu bot.c.
 *
 * O que você escreve é só a função:
 *
 *     Acao pensar(const Estado *e);
 *
 * Ela é chamada 10 vezes por segundo (uma vez por frame). Ela recebe o que o
 * seu tanque enxerga (Estado) e devolve o que ele deve fazer (Acao).
 * Não escreva main(), não leia o teclado (scanf) e não use printf para
 * responder: a arena cuida disso. Para depurar, use
 *     fprintf(stderr, "frame %d: x=%.1f\n", e->frame, meu.x);
 *
 * Unidades:
 *   tempo ........ frames (1 frame = 0,1 segundo; 10 frames por segundo)
 *   posição ...... unidades da arena (o mapa tem 1000 x 1000)
 *   velocidade ... unidades por frame
 *   ângulos ...... graus, de 0 a 360; 0 aponta para +x e o ângulo cresce de
 *                  +x para +y (90 aponta para +y)
 * ============================================================================= */
#ifndef GAMEJAM_ARENA_H
#define GAMEJAM_ARENA_H

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {   /* permite que a parte interna da arena (C++) chame pensar() */
#endif

#define MAX_INIMIGOS 32   /* inimigos vivos (a arena comporta até 20 tanques)  */
#define MAX_PERIGOS  64   /* perigos ativos; os excedentes são descartados     */
#define MAX_ITENS    64   /* itens no mapa; os excedentes são descartados      */
#define MAX_PONTOS   16   /* pontos do objetivo (só nas fases de treino)       */
#define TAM_ID_PONTO 16   /* id do ponto ("B", "P1"...), com o '\0' final       */

/* Valores para Acao.mover */
#define FRENTE  1.0       /* anda para a frente: 5 unidades por frame          */
#define PARADO  0.0       /* fica parado (para no mesmo frame, sem inércia)    */
#define RE     (-1.0)     /* anda de ré: 5 unidades por frame para trás        */

/* Seu tanque (no Estado ele se chama eu) */
typedef struct {
    int    id;
    double x, y;              /* posição do centro                           */
    double vel_x, vel_y;      /* velocidade, em unidades por frame           */
    double angulo_chassi;     /* para onde o chassi aponta (graus, 0 a 360)  */
    double angulo_torre;      /* para onde a torre aponta (graus, 0 a 360)   */
    double vida;              /* 0 a 100; em 0 o tanque é destruído          */
    double energia;           /* 0 a 100; atirar e o escudo gastam energia   */
    int    frames_para_atirar;/* frames até poder atirar de novo (0 = pode)  */
    bool   escudo_ativo;      /* o escudo está ligado neste frame            */
} Tanque;

/* Inimigo vivo */
typedef struct {
    int    id;
    double x, y;              /* posição do centro                           */
    double vel_x, vel_y;      /* velocidade, em unidades por frame           */
} Inimigo;

typedef enum { PERIGO_ARTILHARIA = 0, PERIGO_LASER = 1 } TipoPerigo;

/* Perigo da arena (artilharia ou laser) */
typedef struct {
    int        id;
    TipoPerigo tipo;              /* PERIGO_ARTILHARIA ou PERIGO_LASER        */
    double     x, y;              /* centro                                   */
    double     raio;              /* alcance do efeito, em unidades           */
    int        frames_ate_impacto;/* frames até cair/ligar (0 = já está ativo)*/
    double     angulo;            /* direção do feixe do laser (graus)        */
    int        frames_restantes;  /* frames que o laser ainda fica ligado     */
} Perigo;

typedef enum { ITEM_CURA = 0, ITEM_ENERGIA = 1 } TipoItem;

/* Item que o tanque coleta passando por cima */
typedef struct {
    int      id;
    TipoItem tipo;            /* ITEM_CURA ou ITEM_ENERGIA                   */
    double   x, y;
} Item;

/* Ponto do objetivo de uma fase de treino ("chegue ao ponto B").
 * É só uma marca no chão: não bloqueia e não é inimigo. */
typedef struct {
    char   id[TAM_ID_PONTO];  /* nome do ponto, igual ao do texto da fase    */
    double x, y;              /* centro                                      */
    double raio;              /* o tanque está no ponto se o centro dele     */
                              /* estiver a até raio unidades deste centro    */
} Ponto;

/* O que o seu tanque enxerga neste frame */
typedef struct {
    int     frame;                   /* número do frame (começa em 0)        */
    Tanque  eu;                      /* o seu tanque                         */
    int     n_inimigos;              /* quantos inimigos há em inimigos[]    */
    Inimigo inimigos[MAX_INIMIGOS];
    int     n_perigos;               /* quantos perigos há em perigos[]      */
    Perigo  perigos[MAX_PERIGOS];
    int     n_itens;                 /* quantos itens há em itens[]          */
    Item    itens[MAX_ITENS];
    int     n_pontos;                /* fases de treino: quantos pontos há   */
    Ponto   pontos[MAX_PONTOS];      /* no torneio, n_pontos é sempre 0      */
} Estado;

/* O que o seu tanque faz neste frame. Preencha os 5 campos. */
typedef struct {
    double mover;         /* FRENTE, PARADO ou RE (0.5 = metade da velocidade) */
    double direcao;       /* para onde o chassi deve apontar (graus). O tanque */
                          /* gira até 9 graus por frame até chegar lá. Para    */
                          /* manter a direção: a.direcao = meu.angulo_chassi   */
    double angulo_torre;  /* para onde a torre aponta (graus); muda na hora    */
    int    atirar;        /* 1 (ou true) atira; 0 (ou false) não atira         */
    int    ligar_escudo;  /* 1 (ou true) liga o escudo neste frame; 0 desliga  */
} Acao;

/* A função que você escreve */
Acao pensar(const Estado *e);

/* Em modo C99 estrito o math.h não define M_PI */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
}
#endif

#endif /* GAMEJAM_ARENA_H */
