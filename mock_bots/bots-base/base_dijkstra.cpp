// =============================================================================
// NPC "dijkstra" - nível DIFÍCIL (C++20)
// =============================================================================
// Estratégia: atirador de elite. Mira o adversário mais próximo ANTECIPANDO o
// movimento dele (resolve a interceptação do projétil, que anda a 300 u/s) e
// mantém uma faixa de 200 a 300 de distância, circulando. O movimento é a soma de forças
// (campos de potencial): anel de distância do alvo, fuga de artilharia e do
// centro do laser, itens que vale pegar (cura com vida baixa, energia com pouca
// energia) e puxão para o centro quando a zona de radiação fecha (tick > 600).
// Liga o escudo se o adversário está perto e a vida está baixa.
// =============================================================================

#include <algorithm>
#include <cmath>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// -----------------------------------------------------------------------------
// AJUDANTES (opcional: renomeie, altere ou apague)
// -----------------------------------------------------------------------------
// Eles só facilitam o acesso aos atributos do estado e não decidem nada por
// você. Se preferir, apague esta seção inteira e leia o json 'estado' direto
// (veja o formato em README.md / enunciado).

enum TipoHazard { HAZARD_ARTILHARIA = 0, HAZARD_LASER = 1 };
enum TipoItem { ITEM_CURA = 0, ITEM_ENERGIA = 1 };

// O seu tanque (devolvido por eu)
struct Tanque {
    int    id = 0;               // identificador
    double x = 0, y = 0;         // posição (arena de 1000 x 1000)
    double vx = 0, vy = 0;       // velocidade, em unidades/s
    double angle = 0;            // direção do chassi, graus [0, 360)
    double turret_angle = 0;     // direção da torre, graus [0, 360)
    double hp = 0;               // vida, de 0 a 100
    double energy = 0;           // energia, de 0 a 100
    double cooldown = 0;         // segundos até poder atirar (0 = pronto)
    bool   shield = false;       // escudo ativo agora
};

// Um adversário vivo (a vida dele não é informada)
struct Inimigo {
    int    id = 0;
    double x = 0, y = 0;
    double vx = 0, vy = 0;
};

// Um perigo da arena (artilharia ou laser)
struct Hazard {
    int        id = 0;
    TipoHazard tipo = HAZARD_ARTILHARIA;
    double     x = 0, y = 0;
    double     radius = 0;       // raio de efeito
    double     timer = 0;        // segundos até o impacto / a ativação
    double     angle = 0;        // direção do feixe (laser), graus
    double     duration = 0;     // segundos de efeito (laser)
};

// Um item coletável no mapa
struct Item {
    int      id = 0;
    TipoItem tipo = ITEM_CURA;
    double   x = 0, y = 0;
};

// Número do tick atual (10 ticks por segundo)
int tick(const json& estado) {
    return estado.value("tick", 0);
}

// Seu tanque
Tanque eu(const json& estado) {
    Tanque t;
    if (!estado.contains("self")) return t;
    const json& d = estado["self"];
    t.id = d.value("id", 0);
    t.x = d.value("x", 0.0);
    t.y = d.value("y", 0.0);
    t.vx = d.value("vx", 0.0);
    t.vy = d.value("vy", 0.0);
    t.angle = d.value("angle", 0.0);
    t.turret_angle = d.value("turret_angle", 0.0);
    t.hp = d.value("hp", 0.0);
    t.energy = d.value("energy", 0.0);
    t.cooldown = d.value("cooldown", 0.0);
    t.shield = d.value("shield", false);
    return t;
}

// Lista dos adversários vivos
std::vector<Inimigo> inimigos(const json& estado) {
    std::vector<Inimigo> lista;
    if (!estado.contains("enemies")) return lista;
    for (const json& d : estado["enemies"]) {
        Inimigo i;
        i.id = d.value("id", 0);
        i.x = d.value("x", 0.0);
        i.y = d.value("y", 0.0);
        i.vx = d.value("vx", 0.0);
        i.vy = d.value("vy", 0.0);
        lista.push_back(i);
    }
    return lista;
}

