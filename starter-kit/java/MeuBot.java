// =============================================================================
// GAME JAM PET 2026 - STARTER KIT JAVA
// =============================================================================
// O que você precisa fazer: escrever o método pensar(), mais abaixo.
// Ele é chamado 10 vezes por segundo e devolve o que o seu tanque vai fazer.
//
// Regras do código enviado:
// - Um único arquivo, sem "package". Tudo o que você criar (métodos, classes
//   auxiliares) fica DENTRO da classe principal, como nas classes aninhadas
//   abaixo.
// - Não crie main() nem use System.in: a arena já tem o executor dela e chama
//   pensar() por você. System.out vai para o log de erros e é descartado.
// - A assinatura de pensar() não pode mudar:
//       public static JsonObject pensar(JsonObject estado)
// - Campos static valem durante a partida inteira (sua memória).
// - A biblioteca Gson (com.google.gson) já está disponível.
// - O pensar() abaixo lê todos os campos só para mostrar o que existe: os avisos
//   de "variável não usada" da IDE somem quando você usar os valores.
// =============================================================================

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import java.util.ArrayList;
import java.util.List;

public class MeuBot {

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


    // =========================================================================
    // SEU CÓDIGO COMEÇA AQUI
    // =========================================================================

    public static JsonObject pensar(JsonObject estado) {
        // --- O jogo ---
        int t = tick(estado);                   // número do tick atual (a partida tem até 1500)

        // --- O seu tanque ---
        Tanque meu = eu(estado);
        double x = meu.x;                       // posição horizontal
        double y = meu.y;                       // posição vertical
        double vx = meu.vx;                     // velocidade em x (unidades/s)
        double vy = meu.vy;                     // velocidade em y (unidades/s)
        double angulo = meu.angle;              // para onde o chassi aponta (graus)
        double torre = meu.turretAngle;         // para onde a torre aponta (graus)
        double vida = meu.hp;                   // vida: chegando a 0, você é eliminado
        double energia = meu.energy;            // energia: tiro e escudo gastam
        double recarga = meu.cooldown;          // segundos até poder atirar de novo
        boolean escudo = meu.shield;            // true se o escudo está ligado

        // --- Adversários vivos ---
        for (Inimigo inimigo : inimigos(estado)) {
            int id = inimigo.id;                // identificador
            double ix = inimigo.x;              // posição
            double iy = inimigo.y;
            double ivx = inimigo.vx;            // velocidade
            double ivy = inimigo.vy;
        }

        // --- Perigos da arena ---
        for (Hazard perigo : perigos(estado)) {
            int id = perigo.id;                 // identificador
            int tipo = perigo.tipo;             // HAZARD_ARTILHARIA ou HAZARD_LASER
            double px = perigo.x;               // posição
            double py = perigo.y;
            double raio = perigo.radius;        // raio de efeito
            double prazo = perigo.timer;        // segundos até o impacto / a ativação
            double direcao = perigo.angle;      // direção do feixe (laser)
            double duracao = perigo.duration;   // segundos de efeito (laser)
        }

        // --- Itens no mapa ---
        for (Item item : itens(estado)) {
            int id = item.id;                   // identificador
            int tipo = item.tipo;               // ITEM_CURA ou ITEM_ENERGIA
            double ix = item.x;                 // posição
            double iy = item.y;
        }

        // --- A ação: andar para a frente girando devagar, torre parada, sem atirar ---
        return montarAcao(
            1.0,        // throttle: -1.0 (ré) a 1.0 (frente)
            0.5,        // turn: -1.0 a 1.0 (positivo aumenta o ângulo do chassi)
            // torre,   // turretAngle: direção da torre (variante sem ele: mantém como está)
            false,      // shoot: true para atirar (precisa de recarga == 0 e energia)
            false);     // shield: true para ligar o escudo (gasta energia)
    }
}
