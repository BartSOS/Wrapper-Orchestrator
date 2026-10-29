// Armadilha: campos da ação com o tipo "errado" (shoot e shield como inteiro, turn como bool).
// Antes, "shoot": 1 lançava type_error na engine e a ação inteira do tick era descartada:
// este bot nunca atirava. Agora 1 vale como verdadeiro e o campo com tipo errado (turn)
// perde só ele. O teste de robustez confere que este bot abate alguém.
#include <cmath>

json pensar(const json& estado) {
    const json& eu = estado["self"];
    double ex = eu["x"].get<double>(), ey = eu["y"].get<double>();
    double melhor = 1e18, ax = 500.0, ay = 500.0;
    for (const auto& in : estado["enemies"]) {
        double dx = in["x"].get<double>() - ex, dy = in["y"].get<double>() - ey;
        double d = dx * dx + dy * dy;
        if (d < melhor) {
            melhor = d;
            ax = in["x"].get<double>();
            ay = in["y"].get<double>();
        }
    }
    double torre = std::atan2(ay - ey, ax - ex) * 180.0 / M_PI;
    return {{"throttle", 0}, {"turn", true}, {"turret_angle", torre}, {"shoot", 1}, {"shield", 0}};
}
