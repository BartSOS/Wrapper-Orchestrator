/**
 * ============================================================================
 * ENGINE.CPP - Motor de Simulação e Juiz de Arena Top-Down (Game Jam IA)
 * ============================================================================
 * Padrão: C++20 (POSIX Linux)
 * Dependência: <nlohmann/json.hpp>
 * Compilação: g++ -O3 -std=c++20 engine.cpp -o engine
 * Execução:   ./engine "python3 bot1.py" "./bot_cpp" "java -cp . Bot"
 * Ambiente:   TOPCOM_SEED (inteiro de 0 a 4294967295; sem ela, a seed é sorteada)
 *             TOPCOM_MAX_BOTS (limite de bots; padrão 20)
 * Saída:      match_frames.json e replays/partida_<id>.json no diretório atual,
 *             com seed, ranking (bots[]) e todos os frames.
 * ============================================================================
 */

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <cmath>
#include <algorithm>
#include <random>
#include <map>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cerrno>

// Dependências POSIX para IPC e Subprocessos
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <signal.h>

// Biblioteca Header-Only JSON
#include "nlohmann/json.hpp"
using json = nlohmann::json;

// ============================================================================
// 1. CONSTANTES DA ARENA E FÍSICA
// ============================================================================
namespace Config {
    constexpr double ARENA_WIDTH = 1000.0;
    constexpr double ARENA_HEIGHT = 1000.0;
    constexpr int    MAX_TICKS = 1500;
    constexpr double TICK_DT = 0.1;           // 10 Hz (0.1s por tick)
    constexpr int    MAX_BOTS_PADRAO = 20;    // limite do evento; TOPCOM_MAX_BOTS sobrepõe (benchmark)
    constexpr int    RESPOSTA_MS = 50;        // prazo de resposta por tick (20 ticks sem resposta = eliminado)
    constexpr int    PARTIDA_INICIO_MS = 5000;// prazo do tick 0 para todos os bots terminarem de iniciar

    // Nascimento em círculo: raio 360 até 20 bots (equilíbrio do evento). Acima disso o raio
    // cresce para manter SPAWN_MIN_GAP entre centros vizinhos; se nem o raio máximo bastar,
    // os tanques são distribuídos em anéis concêntricos.
    constexpr double SPAWN_RADIUS = 360.0;
    constexpr double SPAWN_RADIUS_MAX = 480.0; // 500 - TANK_RADIUS: o tanque nasce dentro da arena
    constexpr double SPAWN_MIN_GAP = 50.0;     // diâmetro do tanque (40) + folga
    constexpr double SPAWN_RING_GAP = 60.0;    // distância entre anéis concêntricos

    constexpr double TANK_RADIUS = 20.0;
    constexpr double TANK_MAX_SPEED = 50.0;   // unidades/s
    constexpr double TANK_TURN_RATE = 90.0;   // graus/s
    constexpr double TANK_MAX_HP = 100.0;
    constexpr double TANK_MAX_ENERGY = 100.0;
    constexpr double ENERGY_REGEN = 5.0;      // Recuperação vagarosa (ex: 20s para encher)
    constexpr double SHIELD_COST = 50.0;      // energia/s
    constexpr double SHOOT_COST = 20.0;       // energia/disparo
    constexpr double SHOOT_COOLDOWN = 1.0;    // segundos

    constexpr double PROJ_SPEED = 300.0;
    constexpr double PROJ_RADIUS = 4.0;
    constexpr double PROJ_DAMAGE = 25.0;

    constexpr double ITEM_RADIUS = 15.0;
    constexpr double ITEM_HEAL_VAL = 30.0;
    constexpr double ITEM_ENERGY_VAL = 50.0;

    constexpr double ARTILLERY_RADIUS = 80.0;
    constexpr double ARTILLERY_DAMAGE = 40.0;
    constexpr double ARTILLERY_TIMER = 3.0;   // tempo até o impacto

    constexpr double LASER_WIDTH = 6.0;
    constexpr double LASER_DPS = 30.0;        // dano por segundo no feixe
}

// ============================================================================
// 2. MATEMÁTICA VETORIAL E FUNÇÕES GEOMÉTRICAS
// ============================================================================
namespace Math2D {
    struct Vec2 {
        double x = 0.0;
        double y = 0.0;

        Vec2() = default;
        Vec2(double _x, double _y) : x(_x), y(_y) {}

        Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
        Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
        Vec2 operator*(double s) const { return {x * s, y * s}; }

        double length() const { return std::hypot(x, y); }
        double dist(const Vec2& o) const { return (*this - o).length(); }
    };

    double deg2rad(double deg) { return deg * M_PI / 180.0; }
    double rad2deg(double rad) { return rad * 180.0 / M_PI; }

    // Normaliza ângulos para o intervalo [0.0, 360.0)
    double normalizeAngle(double deg) {
        deg = std::fmod(deg, 360.0);
        if (deg < 0) deg += 360.0;
        return deg;
    }

    // Calcula a distância mais curta de um ponto P ao segmento de reta AB
    double distPointToSegment(Vec2 P, Vec2 A, Vec2 B) {
        Vec2 AB = B - A;
        Vec2 AP = P - A;
        double ab_len_sq = AB.x * AB.x + AB.y * AB.y;
        if (ab_len_sq == 0.0) return AP.length();

        double t = (AP.x * AB.x + AP.y * AB.y) / ab_len_sq;
        t = std::clamp(t, 0.0, 1.0);
        Vec2 projection = A + AB * t;
        return P.dist(projection);
    }
}
using namespace Math2D;

