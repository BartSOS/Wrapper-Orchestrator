// =============================================================================
// TOPCOM ARENA - STARTER KIT C++20
// =============================================================================
// Instruções para o competidor:
// - Implemente apenas a função pensar(const json& estado).
// - Não utilize std::cin, std::cout nem crie a função main().
// - A arena injeta automaticamente as bibliotecas padrão, o tipo json e a main().
// =============================================================================

json pensar(const json& estado) {
    const auto& self = estado["self"];
    double meu_angulo = self.value("angle", 0.0);

    // Exemplo de ação inicial: andar para frente e girar suavemente
    return {
        {"throttle", 1.0},
        {"turn", 0.5},
        {"turret_angle", meu_angulo},
        {"shoot", false},
        {"shield", false}
    };
}
