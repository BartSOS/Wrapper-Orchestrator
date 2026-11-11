// Armadilha: o pensar() lança exceção em alguns frames, inclusive uma que não é
// std::exception. Antes o wrapper não respondia nesses frames e o bot dessincronizava;
// agora a resposta é {} (parado) e o bot continua na partida.
#include "arena.hpp"
#include <stdexcept>
using namespace std;

Acao pensar(const Estado& e) {
    int t = e.frame;
    if (t % 11 == 3) {
        throw runtime_error("erro proposital");
    }
    if (t % 11 == 8) {
        throw 42;
    }
    Tanque meu = e.eu;
    Acao acao;
    acao.mover = 0.7;
    acao.direcao = meu.angulo_chassi + 4.0;
    acao.angulo_torre = meu.angulo_torre;
    acao.atirar = false;
    acao.ligar_escudo = false;
    return acao;
}
