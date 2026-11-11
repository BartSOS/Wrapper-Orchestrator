/* Armadilha: valores NaN e infinito na ação. O wrapper de C troca o valor que não é
 * finito pelo padrão só naquele campo (mover vira PARADO, direcao e angulo_torre mantêm)
 * e o resto da ação vale. */
#include "arena.h"

Acao pensar(const Estado *e) {
    Tanque meu = e->eu;
    Acao acao;
    acao.mover = FRENTE;
    acao.direcao = meu.angulo_chassi + 5.0;
    acao.angulo_torre = meu.angulo_torre;
    acao.atirar = 0;
    acao.ligar_escudo = 0;
    if (e->frame % 9 == 4) {
        acao.mover = NAN;
    }
    if (e->frame % 9 == 6) {
        acao.direcao = INFINITY;
    }
    if (e->frame % 9 == 7) {
        acao.angulo_torre = NAN;
    }
    return acao;
}
