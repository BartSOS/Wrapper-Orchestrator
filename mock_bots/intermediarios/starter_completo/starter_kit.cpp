/**
 * ============================================================================
 * TOPCOM ARENA - STARTER KIT C++20
 * ============================================================================
 * INSTRUÇÕES PARA O PARTICIPANTE:
 * - Não crie a função main() nem utilize cin/cout.
 * - A arena compila seu código junto com o wrapper oficial de I/O.
 * - O tipo 'json' e as bibliotecas essenciais (<cmath>, <string>, etc.)
 *   já estão automaticamente disponíveis no escopo.
 * - Implemente apenas a função pensar(const json& estado).
 * ============================================================================
 */

json pensar(const json& estado) {
    // 1. Extração dos dados do tanque do participante
    const auto& self = estado["self"];
    double minha_x = self["x"].get<double>();
    double minha_y = self["y"].get<double>();
    double meu_angulo = self["angle"].get<double>();
    double meu_cooldown = self["cooldown"].get<double>();
    double minha_energia = self["energy"].get<double>();

    // 2. Ação Neutra Base (Andar em círculos se estiver sozinho)
    json acao;
    acao["throttle"] = 1.0;
    acao["turn"] = 0.5;
    acao["turret_angle"] = meu_angulo;
    acao["shoot"] = false;
    acao["shield"] = false;

    // 3. Radar de Inimigos
    double menor_distancia = 99999.0;
    json alvo = nullptr;

    if (estado.contains("enemies") && estado["enemies"].is_array()) {
        for (const auto& inimigo : estado["enemies"]) {
            double dx = inimigo["x"].get<double>() - minha_x;
            double dy = inimigo["y"].get<double>() - minha_y;
            double dist = std::hypot(dx, dy);

            if (dist < menor_distancia) {
                menor_distancia = dist;
                alvo = inimigo;
            }
        }
    }

    // 4. Se encontrou inimigo, mira e toma decisão de tiro
    if (alvo != nullptr) {
        double alvo_x = alvo["x"].get<double>() - minha_x;
        double alvo_y = alvo["y"].get<double>() - minha_y;

        double angulo_rad = std::atan2(alvo_y, alvo_x);
        double angulo_graus = angulo_rad * 180.0 / M_PI;
        double angulo_alvo = std::fmod((angulo_graus + 360.0), 360.0);

        acao["turret_angle"] = angulo_alvo;

        // Controle de Rotação do Chassi
        double diff_ang = angulo_alvo - meu_angulo;
        if (diff_ang > 180.0) diff_ang -= 360.0;
        if (diff_ang < -180.0) diff_ang += 360.0;

        if (diff_ang > 10.0) acao["turn"] = 1.0;
        else if (diff_ang < -10.0) acao["turn"] = -1.0;
        else acao["turn"] = 0.0;

        // Fuga se estiver perto demais, ou avanço
        if (menor_distancia < 120.0) acao["throttle"] = -1.0;
        else acao["throttle"] = 1.0;

        // Disparo se tiver recarga pronta
        if (menor_distancia < 600.0 && meu_cooldown == 0.0 && minha_energia > 30.0) {
            acao["shoot"] = true;
        }
    }

    return acao;
}