// ============================================================================
// 3. COMUNICAÇÃO IPC E SUBPROCESSOS POSIX
// ============================================================================
// Protocolo (uma linha JSON por mensagem):
// - cada bot tem no máximo UM estado pendente. Se ainda deve a resposta ao estado anterior,
//   a engine espera por ela (e a descarta, por estar atrasada) antes de mandar o estado novo.
//   Assim uma resposta atrasada nunca é aplicada a outro tick e o pipe de entrada do bot
//   não enche (um bot travado não bloqueia a engine);
// - linhas que não são objeto JSON são ignoradas;
// - o stderr do bot (print/printf do aluno, desviados pelos wrappers) vai para /dev/null e
//   não se mistura às respostas.
class BotProcess {
private:
    pid_t pid = -1;
    int pipe_in[2] = {-1, -1};  // Engine -> Bot (stdin)
    int pipe_out[2] = {-1, -1}; // Bot -> Engine (stdout)
    bool alive = false;
    bool pendente = false;      // estado enviado ainda sem resposta
    std::string read_buffer;

    using Relogio = std::chrono::steady_clock;

    // Escreve a mensagem inteira (o pipe tem no máximo um estado pendente, então não enche)
    bool escrever(const std::string& msg) {
        size_t enviado = 0;
        while (enviado < msg.size()) {
            ssize_t n = write(pipe_in[1], msg.data() + enviado, msg.size() - enviado);
            if (n < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            enviado += static_cast<size_t>(n);
        }
        return true;
    }

    // Próximo objeto JSON vindo do bot até o prazo; descarta linhas que não são objeto JSON
    bool lerObjeto(json& out, Relogio::time_point prazo) {
        while (true) {
            size_t pos;
            while ((pos = read_buffer.find('\n')) != std::string::npos) {
                std::string linha = read_buffer.substr(0, pos);
                read_buffer.erase(0, pos + 1);
                json j = json::parse(linha, nullptr, false);
                if (!j.is_discarded() && j.is_object()) {
                    out = std::move(j);
                    return true;
                }
            }

            auto agora = Relogio::now();
            if (agora >= prazo) return false;
            long restante_us = std::chrono::duration_cast<std::chrono::microseconds>(prazo - agora).count();

            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(pipe_out[0], &read_fds);
            struct timeval tv;
            tv.tv_sec = restante_us / 1000000;
            tv.tv_usec = restante_us % 1000000;

            int ready = select(pipe_out[0] + 1, &read_fds, nullptr, nullptr, &tv);
            if (ready < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (ready == 0) return false; // prazo esgotado

            bool fim = false;
            char buffer[4096];
            while (true) {
                ssize_t bytes = read(pipe_out[0], buffer, sizeof(buffer));
                if (bytes > 0) {
                    read_buffer.append(buffer, static_cast<size_t>(bytes));
                } else {
                    if (bytes == 0) fim = true; // bot fechou a saída
                    break;
                }
            }
            if (fim && read_buffer.find('\n') == std::string::npos) return false;
        }
    }

public:
    int id;
    std::string command;

    BotProcess(int _id, const std::string& _cmd) : id(_id), command(_cmd) {}

    ~BotProcess() {
        terminate();
    }

    bool start() {
        // O_CLOEXEC: os bots iniciados depois não herdam os pipes dos anteriores
        if (pipe2(pipe_in, O_CLOEXEC) < 0 || pipe2(pipe_out, O_CLOEXEC) < 0) {
            std::cerr << "[IPC ERRO] Falha ao criar pipes POSIX para o Bot " << id << "\n";
            return false;
        }

        pid = fork();
        if (pid < 0) {
            std::cerr << "[IPC ERRO] Falha no fork() para o Bot " << id << "\n";
            return false;
        }

        if (pid == 0) {
            // --- PROCESSO FILHO ---
            // Grupo de processos próprio: terminate() derruba o bot inteiro (sh, timeout, bot)
            setpgid(0, 0);

            // Fechar pontas não utilizadas dos pipes
            close(pipe_in[1]);
            close(pipe_out[0]);

            // Redirecionar stdin e stdout. O stderr vai para /dev/null: misturado ao stdout,
            // cada print do aluno virava uma "resposta" e atrasava o bot em um tick.
            dup2(pipe_in[0], STDIN_FILENO);
            dup2(pipe_out[1], STDOUT_FILENO);
            int devnull = open("/dev/null", O_WRONLY);
            if (devnull >= 0) {
                dup2(devnull, STDERR_FILENO);
                close(devnull);
            }

            close(pipe_in[0]);
            close(pipe_out[1]);

            // Executar via shell para suportar argumentos complexos (ex: "python3 bot.py")
            std::string cmd = "exec " + command;
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            _exit(127); // Se execl falhar
        }

        // --- PROCESSO PAI ---
        close(pipe_in[0]);
        close(pipe_out[1]);

        // Configurar pipe_out para modo não-bloqueante via fcntl
        int flags = fcntl(pipe_out[0], F_GETFL, 0);
        fcntl(pipe_out[0], F_SETFL, flags | O_NONBLOCK);

        alive = true;
        return true;
    }

    bool checkProcessAlive() {
        if (!alive || pid <= 0) return false;
        int status = 0;
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret > 0) {
            alive = false;
            return false;
        }
        return true;
    }

    // Envia o estado e espera, até timeout_ms, a resposta a ESTE estado. Se o bot ainda
    // devia a resposta ao estado anterior, ela é esperada no mesmo prazo e descartada;
    // sem ela, o estado novo nem é enviado (conta como tick sem resposta).
    bool trocar(const std::string& estado, json& resposta, int timeout_ms) {
        if (!alive) return false;
        if (!checkProcessAlive()) return false;

        auto prazo = Relogio::now() + std::chrono::milliseconds(timeout_ms);
        if (pendente) {
            json atrasada;
            if (!lerObjeto(atrasada, prazo)) return false;
            pendente = false;
        }

        if (!escrever(estado + "\n")) {
            alive = false;
            return false;
        }
        pendente = true;

        if (!lerObjeto(resposta, prazo)) return false;
        pendente = false;
        return true;
    }

    void terminate() {
        if (pid > 0) {
            kill(-pid, SIGKILL); // grupo inteiro: sh/timeout, o bot e eventuais filhos dele
            if (alive) {
                kill(pid, SIGKILL);
                waitpid(pid, nullptr, WNOHANG);
            }
            alive = false;
            pid = -1;
        }
        if (pipe_in[1] >= 0) { close(pipe_in[1]); pipe_in[1] = -1; }
        if (pipe_out[0] >= 0) { close(pipe_out[0]); pipe_out[0] = -1; }
    }

    bool isAlive() const { return alive; }
    bool isAlive() {
        if (!alive) return false;
        return checkProcessAlive();
    }
};

// ============================================================================
// 4. ESTRUTURAS DE DADOS DA ARENA
// ============================================================================
struct Tank {
    int id;
    Vec2 pos;
    Vec2 vel;
    double angle = 0.0;       // Angulo do chassi (graus)
    double turret_angle = 0.0;// Angulo da torreta (graus)
    double hp = Config::TANK_MAX_HP;
    double energy = Config::TANK_MAX_ENERGY;
    double cooldown = 0.0;
    bool shield_active = false;
    int kills = 0;
    bool is_alive = true;
    int consecutive_timeouts = 0;
    int tick_eliminacao = -1; // tick em que foi eliminado (-1 = vivo)
};

struct Projectile {
    int id;
    int owner_id;
    Vec2 pos;
    Vec2 vel;
};

struct Hazard {
    int id;
    std::string type; // "ARTILLERY" ou "LASER"
    Vec2 pos;
    double radius = 0.0;
    double duration = 0.0;
    double timer = 0.0;       // Para ARTILLERY
    double angle = 0.0;       // Para LASER
    double angular_vel = 0.0; // Em graus/s para LASER
};

struct Item {
    int id;
    std::string type; // "HEAL" ou "ENERGY"
    Vec2 pos;
};

struct Event {
    std::string type; // "SHOOT", "HIT", "EXPLOSION", "ITEM_PICKUP", "ARTILLERY_WARN"
    double x = 0.0;
    double y = 0.0;
    int source_id = -1;
    int target_id = -1;
};

// ============================================================================
// 5. NARRAÇÃO
// ============================================================================

struct MomentoNarrativo {
    int tick;
    double tempo_s;
    std::string tipo;       // "KILL_PROJETIL", "KILL_ARTILHARIA", "KILL_LASER", "CLUTCH_HEAL", "INFO"
    int alvo_id;            // ID de quem sofreu a ação (-1 se não houver)
    int assassino_id;       // ID de quem causou a ação (-1 se não houver)
    std::string texto;      // Texto formatado da narração
};

// ============================================================================
// 6. MOTOR E JUÍZ DA ARENA (CORE ENGINE)
// ============================================================================
class ArenaEngine {
private:
    std::vector<std::unique_ptr<BotProcess>> bots;
    std::vector<Tank> tanks;
    std::vector<Projectile> projectiles;
    std::vector<Hazard> hazards;
    std::vector<Item> items;
    std::vector<Event> current_events;
    std::vector<MomentoNarrativo> narracao;

