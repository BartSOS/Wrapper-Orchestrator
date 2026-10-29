/* Armadilha: valores NaN e infinito na ação. O wrapper de C escreve null no lugar deles
 * e a engine usa o padrão só naquele campo (antes, o null descartava a ação inteira). */
#include "arena.h"

Acao pensar(const Estado *e) {
    Acao a = {0};
    a.turret_angle = e->self.turret_angle;
    a.throttle = (e->tick % 9 == 4) ? NAN : 1.0;
    a.turn = (e->tick % 9 == 6) ? INFINITY : 0.5;
    return a;
}
