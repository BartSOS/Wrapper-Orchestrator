// =============================================================================
// INFRAESTRUTURA DE I/O PARA BOTS EM C++ (WRAPPER OFICIAL DO BOCA)
// Pré-compilado pelo build_bot.sh com o cabeçalho cpp_header.hpp, em duas versões:
//   cpp_footer.o       chama  Acao pensar(const Estado& e)      (arena.hpp)
//   cpp_footer_json.o  chama  json pensar(const json& estado)   (-DGAMEJAM_PENSAR_JSON,
//                                                               bots antigos)
// O bot é ligado à versão que corresponde ao pensar() que ele define.
// Converte o estado da engine para os nomes em português (tempos em frames,
// velocidades por frame) e a ação de volta para o protocolo da engine:
//   mover -> throttle, direcao -> turn, angulo_torre -> turret_angle,
//   atirar -> shoot, ligar_escudo -> shield.
// A mesma tradução existe em c_wrapper.cpp, arena.py e Arena.java. Chaves antigas
// (throttle, turn...) num json devolvido continuam aceitas, com um [AVISO] no stderr.
// =============================================================================
#include <cstdio>
#include <set>
#include <unistd.h>

namespace {

constexpr double SEGUNDOS_POR_FRAME = 0.1;  // TICK_DT da engine
constexpr double GIRO_POR_FRAME = 9.0;      // TANK_TURN_RATE (90 graus/s) * TICK_DT

double num(const json& j, const char* k) {
    if (!j.is_object()) return 0.0;
    auto it = j.find(k);
    return (it != j.end() && it->is_number()) ? it->get<double>() : 0.0;
}

int inteiro(const json& j, const char* k) {
    return static_cast<int>(std::lround(num(j, k)));
}

bool booleano(const json& j, const char* k) {
    if (!j.is_object()) return false;
    auto it = j.find(k);
    if (it == j.end()) return false;
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_number()) return it->get<double>() != 0.0;
    return false;
}