    json match_replay;
    int tick_count = 0;
    int next_entity_id = 1000;
    std::mt19937 rng;
    uint32_t seed = 0;
    std::chrono::steady_clock::time_point inicio_partida = std::chrono::steady_clock::now();

    struct Nascimento {
        Vec2 pos;
        double angle; // graus, olhando para o centro da arena
    };

    // Posições de nascimento dos n tanques. Até 20 bots: círculo de raio 360 (comportamento
    // original). Acima disso o raio cresce para não haver sobreposição; se passar do raio
    // máximo, os tanques são distribuídos em anéis concêntricos, de fora para dentro.
    static std::vector<Nascimento> posicoesDeNascimento(size_t n) {
        const double cx = Config::ARENA_WIDTH / 2.0;
        const double cy = Config::ARENA_HEIGHT / 2.0;

        // Raio mínimo para k tanques num anel com SPAWN_MIN_GAP entre centros vizinhos
        auto raio_minimo = [](size_t k) {
            return k < 2 ? 0.0 : Config::SPAWN_MIN_GAP / (2.0 * std::sin(M_PI / k));
        };
        // Quantos tanques cabem num anel de raio r
        auto capacidade = [](double r) -> size_t {
            double s = Config::SPAWN_MIN_GAP / (2.0 * r);
            return s >= 1.0 ? 1 : static_cast<size_t>(std::floor(M_PI / std::asin(s)));
        };

        std::vector<std::pair<double, size_t>> aneis; // (raio, quantidade de tanques)
        double r_unico = std::max(Config::SPAWN_RADIUS, raio_minimo(n));
        if (r_unico <= Config::SPAWN_RADIUS_MAX) {
            aneis.push_back({r_unico, n});
        } else {
            size_t restantes = n;
            double r = Config::SPAWN_RADIUS_MAX;
            while (restantes > 0) {
                // O último anel possível recebe todos os restantes (só com centenas de bots)
                size_t k = (r < Config::SPAWN_RING_GAP) ? restantes : std::min(capacidade(r), restantes);
                aneis.push_back({r, k});
                restantes -= k;
                r = std::max(0.0, r - Config::SPAWN_RING_GAP);
            }
        }

        std::vector<Nascimento> pos;
        for (size_t a = 0; a < aneis.size(); ++a) {
            const auto [raio, k] = aneis[a];
            double defasagem = (a % 2 == 1) ? M_PI / k : 0.0; // anéis internos intercalados
            for (size_t i = 0; i < k; ++i) {
                double angle_rad = (2.0 * M_PI * i) / k + defasagem;
                pos.push_back({{cx + raio * std::cos(angle_rad), cy + raio * std::sin(angle_rad)},
                               std::fmod((angle_rad * 180.0 / M_PI) + 180.0, 360.0)});
            }
        }
        return pos;
    }

public:
    ArenaEngine(const std::vector<std::string>& bot_commands, uint32_t seed_partida) : seed(seed_partida) {
        rng.seed(seed);

        // Inicia e posiciona os tanques na arena (main já aplicou o limite)
        auto nascimentos = posicoesDeNascimento(bot_commands.size());

        for (size_t i = 0; i < bot_commands.size(); ++i) {
            Tank t;
            t.id = i + 1;
            t.pos = nascimentos[i].pos;
            t.angle = nascimentos[i].angle;
            t.turret_angle = t.angle;

            auto bot = std::make_unique<BotProcess>(i + 1, bot_commands[i]);
            if (bot->start()) {
                bots.push_back(std::move(bot));
            } else {
                // Sem processo não há como jogar: o tanque entra eliminado e aparece no ranking
                t.is_alive = false;
                t.hp = 0.0;
                registrarMomento("KILL_DESCONEXAO", "Bot ID [" + std::to_string(t.id) + "] nao pode ser iniciado e foi desativado!", t.id, -1, "[DESCONEXAO]");
            }
            tanks.push_back(t);
        }

        match_replay["config"] = {
            {"arena_width", Config::ARENA_WIDTH},
            {"arena_height", Config::ARENA_HEIGHT},
            {"max_ticks", Config::MAX_TICKS}
        };
        match_replay["frames"] = json::array();
    }

