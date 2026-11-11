// Bot de teste (C++20) - ERRO DE COMPILAÇÃO PROPOSITAL.
// Implementa Acao pensar(const Estado& e) com a assinatura correta, para passar
// pela checagem estrutural do validador e chegar ao g++, que deve recusar:
//   1. ponto e vírgula ausente após a atribuição de "mover";
//   2. identificador não declarado (velocidade_maxima);
//   3. conversão inválida de std::string para double.

Acao pensar(const Estado& e) {
    Acao a;
    double x = e.eu.x;
    double y = e.eu.y;

    a.mover = FRENTE
    a.direcao = velocidade_maxima * 0.5;
    double angulo = std::string("noventa");
    a.angulo_torre = angulo + x - y;
    a.atirar = 0;
    a.ligar_escudo = 0;
    return a;
}
