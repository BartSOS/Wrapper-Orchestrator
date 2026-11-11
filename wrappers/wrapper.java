import java.util.HashSet;
import java.util.Scanner;
import java.util.Set;
import java.io.PrintStream;
import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;

// =============================================================================
// INFRAESTRUTURA DE I/O PARA BOTS EM JAVA (WRAPPER OFICIAL DO BOCA)
// O build_bot.sh troca CLASS_PLACEHOLDER pela classe da equipe e compila este
// arquivo junto com o Arena.java (arquivo de apoio: Estado, Acao, FRENTE...).
// O bot define: public static Acao pensar(Estado e)
// Converte o estado da engine para os nomes em português (tempos em frames,
// velocidades por frame) e a ação de volta para o protocolo da engine:
//   mover -> throttle, direcao -> turn, angulo_torre -> turret_angle,
//   atirar -> shoot, ligar_escudo -> shield.
// A mesma tradução existe em c_wrapper.cpp, cpp_footer.cpp e arena.py.
// =============================================================================
public class JavaRunner {
    private static final Gson gson = new Gson();
    private static final double SEGUNDOS_POR_FRAME = 0.1;  // TICK_DT da engine
    private static final double GIRO_POR_FRAME = 9.0;      // TANK_TURN_RATE (90 graus/s) * TICK_DT
    private static final Set<String> avisados = new HashSet<>();

    private static void avisar(String chave, String msg) {
        if (avisados.add(chave)) System.err.println("[AVISO] " + msg);
    }

    // ---------------------------------------------------------------- estado
    private static JsonObject objeto(JsonObject j, String k) {
        if (j == null) return new JsonObject();
        JsonElement e = j.get(k);
        return (e != null && e.isJsonObject()) ? e.getAsJsonObject() : new JsonObject();
    }

    private static JsonArray lista(JsonObject j, String k) {
        if (j == null) return new JsonArray();
        JsonElement e = j.get(k);
        return (e != null && e.isJsonArray()) ? e.getAsJsonArray() : new JsonArray();
    }

    private static double num(JsonObject j, String k) {
        JsonElement e = j.get(k);
        if (e == null || !e.isJsonPrimitive()) return 0.0;
        JsonPrimitive p = e.getAsJsonPrimitive();
        if (p.isNumber()) return p.getAsDouble();
        if (p.isBoolean()) return p.getAsBoolean() ? 1.0 : 0.0;
        return 0.0;
    }

    private static int inteiro(JsonObject j, String k) {
        return (int) Math.round(num(j, k));
    }

    private static String texto(JsonObject j, String k) {
        JsonElement e = j.get(k);
        return (e != null && e.isJsonPrimitive() && e.getAsJsonPrimitive().isString()) ? e.getAsString() : "";
    }

    private static int frames(JsonObject j, String k) {
        return (int) Math.round(num(j, k) / SEGUNDOS_POR_FRAME);
    }

    private static double porFrame(JsonObject j, String k) {
        return num(j, k) * SEGUNDOS_POR_FRAME;
    }

    static Arena.Estado estadoDe(JsonObject j) {
        Arena.Estado e = new Arena.Estado();
        e.frame = inteiro(j, "tick");
        JsonObject s = objeto(j, "self");
        e.eu.id = inteiro(s, "id");
        e.eu.x = num(s, "x");
        e.eu.y = num(s, "y");
        e.eu.vel_x = porFrame(s, "vx");
        e.eu.vel_y = porFrame(s, "vy");
        e.eu.angulo_chassi = num(s, "angle");
        e.eu.angulo_torre = num(s, "turret_angle");
        e.eu.vida = num(s, "hp");
        e.eu.energia = num(s, "energy");
        e.eu.frames_para_atirar = frames(s, "cooldown");
        e.eu.escudo_ativo = num(s, "shield") != 0.0;

        JsonArray ens = lista(j, "enemies");
        e.inimigos = new Arena.Inimigo[ens.size()];
        for (int i = 0; i < ens.size(); i++) {
            JsonObject en = ens.get(i).isJsonObject() ? ens.get(i).getAsJsonObject() : new JsonObject();
            Arena.Inimigo in = new Arena.Inimigo();
            in.id = inteiro(en, "id");
            in.x = num(en, "x");
            in.y = num(en, "y");
            in.vel_x = porFrame(en, "vx");
            in.vel_y = porFrame(en, "vy");
            e.inimigos[i] = in;
        }
        JsonArray hzs = lista(j, "hazards");
        e.perigos = new Arena.Perigo[hzs.size()];
        for (int i = 0; i < hzs.size(); i++) {
            JsonObject hz = hzs.get(i).isJsonObject() ? hzs.get(i).getAsJsonObject() : new JsonObject();
            Arena.Perigo p = new Arena.Perigo();
            p.id = inteiro(hz, "id");
            p.tipo = "LASER".equals(texto(hz, "type")) ? Arena.PERIGO_LASER : Arena.PERIGO_ARTILHARIA;
            p.x = num(hz, "x");
            p.y = num(hz, "y");
            p.raio = num(hz, "radius");
            p.frames_ate_impacto = frames(hz, "timer");
            p.angulo = num(hz, "angle");
            p.frames_restantes = frames(hz, "duration");
            e.perigos[i] = p;
        }
        JsonArray its = lista(j, "items");
        e.itens = new Arena.Item[its.size()];
        for (int i = 0; i < its.size(); i++) {
            JsonObject it = its.get(i).isJsonObject() ? its.get(i).getAsJsonObject() : new JsonObject();
            Arena.Item item = new Arena.Item();
            item.id = inteiro(it, "id");
            item.tipo = "ENERGY".equals(texto(it, "type")) ? Arena.ITEM_ENERGIA : Arena.ITEM_CURA;
            item.x = num(it, "x");
            item.y = num(it, "y");
            e.itens[i] = item;
        }
        JsonArray ps = lista(objeto(j, "objetivo"), "pontos");
        e.pontos = new Arena.Ponto[ps.size()];
        for (int i = 0; i < ps.size(); i++) {
            JsonObject p = ps.get(i).isJsonObject() ? ps.get(i).getAsJsonObject() : new JsonObject();
            Arena.Ponto pt = new Arena.Ponto();
            pt.id = texto(p, "id");
            pt.x = num(p, "x");
            pt.y = num(p, "y");
            pt.raio = num(p, "raio");
            e.pontos[i] = pt;
        }
        e.n_inimigos = e.inimigos.length;
        e.n_perigos = e.perigos.length;
        e.n_itens = e.itens.length;
        e.n_pontos = e.pontos.length;
        return e;
    }

