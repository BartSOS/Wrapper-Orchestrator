// Armadilha: campos booleanos escritos como inteiro, como é costume em C e C++
// (a.atirar = 1, a.ligar_escudo = 0). O wrapper precisa tratar 1 como verdadeiro:
// antes, um "shoot": 1 lançava type_error na engine e a ação inteira do frame era
// descartada, e este bot nunca atirava. O teste de robustez confere que ele abate alguém.
#include <cmath>

Acao pensar(const Estado& e) {
    Acao a;
    double melhor = 1e18, ax = 500.0, ay = 500.0;
    for (const Inimigo& in : e.inimigos) {
        double dx = in.x - e.eu.x, dy = in.y - e.eu.y;
        double d = dx * dx + dy * dy;
        if (d < melhor) {
            melhor = d;
            ax = in.x;
            ay = in.y;
        }
    }
    a.mover = PARADO;
    a.angulo_torre = std::atan2(ay - e.eu.y, ax - e.eu.x) * 180.0 / M_PI;
    a.atirar = 1;
    a.ligar_escudo = 0;
    return a;
}