// Lista dos perigos ativos (artilharia e laser)
std::vector<Hazard> perigos(const json& estado) {
    std::vector<Hazard> lista;
    if (!estado.contains("hazards")) return lista;
    for (const json& d : estado["hazards"]) {
        Hazard h;
        h.id = d.value("id", 0);
        h.tipo = d.value("type", std::string()) == "LASER" ? HAZARD_LASER : HAZARD_ARTILHARIA;
        h.x = d.value("x", 0.0);
        h.y = d.value("y", 0.0);
        h.radius = d.value("radius", 0.0);
        h.timer = d.value("timer", 0.0);
        h.angle = d.value("angle", 0.0);
        h.duration = d.value("duration", 0.0);
        lista.push_back(h);
    }
    return lista;
}

// Lista dos itens no mapa
std::vector<Item> itens(const json& estado) {
    std::vector<Item> lista;
    if (!estado.contains("items")) return lista;
    for (const json& d : estado["items"]) {
        Item it;
        it.id = d.value("id", 0);
        it.tipo = d.value("type", std::string()) == "ENERGY" ? ITEM_ENERGIA : ITEM_CURA;
        it.x = d.value("x", 0.0);
        it.y = d.value("y", 0.0);
        lista.push_back(it);
    }
    return lista;
}

// Monta a ação no formato da arena, com os tipos certos.
//   throttle: -1.0 (ré) a 1.0 (frente)
//   turn: -1.0 a 1.0 (positivo aumenta o ângulo do chassi)
//   turret_angle: direção absoluta da torre em graus (a variante sem este
//                 parâmetro mantém a torre onde está)
//   shoot, shield: true/false
json montar_acao(double throttle, double turn, double turret_angle, bool shoot, bool shield) {
    return json{{"throttle", throttle}, {"turn", turn}, {"turret_angle", turret_angle},
                {"shoot", shoot}, {"shield", shield}};
}

json montar_acao(double throttle, double turn, bool shoot, bool shield) {
    return json{{"throttle", throttle}, {"turn", turn}, {"shoot", shoot}, {"shield", shield}};
}

// Matemática básica (ângulos em graus; 0 = +x, cresce de +x para +y)

// Distância entre dois pontos
double distancia(double x1, double y1, double x2, double y2) {
    return std::hypot(x2 - x1, y2 - y1);
}

// Leva qualquer ângulo para o intervalo [0, 360)
double normalizar_angulo(double graus) {
    double a = std::fmod(graus, 360.0);
    return a < 0 ? a + 360.0 : a;
}

// Ângulo, em graus, do ponto (x1, y1) olhando para o ponto (x2, y2)
double angulo_para(double x1, double y1, double x2, double y2) {
    const double pi = 3.14159265358979323846;
    return normalizar_angulo(std::atan2(y2 - y1, x2 - x1) * 180.0 / pi);
}

// Menor giro, em graus, para ir do ângulo 'de' ao ângulo 'para': de -180 (exclusivo) a 180.
// Positivo = girar no sentido em que o ângulo aumenta.
double diferenca_angulo(double de, double para) {
    double d = normalizar_angulo(para - de);
    return d > 180.0 ? d - 360.0 : d;
}

// -----------------------------------------------------------------------------
// FIM DOS AJUDANTES
// -----------------------------------------------------------------------------

namespace {

const double PI = 3.14159265358979323846;
const double VELOCIDADE_PROJETIL = 300.0;   // unidades por segundo

// Vetor soma das forças que atuam no tanque
struct Forca {
    double x = 0, y = 0;
};

// Soma ao vetor um empurrão de intensidade 'peso' na direção 'angulo' (graus)
void empurrar(Forca& f, double angulo, double peso) {
    f.x += std::cos(angulo * PI / 180.0) * peso;
    f.y += std::sin(angulo * PI / 180.0) * peso;
}

// Direção da torre para acertar um adversário em movimento: resolve o instante t
// em que o projétil (velocidade constante) encontra o alvo andando em linha reta.
double mira_com_antecipacao(const Tanque& meu, const Inimigo& alvo) {
    double dx = alvo.x - meu.x, dy = alvo.y - meu.y;
    double a = alvo.vx * alvo.vx + alvo.vy * alvo.vy - VELOCIDADE_PROJETIL * VELOCIDADE_PROJETIL;
    double b = 2.0 * (dx * alvo.vx + dy * alvo.vy);
    double c = dx * dx + dy * dy;
    double t = 0.0;
    if (std::fabs(a) < 1e-9) {
        if (std::fabs(b) > 1e-9) t = -c / b;
    } else {
        double disc = b * b - 4.0 * a * c;
        if (disc >= 0.0) {
            double s = std::sqrt(disc);
            double t1 = (-b - s) / (2.0 * a), t2 = (-b + s) / (2.0 * a);
            t = (t1 > 0 && t2 > 0) ? std::min(t1, t2) : std::max(t1, t2);
        }
    }
    t = std::max(0.0, std::min(t, 3.0));
    return angulo_para(meu.x, meu.y, alvo.x + alvo.vx * t, alvo.y + alvo.vy * t);
}

}  // namespace