    // ----------------------------------------------------------------- acao
    private static double turnPara(double direcao, double anguloAtual) {
        double dif = (direcao - anguloAtual) % 360.0;
        if (dif > 180.0) dif -= 360.0;
        if (dif < -180.0) dif += 360.0;
        return Math.max(-1.0, Math.min(1.0, dif / GIRO_POR_FRAME));
    }

    private static double limitar(double v) {
        if (Double.isNaN(v) || Double.isInfinite(v)) return 0.0;
        return Math.max(-1.0, Math.min(1.0, v));
    }

    static JsonObject comandoDe(Arena.Acao a, Arena.Estado e) {
        JsonObject saida = new JsonObject();
        if (Double.isInfinite(a.mover) || Double.isNaN(a.mover)) {
            avisar("mover_nan", "acao.mover nao e um numero valido: o tanque ficou parado.");
        }
        saida.addProperty("throttle", limitar(a.mover));
        boolean temDirecao = !Double.isNaN(a.direcao) && !Double.isInfinite(a.direcao);
        saida.addProperty("turn", temDirecao ? turnPara(a.direcao, e.eu.angulo_chassi) : 0.0);
        if (!Double.isNaN(a.angulo_torre) && !Double.isInfinite(a.angulo_torre)) {
            saida.addProperty("turret_angle", a.angulo_torre);
        }
        saida.addProperty("shoot", a.atirar);
        saida.addProperty("shield", a.ligar_escudo);
        return saida;
    }

    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        PrintStream originalOut = System.out;
        System.setOut(System.err); // Redireciona System.out para evitar poluição no IPC

        // Toda linha de estado recebe exatamente uma resposta: se o pensar() lançar qualquer
        // erro (inclusive StackOverflowError) ou devolver null/NaN, a resposta é a ação vazia {}.
        // Sem isso a engine esperaria a resposta que não veio, e o bot seria eliminado.
        while (scanner.hasNextLine()) {
            String linha = scanner.nextLine();
            if (linha.trim().isEmpty()) continue;
            String resposta = "{}";
            try {
                JsonObject estadoArena = gson.fromJson(linha, JsonObject.class);
                JsonObject comandoSaida = null;
                Arena.Estado e = estadoDe(estadoArena);
                Arena.Acao r = CLASS_PLACEHOLDER.pensar(e);
                if (r == null) {
                    System.err.println("[ERRO NO BOT JAVA]: pensar() devolveu null; termine com 'return a;' "
                            + "(neste frame o tanque ficou parado)");
                } else {
                    comandoSaida = comandoDe(r, e);
                }
                if (comandoSaida != null) {
                    String texto = comandoSaida.toString();
                    // O Gson escreve NaN/Infinity, que não são JSON válido
                    if (texto.contains("NaN") || texto.contains("Infinity")) {
                        System.err.println("[ERRO NO BOT JAVA]: a acao tem NaN ou Infinity (neste frame o tanque ficou parado)");
                    } else {
                        resposta = texto;
                    }
                }
            } catch (Throwable e) {
                System.err.println("[ERRO NO BOT JAVA]: " + e + " (neste frame o tanque ficou parado)");
            }
            originalOut.println(resposta);
            originalOut.flush();
        }
        scanner.close();
    }
}