std::string texto(const json& j, const char* k) {
    if (!j.is_object()) return std::string();
    auto it = j.find(k);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

int frames(const json& j, const char* k) {
    return static_cast<int>(std::lround(num(j, k) / SEGUNDOS_POR_FRAME));
}

double por_frame(const json& j, const char* k) {
    return num(j, k) * SEGUNDOS_POR_FRAME;
}

double turn_para(double direcao, double angulo_atual) {
    double dif = std::fmod(direcao - angulo_atual, 360.0);
    if (dif > 180.0) dif -= 360.0;
    if (dif < -180.0) dif += 360.0;
    return std::clamp(dif / GIRO_POR_FRAME, -1.0, 1.0);
}

const json& filho(const json& j, const char* k) {
    static const json vazio = json::object();
    if (!j.is_object()) return vazio;
    auto it = j.find(k);
    return it != j.end() ? *it : vazio;
}

Estado estado_de(const json& j) {
    Estado e;
    e.frame = inteiro(j, "tick");
    const json& s = filho(j, "self");
    e.eu.id = inteiro(s, "id");
    e.eu.x = num(s, "x");
    e.eu.y = num(s, "y");
    e.eu.vel_x = por_frame(s, "vx");
    e.eu.vel_y = por_frame(s, "vy");
    e.eu.angulo_chassi = num(s, "angle");
    e.eu.angulo_torre = num(s, "turret_angle");
    e.eu.vida = num(s, "hp");
    e.eu.energia = num(s, "energy");
    e.eu.frames_para_atirar = frames(s, "cooldown");
    e.eu.escudo_ativo = booleano(s, "shield");

    const json& ens = filho(j, "enemies");
    if (ens.is_array()) {
        for (const json& en : ens) {
            Inimigo in;
            in.id = inteiro(en, "id");
            in.x = num(en, "x");
            in.y = num(en, "y");
            in.vel_x = por_frame(en, "vx");
            in.vel_y = por_frame(en, "vy");
            e.inimigos.push_back(in);
        }
    }
    const json& hzs = filho(j, "hazards");
    if (hzs.is_array()) {
        for (const json& hz : hzs) {
            Perigo p;
            p.id = inteiro(hz, "id");
            p.tipo = texto(hz, "type") == "LASER" ? PERIGO_LASER : PERIGO_ARTILHARIA;
            p.x = num(hz, "x");
            p.y = num(hz, "y");
            p.raio = num(hz, "radius");
            p.frames_ate_impacto = frames(hz, "timer");
            p.angulo = num(hz, "angle");
            p.frames_restantes = frames(hz, "duration");
            e.perigos.push_back(p);
        }
    }
    const json& its = filho(j, "items");
    if (its.is_array()) {
        for (const json& it : its) {
            Item item;
            item.id = inteiro(it, "id");
            item.tipo = texto(it, "type") == "ENERGY" ? ITEM_ENERGIA : ITEM_CURA;
            item.x = num(it, "x");
            item.y = num(it, "y");
            e.itens.push_back(item);
        }
    }
    const json& ps = filho(filho(j, "objetivo"), "pontos");
    if (ps.is_array()) {
        for (const json& p : ps) {
            if (!p.is_object()) continue;
            Ponto pt;
            pt.id = texto(p, "id");
            pt.x = num(p, "x");
            pt.y = num(p, "y");
            pt.raio = num(p, "raio");
            e.pontos.push_back(pt);
        }
    }
    e.n_inimigos = static_cast<int>(e.inimigos.size());
    e.n_perigos = static_cast<int>(e.perigos.size());
    e.n_itens = static_cast<int>(e.itens.size());
    e.n_pontos = static_cast<int>(e.pontos.size());
    return e;
}

json comando_de(const Acao& a, double angulo_chassi) {
    json saida;
    double mover = std::isfinite(a.mover) ? a.mover : 0.0;
    saida["throttle"] = std::clamp(mover, -1.0, 1.0);
    saida["turn"] = std::isfinite(a.direcao) ? turn_para(a.direcao, angulo_chassi) : 0.0;
    if (std::isfinite(a.angulo_torre)) saida["turret_angle"] = a.angulo_torre;
    saida["shoot"] = a.atirar;
    saida["shield"] = a.ligar_escudo;
    return saida;
}

#ifdef GAMEJAM_PENSAR_JSON
std::set<std::string> avisados;

void avisar(const std::string& chave, const std::string& msg) {
    if (!avisados.insert(chave).second) return;
    std::cerr << "[AVISO] " << msg << std::endl;
}

// json devolvido pelo pensar() antigo: chaves novas ou antigas (com aviso) -> comando da engine
json comando_de_json(const json& acao, const json& estado) {
    if (!acao.is_object()) {
        throw std::runtime_error("pensar() devolveu um json que nao e um objeto");
    }
    static const std::map<std::string, std::string> antigas = {
        {"throttle", "mover"}, {"turn", "direcao"}, {"turret_angle", "angulo_torre"},
        {"shoot", "atirar"}, {"shield", "ligar_escudo"}};
    static const std::set<std::string> novas = {"mover", "direcao", "angulo_torre", "atirar", "ligar_escudo"};
    for (auto it = acao.begin(); it != acao.end(); ++it) {
        const std::string& k = it.key();
        auto ant = antigas.find(k);
        if (ant != antigas.end()) {
            if (k == "turn") {
                avisar(k, "use 'direcao' (para onde o tanque deve apontar, em graus) em vez de 'turn'. "
                          "A chave antiga ainda funciona.");
            } else {
                avisar(k, "use '" + ant->second + "' em vez de '" + k + "' na acao. A chave antiga ainda funciona.");
            }
        } else if (!novas.count(k)) {
            avisar("?" + k, "a acao tem a chave '" + k + "', que nao existe e foi ignorada. "
                            "Use: mover, direcao, angulo_torre, atirar, ligar_escudo.");
        }
    }
    auto numero = [&](const char* nova, const char* antiga, bool& tem) -> double {
        for (const char* k : {nova, antiga}) {
            auto it = acao.find(k);
            if (it == acao.end() || it->is_null()) continue;
            if (it->is_boolean()) { tem = true; return it->get<bool>() ? 1.0 : 0.0; }
            if (it->is_number()) { tem = true; return it->get<double>(); }
            avisar(std::string("tipo_") + k, std::string("'") + k + "' nao e um numero: ignorado.");
        }
        tem = false;
        return 0.0;
    };
    bool tem = false;
    json saida;
    double mover = numero("mover", "throttle", tem);
    saida["throttle"] = std::clamp(std::isfinite(mover) ? mover : 0.0, -1.0, 1.0);
    double direcao = numero("direcao", "", tem);
    if (tem && std::isfinite(direcao)) {
        saida["turn"] = turn_para(direcao, num(filho(estado, "self"), "angle"));
    } else {
        double turn = numero("", "turn", tem);
        saida["turn"] = std::clamp(std::isfinite(turn) ? turn : 0.0, -1.0, 1.0);
    }
    double torre = numero("angulo_torre", "turret_angle", tem);
    if (tem && std::isfinite(torre)) saida["turret_angle"] = torre;
    saida["shoot"] = numero("atirar", "shoot", tem) != 0.0;
    saida["shield"] = numero("ligar_escudo", "shield", tem) != 0.0;
    return saida;
}
#endif

}  // namespace

int main() {
    // Protege o canal com a Engine: std::cout/printf do competidor passam a ir para o
    // stderr, e as respostas usam uma cópia privada do stdout original.
    int fd_canal = dup(STDOUT_FILENO);
    dup2(STDERR_FILENO, STDOUT_FILENO);
    FILE* canal = fdopen(fd_canal, "w");
    if (!canal) return 1;

    // Toda linha de estado recebe exatamente uma resposta: se o pensar() lançar qualquer
    // exceção, a resposta é a ação vazia {} (sem isso a engine esperaria a resposta que
    // não veio, e o bot seria eliminado 20 frames depois).
    std::string linha;
    while (std::getline(std::cin, linha)) {
        if (linha.empty()) continue;
        std::string resposta = "{}";
        try {
            json estado_arena = json::parse(linha);
#ifdef GAMEJAM_PENSAR_JSON
            resposta = comando_de_json(pensar(estado_arena), estado_arena).dump();
#else
            Estado e = estado_de(estado_arena);
            Acao a = pensar(e);
            resposta = comando_de(a, e.eu.angulo_chassi).dump();
#endif
        } catch (const std::exception& ex) {
            std::cerr << "[ERRO NO BOT C++]: " << ex.what() << " (neste frame o tanque ficou parado)" << std::endl;
        } catch (...) {
            std::cerr << "[ERRO NO BOT C++]: excecao desconhecida (neste frame o tanque ficou parado)" << std::endl;
        }
        std::fprintf(canal, "%s\n", resposta.c_str());
        std::fflush(canal);
        std::cout.flush();
    }
    return 0;
}