    void spawnRandomHazardsAndItems() {
        // Gerador periódico de itens de cura ou energia
        if (tick_count % 150 == 0 && items.size() < 4) {
            std::uniform_real_distribution<double> dist_x(50, Config::ARENA_WIDTH - 50);
            std::uniform_real_distribution<double> dist_y(50, Config::ARENA_HEIGHT - 50);
            std::uniform_int_distribution<int> dist_type(0, 1);

            Item item;
            item.id = next_entity_id++;
            item.pos = {dist_x(rng), dist_y(rng)};
            item.type = dist_type(rng) == 0 ? "HEAL" : "ENERGY";
            items.push_back(item);
        }

        // Gerador de feixes Laser ou bombardeios de Artilharia (acelera conforme o tempo passa)
        int hazard_freq = (tick_count > 400) ? 90 : 200;
        int max_hazards = (tick_count > 400) ? 5 : 3;
        if (tick_count % hazard_freq == 0 && hazards.size() < max_hazards) {
            std::uniform_real_distribution<double> dist_x(100, Config::ARENA_WIDTH - 100);
            std::uniform_real_distribution<double> dist_y(100, Config::ARENA_HEIGHT - 100);
            std::uniform_int_distribution<int> dist_type(0, 1);

            Hazard h;
            h.id = next_entity_id++;
            if (dist_type(rng) == 0) {
                h.type = "ARTILLERY";
                h.pos = {dist_x(rng), dist_y(rng)};
                h.radius = (tick_count > 400) ? Config::ARTILLERY_RADIUS * 1.3 : Config::ARTILLERY_RADIUS;
                h.timer = Config::ARTILLERY_TIMER;
                current_events.push_back({"ARTILLERY_WARN", h.pos.x, h.pos.y, -1, -1});
            } else {
                h.type = "LASER";
                h.pos = {Config::ARENA_WIDTH / 2.0, Config::ARENA_HEIGHT / 2.0};
                // Ângulo inicial sorteado pelo rng da partida (seed): sem isso o feixe nascia
                // sempre em 0 graus e queimava sempre o mesmo setor da arena.
                std::uniform_real_distribution<double> dist_ang(0.0, 360.0);
                h.angle = dist_ang(rng);
                h.angular_vel = 30.0; // feixe rotatório
                h.duration = 5.0;
                h.timer = 2.5;
            }
            hazards.push_back(h);
        }

        // Tempestade de Radiação / Fechamento de Zona após o tick 600 (60s)
        if (tick_count > 600 && tick_count % 10 == 0) {
            double safe_radius = std::max(80.0, 550.0 - (tick_count - 600) * 0.7);
            Vec2 center = {Config::ARENA_WIDTH / 2.0, Config::ARENA_HEIGHT / 2.0};
            for (auto& t : tanks) {
                if (!t.is_alive) continue;
                if (t.pos.dist(center) > safe_radius) {
                    t.hp -= 2.5; // Dano contínuo fora da zona segura
                    if (t.hp <= 0.0) {
                        t.is_alive = false;
                        registrarMomento("KILL_ZONA", "Bot ID [" + std::to_string(t.id) + "] foi consumido pela tempestade de radiacao!", t.id, -1, "[ZONA]");
                    }
                }
            }
        }
    }

    // Monta o estado de visão personalizado para enviar ao stdin do bot
    json buildPerceptionPayload(const Tank& self) {
        json j;
        j["tick"] = tick_count;
        j["self"] = {
            {"id", self.id},
            {"x", self.pos.x}, {"y", self.pos.y},
            {"vx", self.vel.x}, {"vy", self.vel.y},
            {"angle", self.angle}, {"turret_angle", self.turret_angle},
            {"hp", self.hp}, {"energy", self.energy},
            {"cooldown", self.cooldown}, {"shield", self.shield_active}
        };

        j["enemies"] = json::array();
        for (const auto& t : tanks) {
            if (t.id != self.id && t.is_alive) {
                j["enemies"].push_back({
                    {"id", t.id},
                    {"x", t.pos.x}, {"y", t.pos.y},
                    {"vx", t.vel.x}, {"vy", t.vel.y}
                });
            }
        }

        j["hazards"] = json::array();
        for (const auto& h : hazards) {
            j["hazards"].push_back({
                {"type", h.type},
                {"x", h.pos.x}, {"y", h.pos.y},
                {"radius", h.radius}, {"timer", h.timer},
                {"angle", h.angle},
                {"duration", h.duration},
                {"id", h.id}
            });
        }

        j["items"] = json::array();
        for (const auto& item : items) {
            j["items"].push_back({
                {"id", item.id}, {"type", item.type},
                {"x", item.pos.x}, {"y", item.pos.y}
            });
        }

        return j;
    }

