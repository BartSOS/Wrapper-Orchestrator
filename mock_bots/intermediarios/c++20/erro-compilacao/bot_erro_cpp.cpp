// Bot de teste (C++20) - ERRO DE COMPILAÇÃO PROPOSITAL.
// Implementa json pensar(const json& estado) com a assinatura correta, para passar
// pela checagem estrutural do validador e chegar ao g++, que deve recusar:
//   1. ponto e vírgula ausente após a atribuição de "throttle";
//   2. identificador não declarado (velocidade_maxima);
//   3. conversão inválida de std::string para double.

json pensar(const json& estado) {
    json acao;
    double x = estado["self"]["x"].get<double>();
    double y = estado["self"]["y"].get<double>();

    acao["throttle"] = 1.0
    acao["turn"] = velocidade_maxima * 0.5;
    double angulo = std::string("noventa");
    acao["turret_angle"] = angulo + x - y;
    acao["shoot"] = false;
    acao["shield"] = false;
    return acao;
}
