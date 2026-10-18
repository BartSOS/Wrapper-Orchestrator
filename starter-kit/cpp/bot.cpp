// =============================================================================
// GAME JAM PET 2026 - STARTER KIT C++20
// =============================================================================
// O que você precisa fazer: escrever a função pensar(), mais abaixo.
// Ela é chamada 10 vezes por segundo e devolve o que o seu tanque vai fazer.
//
// Regras do código enviado:
// - Não crie main(): a arena já tem a dela e chama pensar() por você.
// - Não use std::cin nem std::cout (a saída vai para o log de erros e é
//   descartada; use std::cerr se quiser depurar).
// - A assinatura de pensar() não pode mudar: json pensar(const json& estado)
// - Variáveis globais e static valem durante a partida inteira (sua memória).
// - O pensar() abaixo lê todos os campos só para mostrar o que existe: os avisos
//   de "variável não usada" do compilador somem quando você usar os valores.
// =============================================================================

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


// =============================================================================
// SEU CÓDIGO COMEÇA AQUI
// =============================================================================

json pensar(const json& estado) {
    // --- O jogo ---
    int t = tick(estado);                 // número do tick atual (a partida tem até 1500)

    // --- O seu tanque ---
    Tanque meu = eu(estado);
    double x = meu.x;                     // posição horizontal
    double y = meu.y;                     // posição vertical
    double vx = meu.vx;                   // velocidade em x (unidades/s)
    double vy = meu.vy;                   // velocidade em y (unidades/s)
    double angulo = meu.angle;            // para onde o chassi aponta (graus)
    double torre = meu.turret_angle;      // para onde a torre aponta (graus)
    double vida = meu.hp;                 // vida: chegando a 0, você é eliminado
    double energia = meu.energy;          // energia: tiro e escudo gastam
    double recarga = meu.cooldown;        // segundos até poder atirar de novo
    bool escudo = meu.shield;             // true se o escudo está ligado

    // --- Adversários vivos ---
    for (const Inimigo& inimigo : inimigos(estado)) {
        int id = inimigo.id;              // identificador
        double ix = inimigo.x;            // posição
        double iy = inimigo.y;
        double ivx = inimigo.vx;          // velocidade
        double ivy = inimigo.vy;
    }

    // --- Perigos da arena ---
    for (const Hazard& perigo : perigos(estado)) {
        int id = perigo.id;               // identificador
        TipoHazard tipo = perigo.tipo;    // HAZARD_ARTILHARIA ou HAZARD_LASER
        double px = perigo.x;             // posição
        double py = perigo.y;
        double raio = perigo.radius;      // raio de efeito
        double prazo = perigo.timer;      // segundos até o impacto / a ativação
        double direcao = perigo.angle;    // direção do feixe (laser)
        double duracao = perigo.duration; // segundos de efeito (laser)
    }

    // --- Itens no mapa ---
    for (const Item& item : itens(estado)) {
        int id = item.id;                 // identificador
        TipoItem tipo = item.tipo;        // ITEM_CURA ou ITEM_ENERGIA
        double ix = item.x;               // posição
        double iy = item.y;
    }

    // --- A ação: andar para a frente girando devagar, torre parada, sem atirar ---
    return montar_acao(
        1.0,      // throttle: -1.0 (ré) a 1.0 (frente)
        0.5,      // turn: -1.0 a 1.0 (positivo aumenta o ângulo do chassi)
        // torre, // turret_angle: direção da torre (variante sem ele: mantém como está)
        false,    // shoot: true para atirar (precisa de recarga == 0 e energia)
        false);   // shield: true para ligar o escudo (gasta energia)
}