    void processBotCommands() {
        for (auto& bot : bots) {
            // Identifica a struct original do tanque
            auto it = std::find_if(tanks.begin(), tanks.end(),
                                   [&](const Tank& t) { return t.id == bot->id; });
            if (it == tanks.end() || !it->is_alive) continue;
            Tank& tank = *it;

            // Se o processo do bot encerrou ou morreu no sistema operacional (regra 8.1.2).
            // Antes este teste vinha depois de um "continue" para processos mortos, e o
            // tanque de um bot encerrado ficava vivo e parado até o fim da partida.
            if (!bot->isAlive()) {
                tank.is_alive = false;
                tank.hp = 0.0;
                registrarMomento("KILL_DESCONEXAO", "Bot ID [" + std::to_string(tank.id) + "] encerrou a conexao com o controlador e foi desativado!", tank.id, -1, "[DESCONEXAO]");
                continue;
            }

            // Prazo de resposta: 50 ms por tick. No tick 0 vale o prazo de partida, para o
            // processo do bot terminar de iniciar (a JVM leva centenas de ms).
            int prazo_ms = Config::RESPOSTA_MS;
            if (tick_count == 0) {
                auto restante = std::chrono::duration_cast<std::chrono::milliseconds>(
                    inicio_partida + std::chrono::milliseconds(Config::PARTIDA_INICIO_MS) - std::chrono::steady_clock::now()).count();
                prazo_ms = static_cast<int>(std::max<long long>(Config::RESPOSTA_MS, restante));
            }

            // Envia a percepção pelo stdin do bot e espera a resposta a ela pelo stdout
            json res;
            if (bot->trocar(buildPerceptionPayload(tank).dump(), res, prazo_ms)) {
                tank.consecutive_timeouts = 0;
                try {
                    // --- CLAMPING RIGOROSO ANTICHEATING ---
                    double throttle = std::clamp(res.value("throttle", 0.0), -1.0, 1.0);
                    double turn = std::clamp(res.value("turn", 0.0), -1.0, 1.0);
                    double turret = normalizeAngle(res.value("turret_angle", tank.turret_angle));
                    bool shoot = res.value("shoot", false);
                    bool shield = res.value("shield", false);

                    // Atualização cinemática de rotação e aceleração
                    tank.angle = normalizeAngle(tank.angle + turn * Config::TANK_TURN_RATE * Config::TICK_DT);
                    tank.turret_angle = turret;

                    double rad = deg2rad(tank.angle);
                    double target_speed = throttle * Config::TANK_MAX_SPEED;
                    tank.vel.x = std::cos(rad) * target_speed;
                    tank.vel.y = std::sin(rad) * target_speed;

                    // --- NOVA GESTÃO DE ESCUDO ---
                    // Se o escudo está desligado, o bot tenta ligar, mas a energia não é 100%, o Juiz nega.
                    // --- GESTÃO DE ESCUDO ---
                    if (!tank.shield_active && shield && tank.energy <= Config::TANK_MAX_ENERGY * 0.5f) {
                        shield = false; 
                    }

                    if (shield && tank.energy > 0.0) {
                        // Aplica o gasto desenfreado
                        tank.shield_active = true;
                        tank.energy -= Config::SHIELD_COST * Config::TICK_DT;
                        
                        // Se a energia zerar durante o uso, o escudo quebra na mesma hora
                        if (tank.energy <= 0.0) {
                            tank.energy = 0.0;
                            tank.shield_active = false;
                        }
                    } else {
                        // Desliga se o bot pedir ou se não houver comando válido
                        tank.shield_active = false;
                    }

                    // Disparo de Projétil
                    if (shoot && tank.cooldown <= 0.0 && tank.energy >= Config::SHOOT_COST) {
                        tank.cooldown = Config::SHOOT_COOLDOWN;
                        tank.energy -= Config::SHOOT_COST;

                        double t_rad = deg2rad(tank.turret_angle);
                        Projectile proj;
                        proj.id = next_entity_id++;
                        proj.owner_id = tank.id;
                        // Projétil nasce logo à frente da torreta
                        proj.pos = tank.pos + Vec2(std::cos(t_rad), std::sin(t_rad)) * (Config::TANK_RADIUS + 5.0);
                        proj.vel = Vec2(std::cos(t_rad), std::sin(t_rad)) * Config::PROJ_SPEED;
                        projectiles.push_back(proj);

                        current_events.push_back({"SHOOT", proj.pos.x, proj.pos.y, tank.id, -1});
                    }
                } catch (...) {
                    // JSON malformado ou erro de parse é ignorado para não derrubar a simulação
                }
            } else {
                tank.consecutive_timeouts++;
                // Se o bot não responder por 20 ticks seguidos (2 segundos)
                if (tank.consecutive_timeouts >= 20) {
                    tank.is_alive = false;
                    tank.hp = 0.0;
                    registrarMomento("KILL_TIMEOUT", "Bot ID [" + std::to_string(tank.id) + "] parou de responder (timeout) e foi desativado!", tank.id, -1, "[TIMEOUT]");
                }
            }
        }
    }

