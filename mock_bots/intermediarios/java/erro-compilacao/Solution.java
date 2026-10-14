import com.google.gson.JsonObject;

// Bot de teste (Java) - ERRO DE COMPILAÇÃO PROPOSITAL.
// Implementa public static JsonObject pensar(JsonObject estado) com a assinatura correta,
// para passar pela checagem estrutural do validador e chegar ao javac, que deve recusar:
//   1. tipos incompatíveis (String atribuída a double);
//   2. símbolo não declarado (velocidadeMaxima).
public class Solution {
    public static JsonObject pensar(JsonObject estado) {
        JsonObject self = estado.getAsJsonObject("self");
        double angulo = "noventa graus";
        JsonObject acao = new JsonObject();
        acao.addProperty("throttle", velocidadeMaxima);
        acao.addProperty("turn", 0.0);
        acao.addProperty("turret_angle", angulo + self.get("angle").getAsDouble());
        acao.addProperty("shoot", true);
        acao.addProperty("shield", false);
        return acao;
    }
}
