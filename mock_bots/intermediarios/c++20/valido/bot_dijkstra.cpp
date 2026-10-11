#include <cmath>

json pensar(const json& estado) {
    const auto& self = estado["self"];
    double x = self["x"].get<double>();
    double y = self["y"].get<double>();
    double angulo = self["angle"].get<double>();

    json acao;
    acao["throttle"] = 0.0; // Fica parado (Sniper)
    acao["turn"] = 0.0;
    acao["turret_angle"] = std::fmod(angulo + 5.0, 360.0); // Radar giratorio
    acao["shoot"] = false;
    acao["shield"] = false;

    double menor_dist = 9999.0;
    for (const auto& inimigo : estado["enemies"]) {
        double dist = std::hypot(inimigo["x"].get<double>() - x, inimigo["y"].get<double>() - y);
        if (dist < menor_dist) {
            menor_dist = dist;
            double alvo_x = inimigo["x"].get<double>() - x;
            double alvo_y = inimigo["y"].get<double>() - y;
            double angulo_alvo = std::fmod((std::atan2(alvo_y, alvo_x) * 180.0 / M_PI) + 360.0, 360.0);
            
            acao["turret_angle"] = angulo_alvo;
            if (menor_dist < 800.0) acao["shoot"] = true; // Atira de longe
        }
    }

    return acao;
}