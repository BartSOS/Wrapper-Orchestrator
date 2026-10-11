// =============================================================================
// TOPCOM ARENA - STARTER KIT JAVA
// =============================================================================
// Instruções para o competidor:
// - Implemente apenas o método estático pensar(JsonObject estado).
// - Não utilize System.in, System.out nem crie o método main().
// - A arena compila seu bot junto com o executor oficial e biblioteca Gson.
// =============================================================================

import com.google.gson.JsonObject;

public class starter_kit {

    public static JsonObject pensar(JsonObject estado) {
        JsonObject self = estado.getAsJsonObject("self");
        double meuAngulo = self.get("angle").getAsDouble();

        // Exemplo de ação inicial: andar para frente e girar suavemente
        JsonObject acao = new JsonObject();
        acao.addProperty("throttle", 1.0);
        acao.addProperty("turn", 0.5);
        acao.addProperty("turret_angle", meuAngulo);
        acao.addProperty("shoot", false);
        acao.addProperty("shield", false);

        return acao;
    }
}