json pensar(const json& estado) {
    Tanque meu = eu(estado);
    int agora = tick(estado);
    Forca f;
    double torre = meu.turret_angle;
    bool atirar = false, escudo = false;

    // Alvo: o adversário mais próximo
    const std::vector<Inimigo> lista_inimigos = inimigos(estado);
    const Inimigo* alvo = nullptr;
    double menor = 1e9;
    for (const Inimigo& i : lista_inimigos) {
        double d = distancia(meu.x, meu.y, i.x, i.y);
        if (d < menor) {
            menor = d;
            alvo = &i;
        }
    }
    if (alvo != nullptr) {
        double para_o_alvo = angulo_para(meu.x, meu.y, alvo->x, alvo->y);
        torre = mira_com_antecipacao(meu, *alvo);
        atirar = menor < 600 && meu.cooldown <= 0 && meu.energy >= 20;
        // a volta em torno do alvo troca de sentido a cada 4 s, para não ser previsível
        double sentido = (agora / 40) % 2 == 0 ? 90.0 : -90.0;
        if (menor > 300) empurrar(f, para_o_alvo, 1.0);              // longe: aproxima
        else if (menor < 200) empurrar(f, para_o_alvo + 180.0, 1.0); // perto: recua
        else empurrar(f, para_o_alvo + sentido, 1.0);                // na faixa: circula
        escudo = menor < 250 && meu.hp < 50 && meu.energy > 60;
    }

    // Itens que valem a viagem: cura com vida baixa, energia com pouca energia
    for (const Item& item : itens(estado)) {
        bool quero = (item.tipo == ITEM_CURA && meu.hp < 70) || (item.tipo == ITEM_ENERGIA && meu.energy < 40);
        if (quero && distancia(meu.x, meu.y, item.x, item.y) < 250) {
            empurrar(f, angulo_para(meu.x, meu.y, item.x, item.y), 1.5);
        }
    }

    // Perigos: sai do raio da artilharia e fica longe do centro do laser
    for (const Hazard& perigo : perigos(estado)) {
        double d = distancia(meu.x, meu.y, perigo.x, perigo.y);
        double fugir = angulo_para(perigo.x, perigo.y, meu.x, meu.y);
        if (perigo.tipo == HAZARD_ARTILHARIA && d < perigo.radius + 60) empurrar(f, fugir, 4.0);
        if (perigo.tipo == HAZARD_LASER && d < 160) empurrar(f, fugir, 3.0);
    }

    // Zona de radiação: depois do tick 600 o raio seguro encolhe a partir do centro
    double raio_seguro = agora > 600 ? std::max(80.0, 550.0 - (agora - 600) * 0.7) : 550.0;
    if (distancia(meu.x, meu.y, 500, 500) > raio_seguro - 80) {
        empurrar(f, angulo_para(meu.x, meu.y, 500, 500), 3.0);
    }

    // Segue a força resultante: gira até a direção (9 graus por tick no máximo) e anda
    double direcao = (f.x == 0 && f.y == 0) ? meu.angle : angulo_para(0, 0, f.x, f.y);
    double dif = diferenca_angulo(meu.angle, direcao);
    double giro = std::max(-1.0, std::min(1.0, dif / 9.0));
    double acel = std::fabs(dif) < 60 ? 1.0 : 0.2;
    return montar_acao(acel, giro, torre, atirar, escudo);
}
