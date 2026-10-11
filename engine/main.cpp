/**
 * ============================================================================
 * ENGINE.CPP - Motor de Simulação e Juiz de Arena Top-Down (Game Jam IA)
 * ============================================================================
 * Padrão: C++17 (POSIX Linux)
 * Dependência: <nlohmann/json.hpp>
 * Compilação: g++ -O3 -std=c++17 engine.cpp -o engine
 * Execução:   ./engine "python3 bot1.py" "./bot_cpp" "java -cp . Bot"
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
class BotProcess {
private:
    pid_t pid = -1;
    int pipe_in[2] = {-1, -1};  // Engine -> Bot (stdin)
    int pipe_out[2] = {-1, -1}; // Bot -> Engine (stdout)
    bool alive = false;
    std::string read_buffer;

public:
    int id;
    std::string command;

    BotProcess(int _id, const std::string& _cmd) : id(_id), command(_cmd) {}

    ~BotProcess() {
        terminate();
    }

    bool start() {
        if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
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
            // Fechar pontas não utilizadas dos pipes
            close(pipe_in[1]);
            close(pipe_out[0]);

            // Redirecionar stdin e stdout
            dup2(pipe_in[0], STDIN_FILENO);
            dup2(pipe_out[1], STDOUT_FILENO);
            dup2(pipe_out[1], STDERR_FILENO); // Redireciona stderr para análise unificada

            close(pipe_in[0]);
            close(pipe_out[1]);

            // Executar via shell para suportar argumentos complexos (ex: "python3 bot.py")
            execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
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

    void sendPayload(const std::string& data) {
        if (!alive) return;
        if (!checkProcessAlive()) return;
        std::string msg = data + "\n";
        ssize_t bytes = write(pipe_in[1], msg.c_str(), msg.size());
        if (bytes < 0) {
            alive = false;
        }
    }

    // Leitura não-bloqueante utilizando POSIX select para evitar travamentos
    bool readResponse(std::string& out_line, int timeout_ms = 50) {
        if (!alive) return false;
        if (!checkProcessAlive()) return false;

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(pipe_out[0], &read_fds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = timeout_ms * 1000;

        int ready = select(pipe_out[0] + 1, &read_fds, nullptr, nullptr, &tv);
        if (ready > 0 && FD_ISSET(pipe_out[0], &read_fds)) {
            char buffer[1024];
            while (true) {
                ssize_t bytes = read(pipe_out[0], buffer, sizeof(buffer) - 1);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    read_buffer += buffer;
                } else {
                    break;
                }
            }
        }

        // Procura por quebra de linha determinando o fim do comando JSON
        size_t pos = read_buffer.find('\n');
        if (pos != std::string::npos) {
            out_line = read_buffer.substr(0, pos);
            read_buffer.erase(0, pos + 1);
            return true;
        }

        return false;
    }

    void terminate() {
        if (alive && pid > 0) {
            kill(pid, SIGKILL);
            waitpid(pid, nullptr, WNOHANG);
            alive = false;
        }
        if (pipe_in[1] >= 0) close(pipe_in[1]);
        if (pipe_out[0] >= 0) close(pipe_out[0]);
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

public:
    ArenaEngine(const std::vector<std::string>& bot_commands) {
        rng.seed(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Inicia e posiciona os tanques distribuídos em círculo na arena (até 20 bots)
        size_t total_bots = std::min(bot_commands.size(), size_t(20));
        double center_x = Config::ARENA_WIDTH / 2.0;
        double center_y = Config::ARENA_HEIGHT / 2.0;
        double spawn_radius = 360.0;

        for (size_t i = 0; i < total_bots; ++i) {
            auto bot = std::make_unique<BotProcess>(i + 1, bot_commands[i]);
            if (bot->start()) {
                bots.push_back(std::move(bot));
                Tank t;
                t.id = i + 1;
                double angle_rad = (2.0 * M_PI * i) / total_bots;
                t.pos = { center_x + spawn_radius * std::cos(angle_rad),
                          center_y + spawn_radius * std::sin(angle_rad) };
                t.angle = std::fmod((angle_rad * 180.0 / M_PI) + 180.0, 360.0); // Olhando para o centro da arena
                t.turret_angle = t.angle;
                tanks.push_back(t);
            }
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
                h.angle = 0.0;
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
            if (!bot->isAlive()) continue;

            // Identifica a struct original do tanque
            auto it = std::find_if(tanks.begin(), tanks.end(),
                                   [&](const Tank& t) { return t.id == bot->id; });
            if (it == tanks.end() || !it->is_alive) continue;
            Tank& tank = *it;

            // Se o processo do bot encerrou ou morreu no sistema operacional
            if (!bot->isAlive()) {
                tank.is_alive = false;
                tank.hp = 0.0;
                registrarMomento("KILL_DESCONEXAO", "Bot ID [" + std::to_string(tank.id) + "] encerrou a conexao com o controlador e foi desativado!", tank.id, -1, "[DESCONEXAO]");
                continue;
            }

            // Envia Percepção JSON via pipe_in
            json payload = buildPerceptionPayload(tank);
            bot->sendPayload(payload.dump());

            // Lê resposta via pipe_out (com timeout não-bloqueante de 50ms)
            std::string response_str;
            if (bot->readResponse(response_str, 50)) {
                tank.consecutive_timeouts = 0;
                try {
                    json res = json::parse(response_str);

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

    void run() {
        std::cout << "[ENGINE] Início de simulação! Bots conectados: " << bots.size() << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        while (tick_count < Config::MAX_TICKS && countAliveTanks() > 1) {
            spawnRandomHazardsAndItems();
            processBotCommands();
            updatePhysicsAndCollisions();
            recordFrame();

            tick_count++;
            // Em modo offline para geração ultrarrápida de replay, não utilizamos sleep(dt)
        }

        registrarMomento("INFO", "Combate encerrado! Processando resultados...", -1, -1, "[FIM]");
        exportReplayAndSummary();
    }

    void exportReplayAndSummary() {
        // 1. Cálculo do Vencedor e Placar Final
        std::cout << "\n==================================================\n";
        std::cout << "               PLACAR FINAL DA ARENA              \n";
        std::cout << "==================================================\n";
        std::cout << "Duração da Batalha: " << tick_count << " ticks (" << (tick_count * Config::TICK_DT) << "s)\n\n";

        // Ordenação por: 1º Vivo > 2º Kills > 3º HP Restante
        std::vector<Tank*> sorted_tanks;
        for (auto& t : tanks) sorted_tanks.push_back(&t);

        std::sort(sorted_tanks.begin(), sorted_tanks.end(), [](const Tank* a, const Tank* b) {
            if (a->is_alive != b->is_alive) return a->is_alive > b->is_alive;
            if (a->kills != b->kills) return a->kills > b->kills;
            return a->hp > b->hp;
        });

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

        // 2. Monta destaques e momentos narrativos
        json j_destaques = json::array();
        for (const auto& m : narracao) {
            j_destaques.push_back({
                {"tick", m.tick},
                {"tempo_s", m.tempo_s},
                {"tipo", m.tipo},
                {"alvo_id", m.alvo_id},
                {"assassino_id", m.assassino_id},
                {"log", m.texto}
            });
        }

        // 3. Exportar o resumo estatístico para a pasta replays/
        std::string caminho_resumo = "replays/partida_" + std::to_string(now) + ".json";
        std::ofstream resumo(caminho_resumo);
        if (resumo.is_open()) {
            json j_resumo;
            j_resumo["partida_id"] = std::to_string(now);
            j_resumo["status"] = "concluido";
            j_resumo["vencedor"] = vencedor_nome;
            j_resumo["turnos"] = tick_count;
            j_resumo["duracao_ms"] = static_cast<int>(tick_count * Config::TICK_DT * 1000);
            j_resumo["destaques"] = j_destaques;

            resumo << j_resumo.dump(2);
            resumo.close();
            std::cout << "[ENGINE] Resumo estatístico salvo em: " << caminho_resumo << "\n";
        }

        // 4. Exportação do Arquivo JSON de Replay Completo COM TODOS OS FRAMES
        match_replay["partida_id"] = std::to_string(now);
        match_replay["status"] = "concluido";
        match_replay["vencedor"] = vencedor_nome;
        match_replay["turnos"] = tick_count;
        match_replay["duracao_ms"] = static_cast<int>(tick_count * Config::TICK_DT * 1000);
        match_replay["destaques"] = j_destaques;

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
        std::cerr << "Uso: " << argv[0] << " \"comando_bot_1\" \"comando_bot_2\" ... [até 20 bots]\n";
        std::cerr << "Exemplo: " << argv[0] << " \"python3 bot.py\" \"./bot_cpp\" \"java -cp . Bot\"\n";
        return 1;
    }

    std::vector<std::string> bot_commands;
    for (int i = 1; i < argc && i <= 20; ++i) {
        bot_commands.push_back(argv[i]);
    }

    ArenaEngine arena(bot_commands);
    arena.run();

    return 0;
}