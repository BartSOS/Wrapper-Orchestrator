// =============================================================================
// INFRAESTRUTURA DE I/O PARA BOTS EM C (WRAPPER OFICIAL DO BOCA)
// Compilado em C++20 e ligado ao objeto do bot (C99) pelo build_bot.sh.
// Converte cada linha JSON do estado da arena para as structs de arena.h (nomes
// em português, tempos em frames, velocidades por frame), chama pensar() do
// competidor e traduz a Acao de volta para o protocolo da engine:
//   mover -> throttle, direcao -> turn, angulo_torre -> turret_angle,
//   atirar -> shoot, ligar_escudo -> shield.
// A mesma tradução existe em wrapper.py/arena.py, arena.hpp e Arena.java.
// =============================================================================
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include "arena.h"

using json = nlohmann::json;

namespace {

constexpr double SEGUNDOS_POR_FRAME = 0.1;  // TICK_DT da engine
constexpr double GIRO_POR_FRAME = 9.0;      // TANK_TURN_RATE (90 graus/s) * TICK_DT

double num(const json& j, const char* k) {
    auto it = j.find(k);
    return (it != j.end() && it->is_number()) ? it->get<double>() : 0.0;
}

int inteiro(const json& j, const char* k) {
    auto it = j.find(k);
    return (it != j.end() && it->is_number()) ? static_cast<int>(std::lround(it->get<double>())) : 0;
}

bool booleano(const json& j, const char* k) {
    auto it = j.find(k);
    if (it == j.end()) return false;
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_number()) return it->get<double>() != 0.0;
    return false;
}

std::string texto(const json& j, const char* k) {
    auto it = j.find(k);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

// Segundos da engine -> frames inteiros
int frames(const json& j, const char* k) {
    return static_cast<int>(std::lround(num(j, k) / SEGUNDOS_POR_FRAME));
}

// Unidades por segundo da engine -> unidades por frame
double por_frame(const json& j, const char* k) {
    return num(j, k) * SEGUNDOS_POR_FRAME;
}

// direcao desejada (graus) -> turn da engine: diferença no menor sentido, dividida
// pelo giro máximo de um frame, limitada a [-1, 1]
double turn_para(double direcao, double angulo_atual) {
    double dif = std::fmod(direcao - angulo_atual, 360.0);
    if (dif > 180.0) dif -= 360.0;
    if (dif < -180.0) dif += 360.0;
    return std::clamp(dif / GIRO_POR_FRAME, -1.0, 1.0);
}

void preencher_estado(const json& j, Estado* e) {
    *e = Estado{};
    e->frame = inteiro(j, "tick");

    if (j.contains("self") && j["self"].is_object()) {
        const json& s = j["self"];
        e->eu.id = inteiro(s, "id");
        e->eu.x = num(s, "x");
        e->eu.y = num(s, "y");
        e->eu.vel_x = por_frame(s, "vx");
        e->eu.vel_y = por_frame(s, "vy");
        e->eu.angulo_chassi = num(s, "angle");
        e->eu.angulo_torre = num(s, "turret_angle");
        e->eu.vida = num(s, "hp");
        e->eu.energia = num(s, "energy");
        e->eu.frames_para_atirar = frames(s, "cooldown");
        e->eu.escudo_ativo = booleano(s, "shield");
    }

    if (j.contains("enemies") && j["enemies"].is_array()) {
        for (const json& en : j["enemies"]) {
            if (e->n_inimigos >= MAX_INIMIGOS) break;
            Inimigo& in = e->inimigos[e->n_inimigos++];
            in.id = inteiro(en, "id");
            in.x = num(en, "x");
            in.y = num(en, "y");
            in.vel_x = por_frame(en, "vx");
            in.vel_y = por_frame(en, "vy");
        }
    }

    if (j.contains("hazards") && j["hazards"].is_array()) {
        for (const json& hz : j["hazards"]) {
            if (e->n_perigos >= MAX_PERIGOS) break;
            Perigo& p = e->perigos[e->n_perigos++];
            p.id = inteiro(hz, "id");
            p.tipo = texto(hz, "type") == "LASER" ? PERIGO_LASER : PERIGO_ARTILHARIA;
            p.x = num(hz, "x");
            p.y = num(hz, "y");
            p.raio = num(hz, "radius");
            p.frames_ate_impacto = frames(hz, "timer");
            p.angulo = num(hz, "angle");
            p.frames_restantes = frames(hz, "duration");
        }
    }

    if (j.contains("items") && j["items"].is_array()) {
        for (const json& it : j["items"]) {
            if (e->n_itens >= MAX_ITENS) break;
            Item& item = e->itens[e->n_itens++];
            item.id = inteiro(it, "id");
            item.tipo = texto(it, "type") == "ENERGY" ? ITEM_ENERGIA : ITEM_CURA;
            item.x = num(it, "x");
            item.y = num(it, "y");
        }
    }

    // Fases de treino: "objetivo": {"pontos": [{"id","x","y","raio"}, ...]}
    auto ob = j.find("objetivo");
    if (ob != j.end() && ob->is_object()) {
        auto ps = ob->find("pontos");
        if (ps != ob->end() && ps->is_array()) {
            for (const json& p : *ps) {
                if (e->n_pontos >= MAX_PONTOS) break;
                if (!p.is_object()) continue;
                Ponto& pt = e->pontos[e->n_pontos++];
                std::string id = texto(p, "id");
                std::snprintf(pt.id, sizeof(pt.id), "%s", id.c_str());
                pt.x = num(p, "x");
                pt.y = num(p, "y");
                pt.raio = num(p, "raio");
            }
        }
    }
}

// Acao (português) -> comando da engine. direcao ou angulo_torre que não são números
// finitos (NaN) valem como "mantém".
json traduzir_acao(const Acao& a, const Estado& e) {
    json saida;
    double mover = std::isfinite(a.mover) ? a.mover : 0.0;
    saida["throttle"] = std::clamp(mover, -1.0, 1.0);
    saida["turn"] = std::isfinite(a.direcao) ? turn_para(a.direcao, e.eu.angulo_chassi) : 0.0;
    if (std::isfinite(a.angulo_torre)) saida["turret_angle"] = a.angulo_torre;
    saida["shoot"] = a.atirar != 0;
    saida["shield"] = a.ligar_escudo != 0;
    return saida;
}

}  // namespace

int main() {
    // Protege o canal com a Engine: printf()/puts() do competidor passam a ir
    // para stderr, e as respostas usam uma cópia privada do stdout original.
    int fd_canal = dup(STDOUT_FILENO);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    FILE* canal = fdopen(fd_canal, "w");
    if (!canal) return 1;

    static Estado estado;  // grande demais para a pilha em alguns ambientes
    std::string linha;
    while (std::getline(std::cin, linha)) {
        if (linha.empty()) continue;
        json saida = {{"throttle", 0.0}, {"turn", 0.0}, {"shoot", false}, {"shield", false}};
        try {
            preencher_estado(json::parse(linha), &estado);
            Acao a = pensar(&estado);
            saida = traduzir_acao(a, estado);
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "[ERRO NO WRAPPER C]: %s\n", ex.what());
        }
        std::fprintf(canal, "%s\n", saida.dump().c_str());
        std::fflush(canal);
        std::fflush(stdout);
    }
    return 0;
}
