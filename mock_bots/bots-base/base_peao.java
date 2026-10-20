// =============================================================================
// NPC "peao" - nível FÁCIL (Java)
// =============================================================================
// Estratégia: anda de ré girando em círculos, com a torre varrendo a arena, e
// atira sempre que pode, sem mirar em ninguém. Não olha o estado da arena.
// Serve de adversário para quem está começando e de exemplo de memória entre
// ticks (campo static) e de uso dos ajudantes em Java.
// =============================================================================

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.util.ArrayList;
import java.util.List;

public class Solution {

    // -------------------------------------------------------------------------
    // AJUDANTES (opcional: renomeie, altere ou apague)
    // -------------------------------------------------------------------------
    // Eles só facilitam o acesso aos atributos do estado e não decidem nada por
    // você. Se preferir, apague esta seção inteira e leia o JsonObject 'estado'
    // direto (veja o formato em README.md / enunciado).

    public static final int HAZARD_ARTILHARIA = 0;  // explode em (x, y) com raio 'radius' quando 'timer' zera
    public static final int HAZARD_LASER = 1;       // feixe que gira a partir de (x, y) na direção 'angle'
    public static final int ITEM_CURA = 0;          // recupera vida
    public static final int ITEM_ENERGIA = 1;       // recupera energia

    /** O seu tanque (devolvido por eu). */
    public static class Tanque {
        public int id;                  // identificador
        public double x, y;             // posição (arena de 1000 x 1000)
        public double vx, vy;           // velocidade, em unidades/s
        public double angle;            // direção do chassi, graus [0, 360)
        public double turretAngle;      // direção da torre, graus [0, 360)
        public double hp;               // vida, de 0 a 100
        public double energy;           // energia, de 0 a 100
        public double cooldown;         // segundos até poder atirar (0 = pronto)
        public boolean shield;          // escudo ativo agora
    }

    /** Um adversário vivo (a vida dele não é informada). */
    public static class Inimigo {
        public int id;
        public double x, y;
        public double vx, vy;
    }

    /** Um perigo da arena (artilharia ou laser). */
    public static class Hazard {
        public int id;
        public int tipo;                // HAZARD_ARTILHARIA ou HAZARD_LASER
        public double x, y;
        public double radius;           // raio de efeito
        public double timer;            // segundos até o impacto / a ativação
        public double angle;            // direção do feixe (laser), graus
        public double duration;         // segundos de efeito (laser)
    }

    /** Um item coletável no mapa. */
    public static class Item {
        public int id;
        public int tipo;                // ITEM_CURA ou ITEM_ENERGIA
        public double x, y;
    }

    /** Lê um número do JSON (0 se a chave não existir). */
    private static double num(JsonObject o, String chave) {
        return o.has(chave) ? o.get(chave).getAsDouble() : 0.0;
    }

    /** Número do tick atual (10 ticks por segundo). */
    public static int tick(JsonObject estado) {
        return (int) num(estado, "tick");
    }

    /** Seu tanque. */
    public static Tanque eu(JsonObject estado) {
        Tanque t = new Tanque();
        if (!estado.has("self")) return t;
        JsonObject d = estado.getAsJsonObject("self");
        t.id = (int) num(d, "id");
        t.x = num(d, "x");
        t.y = num(d, "y");
        t.vx = num(d, "vx");
        t.vy = num(d, "vy");
        t.angle = num(d, "angle");
        t.turretAngle = num(d, "turret_angle");
        t.hp = num(d, "hp");
        t.energy = num(d, "energy");
        t.cooldown = num(d, "cooldown");
        t.shield = d.has("shield") && d.get("shield").getAsBoolean();
        return t;
    }

    /** Lista dos adversários vivos. */
    public static List<Inimigo> inimigos(JsonObject estado) {
        List<Inimigo> lista = new ArrayList<>();
        if (!estado.has("enemies")) return lista;
        for (JsonElement e : estado.getAsJsonArray("enemies")) {
            JsonObject d = e.getAsJsonObject();
            Inimigo i = new Inimigo();
            i.id = (int) num(d, "id");
            i.x = num(d, "x");
            i.y = num(d, "y");
            i.vx = num(d, "vx");
            i.vy = num(d, "vy");
            lista.add(i);
        }
        return lista;
    }

