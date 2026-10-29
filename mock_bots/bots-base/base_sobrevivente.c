/* =============================================================================
 * NPC "sobrevivente" - nível DIFÍCIL (C99)
 * =============================================================================
 * Estratégia: luta à distância sem se deixar encurralar (kiting). Recua quando o
 * adversário mais próximo chega a menos de 300 (atirando de costas: a torre gira
 * na hora), aproxima quando ele passa de 500 e, no meio-termo, circula trocando
 * o sentido da volta em intervalos irregulares, para não ser previsível. Atira
 * sempre que pode, com mira antecipada (o projétil anda a 300 u/s), e guarda
 * energia para o escudo só quando a vida está baixa. Busca cura e energia
 * próximas quando precisa, volta para dentro da zona segura antes de ela fechar
 * (prevê o raio 4 s à frente) e foge da artilharia e do centro quando há laser.
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
#define CENTRO              500.0   /* centro da arena (1000 x 1000) */

/* Raio da zona segura no tick dado: fecha 0,7 por tick a partir do tick 600, até 80 */
static double raio_seguro(int tick) {
    double r = tick > 600 ? 550.0 - (tick - 600) * 0.7 : 550.0;
    return r < 80.0 ? 80.0 : r;
}

/* Sentido da volta em torno do alvo (+1 ou -1): troca em intervalos de 1,2 a 3,3 s,
 * sorteados por um embaralhamento simples do número do bloco (difícil de prever) */
static double sentido_da_volta(int tick, int id) {
    unsigned bloco = 0, inicio = 0;
    for (;;) {
        unsigned h = (bloco + 1u) * 2654435761u ^ (unsigned)id * 40503u;
        unsigned tamanho = 12u + (h >> 7) % 22u;   /* 12 a 33 ticks */
        if ((unsigned)tick < inicio + tamanho) return (h & 1u) ? 1.0 : -1.0;
        inicio += tamanho;
        bloco++;
    }
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

/* Perto da parede (folga em unidades): recuar para lá encurralaria o tanque */
static int perto_da_borda(const Tanque *eu, double folga) {
    return eu->x < folga || eu->x > 1000.0 - folga || eu->y < folga || eu->y > 1000.0 - folga;
}

Acao pensar(const Estado *e) {
    const Tanque *eu = &e->self;
    Acao a = {0};
    a.turret_angle = eu->turret_angle;

    /* Sem adversários: volta para o centro */
    double direcao = angulo_para(eu->x, eu->y, CENTRO, CENTRO);
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
        /* Distância: recua quando ele chega perto, aproxima quando está longe e, no
         * meio-termo, circula trocando o sentido em intervalos irregulares */
        if (menor < 300.0 && !perto_da_borda(eu, 100.0)) direcao = para_o_alvo + 180.0;
        else if (menor > 500.0) direcao = para_o_alvo;
        else direcao = para_o_alvo + 90.0 * sentido_da_volta(e->tick, eu->id);

        /* Atira sempre que pode, com mira antecipada; com a vida baixa, guarda energia
         * para o escudo (só liga acima de 50 e gasta 50 por segundo) */
        double reserva = (eu->hp <= 50.0 && !eu->shield) ? 60.0 : 20.0;
        a.turret_angle = mira_com_antecipacao(eu, in);
        a.shoot = menor < 550.0 && eu->cooldown <= 0.0 && eu->energy >= reserva;
        a.shield = eu->shield ? (menor < 200.0 && eu->energy > 15.0)
                              : (eu->hp <= 50.0 && menor < 150.0 && eu->energy > 55.0);
    }

    /* Itens próximos: cura com a vida baixa, energia com a energia baixa */
    double melhor_item = 1e9;
    for (int i = 0; i < e->n_itens; i++) {
        const Item *it = &e->itens[i];
        int quero = (it->tipo == ITEM_CURA && eu->hp < 60.0) || (it->tipo == ITEM_ENERGIA && eu->energy < 40.0);
        double d = distancia(eu->x, eu->y, it->x, it->y);
        if (quero && d < 250.0 && d < melhor_item) {
            melhor_item = d;
            direcao = angulo_para(eu->x, eu->y, it->x, it->y);
        }
    }

    /* Zona: depois do tick 600 volta para dentro antes de ela fechar (raio previsto 4 s à frente) */
    double do_centro = distancia(eu->x, eu->y, CENTRO, CENTRO);
    if (do_centro > raio_seguro(e->tick + 40) - 40.0) direcao = angulo_para(eu->x, eu->y, CENTRO, CENTRO);

    /* Perigos (prioridade máxima): sai do raio da artilharia e do centro quando há laser */
    for (int i = 0; i < e->n_hazards; i++) {
        const Hazard *h = &e->hazards[i];
        if (h->tipo == HAZARD_ARTILHARIA && distancia(eu->x, eu->y, h->x, h->y) < h->radius + 40.0) {
            direcao = angulo_para(h->x, h->y, eu->x, eu->y);
            break;
        }
        if (h->tipo == HAZARD_LASER && do_centro < 120.0) direcao = angulo_para(CENTRO, CENTRO, eu->x, eu->y);
    }

    /* Segue a direção escolhida na velocidade máxima */
    double dif = diferenca_angulo(eu->angle, direcao);
    a.turn = dif / 9.0;   /* o chassi gira no máximo 9 graus por tick */
    if (a.turn > 1.0) a.turn = 1.0;
    if (a.turn < -1.0) a.turn = -1.0;
    a.throttle = 1.0;
    return a;
}