    void updatePhysicsAndCollisions() {
        // 1. Atualização e restrição dos Tanques na Arena
        for (auto& t : tanks) {
            if (!t.is_alive) continue;

            t.pos = t.pos + t.vel * Config::TICK_DT;
            t.pos.x = std::clamp(t.pos.x, Config::TANK_RADIUS, Config::ARENA_WIDTH - Config::TANK_RADIUS);
            t.pos.y = std::clamp(t.pos.y, Config::TANK_RADIUS, Config::ARENA_HEIGHT - Config::TANK_RADIUS);

            t.cooldown = std::max(0.0, t.cooldown - Config::TICK_DT);
            if (!t.shield_active) {
                t.energy = std::min(Config::TANK_MAX_ENERGY, t.energy + Config::ENERGY_REGEN * Config::TICK_DT);
            }
        }

        // 2. Cinemática dos Projéteis e Colisões
        for (auto it = projectiles.begin(); it != projectiles.end(); ) {
            it->pos = it->pos + it->vel * Config::TICK_DT;

            bool removed = false;
            // Verifica limites das paredes
            if (it->pos.x < 0 || it->pos.x > Config::ARENA_WIDTH ||
                it->pos.y < 0 || it->pos.y > Config::ARENA_HEIGHT) {
                it = projectiles.erase(it);
                continue;
            }

            // Colisão Tiros vs Tanques
            for (auto& t : tanks) {
                if (!t.is_alive || t.id == it->owner_id) continue;

                if (t.pos.dist(it->pos) <= (Config::TANK_RADIUS + Config::PROJ_RADIUS)) {
                    if (!t.shield_active) {
                        t.hp -= Config::PROJ_DAMAGE;
                    } else {
                        t.energy = std::max(0.0, t.energy - 10.0); // Impacto no escudo absorve energia
                    }

                    current_events.push_back({"HIT", t.pos.x, t.pos.y, it->owner_id, t.id});

                    if (t.hp <= 0.0) {
                        t.is_alive = false;
                        current_events.push_back({"EXPLOSION", t.pos.x, t.pos.y, it->owner_id, t.id});
                        // Atribuição de Kills ao atirador
                        auto killer = std::find_if(tanks.begin(), tanks.end(),
                                                   [&](const Tank& k) { return k.id == it->owner_id; });
                        if (killer != tanks.end()) killer->kills++;

                        registrarMomento("KILL_PROJETIL", 
                            "Bot ID [" + std::to_string(t.id) + "] foi DESTRUIDO por um disparo certeiro do Bot ID [" + std::to_string(it->owner_id) + "]!", 
                            t.id, it->owner_id, "[ABATE]");
                    }
                    removed = true;
                    break;
                }
            }

            if (removed) {
                it = projectiles.erase(it);
            } else {
                ++it;
            }
        }

        // 3. Colisões com Itens da Arena
        for (auto it = items.begin(); it != items.end(); ) {
            bool collected = false;
            for (auto& t : tanks) {
                if (!t.is_alive) continue;
                if (t.pos.dist(it->pos) <= (Config::TANK_RADIUS + Config::ITEM_RADIUS)) {
                    if (it->type == "HEAL") {
                        if (t.hp <= 30.0) {
                            registrarMomento("CLUTCH_HEAL", 
                                "Bot ID [" + std::to_string(t.id) + "] estava por um fio (" + std::to_string((int)t.hp) + " HP) e conseguiu um Kit Medico de emergencia!", 
                                t.id, -1, "[CURA]");
                        }

                        t.hp = std::min(Config::TANK_MAX_HP, t.hp + Config::ITEM_HEAL_VAL);
                    } else if (it->type == "ENERGY") {
                        t.energy = std::min(Config::TANK_MAX_ENERGY, t.energy + Config::ITEM_ENERGY_VAL);
                    }
                    current_events.push_back({"ITEM_PICKUP", it->pos.x, it->pos.y, t.id, -1});
                    collected = true;
                    break;
                }
            }
            if (collected) it = items.erase(it);
            else ++it;
        }

        // 4. Atualização e Dano dos Hazards (Artilharia e Lasers)
        for (auto it = hazards.begin(); it != hazards.end(); ) {
            
            // ================================================================
            // LÓGICA DA ARTILHARIA
            // ================================================================
            if (it->type == "ARTILLERY") {
                it->timer -= Config::TICK_DT;
                
                if (it->timer <= 0.0) {
                    current_events.push_back({"EXPLOSION", it->pos.x, it->pos.y, -1, -1});
                    for (auto& t : tanks) {
                        if (t.is_alive && t.pos.dist(it->pos) <= (it->radius + Config::TANK_RADIUS)) {
                            double dmg = t.shield_active ? Config::ARTILLERY_DAMAGE * 0.2 : Config::ARTILLERY_DAMAGE;
                            t.hp -= dmg;
                            if (t.hp <= 0.0) 
                            {
                                t.is_alive = false;
                                registrarMomento("KILL_ARTILHARIA", 
                                    "Bot ID [" + std::to_string(t.id) + "] nao conseguiu escapar e foi PULVERIZADO por um bombardeio de Artilharia!", 
                                    t.id, -1, "[BOMBA]");
                            }
                        }
                    }
                    // Apaga da lista e já pega o próximo iterador
                    it = hazards.erase(it);
                    continue; // Pula o ++it lá do final
                }
            } 
            // ================================================================
            // LÓGICA DO LASER
            // ================================================================
            else if (it->type == "LASER") {
                // O Laser sempre gira, mesmo durante o aviso, para o alvo tentar prever
                it->angle = normalizeAngle(it->angle + it->angular_vel * Config::TICK_DT);

                if (it->timer > 0.0) {
                    // FASE 1: AVISO (Contagem regressiva, não dá dano)
                    it->timer -= Config::TICK_DT;
                } 
                else if (it->duration > 0.0) {
                    // FASE 2: ATIRANDO (Tempo esgotou, começou a queimar)
                    it->duration -= Config::TICK_DT;

                    double rad = deg2rad(it->angle);
                    Vec2 laser_end = it->pos + Vec2(std::cos(rad), std::sin(rad)) * 1500.0;

                    for (auto& t : tanks) {
                        if (!t.is_alive) continue;
                        double dist = distPointToSegment(t.pos, it->pos, laser_end);
                        if (dist <= (Config::TANK_RADIUS + Config::LASER_WIDTH)) {
                            double dmg = (t.shield_active ? Config::LASER_DPS * 0.1 : Config::LASER_DPS) * Config::TICK_DT;
                            t.hp -= dmg;
                            if (t.hp <= 0.0)
                            {
                                t.is_alive = false;
                                registrarMomento("KILL_LASER", 
                                    "Bot ID [" + std::to_string(t.id) + "] entrou no caminho do feixe orbital e foi DERRETIDO pelo Laser!", 
                                    t.id, -1, "[LASER]");
                            }
                        }
                    }
                } 
                else {
                    // FASE 3: DESLIGAR (A duração acabou)
                    // Apaga o laser da arena e atualiza o iterador
                    it = hazards.erase(it);
                    continue; // Pula o ++it lá do final
                }
            }

            // Só avança o iterador manualmente se não tiver deletado nada neste ciclo
            ++it;
        }
    }

