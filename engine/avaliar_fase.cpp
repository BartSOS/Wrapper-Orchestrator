// ==============================================================================
// Game Jam PET - Avaliador das fases de treino (contrato C-F2)
// ==============================================================================
// Uso:  avaliar_fase <fase.json> <match_frames.json>
// Saída (stdout): um JSON com o veredito da tentativa:
//   {"fase", "passou", "baloes", "motivo", "metricas": {ticks, tiros, vida, dano_recebido},
//    "objetivos": [{"tipo", "ok", "detalhe"}], "proximo_balao", "tick_final", "seed",
//    "pontos_consumidos": {"<id do ponto>": tick}}
// pontos_consumidos: tick em que cada ponto passou a contar (chegar: a entrada no raio; ficar_no_raio:
// o último dos ticks seguidos; visitar_em_ordem: a entrada na vez dele). Ponto que não contou fica de
// fora. O script.sh copia esse tick para fase.pontos[].consumido_tick do replay (o visualizador o esconde).
// Código de saída: 0 quando avaliou (passou ou não); 2 quando a entrada é inválida
// (arquivo ausente, JSON quebrado, tipo de objetivo ou métrica desconhecidos, ponto/alvo
// que o replay não traz). Nesse caso o stdout traz {"fase", "erro"} e o stderr a mensagem.
//
// Regras (documentadas também no SKILL.md da boca-gerenciar-engine):
// - Jogador = o tanque dos frames sem "alvo": true (e fora de fase.alvos[].tank_id).
// - Pontos e alvos efetivos (já sorteados) vêm do bloco "fase" do match_frames.json; se ele
//   faltar, valem os números fixos do cenário do fase.json (faixa [min,max] sem o bloco = erro).
// - chegar / ficar_no_raio / visitar_em_ordem: distância do CENTRO do tanque ao ponto <= raio.
// - acertar_alvos: o alvo conta quando morre (alive=false) até ate_tick; max_tiros conta os
//   tiros do jogador até o tick em que o último alvo caiu.
// - sobreviver: vivo em todos os frames até ate_tick (ou até o último tick da partida, quando
//   ate_tick >= max_ticks) e, se houver vida_min, com vida >= vida_min no fim.
// - Métricas: ticks = tick em que o último objetivo foi cumprido (se não passou, o último tick
//   do replay); tiros = disparos do jogador; vida = vida no fim; dano_recebido = soma das perdas.
// - Balões: passou = 1. O nível k (2, 3) vale quando TODAS as entradas com "baloes": k
//   valem (metrica <= max; ou >= min) e o nível k-1 também valeu. Nível sem entradas não vale.
// Compilar: g++ -std=c++20 -O2 -o build/avaliar_fase engine/avaliar_fase.cpp
// ==============================================================================
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace {

constexpr double RAIO_TANQUE = 20.0;
constexpr double RAIO_PROJETIL = 4.0;
constexpr double DT = 0.1;
constexpr double RAIO_ARTILHARIA_EFETIVO = 80.0 + RAIO_TANQUE;

struct EntradaInvalida : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Ponto {
    std::string id;
    double x = 0, y = 0, raio = 0;
};

struct Alvo {
    std::string id;
    int tank_id = -1;
};

struct Projetil {
    int id;
    double x, y, vx, vy;
};

struct Quadro {
    int tick = 0;
    bool tem_jogador = false;
    bool vivo = false;
    double x = 0, y = 0, hp = 0;
    std::map<int, bool> alvo_vivo;                       // tank_id -> vivo
    std::map<int, std::pair<double, double>> alvo_pos;   // tank_id -> (x, y)
    std::vector<Projetil> projeteis;                     // só os do jogador
    int tiros = 0;                                       // SHOOT do jogador neste tick
    bool artilharia_perto = false;                       // explosão de artilharia que alcança o jogador
    bool atingido_por_tiro = false;                      // HIT com o jogador como alvo
};

struct ResultadoObjetivo {
    explicit ResultadoObjetivo(std::string t) : tipo(std::move(t)) {}
    std::string tipo;
    bool ok = false;
    int tick = -1;          // tick em que foi cumprido
    std::string detalhe;    // curto, para a tabela de objetivos
    std::string motivo;     // frase completa (sucesso ou falha)
};

// ------------------------------------------------------------------------------
// Utilidades
// ------------------------------------------------------------------------------
std::string inteiro(double v) { return std::to_string(static_cast<long long>(std::llround(v))); }

std::string posicao(double x, double y) { return "(" + inteiro(x) + ", " + inteiro(y) + ")"; }

std::string umDecimal(double v) {
    std::ostringstream s;
    s.setf(std::ios::fixed);
    s.precision(1);
    s << v;
    std::string r = s.str();
    std::replace(r.begin(), r.end(), '.', ',');
    return r;
}

double dist(double x1, double y1, double x2, double y2) { return std::hypot(x2 - x1, y2 - y1); }

// Distância do ponto (px, py) ao segmento (ax, ay)-(bx, by)
double distSegmento(double ax, double ay, double bx, double by, double px, double py) {
    double dx = bx - ax, dy = by - ay;
    double l2 = dx * dx + dy * dy;
    double t = l2 > 0 ? std::clamp(((px - ax) * dx + (py - ay) * dy) / l2, 0.0, 1.0) : 0.0;
    return dist(ax + t * dx, ay + t * dy, px, py);
}

std::string juntar(const std::vector<std::string>& partes, const std::string& sep, const std::string& ultimo) {
    std::string r;
    for (size_t i = 0; i < partes.size(); ++i) {
        if (i > 0) r += (i + 1 == partes.size()) ? ultimo : sep;
        r += partes[i];
    }
    return r;
}

json lerJson(const std::string& caminho, const std::string& nome) {
    std::ifstream f(caminho, std::ios::binary);
    if (!f) throw EntradaInvalida("não foi possível abrir " + nome + ": " + caminho);
    try {
        return json::parse(f);
    } catch (const json::parse_error& e) {
        throw EntradaInvalida(nome + " não é um JSON válido (" + std::string(e.what()) + ")");
    }
}

double numero(const json& j, const char* chave, const std::string& onde) {
    if (!j.contains(chave) || !j[chave].is_number())
        throw EntradaInvalida(onde + ": campo \"" + chave + "\" ausente ou não numérico");
    return j[chave].get<double>();
}

int inteiroCampo(const json& j, const char* chave, const std::string& onde) {
    return static_cast<int>(std::llround(numero(j, chave, onde)));
}

// ------------------------------------------------------------------------------
// Avaliador
// ------------------------------------------------------------------------------
class Avaliador {
public:
    Avaliador(const json& fase, const json& replay) : fase_(fase), replay_(replay) {}

    json avaliar() {
        validarFase();
        carregarCenario();
        carregarQuadros();

        json saida;
        saida["fase"] = id_;
        if (!jogadorApareceu_) {
            saida["passou"] = false;
            saida["baloes"] = 0;
            saida["motivo"] = "Seu tanque não entrou na arena (o bot não iniciou ou foi excluído antes do primeiro tick).";
            saida["metricas"] = {{"ticks", 0}, {"tiros", 0}, {"vida", 0}, {"dano_recebido", 0}};
            saida["objetivos"] = json::array();
            for (const auto& o : fase_["objetivos"])
                saida["objetivos"].push_back({{"tipo", o["tipo"]}, {"ok", false}, {"detalhe", "o tanque não entrou na arena"}});
            saida["proximo_balao"] = "1 balão: cumprir os objetivos da fase.";
            saida["tick_final"] = tickFinal();
            if (seed_) saida["seed"] = *seed_;
            saida["pontos_consumidos"] = json::object();
            return saida;
        }

        std::vector<ResultadoObjetivo> resultados;
        for (const auto& o : fase_["objetivos"]) resultados.push_back(avaliarObjetivo(o));

        bool passou = std::all_of(resultados.begin(), resultados.end(), [](const auto& r) { return r.ok; });
        int tickConclusao = 0;
        for (const auto& r : resultados) tickConclusao = std::max(tickConclusao, r.tick);

        metricas_["ticks"] = passou ? tickConclusao : tickFinal();
        metricas_["tiros"] = totalTiros_;
        metricas_["vida"] = std::max(0.0, vidaFinal_);
        metricas_["dano_recebido"] = danoRecebido_;

        int baloes = 0;
        std::string proxima;
        if (passou) {
            baloes = 1;
            for (int nivel = 2; nivel <= 3; ++nivel) {
                std::vector<std::string> faltam;
                bool temEntradas = false;
                for (const auto& e : fase_.value("baloes", json::array())) {
                    if (e.value("baloes", 0) != nivel) continue;
                    temEntradas = true;
                    std::string falta = conferirBalao(e);
                    if (!falta.empty()) faltam.push_back(falta);
                }
                if (temEntradas && faltam.empty()) {
                    baloes = nivel;
                    continue;
                }
                if (temEntradas) proxima = std::to_string(nivel) + " balões: " + juntar(faltam, ", ", " e ") + ".";
                break;
            }
        } else {
            proxima = "1 balão: cumprir " + std::string(resultados.size() > 1 ? "todos os objetivos" : "o objetivo") + " da fase.";
        }

        // Motivo: no sucesso, as frases de cada objetivo; na falha, só as dos que falharam.
        std::vector<std::string> frases;
        for (const auto& r : resultados)
            if (passou || !r.ok) frases.push_back(r.motivo);
        std::string motivo = juntar(frases, " ", " ");
        if (!passou && morte_) {
            std::string m = fraseMorte();
            if (motivo.find(m) == std::string::npos) motivo += " " + m;
        }

        saida["passou"] = passou;
        saida["baloes"] = baloes;
        saida["motivo"] = motivo;
        saida["metricas"] = {
            {"ticks", static_cast<int>(metricas_["ticks"])},
            {"tiros", static_cast<int>(metricas_["tiros"])},
            {"vida", static_cast<int>(std::llround(metricas_["vida"]))},
            {"dano_recebido", static_cast<int>(std::llround(metricas_["dano_recebido"]))},
        };
        saida["objetivos"] = json::array();
        for (const auto& r : resultados)
            saida["objetivos"].push_back({{"tipo", r.tipo}, {"ok", r.ok}, {"detalhe", r.detalhe}});
        saida["proximo_balao"] = proxima.empty() ? json(nullptr) : json(proxima);
        saida["tick_final"] = tickFinal();
        if (seed_) saida["seed"] = *seed_;
        saida["pontos_consumidos"] = json::object();
        for (const auto& [pid, t] : consumidos_) saida["pontos_consumidos"][pid] = t;
        return saida;
    }

    const std::string& id() const { return id_; }

private:
    const json& fase_;
    const json& replay_;
    std::string id_;
    std::map<std::string, Ponto> pontos_;
    std::map<std::string, int> consumidos_;              // ponto -> tick em que passou a contar
    std::vector<std::string> ordemPontos_;
    std::map<std::string, Alvo> alvos_;
    std::vector<std::string> ordemAlvos_;
    std::set<int> idsAlvo_;
    std::optional<long long> seed_;
    int maxTicks_ = -1;
    double hpInicial_ = 100.0;
    std::optional<int> jogadorDeclarado_;         // fase.jogador.tank_id do replay

    std::vector<Quadro> quadros_;
    bool jogadorApareceu_ = false;
    int jogadorId_ = -1;
    int totalTiros_ = 0;
    double vidaFinal_ = 0;
    double danoRecebido_ = 0;
    double distanciaPercorrida_ = 0;
    std::vector<std::pair<int, double>> danos_;   // (tick, dano) com a causa em causas_
    std::vector<std::string> causas_;
    std::optional<int> morte_;                    // índice do quadro da morte
    std::map<std::string, double> metricas_;

    // Projéteis do jogador: menor distância (segmento) a um alvo vivo e o alvo
    struct InfoProjetil {
        int tickTiro = 0;
        double menor = 1e18;
        std::string alvoMaisPerto;
        bool acertou = false;
        double ux = 0, uy = 0, uvx = 0, uvy = 0;  // última posição e velocidade gravadas
        size_t ultimoQuadro = 0;
    };
    std::map<int, InfoProjetil> projeteis_;

    int tickFinal() const { return quadros_.empty() ? 0 : quadros_.back().tick; }

    // ---------------------------------------------------------------- validação
    void validarFase() {
        if (!fase_.is_object()) throw EntradaInvalida("o arquivo da fase não é um objeto JSON");
        if (!fase_.contains("id") || !fase_["id"].is_string()) throw EntradaInvalida("fase sem \"id\"");
        id_ = fase_["id"].get<std::string>();
        if (!fase_.contains("objetivos") || !fase_["objetivos"].is_array() || fase_["objetivos"].empty())
            throw EntradaInvalida("fase sem \"objetivos\"");
        static const std::set<std::string> tipos = {"chegar", "ficar_no_raio", "visitar_em_ordem", "acertar_alvos", "sobreviver"};
        for (const auto& o : fase_["objetivos"]) {
            if (!o.is_object() || !o.contains("tipo") || !o["tipo"].is_string() || !tipos.count(o["tipo"].get<std::string>()))
                throw EntradaInvalida("objetivo com tipo desconhecido: " + o.dump());
        }
        static const std::set<std::string> metricas = {"ticks", "tiros", "vida", "dano_recebido"};
        if (fase_.contains("estrelas")) throw EntradaInvalida("a fase usa \"estrelas\"; o nome agora é \"baloes\"");
        if (fase_.contains("baloes")) {
            if (!fase_["baloes"].is_array()) throw EntradaInvalida("\"baloes\" não é uma lista");
            for (const auto& e : fase_["baloes"]) {
                if (!e.is_object() || !e.contains("metrica") || !e["metrica"].is_string() ||
                    !metricas.count(e["metrica"].get<std::string>()))
                    throw EntradaInvalida("balão com métrica desconhecida: " + e.dump());
                if (!e.contains("baloes") || !e["baloes"].is_number_integer())
                    throw EntradaInvalida("balão sem o número de balões: " + e.dump());
                bool temMax = e.contains("max") && e["max"].is_number();
                bool temMin = e.contains("min") && e["min"].is_number();
                if (!temMax && !temMin) throw EntradaInvalida("balão sem \"max\" nem \"min\": " + e.dump());
            }
        }
        if (!replay_.is_object() || !replay_.contains("frames") || !replay_["frames"].is_array())
            throw EntradaInvalida("match_frames.json sem a lista \"frames\"");
    }

    // ---------------------------------------------------------------- cenário efetivo
    void carregarCenario() {
        const json cen = fase_.value("cenario", json::object());
        if (cen.contains("max_ticks") && cen["max_ticks"].is_number()) maxTicks_ = cen["max_ticks"].get<int>();
        if (cen.contains("jogador") && cen["jogador"].contains("hp") && cen["jogador"]["hp"].is_number())
            hpInicial_ = cen["jogador"]["hp"].get<double>();

        const json* bloco = replay_.contains("fase") && replay_["fase"].is_object() ? &replay_["fase"] : nullptr;
        if (bloco && bloco->contains("id") && (*bloco)["id"].is_string() && (*bloco)["id"].get<std::string>() != id_)
            throw EntradaInvalida("o replay é da fase \"" + (*bloco)["id"].get<std::string>() + "\", não de \"" + id_ + "\"");
        if (bloco && bloco->contains("seed") && (*bloco)["seed"].is_number_integer()) seed_ = (*bloco)["seed"].get<long long>();
        if (bloco && bloco->contains("max_ticks") && (*bloco)["max_ticks"].is_number()) maxTicks_ = (*bloco)["max_ticks"].get<int>();
        if (bloco && bloco->contains("jogador") && (*bloco)["jogador"].is_object() &&
            (*bloco)["jogador"].contains("tank_id") && (*bloco)["jogador"]["tank_id"].is_number_integer())
            jogadorDeclarado_ = (*bloco)["jogador"]["tank_id"].get<int>();

        // Pontos
        const json* listaPontos = nullptr;
        bool doReplay = false;
        if (bloco && bloco->contains("pontos") && (*bloco)["pontos"].is_array()) {
            listaPontos = &(*bloco)["pontos"];
            doReplay = true;
        } else if (cen.contains("pontos") && cen["pontos"].is_array()) {
            listaPontos = &cen["pontos"];
        }
        if (listaPontos) {
            for (const auto& p : *listaPontos) {
                std::string onde = std::string(doReplay ? "match_frames.fase" : "cenario") + ".pontos";
                if (!p.contains("id") || !p["id"].is_string()) throw EntradaInvalida(onde + ": ponto sem id");
                Ponto pt;
                pt.id = p["id"].get<std::string>();
                pt.x = numero(p, "x", onde + "[" + pt.id + "] (faixa sem o bloco fase do replay?)");
                pt.y = numero(p, "y", onde + "[" + pt.id + "] (faixa sem o bloco fase do replay?)");
                pt.raio = numero(p, "raio", onde + "[" + pt.id + "]");
                pontos_[pt.id] = pt;
                ordemPontos_.push_back(pt.id);
            }
        }

        // Alvos (tank_id só vem do replay)
        if (bloco && bloco->contains("alvos") && (*bloco)["alvos"].is_array()) {
            for (const auto& a : (*bloco)["alvos"]) {
                if (!a.contains("id") || !a["id"].is_string()) throw EntradaInvalida("match_frames.fase.alvos: alvo sem id");
                Alvo al;
                al.id = a["id"].get<std::string>();
                al.tank_id = inteiroCampo(a, "tank_id", "match_frames.fase.alvos[" + al.id + "]");
                alvos_[al.id] = al;
                ordemAlvos_.push_back(al.id);
                idsAlvo_.insert(al.tank_id);
            }
        }

        // Referências dos objetivos
        for (const auto& o : fase_["objetivos"]) {
            std::string tipo = o["tipo"];
            auto exigirPonto = [&](const json& v) {
                if (!v.is_string() || !pontos_.count(v.get<std::string>()))
                    throw EntradaInvalida("objetivo " + tipo + " cita o ponto " + v.dump() + ", que o cenário/replay não traz");
            };
            if (tipo == "chegar" || tipo == "ficar_no_raio") exigirPonto(o.value("ponto", json()));
            if (tipo == "visitar_em_ordem") {
                if (!o.contains("pontos") || !o["pontos"].is_array() || o["pontos"].empty())
                    throw EntradaInvalida("visitar_em_ordem sem a lista \"pontos\"");
                for (const auto& p : o["pontos"]) exigirPonto(p);
            }
            if (tipo == "acertar_alvos") {
                const json v = o.value("alvos", json("todos"));
                if (v.is_string() && v.get<std::string>() == "todos") {
                    if (alvos_.empty()) throw EntradaInvalida("acertar_alvos sem alvos no bloco fase do replay");
                } else if (v.is_array()) {
                    for (const auto& a : v)
                        if (!a.is_string() || !alvos_.count(a.get<std::string>()))
                            throw EntradaInvalida("acertar_alvos cita o alvo " + a.dump() + ", que o replay não traz");
                } else {
                    throw EntradaInvalida("acertar_alvos: \"alvos\" deve ser \"todos\" ou uma lista de ids");
                }
            }
            if (tipo == "ficar_no_raio" && (!o.contains("ticks_seguidos") || !o["ticks_seguidos"].is_number_integer()))
                throw EntradaInvalida("ficar_no_raio sem \"ticks_seguidos\"");
        }
    }

    // ---------------------------------------------------------------- frames
    void carregarQuadros() {
        const auto& frames = replay_["frames"];
        // Jogador: o tank_id declarado no bloco fase; sem ele, o primeiro tanque do primeiro
        // frame que não é alvo
        if (jogadorDeclarado_) jogadorId_ = *jogadorDeclarado_;
        for (const auto& fr : frames) {
            if (jogadorDeclarado_) break;
            if (!fr.contains("tanks") || !fr["tanks"].is_array()) continue;
            for (const auto& t : fr["tanks"]) {
                if (!t.contains("id") || !t["id"].is_number_integer()) continue;
                int tid = t["id"].get<int>();
                if (t.value("alvo", false) || idsAlvo_.count(tid)) continue;
                jogadorId_ = tid;
                break;
            }
            break;
        }

        double hpAnterior = hpInicial_;
        bool temAnterior = false;
        double xAnt = 0, yAnt = 0;
        for (const auto& fr : frames) {
            if (!fr.is_object()) continue;
            Quadro q;
            q.tick = fr.value("tick", static_cast<int>(quadros_.size()));
            if (fr.contains("tanks") && fr["tanks"].is_array()) {
                for (const auto& t : fr["tanks"]) {
                    if (!t.contains("id") || !t["id"].is_number_integer()) continue;
                    int tid = t["id"].get<int>();
                    double x = t.value("x", 0.0), y = t.value("y", 0.0);
                    bool vivo = t.value("alive", true);
                    if (tid == jogadorId_) {
                        q.tem_jogador = true;
                        q.vivo = vivo;
                        q.x = x;
                        q.y = y;
                        q.hp = t.value("hp", 0.0);
                    } else if (idsAlvo_.count(tid)) {
                        q.alvo_vivo[tid] = vivo;
                        q.alvo_pos[tid] = {x, y};
                    }
                }
            }
            if (fr.contains("projectiles") && fr["projectiles"].is_array()) {
                for (const auto& p : fr["projectiles"]) {
                    if (p.value("owner_id", -1) != jogadorId_) continue;
                    q.projeteis.push_back({p.value("id", -1), p.value("x", 0.0), p.value("y", 0.0),
                                           p.value("vx", 0.0), p.value("vy", 0.0)});
                }
            }
            if (fr.contains("events") && fr["events"].is_array()) {
                for (const auto& e : fr["events"]) {
                    std::string tipo = e.value("type", "");
                    if (tipo == "SHOOT" && e.value("source_id", -1) == jogadorId_) q.tiros++;
                    if (tipo == "HIT" && e.value("target_id", -1) == jogadorId_) q.atingido_por_tiro = true;
                    if (tipo == "EXPLOSION" && e.value("source_id", -1) == -1 && q.tem_jogador &&
                        dist(e.value("x", 0.0), e.value("y", 0.0), q.x, q.y) <= RAIO_ARTILHARIA_EFETIVO + 1.0)
                        q.artilharia_perto = true;
                }
            }

            if (q.tem_jogador) {
                jogadorApareceu_ = true;
                if (q.hp < hpAnterior - 1e-6) {
                    double dano = hpAnterior - std::max(0.0, q.hp);
                    if (dano > 0) {
                        danoRecebido_ += dano;
                        danos_.push_back({q.tick, dano});
                        causas_.push_back(q.artilharia_perto ? "artilharia" : q.atingido_por_tiro ? "tiro" : "outro");
                    }
                }
                hpAnterior = q.hp;
                vidaFinal_ = q.hp;
                if (temAnterior && q.vivo) distanciaPercorrida_ += dist(xAnt, yAnt, q.x, q.y);
                xAnt = q.x;
                yAnt = q.y;
                temAnterior = true;
                if (!q.vivo && !morte_) morte_ = quadros_.size();
            }
            totalTiros_ += q.tiros;
            quadros_.push_back(std::move(q));
        }
        if (morte_) vidaFinal_ = 0;
        analisarProjeteis();
    }

    void analisarProjeteis() {
        for (size_t i = 0; i < quadros_.size(); ++i) {
            const Quadro& q = quadros_[i];
            for (const auto& p : q.projeteis) {
                auto [it, novo] = projeteis_.try_emplace(p.id);
                InfoProjetil& info = it->second;
                if (novo) info.tickTiro = q.tick;
                info.ux = p.x;
                info.uy = p.y;
                info.uvx = p.vx;
                info.uvy = p.vy;
                info.ultimoQuadro = i;
                // Trecho percorrido no tick seguinte (de onde está até onde vai estar)
                double bx = p.x + p.vx * DT, by = p.y + p.vy * DT;
                for (const auto& [tid, vivo] : q.alvo_vivo) {
                    if (!vivo) continue;
                    auto [ax, ay] = q.alvo_pos.at(tid);
                    double d = distSegmento(p.x, p.y, bx, by, ax, ay);
                    if (d < info.menor) {
                        info.menor = d;
                        info.alvoMaisPerto = nomeAlvo(tid);
                    }
                }
            }
        }
        // Acertou: a próxima posição (onde a engine testa a colisão) encosta num alvo vivo
        for (auto& [pid, info] : projeteis_) {
            const Quadro& q = quadros_[info.ultimoQuadro];
            double nx = info.ux + info.uvx * DT, ny = info.uy + info.uvy * DT;
            for (const auto& [tid, vivo] : q.alvo_vivo) {
                if (!vivo) continue;
                auto [ax, ay] = q.alvo_pos.at(tid);
                if (dist(nx, ny, ax, ay) <= RAIO_TANQUE + RAIO_PROJETIL + 0.5) info.acertou = true;
            }
        }
    }

    std::string nomeAlvo(int tankId) const {
        for (const auto& [id, a] : alvos_)
            if (a.tank_id == tankId) return id;
        return "#" + std::to_string(tankId);
    }

    // ---------------------------------------------------------------- frases comuns
    std::string fraseMorte() const {
        if (!morte_) return "";
        const Quadro& q = quadros_[*morte_];
        std::string onde = " no tick " + std::to_string(q.tick) + ", na posição " + posicao(q.x, q.y) + ".";
        if (q.artilharia_perto) return "Seu tanque foi destruído pela artilharia" + onde;
        if (q.atingido_por_tiro) return "Seu tanque foi destruído por um tiro" + onde;
        return "Seu tanque parou" + onde + " O bot deixou de responder (20 ticks sem resposta) ou terminou com erro.";
    }

    // Quadros em que o jogador está vivo, até o tick limite
    template <typename F>
    void percorrerVivo(int ateTick, F&& f) const {
        for (const auto& q : quadros_) {
            if (q.tick > ateTick) break;
            if (!q.tem_jogador || !q.vivo) continue;
            if (!f(q)) break;
        }
    }

    int limite(const json& o) const {
        if (o.contains("ate_tick") && o["ate_tick"].is_number()) return o["ate_tick"].get<int>();
        return tickFinal();
    }

    struct MaisPerto {
        double d = 1e18;
        int tick = -1;
        double x = 0, y = 0;
    };

    std::string fraseMaisPerto(const Ponto& p, const MaisPerto& m) const {
        if (m.tick < 0) return "";
        return "o mais perto que chegou foi " + inteiro(m.d) + " unidades (raio " + inteiro(p.raio) + "), no tick " +
               std::to_string(m.tick) + ", na posição " + posicao(m.x, m.y);
    }

    // ---------------------------------------------------------------- objetivos
    ResultadoObjetivo avaliarObjetivo(const json& o) {
        std::string tipo = o["tipo"];
        if (tipo == "chegar") return chegar(o);
        if (tipo == "ficar_no_raio") return ficarNoRaio(o);
        if (tipo == "visitar_em_ordem") return visitarEmOrdem(o);
        if (tipo == "acertar_alvos") return acertarAlvos(o);
        return sobreviver(o);
    }

    // O mesmo ponto em dois objetivos: vale o primeiro tick em que contou.
    void consumir(const std::string& id, int tick) {
        auto it = consumidos_.find(id);
        if (it == consumidos_.end() || tick < it->second) consumidos_[id] = tick;
    }

    ResultadoObjetivo chegar(const json& o) {
        ResultadoObjetivo r{"chegar"};
        const Ponto& p = pontos_.at(o["ponto"].get<std::string>());
        int lim = limite(o);
        MaisPerto m;
        int tickEntrada = -1;
        double dFinal = -1;
        percorrerVivo(1 << 30, [&](const Quadro& q) {
            double d = dist(q.x, q.y, p.x, p.y);
            if (q.tick <= lim) dFinal = d;
            if (q.tick <= lim && d < m.d) m = {d, q.tick, q.x, q.y};
            if (d <= p.raio) {
                tickEntrada = q.tick;
                return false;
            }
            return true;
        });
        std::string lim_s = " (limite " + std::to_string(lim) + ")";
        if (tickEntrada >= 0 && tickEntrada <= lim) {
            r.ok = true;
            r.tick = tickEntrada;
            consumir(p.id, tickEntrada);
            r.detalhe = "ponto " + p.id + " no tick " + std::to_string(tickEntrada) + lim_s;
            r.motivo = "Chegou ao ponto " + p.id + " no tick " + std::to_string(tickEntrada) + ".";
        } else if (tickEntrada >= 0) {
            r.detalhe = "ponto " + p.id + " só no tick " + std::to_string(tickEntrada) + lim_s;
            r.motivo = "Chegou ao ponto " + p.id + " no tick " + std::to_string(tickEntrada) + ", depois do limite (tick " +
                       std::to_string(lim) + ").";
        } else {
            r.detalhe = "não chegou ao ponto " + p.id + "; mais perto: " + inteiro(m.d) + " unidades";
            r.motivo = "Não chegou ao ponto " + p.id + ": " + fraseMaisPerto(p, m) + ".";
            if (m.tick >= 0 && m.d > 3 * p.raio && distanciaPercorrida_ > 100 && dFinal > m.d + 50)
                r.motivo += " O tanque andou " + inteiro(distanciaPercorrida_) + " unidades, mas terminou a " + inteiro(dFinal) +
                            " do ponto, mais longe do que chegou a estar: confira o sinal do turn e se o ângulo está em graus.";
            else if (m.tick >= 0 && m.d > 3 * p.raio && distanciaPercorrida_ > 3 * m.d)
                r.motivo += " O tanque andou bastante (" + inteiro(distanciaPercorrida_) +
                            " unidades) sem se aproximar: confira se o ângulo está em graus e se o sinal do turn está certo.";
            else if (distanciaPercorrida_ < 10)
                r.motivo += " O tanque quase não se mexeu (andou " + inteiro(distanciaPercorrida_) + " unidades).";
        }
        return r;
    }

    ResultadoObjetivo ficarNoRaio(const json& o) {
        ResultadoObjetivo r{"ficar_no_raio"};
        const Ponto& p = pontos_.at(o["ponto"].get<std::string>());
        int lim = limite(o);
        int precisa = std::max(1, o["ticks_seguidos"].get<int>());
        int seguidos = 0, inicio = -1, melhor = 0, entradas = 0;
        int tickSaida = -1;
        double velSaida = 0;
        bool temAnt = false;
        double xa = 0, ya = 0;
        MaisPerto m;
        percorrerVivo(lim, [&](const Quadro& q) {
            double d = dist(q.x, q.y, p.x, p.y);
            if (d < m.d) m = {d, q.tick, q.x, q.y};
            if (d <= p.raio) {
                if (seguidos == 0) {
                    inicio = q.tick;
                    entradas++;
                }
                seguidos++;
                melhor = std::max(melhor, seguidos);
                if (seguidos >= precisa) {
                    r.ok = true;
                    r.tick = q.tick;
                    consumir(p.id, q.tick);
                    return false;
                }
            } else {
                if (seguidos > 0) {
                    tickSaida = q.tick;
                    velSaida = temAnt ? dist(xa, ya, q.x, q.y) : 0;
                }
                seguidos = 0;
            }
            xa = q.x;
            ya = q.y;
            temAnt = true;
            return true;
        });
        std::string req = std::to_string(precisa) + " ticks seguidos no ponto " + p.id;
        if (r.ok) {
            r.detalhe = req + " (do tick " + std::to_string(inicio) + " ao " + std::to_string(r.tick) + "; limite " +
                        std::to_string(lim) + ")";
            r.motivo = "Parou no ponto " + p.id + " no tick " + std::to_string(inicio) + " e ficou " + std::to_string(precisa) +
                       " ticks seguidos (até o tick " + std::to_string(r.tick) + ").";
            return r;
        }
        if (entradas == 0) {
            r.detalhe = "nunca entrou no raio do ponto " + p.id + "; mais perto: " + inteiro(m.d) + " unidades";
            r.motivo = "Nunca entrou no raio do ponto " + p.id + " até o tick " + std::to_string(lim) + ": " + fraseMaisPerto(p, m) + ".";
            return r;
        }
        r.detalhe = "máximo de " + std::to_string(melhor) + " de " + std::to_string(precisa) + " ticks seguidos no ponto " + p.id;
        r.motivo = "Entrou no raio do ponto " + p.id + " " + std::to_string(entradas) + (entradas == 1 ? " vez" : " vezes") +
                   ", mas ficou no máximo " + std::to_string(melhor) + " ticks seguidos (precisa de " + std::to_string(precisa) +
                   " até o tick " + std::to_string(lim) + ").";
        if (seguidos > 0) {
            r.motivo += " Estava lá dentro desde o tick " + std::to_string(inicio) + " quando o prazo acabou.";
        } else if (tickSaida >= 0) {
            r.motivo += " Na última vez saiu no tick " + std::to_string(tickSaida) + " andando " + umDecimal(velSaida) +
                        " unidades por tick: reduza o throttle perto do ponto e mande throttle 0 lá dentro.";
        }
        return r;
    }

    ResultadoObjetivo visitarEmOrdem(const json& o) {
        ResultadoObjetivo r{"visitar_em_ordem"};
        std::vector<std::string> ids;
        for (const auto& p : o["pontos"]) ids.push_back(p.get<std::string>());
        int lim = limite(o);
        size_t prox = 0;
        std::vector<int> ticks;
        std::string foraId;
        int foraTick = -1;
        std::string foraEsperado;
        MaisPerto m;
        percorrerVivo(lim, [&](const Quadro& q) {
            const Ponto& alvo = pontos_.at(ids[prox]);
            double d = dist(q.x, q.y, alvo.x, alvo.y);
            if (d < m.d) m = {d, q.tick, q.x, q.y};
            if (d <= alvo.raio) {
                ticks.push_back(q.tick);
                consumir(alvo.id, q.tick);
                prox++;
                m = MaisPerto{};
                if (prox == ids.size()) {
                    r.ok = true;
                    r.tick = q.tick;
                    return false;
                }
                return true;
            }
            if (foraTick < 0) {
                for (size_t j = prox + 1; j < ids.size(); ++j) {
                    const Ponto& pj = pontos_.at(ids[j]);
                    if (dist(q.x, q.y, pj.x, pj.y) <= pj.raio) {
                        foraId = pj.id;
                        foraTick = q.tick;
                        foraEsperado = ids[prox];
                        break;
                    }
                }
            }
            return true;
        });
        std::vector<std::string> visitados;
        for (size_t i = 0; i < ticks.size(); ++i) visitados.push_back(ids[i] + " (tick " + std::to_string(ticks[i]) + ")");
        if (r.ok) {
            r.detalhe = juntar(visitados, ", ", " e ") + "; limite " + std::to_string(lim);
            r.motivo = "Percorreu os " + std::to_string(ids.size()) + " pontos na ordem; terminou no ponto " + ids.back() +
                       " no tick " + std::to_string(r.tick) + ".";
            return r;
        }
        const Ponto& falta = pontos_.at(ids[prox]);
        r.detalhe = std::to_string(prox) + " de " + std::to_string(ids.size()) + " pontos; faltou " + falta.id;
        if (prox == 0)
            r.motivo = "Não chegou ao " + falta.id + " (o primeiro da lista) até o tick " + std::to_string(lim);
        else
            r.motivo = "Visitou " + juntar(visitados, ", ", " e ") + ", mas não chegou ao " + falta.id + " até o tick " +
                       std::to_string(lim);
        std::string mp = fraseMaisPerto(falta, m);
        r.motivo += mp.empty() ? "." : ": " + mp + ".";
        if (foraTick >= 0)
            r.motivo += " Passou pelo " + foraId + " fora da ordem no tick " + std::to_string(foraTick) + " (não conta: o próximo era o " +
                        foraEsperado + ").";
        return r;
    }

    ResultadoObjetivo acertarAlvos(const json& o) {
        ResultadoObjetivo r{"acertar_alvos"};
        std::vector<std::string> ids;
        const json v = o.value("alvos", json("todos"));
        if (v.is_array())
            for (const auto& a : v) ids.push_back(a.get<std::string>());
        else
            ids = ordemAlvos_;
        int lim = limite(o);
        std::optional<int> maxTiros;
        if (o.contains("max_tiros") && o["max_tiros"].is_number()) maxTiros = o["max_tiros"].get<int>();

        // Tick em que cada alvo morreu
        std::map<std::string, int> morte;
        for (const auto& q : quadros_) {
            for (const auto& id : ids) {
                if (morte.count(id)) continue;
                auto it = q.alvo_vivo.find(alvos_.at(id).tank_id);
                if (it != q.alvo_vivo.end() && !it->second) morte[id] = q.tick;
            }
        }
        std::vector<std::string> dentro, atrasados, faltam;
        int ultimo = -1;
        for (const auto& id : ids) {
            auto it = morte.find(id);
            if (it != morte.end() && it->second <= lim) {
                dentro.push_back(id + " no tick " + std::to_string(it->second));
                ultimo = std::max(ultimo, it->second);
            } else if (it != morte.end()) {
                atrasados.push_back(id + " só no tick " + std::to_string(it->second));
                faltam.push_back(id);
            } else {
                faltam.push_back(id);
            }
        }
        auto tirosAte = [&](int tick) {
            int n = 0;
            for (const auto& q : quadros_)
                if (q.tick <= tick) n += q.tiros;
            return n;
        };
        std::string n_s = std::to_string(ids.size());
        if (faltam.empty()) {
            int tiros = tirosAte(ultimo);
            if (maxTiros && tiros > *maxTiros) {
                r.detalhe = n_s + " de " + n_s + " alvos, mas com " + std::to_string(tiros) + " tiros (máximo " + std::to_string(*maxTiros) + ")";
                r.motivo = "Destruiu os " + n_s + " alvos, mas usou " + std::to_string(tiros) + " tiros; o máximo é " +
                           std::to_string(*maxTiros) + ".";
                return r;
            }
            r.ok = true;
            r.tick = ultimo;
            r.detalhe = n_s + " de " + n_s + " alvos até o tick " + std::to_string(ultimo) + " (limite " + std::to_string(lim) + "), " +
                        std::to_string(tiros) + (tiros == 1 ? " tiro" : " tiros");
            r.motivo = "Destruiu os " + n_s + " alvos (o último no tick " + std::to_string(ultimo) + ") com " + std::to_string(tiros) +
                       (tiros == 1 ? " tiro." : " tiros.");
            return r;
        }
        r.detalhe = std::to_string(dentro.size()) + " de " + n_s + " alvos até o tick " + std::to_string(lim) + "; faltou " +
                    juntar(faltam, ", ", " e ");
        r.motivo = "Destruiu " + std::to_string(dentro.size()) + " de " + n_s + " alvos até o tick " + std::to_string(lim);
        if (!dentro.empty()) r.motivo += " (" + juntar(dentro, ", ", " e ") + ")";
        r.motivo += "; faltou " + juntar(faltam, ", ", " e ") + ".";
        if (!atrasados.empty()) r.motivo += " Depois do limite: " + juntar(atrasados, ", ", " e ") + ".";
        int tiros = tirosAte(lim);
        if (tiros == 0) {
            r.motivo += " Nenhum tiro saiu: o tiro só sai com shoot verdadeiro, cooldown 0 e energia de pelo menos 20.";
            return r;
        }
        int erros = 0;
        const InfoProjetil* melhorErro = nullptr;
        for (const auto& [pid, info] : projeteis_) {
            if (info.tickTiro > lim || info.acertou) continue;
            erros++;
            if (!info.alvoMaisPerto.empty() && (!melhorErro || info.menor < melhorErro->menor)) melhorErro = &info;
        }
        r.motivo += " Foram " + std::to_string(tiros) + (tiros == 1 ? " tiro" : " tiros");
        if (erros == 0) {
            r.motivo += ", e nenhum errou: faltou atirar nos outros alvos a tempo.";
        } else {
            r.motivo += ", " + std::to_string(erros) + (erros == 1 ? " errou" : " erraram");
            if (melhorErro)
                r.motivo += "; o que passou mais perto errou o " + melhorErro->alvoMaisPerto + " por " +
                            inteiro(std::max(0.0, melhorErro->menor - RAIO_TANQUE - RAIO_PROJETIL)) + " unidades (tiro do tick " +
                            std::to_string(melhorErro->tickTiro) + ")";
            r.motivo += ". Confira se o turret_angle está em graus e aponta da sua posição para o alvo.";
        }
        return r;
    }

    ResultadoObjetivo sobreviver(const json& o) {
        ResultadoObjetivo r{"sobreviver"};
        int lim = limite(o);
        int ultimoPossivel = maxTicks_ > 0 ? std::min(lim, maxTicks_ - 1) : lim;
        std::optional<double> vidaMin;
        if (o.contains("vida_min") && o["vida_min"].is_number()) vidaMin = o["vida_min"].get<double>();

        std::vector<std::string> bombas;
        int nBombas = 0;
        for (size_t i = 0; i < danos_.size(); ++i) {
            if (danos_[i].first > lim) break;
            if (causas_[i] == "artilharia") {
                nBombas++;
                bombas.push_back(std::to_string(danos_[i].first));
            }
        }
        std::string lista = nBombas == 0 ? "" : " (ticks " + juntar(bombas, ", ", " e ") + ")";
        std::string recebeu = std::to_string(nBombas) + (nBombas == 1 ? " bombardeio" : " bombardeios") + lista;

        if (morte_ && quadros_[*morte_].tick <= lim) {
            const Quadro& q = quadros_[*morte_];
            r.detalhe = "destruído no tick " + std::to_string(q.tick) + " (limite " + std::to_string(lim) + ")";
            r.motivo = fraseMorte();
            if (nBombas > 0) r.motivo += " Recebeu " + recebeu + ".";
            if (distanciaPercorrida_ < 30)
                r.motivo += " O tanque quase não se mexeu (andou " + inteiro(distanciaPercorrida_) +
                            " unidades): leia estado[\"hazards\"] e fuja assim que o aviso aparecer.";
            return r;
        }
        // Vida no último quadro até o limite
        const Quadro* fim = nullptr;
        for (const auto& q : quadros_)
            if (q.tick <= lim && q.tem_jogador) fim = &q;
        if (!fim || fim->tick < ultimoPossivel) {
            int t = fim ? fim->tick : 0;
            r.detalhe = "a partida terminou no tick " + std::to_string(t) + ", antes do tick " + std::to_string(ultimoPossivel);
            r.motivo = "A partida terminou no tick " + std::to_string(t) + ", antes do tick " + std::to_string(ultimoPossivel) + ".";
            return r;
        }
        double vida = std::max(0.0, fim->hp);
        if (vidaMin && vida < *vidaMin) {
            r.detalhe = "vivo, mas com " + inteiro(vida) + " de vida (mínimo " + inteiro(*vidaMin) + ")";
            r.motivo = "Sobreviveu até o tick " + std::to_string(fim->tick) + ", mas com " + inteiro(vida) +
                       " de vida; o mínimo é " + inteiro(*vidaMin) + ". Recebeu " + recebeu + ".";
            return r;
        }
        r.ok = true;
        r.tick = fim->tick;
        r.detalhe = "vivo no tick " + std::to_string(fim->tick) + " com " + inteiro(vida) + " de vida";
        r.motivo = "Sobreviveu até o tick " + std::to_string(fim->tick) + " com " + inteiro(vida) + " de vida";
        r.motivo += nBombas == 0 ? ", sem levar nenhum bombardeio." : "; levou " + recebeu + ".";
        return r;
    }

    // ---------------------------------------------------------------- balões
    // Vazio quando a entrada vale; senão, a frase do que falta
    std::string conferirBalao(const json& e) const {
        std::string m = e["metrica"];
        double valor = metricas_.at(m);
        bool temMax = e.contains("max") && e["max"].is_number();
        bool temMin = e.contains("min") && e["min"].is_number();
        double lim = temMax ? e["max"].get<double>() : e["min"].get<double>();
        bool ok = temMax ? valor <= lim + 1e-9 : valor >= lim - 1e-9;
        if (temMax && temMin) ok = ok && valor >= e["min"].get<double>() - 1e-9;
        if (ok) return "";
        std::string v = inteiro(valor), l = inteiro(lim);
        if (m == "ticks") return temMax ? "concluir até o tick " + l + " (você: " + v + ")" : "concluir depois do tick " + l;
        if (m == "tiros") return temMax ? "usar no máximo " + l + (lim == 1 ? " tiro" : " tiros") + " (você: " + v + ")"
                                        : "usar pelo menos " + l + " tiros (você: " + v + ")";
        if (m == "dano_recebido") {
            if (temMax && lim <= 0) return "não receber dano (você recebeu " + v + ")";
            return temMax ? "receber no máximo " + l + " de dano (você: " + v + ")" : "receber pelo menos " + l + " de dano";
        }
        return temMin ? "terminar com pelo menos " + l + " de vida (você: " + v + ")"
                      : "terminar com no máximo " + l + " de vida (você: " + v + ")";
    }
};

}  // namespace

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    if (argc != 3) {
        std::cerr << "Uso: avaliar_fase <fase.json> <match_frames.json>\n";
        std::cout << json{{"erro", "uso: avaliar_fase <fase.json> <match_frames.json>"}}.dump() << "\n";
        return 2;
    }
    std::string id;
    try {
        json fase = lerJson(argv[1], "arquivo da fase");
        if (fase.is_object() && fase.contains("id") && fase["id"].is_string()) id = fase["id"].get<std::string>();
        json replay = lerJson(argv[2], "match_frames.json");
        Avaliador av(fase, replay);
        json saida = av.avaliar();
        std::cout << saida.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
        return 0;
    } catch (const EntradaInvalida& e) {
        std::cerr << "[avaliar_fase] entrada inválida: " << e.what() << "\n";
        std::cout << json{{"fase", id}, {"erro", e.what()}}.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "[avaliar_fase] entrada inválida: " << e.what() << "\n";
        std::cout << json{{"fase", id}, {"erro", std::string("entrada inválida: ") + e.what()}}.dump(-1, ' ', false, json::error_handler_t::replace) << "\n";
        return 2;
    }
}