    /** Lista dos perigos ativos (artilharia e laser). */
    public static List<Hazard> perigos(JsonObject estado) {
        List<Hazard> lista = new ArrayList<>();
        if (!estado.has("hazards")) return lista;
        for (JsonElement e : estado.getAsJsonArray("hazards")) {
            JsonObject d = e.getAsJsonObject();
            Hazard h = new Hazard();
            h.id = (int) num(d, "id");
            h.tipo = d.has("type") && d.get("type").getAsString().equals("LASER") ? HAZARD_LASER : HAZARD_ARTILHARIA;
            h.x = num(d, "x");
            h.y = num(d, "y");
            h.radius = num(d, "radius");
            h.timer = num(d, "timer");
            h.angle = num(d, "angle");
            h.duration = num(d, "duration");
            lista.add(h);
        }
        return lista;
    }

    /** Lista dos itens no mapa. */
    public static List<Item> itens(JsonObject estado) {
        List<Item> lista = new ArrayList<>();
        if (!estado.has("items")) return lista;
        for (JsonElement e : estado.getAsJsonArray("items")) {
            JsonObject d = e.getAsJsonObject();
            Item it = new Item();
            it.id = (int) num(d, "id");
            it.tipo = d.has("type") && d.get("type").getAsString().equals("ENERGY") ? ITEM_ENERGIA : ITEM_CURA;
            it.x = num(d, "x");
            it.y = num(d, "y");
            lista.add(it);
        }
        return lista;
    }

    /**
     * Monta a ação no formato da arena, com os tipos certos.
     *   throttle: -1.0 (ré) a 1.0 (frente)
     *   turn: -1.0 a 1.0 (positivo aumenta o ângulo do chassi)
     *   turretAngle: direção absoluta da torre em graus (a variante sem este
     *                parâmetro mantém a torre onde está)
     *   shoot, shield: true/false
     */
    public static JsonObject montarAcao(double throttle, double turn, double turretAngle, boolean shoot, boolean shield) {
        JsonObject acao = montarAcao(throttle, turn, shoot, shield);
        acao.addProperty("turret_angle", turretAngle);
        return acao;
    }

    public static JsonObject montarAcao(double throttle, double turn, boolean shoot, boolean shield) {
        JsonObject acao = new JsonObject();
        acao.addProperty("throttle", throttle);
        acao.addProperty("turn", turn);
        acao.addProperty("shoot", shoot);
        acao.addProperty("shield", shield);
        return acao;
    }

    // Matemática básica (ângulos em graus; 0 = +x, cresce de +x para +y)

    /** Distância entre dois pontos. */
    public static double distancia(double x1, double y1, double x2, double y2) {
        return Math.hypot(x2 - x1, y2 - y1);
    }

    /** Leva qualquer ângulo para o intervalo [0, 360). */
    public static double normalizarAngulo(double graus) {
        double a = graus % 360.0;
        return a < 0 ? a + 360.0 : a;
    }

    /** Ângulo, em graus, do ponto (x1, y1) olhando para o ponto (x2, y2). */
    public static double anguloPara(double x1, double y1, double x2, double y2) {
        return normalizarAngulo(Math.atan2(y2 - y1, x2 - x1) * 180.0 / Math.PI);
    }

    /**
     * Menor giro, em graus, para ir do ângulo 'de' ao ângulo 'para': de -180 (exclusivo) a 180.
     * Positivo = girar no sentido em que o ângulo aumenta.
     */
    public static double diferencaAngulo(double de, double para) {
        double d = normalizarAngulo(para - de);
        return d > 180.0 ? d - 360.0 : d;
    }

    // -------------------------------------------------------------------------
    // FIM DOS AJUDANTES
    // -------------------------------------------------------------------------

    // Memória entre ticks: a direção da torre, que avança a cada tick
    private static double torre = 0.0;

    public static JsonObject pensar(JsonObject estado) {
        Tanque meu = eu(estado);
        torre = normalizarAngulo(torre + 25.0);
        boolean atirar = meu.cooldown <= 0 && meu.energy >= 20;
        return montarAcao(-1.0, 1.0, torre, atirar, false);
    }
}