    void recordFrame() {
        json frame;
        frame["tick"] = tick_count;
        
        frame["tanks"] = json::array();
        for (const auto& t : tanks) {
            frame["tanks"].push_back({
                {"id", t.id}, {"x", t.pos.x}, {"y", t.pos.y},
                {"angle", t.angle}, {"turret", t.turret_angle},
                {"hp", t.hp}, {"energy", t.energy},
                {"shield", t.shield_active}, {"kills", t.kills}, {"alive", t.is_alive}
            });
        }

        frame["projectiles"] = json::array();
        for (const auto& p : projectiles) {
            frame["projectiles"].push_back({
                {"id", p.id}, 
                {"x", p.pos.x}, 
                {"y", p.pos.y}, 
                {"owner_id", p.owner_id},
                {"vx", p.vel.x}, 
                {"vy", p.vel.y}
            });
        }

        frame["hazards"] = json::array();
        for (const auto& h : hazards) {
            frame["hazards"].push_back({
                {"type", h.type}, {"x", h.pos.x}, {"y", h.pos.y},
                {"radius", h.radius}, {"timer", h.timer}, {"angle", h.angle}, {"id", h.id}, {"duration", h.duration}
            });
        }

        frame["items"] = json::array();
        for (const auto& i : items) {
            frame["items"].push_back({
                {"id", i.id}, {"type", i.type}, {"x", i.pos.x}, {"y", i.pos.y}
            });
        }

        frame["events"] = json::array();
        for (const auto& e : current_events) {
            frame["events"].push_back({
                {"type", e.type}, {"x", e.x}, {"y", e.y},
                {"source_id", e.source_id}, {"target_id", e.target_id}
            });
        }

        match_replay["frames"].push_back(frame);
        current_events.clear();
    }

    int countAliveTanks() const {
        return std::count_if(tanks.begin(), tanks.end(), [](const Tank& t) { return t.is_alive; });
    }

    // Registra o tick de eliminação dos tanques que morreram neste tick e encerra o processo
    // do bot (um bot eliminado não recebe mais estados nem deve gastar CPU/memória)
    void marcarEliminados() {
        for (auto& t : tanks) {
            if (!t.is_alive && t.tick_eliminacao < 0) {
                t.tick_eliminacao = tick_count;
                for (auto& bot : bots) {
                    if (bot->id == t.id) bot->terminate();
                }
            }
        }
    }

    // Classificação da partida (regulamento 8.4.1): 1º vivo > 2º abates > 3º vida restante.
    // Desempates (o regulamento não define): eliminado mais tarde > menor id (ordem do slot).
    // A vida de tanques destruídos conta como 0 (o HP interno pode ficar negativo).
    std::vector<const Tank*> classificacao() const {
        std::vector<const Tank*> ordem;
        for (const auto& t : tanks) ordem.push_back(&t);
        std::sort(ordem.begin(), ordem.end(), [](const Tank* a, const Tank* b) {
            if (a->is_alive != b->is_alive) return a->is_alive > b->is_alive;
            if (a->kills != b->kills) return a->kills > b->kills;
            double hp_a = std::max(0.0, a->hp), hp_b = std::max(0.0, b->hp);
            if (hp_a != hp_b) return hp_a > hp_b;
            if (a->tick_eliminacao != b->tick_eliminacao) {
                if (a->tick_eliminacao < 0) return true;   // vivo
                if (b->tick_eliminacao < 0) return false;
                return a->tick_eliminacao > b->tick_eliminacao;
            }
            return a->id < b->id;
        });
        return ordem;
    }

    void run() {
        std::cout << "[ENGINE] Início de simulação! Bots conectados: " << bots.size() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        inicio_partida = std::chrono::steady_clock::now();

        while (tick_count < Config::MAX_TICKS && countAliveTanks() > 1) {
            spawnRandomHazardsAndItems();
            processBotCommands();
            updatePhysicsAndCollisions();
            marcarEliminados();
            recordFrame();

            tick_count++;
            // Em modo offline para geração ultrarrápida de replay, não utilizamos sleep(dt)
        }

        registrarMomento("INFO", "Combate encerrado! Processando resultados...", -1, -1, "[FIM]");
        exportReplayAndSummary();
    }

