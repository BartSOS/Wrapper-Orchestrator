// Bot de teste (C++20) - Sniper: fica parado, mira no inimigo mais próximo e atira de longe.
#include "arena.hpp"
using namespace std;

double distancia(double x1, double y1, double x2, double y2);
double angulo_para(double x1, double y1, double x2, double y2);

Acao pensar(const Estado& e) {
    Tanque meu = e.eu;
    Acao acao;
    acao.mover = PARADO;                       // fica parado (sniper)
    acao.direcao = meu.angulo_chassi;
    acao.angulo_torre = meu.angulo_chassi + 5.0;  // sem inimigo: torre um pouco ao lado
    acao.atirar = false;
    acao.ligar_escudo = false;

    double menor = 9999.0;
    for (int i = 0; i < e.n_inimigos; i++) {
        Inimigo in = e.inimigos[i];
        double d = distancia(meu.x, meu.y, in.x, in.y);
        if (d < menor) {
            menor = d;
            acao.angulo_torre = angulo_para(meu.x, meu.y, in.x, in.y);
            if (d < 800.0) {
                acao.atirar = true;            // atira de longe
            }
        }
    }
    return acao;
}

double distancia(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

double angulo_para(double x1, double y1, double x2, double y2) {
    double graus = atan2(y2 - y1, x2 - x1) * 180.0 / M_PI;
    if (graus < 0) {
        graus = graus + 360.0;
    }
    return graus;
}
