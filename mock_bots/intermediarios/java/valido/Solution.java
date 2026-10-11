import com.google.gson.JsonObject;
import com.google.gson.JsonArray;

public class Solution {
    public static JsonObject pensar(JsonObject estado) {
        JsonObject acao = new JsonObject();
        acao.addProperty("throttle", -1.0); // Sempre em marcha re
        acao.addProperty("turn", 1.0);      // Girando igual um peao
        acao.addProperty("turret_angle", 0.0);
        acao.addProperty("shoot", true);    // Atirando loucamente
        acao.addProperty("shield", false);
        return acao;
    }
}
