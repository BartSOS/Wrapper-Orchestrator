import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

// Bot de teste do modo cenário (frente engine), em Java: vai até o primeiro ponto de
// estado.objetivo.pontos e para dentro do raio. Sem a chave, fica parado.
public class VaiPontoJava {
    public static JsonObject pensar(JsonObject estado) {
        JsonObject eu = estado.getAsJsonObject("self");
        JsonObject acao = new JsonObject();
        acao.addProperty("throttle", 0.0);
        acao.addProperty("turn", 0.0);
        acao.addProperty("turret_angle", eu.get("turret_angle").getAsDouble());
        acao.addProperty("shoot", false);
        acao.addProperty("shield", false);
        if (!estado.has("objetivo")) return acao;
        JsonArray pontos = estado.getAsJsonObject("objetivo").getAsJsonArray("pontos");
        if (pontos.size() == 0) return acao;
        JsonObject p = pontos.get(0).getAsJsonObject();
        double dx = p.get("x").getAsDouble() - eu.get("x").getAsDouble();
        double dy = p.get("y").getAsDouble() - eu.get("y").getAsDouble();
        if (Math.hypot(dx, dy) <= p.get("raio").getAsDouble() * 0.5) return acao;
        double alvo = Math.toDegrees(Math.atan2(dy, dx));
        double erro = ((alvo - eu.get("angle").getAsDouble() + 540.0) % 360.0) - 180.0;
        acao.addProperty("turn", Math.max(-1.0, Math.min(1.0, erro / 9.0)));
        acao.addProperty("throttle", Math.abs(erro) < 30.0 ? 1.0 : 0.2);
        return acao;
    }
}
