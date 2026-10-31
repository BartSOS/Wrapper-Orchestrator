// =============================================================================
// INFRAESTRUTURA DE I/O PARA BOTS EM C (WRAPPER OFICIAL DO BOCA)
// Compilado em C++20 e ligado ao objeto do bot (C99) pelo build_bot.sh.
// Converte cada linha JSON do estado da arena para as structs de arena.h,
// chama pensar() do competidor e devolve a Acao como JSON para a Engine.
// =============================================================================
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include "arena.h"

using json = nlohmann::json;

static double num(const json& j, const char* k) {
    auto it = j.find(k);
    return (it != j.end() && it->is_number()) ? it->get<double>() : 0.0;
}

static int inteiro(const json& j, const char* k) {
    auto it = j.find(k);
    return (it != j.end() && it->is_number()) ? it->get<int>() : 0;
}

static std::string texto(const json& j, const char* k) {
    auto it = j.find(k);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

static void preencher_estado(const json& j, Estado* e) {
    *e = Estado{};
    e->tick = inteiro(j, "tick");

    if (j.contains("self") && j["self"].is_object()) {
        const json& s = j["self"];
        e->self.id = inteiro(s, "id");
        e->self.x = num(s, "x");
        e->self.y = num(s, "y");
        e->self.vx = num(s, "vx");
        e->self.vy = num(s, "vy");
        e->self.angle = num(s, "angle");
        e->self.turret_angle = num(s, "turret_angle");
        e->self.hp = num(s, "hp");
        e->self.energy = num(s, "energy");
        e->self.cooldown = num(s, "cooldown");
        e->self.shield = s.value("shield", false);
    }

    if (j.contains("enemies") && j["enemies"].is_array()) {
        for (const json& en : j["enemies"]) {
            if (e->n_inimigos >= MAX_INIMIGOS) break;
            Inimigo& in = e->inimigos[e->n_inimigos++];
            in.id = inteiro(en, "id");
            in.x = num(en, "x");
            in.y = num(en, "y");
            in.vx = num(en, "vx");
            in.vy = num(en, "vy");
        }
    }

    if (j.contains("hazards") && j["hazards"].is_array()) {
        for (const json& hz : j["hazards"]) {
            if (e->n_hazards >= MAX_HAZARDS) break;
            Hazard& h = e->hazards[e->n_hazards++];
            h.id = inteiro(hz, "id");
            h.tipo = texto(hz, "type") == "LASER" ? HAZARD_LASER : HAZARD_ARTILHARIA;
            h.x = num(hz, "x");
            h.y = num(hz, "y");
            h.radius = num(hz, "radius");
            h.timer = num(hz, "timer");
            h.angle = num(hz, "angle");
            h.duration = num(hz, "duration");
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
            saida = {
                {"throttle", a.throttle},
                {"turn", a.turn},
                {"turret_angle", a.turret_angle},
                {"shoot", a.shoot},
                {"shield", a.shield}
            };
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "[ERRO NO WRAPPER C]: %s\n", ex.what());
        }
        std::fprintf(canal, "%s\n", saida.dump().c_str());
        std::fflush(canal);
        std::fflush(stdout);
    }
    return 0;
}
