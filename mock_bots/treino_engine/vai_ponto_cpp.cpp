// Bot de teste do modo cenário (frente engine), em C++20: vai até o primeiro ponto de
// estado["objetivo"]["pontos"] e para dentro do raio. Sem a chave, fica parado.
json pensar(const json& estado) {
    const json& eu = estado["self"];
    json acao = {{"throttle", 0.0}, {"turn", 0.0}, {"turret_angle", eu["turret_angle"]}, {"shoot", false}, {"shield", false}};
    if (!estado.contains("objetivo") || estado["objetivo"]["pontos"].empty()) return acao;
    const json& p = estado["objetivo"]["pontos"][0];
    double dx = p["x"].get<double>() - eu["x"].get<double>();
    double dy = p["y"].get<double>() - eu["y"].get<double>();
    if (std::hypot(dx, dy) <= p["raio"].get<double>() * 0.5) return acao;
    double alvo = std::atan2(dy, dx) * 180.0 / M_PI;
    double erro = std::fmod(alvo - eu["angle"].get<double>() + 540.0, 360.0) - 180.0;
    acao["turn"] = std::clamp(erro / 9.0, -1.0, 1.0);
    acao["throttle"] = std::fabs(erro) < 30.0 ? 1.0 : 0.2;
    return acao;
}
