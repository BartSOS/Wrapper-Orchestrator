// Armadilha: o pensar() lança exceção em alguns ticks, inclusive uma que não é
// std::exception. Antes o wrapper não respondia nesses ticks e o bot dessincronizava;
// agora a resposta é {} (parado) e o bot continua na partida.
#include <stdexcept>

json pensar(const json& estado) {
    int t = estado["tick"].get<int>();
    if (t % 11 == 3) throw std::runtime_error("erro proposital");
    if (t % 11 == 8) throw 42;
    return {{"throttle", 0.7}, {"turn", 0.4}};
}