    void exportReplayAndSummary() {
        marcarEliminados();

        // 1. Cálculo do Vencedor e Placar Final
        std::cout << "\n==================================================\n";
        std::cout << "               PLACAR FINAL DA ARENA              \n";
        std::cout << "==================================================\n";
        std::cout << "Duração da Batalha: " << tick_count << " ticks (" << (tick_count * Config::TICK_DT) << "s)\n";
        std::cout << "Seed da partida: " << seed << "\n\n";

        auto sorted_tanks = classificacao();

        // Ranking em ordem de id (bots[id-1]); a posição vem do critério de classificação.
        // O script.sh troca "Bot_<id>" pelo nome da equipe e acrescenta a linguagem.
        json ranking = json::array();
        for (const auto& t : tanks) {
            size_t posicao = std::find(sorted_tanks.begin(), sorted_tanks.end(), &t) - sorted_tanks.begin() + 1;
            ranking.push_back({
                {"id", t.id},
                {"nome", "Bot_" + std::to_string(t.id)},
                {"posicao", posicao},
                {"vivo", t.is_alive},
                {"kills", t.kills},
                {"hp", std::max(0.0, t.hp)},
                {"vencedor", posicao == 1},
                {"tick_eliminacao", t.tick_eliminacao < 0 ? json(nullptr) : json(t.tick_eliminacao)}
            });
        }

        for (size_t i = 0; i < sorted_tanks.size(); ++i) {
            const auto* t = sorted_tanks[i];
            std::cout << (i + 1) << "º Lugar: Bot ID [" << t->id << "] "
                      << (t->is_alive ? "(VIVO)" : "(DESTRUÍDO)")
                      << " | Kills: " << t->kills
                      << " | HP Final: " << std::max(0.0, t->hp) << "\n";
        }

        std::string vencedor_nome = "Empate";
        if (!sorted_tanks.empty()) {
            vencedor_nome = "Bot_" + std::to_string(sorted_tanks[0]->id);
            std::cout << "--------------------------------------------------\n";
            std::cout << "[VENCEDOR DA PARTIDA] BOT ID [" << sorted_tanks[0]->id << "]!\n";
        }
        std::cout << "==================================================\n";

        auto now = std::chrono::system_clock::now().time_since_epoch().count();

        // 2. Exportar o resumo da partida para a pasta replays/
        std::string caminho_resumo = "replays/partida_" + std::to_string(now) + ".json";
        std::ofstream resumo(caminho_resumo);
        if (resumo.is_open()) {
            json j_resumo;
            j_resumo["partida_id"] = std::to_string(now);
            j_resumo["status"] = "concluido";
            j_resumo["seed"] = seed;
            j_resumo["vencedor"] = vencedor_nome;
            j_resumo["turnos"] = tick_count;
            j_resumo["duracao_ms"] = static_cast<int>(tick_count * Config::TICK_DT * 1000);
            j_resumo["bots"] = ranking;

            resumo << j_resumo.dump(2);
            resumo.close();
            std::cout << "[ENGINE] Resumo da partida salvo em: " << caminho_resumo << "\n";
        }

        // 3. Exportação do Arquivo JSON de Replay Completo COM TODOS OS FRAMES
        match_replay["partida_id"] = std::to_string(now);
        match_replay["status"] = "concluido";
        match_replay["seed"] = seed;
        match_replay["vencedor"] = vencedor_nome;
        match_replay["turnos"] = tick_count;
        match_replay["duracao_ms"] = static_cast<int>(tick_count * Config::TICK_DT * 1000);
        match_replay["bots"] = ranking;

        std::ofstream file("match_frames.json");
        if (file.is_open()) {
            file << match_replay.dump(2);
            file.close();
            std::cout << "\n[ENGINE] Replay da arena exportado com sucesso em: match_frames.json ("
                      << match_replay["frames"].size() << " frames gravados)\n";
        } else {
            std::cerr << "[ENGINE ERRO] Falha ao gravar arquivo match_frames.json\n";
        }
    }

    void registrarMomento(const std::string& tipo, const std::string& texto, int alvo_id = -1, int assassino_id = -1, const std::string& icone = "[INFO]") {
        MomentoNarrativo m;
        m.tick = tick_count;
        m.tempo_s = tick_count * Config::TICK_DT;
        m.tipo = tipo;
        m.alvo_id = alvo_id;
        m.assassino_id = assassino_id;
        
        // Formata com tempo fixo (ex: [014.2s | Tick 0142])
        std::ostringstream ss;
        ss << "[" << std::fixed << std::setprecision(1) << std::setw(5) << std::setfill('0') << m.tempo_s 
           << "s | Tick " << std::setw(4) << std::setfill('0') << m.tick << "] " 
           << icone << " " << texto;
        m.texto = ss.str();
        
        narracao.push_back(m);
        
        // TRANSMISSÃO AO VIVO NO TERMINAL:
        std::cout << "  " << m.texto << "\n";
    }
};

// ============================================================================
// 7. PONTO DE ENTRADA (MAIN CLI)
// ============================================================================
int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " \"comando_bot_1\" \"comando_bot_2\" ... [até " << Config::MAX_BOTS_PADRAO
                  << " bots; TOPCOM_MAX_BOTS altera o limite]\n";
        std::cerr << "Exemplo: " << argv[0] << " \"python3 bot.py\" \"./bot_cpp\" \"java -cp . Bot\"\n";
        return 1;
    }

    int max_bots = Config::MAX_BOTS_PADRAO;
    if (const char* env = std::getenv("TOPCOM_MAX_BOTS")) {
        int v = std::atoi(env);
        if (v > 0) max_bots = v;
    }

    // Seed da partida: TOPCOM_SEED (0 a 4294967295) reproduz hazards e itens; sem ela, sorteia.
    // Valor inválido também cai no sorteio, com aviso. A seed usada vai para o JSON.
    uint32_t seed = 0;
    bool seed_informada = false;
    if (const char* env = std::getenv("TOPCOM_SEED")) {
        char* fim = nullptr;
        errno = 0;
        unsigned long long v = std::strtoull(env, &fim, 10);
        bool so_digitos = *env != '\0' && std::all_of(env, env + std::strlen(env), [](char c) { return c >= '0' && c <= '9'; });
        if (so_digitos && errno == 0 && fim && *fim == '\0' && v <= UINT32_MAX) {
            seed = static_cast<uint32_t>(v);
            seed_informada = true;
        } else {
            std::cerr << "[ENGINE AVISO] TOPCOM_SEED invalida ('" << env << "'): use um inteiro de 0 a 4294967295. Sorteando outra.\n";
        }
    }
    if (!seed_informada) {
        uint64_t mistura = static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        try {
            std::random_device rd;
            mistura ^= (static_cast<uint64_t>(rd()) << 32) | rd();
        } catch (...) {
            // sem random_device: fica só o relógio
        }
        seed = static_cast<uint32_t>(mistura ^ (mistura >> 32));
    }
    std::cout << "[ENGINE] Seed da partida: " << seed << (seed_informada ? " (TOPCOM_SEED)" : " (sorteada)") << "\n";

    std::vector<std::string> bot_commands;
    for (int i = 1; i < argc && i <= max_bots; ++i) {
        bot_commands.push_back(argv[i]);
    }

    ArenaEngine arena(bot_commands, seed);
    arena.run();

    return 0;
}